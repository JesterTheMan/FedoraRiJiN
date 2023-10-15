#include "MovementSimulation.h"
#pragma warning (disable : 4018)
#pragma warning (disable : 4552)

//we'll use this to set current player's command, without it CGameMovement::CheckInterval will try to access a nullptr
static CUserCmd DummyCmd = {};

bool CMovementSimulation::GetVelocity(CBaseEntity* pEntity, Vec3& vVelOut, bool bMoveData)
{
	if (!pEntity)
		return false;

	bool bReturn = false;
	
	switch (Vars::Aimbot::Projectile::VelMode.Value)
	{
	case 0:
	{
		const int iEntIndex = pEntity->GetIndex();
		if (m_Positions[iEntIndex].size() > 1)
		{
			const auto& vRecord1 = m_Positions[iEntIndex][0];
			const auto& vRecord2 = m_Positions[iEntIndex][1];

			vVelOut = (vRecord1.m_vecOrigin - vRecord2.m_vecOrigin) / (vRecord1.m_flSimTime - vRecord2.m_flSimTime);
			bReturn = true;
		}
		break;
	}
	case 1:
	{
		vVelOut = pEntity->m_vecVelocity();
		bReturn = true;
		break;
	}
	case 2:
	{
		vVelOut = pEntity->GetVelocity();
		bReturn = true;
		break;
	}
	case 3:
	{
		vVelOut = pEntity->GetVecVelocity();
		bReturn = true;
		break;
	}
	case 4:
	{
		pEntity->EstimateAbsVelocity(vVelOut);
		bReturn = true;
		break;
	}
	}

	if (bReturn)
	{
		if (pEntity->m_fFlags() & FL_ONGROUND && !vVelOut.IsZero())
			vVelOut.z = 0.f; // step fix
		if (bMoveData)
		{
			if (fabsf(vVelOut.x) < 0.01f)
				vVelOut.x = 0.015f;
			if (fabsf(vVelOut.y) < 0.01f)
				vVelOut.y = 0.015f;
		}
	}

	return bReturn;
}

void CMovementSimulation::FillInfo()
{
	const auto pLocal = g_EntityCache.GetLocal();
	if (!pLocal)
	{
		m_Positions.clear();
		m_Velocities.clear();
		return;
	}

	{ // hopefully this is more reliable (upd: i don't think it is)
		auto FillPositions = [](CBaseEntity* pEntity, std::map<int, std::deque<PositionData>>& m_Positions)
		{
			const int iEntIndex = pEntity->GetIndex();

			if (!pEntity->IsAlive() || pEntity->IsAGhost() || pEntity->GetDormant())
			{
				m_Positions[iEntIndex].clear();
				return;
			}

			const PositionData vRecord = { pEntity->GetVecOrigin(), pEntity->GetSimulationTime() };
			if (m_Positions[iEntIndex].size() > 0)
			{
				const PositionData vLast = m_Positions[iEntIndex][0];
				if (vRecord.m_flSimTime == vLast.m_flSimTime)
					return;
			}

			m_Positions[iEntIndex].push_front(vRecord);

			if (m_Positions[iEntIndex].size() > 2)
				m_Positions[iEntIndex].pop_back();
		};

		for (const auto& pEntity : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL))
		{
			if (pEntity && pEntity != pLocal) // only appears when in thirdperson ?
				FillPositions(pEntity, m_Positions);
		}
		FillPositions(pLocal, m_Positions);
	}

	if (Vars::Aimbot::Projectile::StrafePredictionGround.Value || Vars::Aimbot::Projectile::StrafePredictionAir.Value)
	{
		auto FillVelocity = [](CBaseEntity* pEntity, std::map<int, std::deque<VelocityData>>& m_Velocities, Vec3 vVelocity)
		{
			const int iEntIndex = pEntity->GetIndex();

			if (!pEntity->IsAlive() || pEntity->IsAGhost() || pEntity->GetDormant() || vVelocity.IsZero())
			{
				m_Velocities[iEntIndex].clear();
				return;
			}

			const VelocityData vRecord = {
				vVelocity,
				pEntity->GetSimulationTime(),
				bool(pEntity->GetFlags() & FL_ONGROUND)
			};

			if (m_Velocities[iEntIndex].size() > 0)
			{
				const VelocityData vLast = m_Velocities[iEntIndex][0];
				if (vRecord.m_flSimTime == vLast.m_flSimTime)
					return;

				const float flLastYaw = Math::VelocityToAngles(vRecord.m_vecVelocity).y;
				const float flYaw = Math::VelocityToAngles(vLast.m_vecVelocity).y;
				const float flDelta = RAD2DEG(Math::AngleDiffRad(DEG2RAD(flLastYaw), DEG2RAD(flYaw)));
				if (fabsf(flDelta) > 90.f)
					m_Velocities[iEntIndex].clear();

				if (vRecord.m_bGrounded != vLast.m_bGrounded)
				{
					m_Velocities[iEntIndex].clear();
					for (int i = 0; i < 2; i++)
						m_Velocities[iEntIndex].push_front(vRecord);
				}
			}

			m_Velocities[iEntIndex].push_front(vRecord);

			if (m_Velocities[iEntIndex].size() > 66)
				m_Velocities[iEntIndex].pop_back();
		};

		for (const auto& pEntity : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL))
		{
			if (pEntity && pEntity != pLocal) // only appears when in thirdperson ?
			{
				Vec3 vVelocity = {};
				if (GetVelocity(pEntity, vVelocity))
					FillVelocity(pEntity, m_Velocities, vVelocity);
			}
		}
		{
			Vec3 vVelocity = {};
			if (GetVelocity(pLocal, vVelocity))
				FillVelocity(pLocal, m_Velocities, vVelocity);
		}
	}
	else
		m_Velocities.clear();
}

