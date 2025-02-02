#include "ESP.h"

#include "../../Vars.h"
#include "../../Color.h"

#include "../../AntiHack/CheaterDetection.h"
#include "../../Backtrack/Backtrack.h"

namespace S
{
	MAKE_SIGNATURE(CTFPlayerSharedUtils_GetEconItemViewByLoadoutSlot, CLIENT_DLL, "55 8B EC 83 EC 08 53 56 57 8B 7D 08 33 F6 8B 87 ? ? ? ? 81 C7 ? ? ? ? 89 45 FC 8D 49 00", 0x0);
	MAKE_SIGNATURE(C_EconItemView_GetItemName, CLIENT_DLL, "56 8B F1 C6 86 ? ? ? ? ? E8 ? ? ? ? 8B 8E ? ? ? ? 5E 85 C9 75 06", 0x0);
}

void CESP::Run()
{
	if (!Vars::ESP::Main::Active.Value)
		return;

	if (const auto& pLocal = g_EntityCache.GetLocal())
	{
		DrawWorld();
		DrawBuildings(pLocal);
		DrawPlayers(pLocal);
	}
}

bool CESP::GetDrawBounds(CBaseEntity* pEntity, int& x, int& y, int& w, int& h)
{
	/*
	Vec3 vMins, vMaxs;
	if (pEntity->IsPlayer())
	{	// is this necessary?
		const bool bIsDucking = pEntity->IsDucking();
		vMins = I::GameMovement->GetPlayerMins(bIsDucking);
		vMaxs = I::GameMovement->GetPlayerMaxs(bIsDucking);
	}
	else
	{
		vMins = pEntity->m_vecMins();
		vMaxs = pEntity->m_vecMaxs();
	}
	*/
	Vec3 vMins = pEntity->m_vecMins(), vMaxs = pEntity->m_vecMaxs();

	matrix3x4& transform = pEntity->GetRgflCoordinateFrame();
	if (pEntity && pEntity->GetIndex() == I::EngineClient->GetLocalPlayer())
	{
		Vec3 vAngles = I::EngineClient->GetViewAngles();
		vAngles.x = vAngles.z = 0.0f;
		Math::AngleMatrix(vAngles, transform);
		Math::MatrixSetColumn(pEntity->GetAbsOrigin(), 3, transform);
	}

	float left = 0.f, right = 0.f, top = 0.f, bottom = 0.f;
	const Vec3 vPoints[] =
	{
		Vec3(0.f, 0.f, vMins.z),
		Vec3(0.f, 0.f, vMaxs.z),
		Vec3(vMins.x, vMins.y, vMaxs.z * 0.5f),
		Vec3(vMins.x, vMaxs.y, vMaxs.z * 0.5f),
		Vec3(vMaxs.x, vMins.y, vMaxs.z * 0.5f),
		Vec3(vMaxs.x, vMaxs.y, vMaxs.z * 0.5f)
	};
	for (int n = 0; n < 6; n++)
	{
		Vec3 trans; Math::VectorTransform(vPoints[n], transform, trans);

		Vec3 vScreenPos;
		if (!Utils::W2S(trans, vScreenPos))
			return false;

		if (vScreenPos.x < -g_ScreenSize.w
			|| vScreenPos.x > g_ScreenSize.w * 2
			|| vScreenPos.y < -g_ScreenSize.h
			|| vScreenPos.y > g_ScreenSize.h * 2)
			return false;

		left = n ? std::min(left, vScreenPos.x) : vScreenPos.x;
		right = n ? std::max(right, vScreenPos.x) : vScreenPos.x;
		top = n ? std::max(top, vScreenPos.y) : vScreenPos.y;
		bottom = n ? std::min(bottom, vScreenPos.y) : vScreenPos.y;
	}

	float x_ = left;
	const float y_ = bottom;
	float w_ = right - left;
	const float h_ = top - bottom;

	//if (Vars::ESP::Players::Box.Value)
	//{
		x_ += (right - left) / 8.0f;
		w_ -= (right - left) / 8.0f * 2.0f;
	//}

	x = static_cast<int>(x_);
	y = static_cast<int>(y_);
	w = static_cast<int>(w_);
	h = static_cast<int>(h_);

	return !(x > g_ScreenSize.w || x + w < 0 || y > g_ScreenSize.h || y + h < 0);
}

