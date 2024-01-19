#include "Logs.h"

#include "../Vars.h"
#include "../Misc/Notifications/Notifications.h"
#include "../../Features/Visuals/Visuals.h"

int attackStringW;
int attackStringH;

static std::string namecolor({ '\x7', 'E', '3', 'B', '4', '5', '5' }); //E3B455
static std::string yellow({ '\x7', 'C', '8', 'A', '9', '0', '0' }); //C8A900
static std::string blue({ '\x7', '0', 'D', '9', '2', 'F', 'F' }); //0D92FF
static std::string red({ '\x7', 'F', 'F', '3', 'A', '3', 'A' }); //FF3A3A
static std::string green({ '\x7', '3', 'A', 'F', 'F', '4', 'D' }); //3AFF4D
static std::string white({ '\x7', 'F', 'F', 'F', 'F', 'F', 'F' }); //FFFFFF
static std::string gsyellow({ '\x7', 'C', 'A', 'C', 'A', 'C', 'A' }); //CACACA
static std::string gsblue({ '\x7', '7', '6', '7', '6', '7', '6' }); //767676
static std::string gsred({ '\x7', '7', '5', '7', '5', '7', '5' }); //757575
static std::string gsgreen({ '\x7', 'B', '0', 'B', '0', 'B', '0' }); //b0b0b0

enum VoteLogger
{
	VoteText = 1 << 0,
	VoteConsole = 1 << 1,
	VoteChat = 1 << 2,
	VoteParty = 1 << 3,
	VoteVerbose = 1 << 5
};