bool CMovementSimulation::Initialize(CBaseEntity* pPlayer, PlayerStorage& playerStorageOut, bool useHitchance, bool cancelStrafe)
{
	const auto pLocal = g_EntityCache.GetLocal();
	if (!pLocal || !pPlayer || !pPlayer->IsPlayer() || pPlayer->deadflag())
	{
		playerStorageOut.m_bInitFailed = playerStorageOut.m_bFailed = true;
		return false;
	}

	G::MoveLines.clear();

	//set player
	playerStorageOut.m_pPlayer = pPlayer;

	//set current command
	playerStorageOut.m_pPlayer->SetCurrentCmd(&DummyCmd);

	//store player's data
	playerStorageOut.m_PlayerData.m_vecOrigin = pPlayer->GetAbsOrigin();
	playerStorageOut.m_PlayerData.m_vecVelocity = pPlayer->m_vecVelocity();
	playerStorageOut.m_PlayerData.m_vecBaseVelocity = pPlayer->m_vecBaseVelocity();
	playerStorageOut.m_PlayerData.m_vecViewOffset = pPlayer->m_vecViewOffset();
	playerStorageOut.m_PlayerData.m_hGroundEntity = pPlayer->m_hGroundEntity();
	playerStorageOut.m_PlayerData.m_vecWorldSpaceCenter = pPlayer->GetWorldSpaceCenter();
	playerStorageOut.m_PlayerData.m_fFlags = pPlayer->m_fFlags();
	playerStorageOut.m_PlayerData.m_flDucktime = pPlayer->m_flDucktime();
	playerStorageOut.m_PlayerData.m_flDuckJumpTime = pPlayer->m_flDuckJumpTime();
	playerStorageOut.m_PlayerData.m_bDucked = pPlayer->m_bDucked();
	playerStorageOut.m_PlayerData.m_bDucking = pPlayer->m_bDucking();
	playerStorageOut.m_PlayerData.m_bInDuckJump = pPlayer->m_bInDuckJump();
	playerStorageOut.m_PlayerData.m_flModelScale = pPlayer->m_flModelScale();

	//store vars
	m_bOldInPrediction = I::Prediction->m_bInPrediction;
	m_bOldFirstTimePredicted = I::Prediction->m_bFirstTimePredicted;
	m_flOldFrametime = I::GlobalVars->frametime;

	//the hacks that make it work
	{
		if (pPlayer->m_fFlags() & FL_DUCKING)
		{
			pPlayer->m_fFlags() &= ~FL_DUCKING; //breaks origin's z if FL_DUCKING is not removed
			pPlayer->m_bDucked() = true; //(mins/maxs will be fine when ducking as long as m_bDucked is true)
			pPlayer->m_flDucktime() = 0.0f;
			pPlayer->m_flDuckJumpTime() = 0.0f;
			pPlayer->m_bDucking() = false;
			pPlayer->m_bInDuckJump() = false;
		}

		pPlayer->m_flModelScale() -= 0.03125f; //fixes issues with corners

		if (pPlayer->m_fFlags() & FL_ONGROUND)
			pPlayer->SetAbsOrigin(pPlayer->GetVecOrigin() + Vector(0, 0, 0.03125f)); //to prevent getting stuck in the ground
		else
		{
			if (pPlayer != pLocal)
				pPlayer->m_hGroundEntity() = -1; // fix for velocity being set to 0 even if in air
		}

		/*
		//for some reason if xy vel is zero it doesn't predict
		if (fabsf(pPlayer->m_vecVelocity().x) < 0.01f)
			pPlayer->m_vecVelocity().x = 0.015f;
		if (fabsf(pPlayer->m_vecVelocity().y) < 0.01f)
			pPlayer->m_vecVelocity().y = 0.015f;
		*/
	}

	// setup move data
	if (!SetupMoveData(playerStorageOut))
	{
		playerStorageOut.m_bFailed = true;
		return false;
	}

	// calculate strafe if desired
	if (!cancelStrafe)
		StrafePrediction(playerStorageOut);

	// really hope this doesn't work like shit
	if (useHitchance && !pPlayer->m_vecVelocity().IsZero())
	{
		const auto& mVelocityRecords = m_Velocities[playerStorageOut.m_pPlayer->GetIndex()];
		const int iSamples = mVelocityRecords.size();

		float flCurrentChance = 1.f, flAverageYaw = 0.f, flCompareYaw = Math::VelocityToAngles(playerStorageOut.m_PlayerData.m_vecVelocity).y;
		for (int i = 0; i < iSamples; i++)
		{
			const float flRecordYaw = Math::VelocityToAngles(mVelocityRecords.at(i).m_vecVelocity).y;

			const float flDelta = RAD2DEG(Math::AngleDiffRad(DEG2RAD(flCompareYaw), DEG2RAD(flRecordYaw)));
			flAverageYaw += flDelta;

			if ((i + 1) % Vars::Aimbot::Projectile::iSamples.Value == 0 || i == iSamples - 1)
			{
				flAverageYaw /= i % Vars::Aimbot::Projectile::iSamples.Value + 1;
				if (fabsf(playerStorageOut.m_flAverageYaw - flAverageYaw) > 0.5f)
					flCurrentChance -= 1.f / ((iSamples - 1) / float(Vars::Aimbot::Projectile::iSamples.Value) + 1);
				flAverageYaw = 0.f;
			}

			flCompareYaw = flRecordYaw;
		}

		if (flCurrentChance < Vars::Aimbot::Projectile::StrafePredictionHitchance.Value)
		{
			playerStorageOut.m_bFailed = true;
			return false;
		}
	}

	return true;
}