void CESP::DrawPlayers(CBaseEntity* pLocal)
{
	if (!Vars::ESP::Players::Active.Value)
		return;

	CTFPlayerResource* cResource = g_EntityCache.GetPR();
	if (!cResource)
		return;

	for (const auto& pPlayer : g_EntityCache.GetGroup(EGroupType::PLAYERS_ALL))
	{
		const int nIndex = pPlayer->GetIndex();

		if (!pPlayer->IsAlive() || pPlayer->IsAGhost())
			continue;

		if (pPlayer->GetDormant())
		{
			if (!Vars::ESP::Main::DormantAlpha.Value)
				continue;
			if (Vars::ESP::Main::DormantPriority.Value)
			{
				PlayerInfo_t pi{};
				if (I::EngineClient->GetPlayerInfo(nIndex, &pi) && G::PlayerPriority[pi.friendsID].Mode < 3)
					continue;
			}
			pPlayer->m_iHealth() = cResource->GetHealth(pPlayer->GetIndex());
		}

		I::VGuiSurface->DrawSetAlphaMultiplier((pPlayer->GetDormant() ? Vars::ESP::Main::DormantAlpha.Value : Vars::ESP::Main::ActiveAlpha.Value) / 255.f);

		if (nIndex != I::EngineClient->GetLocalPlayer())
		{
			if (Vars::ESP::Players::IgnoreCloaked.Value && pPlayer->IsCloaked())
				continue;

			if (Vars::ESP::Players::IgnoreTeam.Value && pPlayer->m_iTeamNum() == pLocal->m_iTeamNum())
			{
				if (Vars::ESP::Players::IgnoreFriends.Value || !g_EntityCache.IsFriend(nIndex))
					continue;
			}
		}
		else if (Vars::ESP::Players::IgnoreLocal.Value)
			continue;

		int x = 0, y = 0, w = 0, h = 0;
		if (GetDrawBounds(pPlayer, x, y, w, h))
		{
			int lOffset = 0, rOffset = 0, bOffset = 2, tOffset = 0;
			const auto& fFontEsp = g_Draw.GetFont(FONT_ESP), & fFontName = g_Draw.GetFont(FONT_NAME);

			const Color_t drawColor = GetTeamColor(pPlayer->m_iTeamNum(), Vars::ESP::Main::EnableTeamEnemyColors.Value);
			const int nMaxHealth = pPlayer->GetMaxHealth(), nHealth = pPlayer->m_iHealth(), nClassNum = pPlayer->m_iClass();

			// Bone ESP
			if (Vars::ESP::Players::Bones.Value)
			{
				DrawBones(pPlayer, { 8, 7, 6, 4 }, drawColor);
				DrawBones(pPlayer, { 11, 10, 9, 4 }, drawColor);
				DrawBones(pPlayer, { 0, 4, 1 }, drawColor);
				DrawBones(pPlayer, { 14, 13, 1 }, drawColor);
				DrawBones(pPlayer, { 17, 16, 1 }, drawColor);
			}

			// Box
			if (Vars::ESP::Players::Box.Value)
				g_Draw.LineRectOutline(x, y, w, h, drawColor, { 0, 0, 0, 255 });

			// Health bar
			if (Vars::ESP::Players::HealthBar.Value)
			{
				const float ratio = float(std::min(nHealth, nMaxHealth)) / nMaxHealth;
				Color_t cColor = nHealth > nMaxHealth ? Vars::Colors::Overheal.Value : Vars::Colors::HealthBar.Value.StartColor.lerp(Vars::Colors::HealthBar.Value.EndColor, ratio);

				g_Draw.FillRectPercent(x - 6, y, 2, h, ratio, cColor, { 0, 0, 0, 255 }, ALIGN_BOTTOM, true);

				lOffset += 5;
			}

			// Health text
			if (Vars::ESP::Players::HealthText.Value)
				g_Draw.String(fFontEsp, x - 5 - lOffset, y + h - h * (float(std::min(nHealth, nMaxHealth)) / nMaxHealth) - 2, nHealth > nMaxHealth ? Vars::Colors::Overheal.Value : Vars::Menu::Theme::Active.Value, ALIGN_TOPRIGHT, "%d", nHealth);

			// Ubercharge bar/text
			if (nClassNum == CLASS_MEDIC)
			{
				if (const auto& pMedGun = pPlayer->GetWeaponFromSlot(SLOT_SECONDARY))
				{
					if (Vars::ESP::Players::UberText.Value)
						g_Draw.String(fFontEsp, x + w + 4, y + h, Vars::Menu::Theme::Active.Value, ALIGN_TOPLEFT, L"%.0f%%", pMedGun->m_flChargeLevel() * 100.0f);

					if (Vars::ESP::Players::UberBar.Value)
					{
						const float flMaxUber = (pMedGun->m_iItemDefinitionIndex() == Medic_s_TheVaccinator ? 400.0f : 100.0f);
						const float flUber = std::min(pMedGun->m_flChargeLevel() * flMaxUber, flMaxUber);
						float ratio = flUber / flMaxUber;

						g_Draw.FillRectPercent(x, y + h + 4, w, 2, ratio, Vars::Colors::UberBar.Value);

						bOffset += 5;
					}
				}
			}

			// Name + priority text
			PlayerInfo_t pi{};
			if (I::EngineClient->GetPlayerInfo(nIndex, &pi))
			{
				const int middle = x + w / 2;
				if (Vars::ESP::Players::Name.Value)
				{
					tOffset += fFontName.nTall + 2;
					g_Draw.String(fFontName, middle, y - tOffset, Vars::Menu::Theme::Active.Value, ALIGN_TOP, Utils::ConvertUtf8ToWide(pi.name).data());
				}

				if (Vars::ESP::Players::PriorityText.Value)
				{
					switch (G::PlayerPriority[pi.friendsID].Mode)
					{
					case 4:
						if (!g_EntityCache.IsFriend(nIndex))
						{
							tOffset += fFontName.nTall + 2;
							g_Draw.String(fFontName, middle, y - tOffset, { 255, 0, 0, 255 }, ALIGN_TOP, "CHEATER");
						}
						break;
					case 3:
						tOffset += fFontName.nTall + 2;
						g_Draw.String(fFontName, middle, y - tOffset, { 255, 255, 0, 255 }, ALIGN_TOP, "RAGE");
						break;
					case 1:
						tOffset += fFontName.nTall + 2;
						g_Draw.String(fFontName, middle, y - tOffset, { 255, 255, 255, 255 }, ALIGN_TOP, "IGNORED");
						break;
					case 0:
						if (pPlayer != pLocal && g_EntityCache.IsFriend(nIndex))
						{
							tOffset += fFontName.nTall + 2;
							g_Draw.String(fFontName, middle, y - tOffset, Vars::Colors::Friend.Value, ALIGN_TOP, "FRIEND");
						}
						break;
					}
				}
			}

			// Class icon
			if (Vars::ESP::Players::ClassIcon.Value)
			{
				const int size = 18 * Vars::Menu::DPI.Value;
				g_Draw.Texture(x + w / 2 - size / 2, y - tOffset - size, size, size, { 255, 255, 255, 255 }, nClassNum);
			}

			// Class text
			if (Vars::ESP::Players::ClassText.Value)
			{
				g_Draw.String(fFontEsp, x + w + 4, y + rOffset, Vars::Menu::Theme::Active.Value, ALIGN_TOPLEFT, L"%ls", GetPlayerClass(nClassNum));
				rOffset += fFontEsp.nTall;
			}

			// Distance
			if (Vars::ESP::Players::Distance.Value)
			{
				if (pPlayer != pLocal)
				{
					const Vec3 vDelta = pPlayer->GetAbsOrigin() - pLocal->GetAbsOrigin();
					const float flDistance = vDelta.Length2D();

					const int Distance = round(flDistance / 52.49);
					g_Draw.String(fFontEsp, x + w / 2, y + h + bOffset, Vars::Menu::Theme::Active.Value, ALIGN_TOP, L"%dM", Distance);
					bOffset += fFontEsp.nTall;
				}
			}

			const auto& pWeapon = pPlayer->GetActiveWeapon();
			if (pWeapon)
			{
				// Weapon text
				if (Vars::ESP::Players::WeaponText.Value)
				{
					static auto getEconItemViewByLoadoutSlot = reinterpret_cast<void* (__cdecl*)(void*, int, void**)>(S::CTFPlayerSharedUtils_GetEconItemViewByLoadoutSlot());
					static auto getItemName = reinterpret_cast<const char* (__thiscall*)(void*)>(S::C_EconItemView_GetItemName());

					int iWeaponSlot = pWeapon->GetSlot();
					int iPlayerClass = pPlayer->m_iClass();

					const char* szItemName = "";

					switch (iPlayerClass)
					{
					case CLASS_SPY:
					{
						switch (iWeaponSlot)
						{
						// Primary (gun)
						case 0: iWeaponSlot = 1; break;
						// Sapper ????
						case 1: iWeaponSlot = 4; break;
						// Knife
						//case 2:
						// Disguise kit
						case 3: iWeaponSlot = 5; break;
						}
						break;
					}
					case CLASS_ENGINEER:
					{
						switch (iWeaponSlot)
						{
						case 3: iWeaponSlot = 5; break;
						case 4: iWeaponSlot = 6; break;
						}
						break;
					}
					}

					void* pCurItemData = getEconItemViewByLoadoutSlot(pPlayer, iWeaponSlot, 0);
					if (pCurItemData)
					{
						szItemName = getItemName(pCurItemData);
						g_Draw.String(fFontEsp, x + (w / 2), y + h + bOffset, Vars::Menu::Theme::Active.Value, ALIGN_TOP, "%ls", szItemName);
						bOffset += fFontEsp.nTall;
					}
				}

				// Weapon icons
				if (Vars::ESP::Players::WeaponIcon.Value)
				{
					if (CHudTexture* pIcon = pWeapon->GetWeaponIcon())
					{
						const float iw = pIcon->Width(), ih = pIcon->Height();
						const float scale = std::clamp(float(w) / std::max(iw, ih * 2), 0.25f, 0.75f) * Vars::Menu::DPI.Value;
						g_Draw.DrawHudTexture(x + float(w) / 2.f - iw / 2.f * scale, y + h + 1 + bOffset, scale, pIcon, Vars::Menu::Theme::Active.Value);
					}
				}
			}

			// Player conditions
			{
				// Lagcomp cond, idea from nitro
				if (!pPlayer->GetDormant() && Vars::ESP::Players::Conditions.Value & (1 << 3) && pPlayer != pLocal)
				{
					if (F::Backtrack.mRecords[pPlayer].size() < 3)
					{
						g_Draw.String(fFontEsp, x + w + 4, y + rOffset, { 255, 95, 95, 255 }, ALIGN_TOPLEFT, "LAGCOMP");
						rOffset += fFontEsp.nTall;
					}
					/*
					const float flDelta = pPlayer->m_flSimulationTime() - pPlayer->m_flOldSimulationTime();
					if (TIME_TO_TICKS(flDelta) != 1)
					{
						bool bDisplay = F::Backtrack.mRecords[pPlayer].size() < 3;
						if (!bDisplay)
						{
							const Vec3 vPrevOrigin = F::Backtrack.mRecords[pPlayer].front().vOrigin;
							const Vec3 vDelta = pPlayer->GetAbsOrigin() - vPrevOrigin;
							if (vDelta.Length2DSqr() > 4096.f)
								bDisplay = true;
						}
						if (bDisplay)
						{
							g_Draw.String(fFontEsp, x + w + 4, y + rOffset, { 255, 95, 95, 255 }, ALIGN_TOPLEFT, "LAGCOMP");
							rOffset += fFontEsp.nTall;
						}
					}
					*/
				}

				// Ping warning, idea from nitro
				if (Vars::ESP::Players::Conditions.Value & (1 << 5) && pPlayer != pLocal)
				{
					int ping = cResource->GetPing(pPlayer->GetIndex());
					if (const INetChannel* netChannel = I::EngineClient->GetNetChannelInfo()) // safety net
					{
						if (!netChannel->IsLoopback() && ping != 0 && (ping >= 200 || ping <= 5))
						{
							g_Draw.String(fFontEsp, x + w + 4, y + rOffset, { 255, 95, 95, 255 }, ALIGN_TOPLEFT, "%dMS", ping);
							rOffset += fFontEsp.nTall;
						}
					}
				}

				// Idea from rijin
				if (Vars::ESP::Players::Conditions.Value & (1 << 4) && pPlayer != pLocal)
				{
					const int kills = cResource->GetKills(pPlayer->GetIndex());
					const int deaths = cResource->GetDeaths(pPlayer->GetIndex());
					if (deaths > 1)
					{
						const int kd = kills / deaths;
						if (kills >= 12 && kd >= 6) //dont just say they have a high kd because they just joined and got a couple kills
						{
							g_Draw.String(fFontEsp, x + w + 4, y + rOffset, { 255, 95, 95, 255 }, ALIGN_TOPLEFT, "HIGH K/D [%d/%d]", kills, deaths);
							rOffset += fFontEsp.nTall;
						}
					}
					else if (kills >= 12)
					{
						g_Draw.String(fFontEsp, x + w + 4, y + rOffset, { 255, 95, 95, 255 }, ALIGN_TOPLEFT, "HIGH K/D [%d]", kills);
						rOffset += fFontEsp.nTall;
					}
				}

				{ // this is here just so i can collapse this entire section to reduce clutter
					auto drawCond = [&rOffset, &fFontEsp](const char* text, Color_t color, int x, int y)
					{
						g_Draw.String(fFontEsp, x, y + rOffset, color, ALIGN_TOPLEFT, text);
						rOffset += fFontEsp.nTall;
					};

					if (Vars::ESP::Players::Conditions.Value & (1 << 0))
					{
						if (pPlayer->InCond(TF_COND_CRITBOOSTED) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_PUMPKIN) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_USER_BUFF) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_DEMO_CHARGE) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_FIRST_BLOOD) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_BONUS_TIME) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_CTF_CAPTURE) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_ON_KILL) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_RAGE_BUFF) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_CARD_EFFECT) ||
							pPlayer->InCond(TF_COND_CRITBOOSTED_RUNE_TEMP))
							drawCond("CRITS", { 255, 107, 108, 255 }, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_ENERGY_BUFF) ||
							pPlayer->InCond(TF_COND_NOHEALINGDAMAGEBUFF))
							drawCond("MINI-CRITS", { 254, 202, 87, 255 }, x + w + 4, y);

						if (pPlayer->m_iHealth() > pPlayer->GetMaxHealth())
							drawCond("HP", Vars::Colors::Overheal.Value, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_HEALTH_BUFF) || pPlayer->InCond(TF_COND_MEGAHEAL) || pPlayer->IsBuffedByKing())
							drawCond("HP+", Vars::Colors::Overheal.Value, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_INVULNERABLE) ||
							pPlayer->InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED) ||
							pPlayer->InCond(TF_COND_INVULNERABLE_USER_BUFF) ||
							pPlayer->InCond(TF_COND_INVULNERABLE_CARD_EFFECT))
							drawCond("UBER", Vars::Colors::UberBar.Value, x + w + 4, y);
						else if (pPlayer->InCond(TF_COND_PHASE))
							drawCond("BONK", { 254, 202, 87, 255 }, x + w + 4, y);

						/* vaccinator effects */
						if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST) || pPlayer->InCond(TF_COND_BULLET_IMMUNE))
							drawCond("BULLET+", { 255, 107, 108, 255 }, x + w + 4, y);
						else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_BULLET_RESIST))
							drawCond("BULLET", { 255, 107, 108, 255 }, x + w + 4, y);
						if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BLAST_RESIST) || pPlayer->InCond(TF_COND_BLAST_IMMUNE))
							drawCond("BLAST+", { 255, 107, 108, 255 }, x + w + 4, y);
						else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_BLAST_RESIST))
							drawCond("BLAST", { 255, 107, 108, 255 }, x + w + 4, y);
						if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_FIRE_RESIST) || pPlayer->InCond(TF_COND_FIRE_IMMUNE))
							drawCond("FIRE+", { 255, 107, 108, 255 }, x + w + 4, y);
						else if (pPlayer->InCond(TF_COND_MEDIGUN_SMALL_FIRE_RESIST))
							drawCond("FIRE", { 255, 107, 108, 255 }, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_OFFENSEBUFF))
							drawCond("BANNER", drawColor, x + w + 4, y);
						if (pPlayer->InCond(TF_COND_DEFENSEBUFF))
							drawCond("BATTALIONS", drawColor, x + w + 4, y);
						if (pPlayer->InCond(TF_COND_REGENONDAMAGEBUFF))
							drawCond("CONCH", drawColor, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_BLASTJUMPING))
							drawCond("BLASTJUMP", { 254, 202, 87, 255 }, x + w + 4, y);
					}
					
					if (Vars::ESP::Players::Conditions.Value & (1 << 1))
					{
						if (pPlayer->InCond(TF_COND_URINE))
							drawCond("JARATE", { 254, 202, 87, 255 }, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_MARKEDFORDEATH) || pPlayer->InCond(TF_COND_MARKEDFORDEATH_SILENT))
							drawCond("MARKED", { 254, 202, 87, 255 }, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_BURNING))
							drawCond("BURN", { 254, 202, 87, 255 }, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_MAD_MILK))
							drawCond("MILK", { 254, 202, 87, 255 }, x + w + 4, y);
					}
					
					if (Vars::ESP::Players::Conditions.Value & (1 << 2))
					{
						if (Vars::Visuals::RemoveTaunts.Value && pPlayer->InCond(TF_COND_TAUNTING)) // i dont really see a need for this condition unless you have this enabled
							drawCond("TAUNT", { 255, 100, 200, 255 }, x + w + 4, y);

						if (pPlayer->m_bFeignDeathReady())
							drawCond("DR", { 254, 202, 87, 255 }, x + w + 4, y);
				
						if (pPlayer->InCond(TF_COND_AIMING))
						{
							if (const auto& pWeapon = pPlayer->GetActiveWeapon())
							{
								if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN)
									drawCond("REV", { 128, 128, 128, 255 }, x + w + 4, y);

								if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
								{
									if ((I::GlobalVars->curtime - pWeapon->m_flChargeBeginTime()) >= 1.0f)
										drawCond("CHARGED", { 254, 202, 87, 255 }, x + w + 4, y);
									else
										drawCond("CHARGING", { 254, 202, 87, 255 }, x + w + 4, y);
								}

								if (pWeapon->GetWeaponID() == TF_WEAPON_PARTICLE_CANNON)
									drawCond("CHARGING", { 254, 202, 87, 255 }, x + w + 4, y);
							}
						}

						if (pPlayer->InCond(TF_COND_ZOOMED))
							drawCond("ZOOM", { 254, 202, 87, 255 }, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_STEALTHED) || pPlayer->InCond(TF_COND_STEALTHED_BLINK) || pPlayer->InCond(TF_COND_STEALTHED_USER_BUFF) || pPlayer->InCond(TF_COND_STEALTHED_USER_BUFF_FADING))
							drawCond(std::format("CLOAK {:.0f}%%", pPlayer->GetInvisPercentage()).c_str(), Vars::Colors::Cloak.Value, x + w + 4, y);

						if (pPlayer->InCond(TF_COND_DISGUISING) || pPlayer->InCond(TF_COND_DISGUISE_WEARINGOFF) || pPlayer->InCond(TF_COND_DISGUISED))
							drawCond("DISGUISE", { 254, 202, 87, 255 }, x + w + 4, y);
					}																				
				}
			}
		}
	}
	I::VGuiSurface->DrawSetAlphaMultiplier(1.0f);
}