// Event info
void CLogs::Event(CGameEvent* pEvent, const FNV1A_t uNameHash)
{
	if (!I::EngineClient->IsConnected() || !I::EngineClient->IsInGame())
		return;

	if (const auto pLocal = g_EntityCache.GetLocal())
	{
		// Voting
		if (Vars::Logging::Logs.Value & (1 << 1) && uNameHash == FNV1A::HashConst("vote_cast"))
		{
			const auto pEntity = I::ClientEntityList->GetClientEntity(pEvent->GetInt("entityid"));
			if (pEntity && pEntity->IsPlayer())
			{
				const bool bVotedYes = pEvent->GetInt("vote_option") == 0;
				PlayerInfo_t pi{};
				I::EngineClient->GetPlayerInfo(pEntity->GetIndex(), &pi);

				std::string string = std::format("{}{} voted {}", (pEntity->m_iTeamNum() == pLocal->m_iTeamNum() ? "" : "[Other team vote] "), (pi.name), (bVotedYes ? "Yes" : "No"));
				std::string cstring = std::format("{}{}{}{}{} voted {}{}.", (white), (pEntity->m_iTeamNum() == pLocal->m_iTeamNum() ? "" : "[Other team vote] "), (namecolor), (pi.name), (white), (bVotedYes ? red : green), (bVotedYes ? "Yes" : "No"));

				if (Vars::Logging::VoteCast::LogTo.Value & (1 << 0))
					F::Notifications.Add(string);
				if (Vars::Logging::VoteCast::LogTo.Value & (1 << 1))
					I::ClientModeShared->m_pChatElement->ChatPrintf(0, std::format("{:06X}{} {}", 0x0C69CF, "[RiJiN]", cstring).c_str());
				if (Vars::Logging::VoteCast::LogTo.Value & (1 << 2))
					I::EngineClient->ClientCmd_Unrestricted(std::format("tf_party_chat \"{}\"", string).c_str());
				if (Vars::Logging::VoteCast::LogTo.Value & (1 << 3))
					Utils::ConLog("Vote Cast", string.c_str(), Vars::Menu::Theme::Accent.Value);
			}
		}

		// Class change
		if (Vars::Logging::Logs.Value & (1 << 2) && uNameHash == FNV1A::HashConst("player_changeclass"))
		{
			if (const auto& pEntity = I::ClientEntityList->GetClientEntity(
				I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"))))
			{
				if (pEntity == pLocal)
					return;

				PlayerInfo_t pi{};
				{
					const int nIndex = pEntity->GetIndex();
					I::EngineClient->GetPlayerInfo(nIndex, &pi);
					if (pi.fakeplayer)
						return; //dont spam chat by giving class changes for bots
				}

				std::string string = std::format("{} is now a {}", (pi.name), (Utils::GetClassByIndex(pEvent->GetInt("class"))));
				std::string cstring = std::format("{}{}{}{}{} is now a {}{}.", (white), "[RiJiN]", (pi.name), (white), " Player ", (Utils::GetClassByIndex(pEvent->GetInt("class"))));

				if (Vars::Logging::ClassChange::LogTo.Value & (1 << 0))
					F::Notifications.Add(string);
				if (Vars::Logging::ClassChange::LogTo.Value & (1 << 1))
					I::ClientModeShared->m_pChatElement->ChatPrintf(0, std::format("{:06X}{} {}", 0x0C69CF, "{RiJiN}", cstring).c_str());
				if (Vars::Logging::ClassChange::LogTo.Value & (1 << 2))
					I::EngineClient->ClientCmd_Unrestricted(std::format("tf_party_chat \"{}\"", string).c_str());
				if (Vars::Logging::ClassChange::LogTo.Value & (1 << 3))
					Utils::ConLog("Class Change", string.c_str(), Vars::Menu::Theme::Accent.Value);
			}
		}

		// Damage
		if (Vars::Logging::Logs.Value & (1 << 3) && uNameHash == FNV1A::HashConst("player_hurt"))
		{
			if (const auto pEntity = I::ClientEntityList->GetClientEntity(I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"))))
			{
				if (pEntity == pLocal)
					return;

				const auto nAttacker = pEvent->GetInt("attacker");
				const auto nHealth = pEvent->GetInt("health");
				const auto nDamage = pEvent->GetInt("damageamount");
				const auto bCrit = pEvent->GetBool("crit");
				const int nIndex = pEntity->GetIndex();

				PlayerInfo_t pi{};
				{
					I::EngineClient->GetPlayerInfo(I::EngineClient->GetLocalPlayer(), &pi);
					if (nAttacker != pi.userID)
						return;
					I::EngineClient->GetPlayerInfo(nIndex, &pi);
				}

				const auto maxHealth = pEntity->GetMaxHealth();
				std::string string = std::format("You hit {} for {} damage{}({} / {})", (pi.name), (nDamage), (bCrit ? " (crit) " : " "), (nHealth), (maxHealth));
				std::string cstring = std::format("{}You hit {}{}{} for {}{} damage{}{}({} / {}.)", (white), (namecolor), (pi.name), (white), (red), (nDamage), (bCrit ? " (crit) " : " "), (namecolor), (nHealth), (maxHealth));

				if (Vars::Logging::Damage::LogTo.Value & (1 << 0))
					F::Notifications.Add(string);
				if (Vars::Logging::Damage::LogTo.Value & (1 << 1))
					I::ClientModeShared->m_pChatElement->ChatPrintf(0, std::format("{:06X}{} {}", 0x0C69CF, "[RiJiN]", cstring).c_str());
				if (Vars::Logging::Damage::LogTo.Value & (1 << 2))
					I::EngineClient->ClientCmd_Unrestricted(std::format("tf_party_chat \"{}\"", string).c_str());
				if (Vars::Logging::Damage::LogTo.Value & (1 << 3))
					Utils::ConLog("Vote Cast", string.c_str(), Vars::Menu::Theme::Accent.Value);
			}
		}
	}
}

// User Message info
void CLogs::UserMessage(UserMessageType type, bf_read& msgData)
{
	if (Vars::Logging::Logs.Value & (1 << 0) && type == VoteStart)
	{
		const int team = msgData.ReadByte();
		const int voteID = msgData.ReadLong();
		const int caller = msgData.ReadByte();
		char reason[64], voteTarget[64];
		msgData.ReadString(reason, 64);
		msgData.ReadString(voteTarget, 64);
		const int target = static_cast<unsigned char>(msgData.ReadByte()) >> 1;

		PlayerInfo_t infoTarget{}, infoCaller{};
		if (const auto& pLocal = g_EntityCache.GetLocal())
		{
			if (target && caller && I::EngineClient->GetPlayerInfo(target, &infoTarget) && I::EngineClient->GetPlayerInfo(caller, &infoCaller))
			{
				const bool bSameTeam = team == pLocal->m_iTeamNum();

				std::string string = std::format("{}{} called a vote on {}", (bSameTeam ? "" : "[Enemy] "), (infoCaller.name), (infoTarget.name));
				std::string cstring = std::format("{}{}{}{}{} called a vote on {}{}.", (white), (bSameTeam ? "" : "[Enemy] "), (namecolor), (infoCaller.name), (white), (namecolor), (infoTarget.name));

				if (Vars::Logging::VoteStart::LogTo.Value & (1 << 0))
					F::Notifications.Add(string);
				if (Vars::Logging::VoteStart::LogTo.Value & (1 << 1))
					I::ClientModeShared->m_pChatElement->ChatPrintf(0, std::format("{:06X}{} {}", 0x0C69CF, "[RiJiN]", cstring).c_str());
				if (Vars::Logging::VoteStart::LogTo.Value & (1 << 2))
					I::EngineClient->ClientCmd_Unrestricted(std::format("tf_party_chat \"{}\"", string).c_str());
				if (Vars::Logging::VoteStart::LogTo.Value & (1 << 3))
					Utils::ConLog("Vote Call", string.c_str(), Vars::Menu::Theme::Accent.Value);
			}
		}
	}
}