bool CMovementSimulation::SetupMoveData(PlayerStorage& playerStorage)
{
	if (!playerStorage.m_pPlayer)
		return false;

	playerStorage.m_MoveData.m_vecAbsOrigin = playerStorage.m_pPlayer->GetVecOrigin();
	if (!GetVelocity(playerStorage.m_pPlayer, playerStorage.m_MoveData.m_vecVelocity, true))
		return false;

	playerStorage.m_MoveData.m_bFirstRunOfFunctions = false;
	playerStorage.m_MoveData.m_bGameCodeMovedPlayer = false;
	playerStorage.m_MoveData.m_nPlayerHandle = reinterpret_cast<IHandleEntity*>(playerStorage.m_pPlayer)->GetRefEHandle();
	//playerStorage.m_MoveData.m_vecVelocity = playerStorage.m_pPlayer->m_vecVelocity();	//	m_vecBaseVelocity hits -1950? // seems generally unreliable
	//if (playerStorage.m_pPlayer->m_fFlags() & FL_ONGROUND && !playerStorage.m_MoveData.m_vecVelocity.IsZero())
	//	playerStorage.m_MoveData.m_vecVelocity.z = 0.015f; // step fix
	playerStorage.m_MoveData.m_flMaxSpeed = playerStorage.m_pPlayer->TeamFortress_CalculateMaxSpeed();
	if (playerStorage.m_PlayerData.m_fFlags & FL_DUCKING)
		playerStorage.m_MoveData.m_flMaxSpeed *= 0.3333f;
	playerStorage.m_MoveData.m_flClientMaxSpeed = playerStorage.m_MoveData.m_flMaxSpeed;

	//need a better way to determine angles probably
	playerStorage.m_MoveData.m_vecViewAngles = { 0.f, Math::VelocityToAngles(playerStorage.m_MoveData.m_vecVelocity).y, 0.f };

	Vec3 vForward = {}, vRight = {};
	Math::AngleVectors(playerStorage.m_MoveData.m_vecViewAngles, &vForward, &vRight, nullptr);

	playerStorage.m_MoveData.m_flForwardMove = (playerStorage.m_MoveData.m_vecVelocity.y - vRight.y / vRight.x * playerStorage.m_MoveData.m_vecVelocity.x) / (vForward.y - vRight.y / vRight.x * vForward.x);
	playerStorage.m_MoveData.m_flSideMove = (playerStorage.m_MoveData.m_vecVelocity.x - vForward.x * playerStorage.m_MoveData.m_flForwardMove) / vRight.x;

	return true;
}