void CESP::DrawBuildings(CBaseEntity* pLocal) const
{
	if (!Vars::ESP::Buildings::Active.Value)
		return;

	I::VGuiSurface->DrawSetAlphaMultiplier(Vars::ESP::Main::ActiveAlpha.Value);
	for (const auto& pBuilding : g_EntityCache.GetGroup(Vars::ESP::Buildings::IgnoreTeam.Value ? EGroupType::BUILDINGS_ENEMIES : EGroupType::BUILDINGS_ALL))
	{
		if (!pBuilding->IsAlive())
			continue;

		const auto& oBuilding = reinterpret_cast<CBaseObject*>(pBuilding);

		int x = 0, y = 0, w = 0, h = 0;
		if (GetDrawBounds(oBuilding, x, y, w, h))
		{
			int lOffset = 0, rOffset = 0, /*bOffset = 0, */tOffset = 0;
			const auto& fFontEsp = g_Draw.GetFont(FONT_ESP), & fFontName = g_Draw.GetFont(FONT_NAME);

			const Color_t drawColor = GetTeamColor(oBuilding->m_iTeamNum(), Vars::ESP::Main::EnableTeamEnemyColors.Value);
			const int nMaxHealth = oBuilding->GetMaxHealth(), nHealth = std::min(oBuilding->GetHealth(), nMaxHealth);

			const auto nType = static_cast<EBuildingType>(oBuilding->GetType());
			const bool bIsMini = oBuilding->GetMiniBuilding();

			// Box
			if (Vars::ESP::Buildings::Box.Value)
				g_Draw.LineRectOutline(x, y, w, h, drawColor, { 0, 0, 0, 255 });

			// Health bar
			if (Vars::ESP::Buildings::HealthBar.Value)
			{
				const float ratio = float(nHealth) / nMaxHealth;
				Color_t cColor = Vars::Colors::HealthBar.Value.StartColor.lerp(Vars::Colors::HealthBar.Value.EndColor, ratio);

				g_Draw.FillRectPercent(x - 6, y, 2, h, ratio, cColor, { 0, 0, 0, 255 }, ALIGN_BOTTOM, true);

				lOffset += 8;
			}

			// Health text
			if (Vars::ESP::Buildings::HealthText.Value)
				g_Draw.String(fFontEsp, x - 2 - lOffset, (y + h) - (float(nHealth) / nMaxHealth * h) - 2, nHealth > nMaxHealth ? Vars::Colors::Overheal.Value : Vars::Menu::Theme::Active.Value, ALIGN_TOPRIGHT, "%d", nHealth);

			// Name
			if (Vars::ESP::Buildings::Name.Value)
			{
				const wchar_t* szName;

				switch (nType)
				{
				case EBuildingType::SENTRY:
					szName = bIsMini ? L"Mini-Sentry" : L"Sentry";
					break;
				case EBuildingType::DISPENSER:
					szName = L"Dispenser";
					break;
				case EBuildingType::TELEPORTER:
					szName = oBuilding->GetObjectMode() ? L"Teleporter Exit" : L"Teleporter Entrance";
					break;
				default: szName = L"Unknown"; break;
				}

				tOffset += fFontName.nTall + 2;
				g_Draw.String(fFontName, x + w / 2, y - tOffset, Vars::Menu::Theme::Active.Value, ALIGN_TOP, szName);
			}

			// Distance
			if (Vars::ESP::Buildings::Distance.Value)
			{
				const Vec3 vDelta = pBuilding->GetAbsOrigin() - pLocal->GetAbsOrigin();
				const float flDistance = vDelta.Length2D();

				const int Distance = round(flDistance / 52.49);
				g_Draw.String(fFontEsp, x + (w / 2), y + h, Vars::Menu::Theme::Active.Value, ALIGN_TOP, L"%dM", Distance);
			}

			// Building owner
			if (Vars::ESP::Buildings::Owner.Value && !oBuilding->GetMapPlaced())
			{
				if (const auto& pOwner = oBuilding->GetOwner())
				{
					PlayerInfo_t pi;
					if (I::EngineClient->GetPlayerInfo(pOwner->GetIndex(), &pi))
					{
						tOffset += fFontEsp.nTall + 2;
						g_Draw.String(fFontEsp, x + w / 2, y - tOffset, { 254, 202, 87, 255 }, ALIGN_TOP, L"Owner: %ls", Utils::ConvertUtf8ToWide(pi.name).data());
					}
				}
			}

			// Building level
			if (Vars::ESP::Buildings::Level.Value && !bIsMini)
			{
				g_Draw.String(fFontEsp, x + w + 4, y + rOffset, { 254, 202, 87, 255 }, ALIGN_TOPLEFT, L"%d/%d",
					oBuilding->GetLevel(), oBuilding->GetHighestLevel());
				rOffset += fFontEsp.nTall;
			}

			// Building conditions
			if (Vars::ESP::Buildings::Condition.Value)
			{
				std::vector<std::wstring> condStrings{};

				const float flConstructed = oBuilding->GetConstructed() * 100.0f;
				if (flConstructed < 100.0f && static_cast<int>(flConstructed) != 0)
				{
					g_Draw.String(fFontEsp, x + w + 4, y + rOffset, { 254, 202, 87, 255 }, ALIGN_TOPLEFT, L"BUILDING: %0.f%%", flConstructed);
					rOffset += fFontEsp.nTall;
				}

				if (nType == EBuildingType::SENTRY && oBuilding->GetControlled())
					condStrings.emplace_back(L"WRANGLED");

				if (oBuilding->GetSapped())
					condStrings.emplace_back(L"SAPPED");
				else if (oBuilding->GetDisabled()) //Building->IsSpook()
					condStrings.emplace_back(L"DISABLED");

				if (oBuilding->IsSentrygun() && !oBuilding->GetConstructing())
				{
					int iShells;
					int iMaxShells;
					int iRockets;
					int iMaxRockets;

					oBuilding->GetAmmoCount(iShells, iMaxShells, iRockets, iMaxRockets);

					if (iShells == 0)
						condStrings.emplace_back(L"NO AMMO");
					if (!bIsMini && iRockets == 0)
						condStrings.emplace_back(L"NO ROCKETS");
				}

				if (!condStrings.empty())
				{
					for (auto& condString : condStrings)
					{
						g_Draw.String(fFontEsp, x + y + 2, y + rOffset, { 254, 202, 87, 255 }, ALIGN_TOPLEFT, condString.data());
						rOffset += fFontEsp.nTall;
					}
				}
			}
		}
	}
	I::VGuiSurface->DrawSetAlphaMultiplier(1.0f);
}