float CMovementSimulation::GetAverageYaw(const int iSamples, PlayerStorage& playerStorage)
{
	const int iEntIndex = playerStorage.m_pPlayer->GetIndex();
	const auto& mVelocityRecords = m_Velocities[iEntIndex];

	if (mVelocityRecords.size() < iSamples)
		return 0.f;

	float flAverageYaw = 0.f, flCompareYaw = playerStorage.m_MoveData.m_vecViewAngles.y;
	for (int i = 0; i < iSamples; i++)
	{
		const float flRecordYaw = Math::VelocityToAngles(mVelocityRecords.at(i).m_vecVelocity).y;

		const float flDelta = RAD2DEG(Math::AngleDiffRad(DEG2RAD(flCompareYaw), DEG2RAD(flRecordYaw)));
		flAverageYaw += flDelta;

		flCompareYaw = flRecordYaw;
	}
	flAverageYaw /= iSamples;
	fmod(flAverageYaw + 180.0f, 360.0f) - 180.0f;

	auto get_velocity_degree = [](float velocity)
	{
		auto tmp = RAD2DEG(atan(30.0f / velocity));

		#define CheckIfNonValidNumber(x) (fpclassify(x) == FP_INFINITE || fpclassify(x) == FP_NAN || fpclassify(x) == FP_SUBNORMAL)
		if (CheckIfNonValidNumber(tmp) || tmp > 90.0f)
			return 90.0f;

		else if (tmp < 0.0f)
			return 0.0f;
		else
			return tmp;
	};

	/*
	const float flMaxDelta = get_velocity_degree(fabsf(flAverageYaw)) / fmaxf((float)iSamples, 2.f);

	if (fabsf(flAverageYaw) > flMaxDelta) {
		m_Velocities[iEntIndex].clear();
		return 0.f;
	}	//	ugly fix for sweaty pricks
	*/

	if (Vars::Debug::DebugInfo.Value)
		Utils::ConLog("MovementSimulation", tfm::format("flAverageYaw calculated to %f from %i", flAverageYaw, iSamples).c_str(), { 83, 255, 83, 255 });

	return flAverageYaw;
}

void CMovementSimulation::StrafePrediction(PlayerStorage& playerStorage)
{
	if (!(playerStorage.m_pPlayer->m_fFlags() & FL_ONGROUND/*playerStorage.m_pPlayer->OnSolid()*/ ? Vars::Aimbot::Projectile::StrafePredictionGround.Value : Vars::Aimbot::Projectile::StrafePredictionAir.Value))
		return;

	float flAverageYaw = GetAverageYaw(Vars::Aimbot::Projectile::iSamples.Value, playerStorage);

	if (fabsf(flAverageYaw) < Vars::Aimbot::Projectile::StrafePredictionMinDifference.Value)
	{
		flAverageYaw = 0.f;
		return;
	}

	if (flAverageYaw == 0.f)
		return;

	if (playerStorage.m_pPlayer->m_fFlags() & FL_ONGROUND/*playerStorage.m_pPlayer->OnSolid()*/)
		playerStorage.m_MoveData.m_vecViewAngles.y += 22.5f * (flAverageYaw > 0.f ? 1.f : -1.f); // fix for ground strafe delay
	playerStorage.m_flAverageYaw = flAverageYaw;
}

void CMovementSimulation::RunTick(PlayerStorage& playerStorage)
{
	if (playerStorage.m_bFailed || !playerStorage.m_pPlayer || playerStorage.m_pPlayer->GetClassID() != ETFClassID::CTFPlayer)
		return;

	playerStorage.PredictionLines.push_back({ playerStorage.m_MoveData.m_vecAbsOrigin, Math::GetRotatedPosition( playerStorage.m_MoveData.m_vecAbsOrigin, Math::VelocityToAngles(playerStorage.m_MoveData.m_vecVelocity * Vec3(1, 1, 0)).Length2D() + 90, Vars::Visuals::SeperatorLength.Value ) });

	//make sure frametime and prediction vars are right
	I::Prediction->m_bInPrediction = true;
	I::Prediction->m_bFirstTimePredicted = false;
	I::GlobalVars->frametime = I::Prediction->m_bEnginePaused ? 0.0f : TICK_INTERVAL;
	
	float airCorrection = 0.f;
	if (playerStorage.m_flAverageYaw)
	{
		if (!(playerStorage.m_pPlayer->m_fFlags() & FL_ONGROUND)/*!playerStorage.m_pPlayer->OnSolid()*/ && playerStorage.m_flAverageYaw)
			airCorrection = 90.f * (playerStorage.m_flAverageYaw > 0.f ? 1.f : -1.f);

		playerStorage.m_MoveData.m_vecViewAngles.y += playerStorage.m_flAverageYaw + airCorrection;
	}
	//else
	//	playerStorage.m_MoveData.m_vecViewAngles.y = Math::VelocityToAngles(playerStorage.m_MoveData.m_vecVelocity).y;

	// occasionally offsets position
	// fucks up velocity (might be map specific?)
	I::TFGameMovement->ProcessMovement(playerStorage.m_pPlayer, &playerStorage.m_MoveData);

	playerStorage.m_MoveData.m_vecViewAngles.y -= airCorrection;
}

void CMovementSimulation::Restore(PlayerStorage& playerStorage)
{
	if (playerStorage.m_bInitFailed || !playerStorage.m_pPlayer)
		return;

	playerStorage.m_pPlayer->SetCurrentCmd(nullptr);

	playerStorage.m_pPlayer->SetAbsOrigin(playerStorage.m_PlayerData.m_vecOrigin);
	playerStorage.m_pPlayer->m_vecVelocity() = playerStorage.m_PlayerData.m_vecVelocity;
	playerStorage.m_pPlayer->m_vecBaseVelocity() = playerStorage.m_PlayerData.m_vecBaseVelocity;
	playerStorage.m_pPlayer->m_vecViewOffset() = playerStorage.m_PlayerData.m_vecViewOffset;
	playerStorage.m_pPlayer->m_hGroundEntity() = playerStorage.m_PlayerData.m_hGroundEntity;
	playerStorage.m_pPlayer->m_fFlags() = playerStorage.m_PlayerData.m_fFlags;
	playerStorage.m_pPlayer->m_flDucktime() = playerStorage.m_PlayerData.m_flDucktime;
	playerStorage.m_pPlayer->m_flDuckJumpTime() = playerStorage.m_PlayerData.m_flDuckJumpTime;
	playerStorage.m_pPlayer->m_bDucked() = playerStorage.m_PlayerData.m_bDucked;
	playerStorage.m_pPlayer->m_bDucking() = playerStorage.m_PlayerData.m_bDucking;
	playerStorage.m_pPlayer->m_bInDuckJump() = playerStorage.m_PlayerData.m_bInDuckJump;
	playerStorage.m_pPlayer->m_flModelScale() = playerStorage.m_PlayerData.m_flModelScale;

	I::Prediction->m_bInPrediction = m_bOldInPrediction;
	I::Prediction->m_bFirstTimePredicted = m_bOldFirstTimePredicted;
	I::GlobalVars->frametime = m_flOldFrametime;

	const bool bFailed = playerStorage.m_bInitFailed;
	memset(&playerStorage, 0, sizeof(PlayerStorage));
	playerStorage.m_bInitFailed = bFailed;
}