void CESP::DrawWorld() const
{
	if (!Vars::ESP::World::Active.Value)
		return;

	Vec3 vScreen = {};
	const auto& fFont = g_Draw.GetFont(FONT_NAME);

	I::VGuiSurface->DrawSetAlphaMultiplier(Vars::ESP::Main::ActiveAlpha.Value);

	for (const auto& health : g_EntityCache.GetGroup(EGroupType::WORLD_HEALTH))
	{
		int x = 0, y = 0, w = 0, h = 0;
		if (Vars::ESP::World::Health.Value && GetDrawBounds(health, x, y, w, h) && Utils::W2S(health->m_vecOrigin(), vScreen))
			g_Draw.String(fFont, vScreen.x, y, Vars::Colors::Health.Value, ALIGN_CENTER, L"Health");
	}

	for(const auto& ammo : g_EntityCache.GetGroup(EGroupType::WORLD_AMMO))
	{
		int x = 0, y = 0, w = 0, h = 0;
		if (Vars::ESP::World::Ammo.Value && GetDrawBounds(ammo, x, y, w, h) && Utils::W2S(ammo->m_vecOrigin(), vScreen))
			g_Draw.String(fFont, vScreen.x, y, Vars::Colors::Ammo.Value, ALIGN_CENTER, L"Ammo");
	}

	for (const auto& NPC : g_EntityCache.GetGroup(EGroupType::WORLD_NPC))
	{
		int x = 0, y = 0, w = 0, h = 0;
		if (Vars::ESP::World::NPC.Value && GetDrawBounds(NPC, x, y, w, h) && Utils::W2S(NPC->m_vecOrigin(), vScreen))
		{
			const wchar_t* szName;
			switch (NPC->GetClassID())
			{
			case ETFClassID::CHeadlessHatman:
				szName = L"Horseless Headless Horsemann"; break;
			case ETFClassID::CTFTankBoss:
				szName = L"Tank"; break;
			case ETFClassID::CMerasmus:
				szName = L"Merasmus"; break;
			case ETFClassID::CZombie:
				szName = L"Skeleton"; break;
			case ETFClassID::CEyeballBoss:
				szName = L"Monoculus"; break;
			default:
				szName = L"Unknown"; break;
			}

			const int nTextTopOffset = fFont.nTall * (5 / 4);
			g_Draw.String(fFont, x + w / 2, y - nTextTopOffset, Vars::Colors::NPC.Value, ALIGN_TOP, szName);
		}
	}

	for (const auto& Bomb : g_EntityCache.GetGroup(EGroupType::WORLD_BOMBS))
	{
		int x = 0, y = 0, w = 0, h = 0;
		if (Vars::ESP::World::Bomb.Value && GetDrawBounds(Bomb, x, y, w, h))
		{
			const wchar_t* szName;
			switch (Bomb->GetClassID())
			{
			case ETFClassID::CTFPumpkinBomb:
				szName = L"Pumpkin Bomb"; break;
			case ETFClassID::CTFGenericBomb:
				szName = L"Bomb"; break;
			default:
				szName = L"Unknown"; break;
			}

			const int nTextTopOffset = fFont.nTall * (5 / 4);
			g_Draw.String(fFont, x + w / 2, y - nTextTopOffset, Vars::Colors::Bomb.Value, ALIGN_TOP, szName);
		}
	}

	for (const auto& Book : g_EntityCache.GetGroup(EGroupType::WORLD_SPELLBOOK))
	{
		int x = 0, y = 0, w = 0, h = 0;
		if (Vars::ESP::World::Spellbook.Value && GetDrawBounds(Book, x, y, w, h))
		{
			const int nTextTopOffset = fFont.nTall * (5 / 4);
			g_Draw.String(fFont, x + w / 2, y - nTextTopOffset, Vars::Colors::Spellbook.Value, ALIGN_TOP, L"Spellbook");
		}
	}

	for (const auto& Gargy : g_EntityCache.GetGroup(EGroupType::WORLD_GARGOYLE))
	{
		int x = 0, y = 0, w = 0, h = 0;
		if (Vars::ESP::World::Gargoyle.Value && GetDrawBounds(Gargy, x, y, w, h))
		{
			const int nTextTopOffset = fFont.nTall * (5 / 4);
			g_Draw.String(fFont, x + w / 2, y - nTextTopOffset, Vars::Colors::Gargoyle.Value, ALIGN_TOP, L"Gargoyle");
		}
	}

	I::VGuiSurface->DrawSetAlphaMultiplier(1.0f);
}

const wchar_t* CESP::GetPlayerClass(int nClassNum)
{
	static const wchar_t* szClasses[] = {
		L"Unknown", L"Scout", L"Sniper", L"Soldier", L"Demoman",
		L"Medic", L"Heavy", L"Pyro", L"Spy", L"Engineer"
	};

	return nClassNum < 10 && nClassNum > 0 ? szClasses[nClassNum] : szClasses[0];
}

void CESP::DrawBones(CBaseEntity* pPlayer, const std::vector<int>& vecBones, Color_t clr)
{
	const size_t nMax = vecBones.size(), nLast = nMax - 1;
	for (size_t n = 0; n < nMax; n++)
	{
		if (n == nLast)
			continue;

		const auto vBone = pPlayer->GetHitboxPos(vecBones[n]);
		const auto vParent = pPlayer->GetHitboxPos(vecBones[n + 1]);

		Vec3 vScreenBone, vScreenParent;

		if (Utils::W2S(vBone, vScreenBone) && Utils::W2S(vParent, vScreenParent))
			g_Draw.Line(vScreenBone.x, vScreenBone.y, vScreenParent.x, vScreenParent.y, clr);
	}
}