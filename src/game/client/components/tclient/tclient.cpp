#include "tclient.h"

#include <base/mem.h>

#include "data_version.h"

#include <base/log.h>

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#undef IMAGE_CURSOR
#undef IGNORE
#endif

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/external/regex.h>
#include <engine/external/tinyexpr.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/vram_stats.h>
#include <engine/shared/json.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/hud_layout.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#if defined(CONF_FAMILY_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

static constexpr const char *TCLIENT_INFO_URL = "https://api.github.com/repos/Kyouki-Arx/VClient/releases/latest";

CTClient::CTClient()
{
	OnReset();
}

void CTClient::ConRandomTee(IConsole::IResult *pResult, void *pUserData) {}

void CTClient::ConchainRandomColor(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CTClient *pThis = static_cast<CTClient *>(pUserData);
	// Resolve type to randomize
	// Check length of type (0 = all, 1 = body, 2 = feet, 3 = skin, 4 = flag)
	bool RandomizeBody = false;
	bool RandomizeFeet = false;
	bool RandomizeSkin = false;
	bool RandomizeFlag = false;

	if(pResult->NumArguments() == 0)
	{
		RandomizeBody = true;
		RandomizeFeet = true;
		RandomizeSkin = true;
		RandomizeFlag = true;
	}
	else if(pResult->NumArguments() == 1)
	{
		const char *Type = pResult->GetString(0);
		int Length = Type ? str_length(Type) : 0;
		if(Length == 1 && Type[0] == '0')
		{ // Randomize all
			RandomizeBody = true;
			RandomizeFeet = true;
			RandomizeSkin = true;
			RandomizeFlag = true;
		}
		else if(Length == 1)
		{
			// Randomize body
			RandomizeBody = Type[0] == '1';
		}
		else if(Length == 2)
		{
			// Check for body and feet
			RandomizeBody = Type[0] == '1';
			RandomizeFeet = Type[1] == '1';
		}
		else if(Length == 3)
		{
			// Check for body, feet and skin
			RandomizeBody = Type[0] == '1';
			RandomizeFeet = Type[1] == '1';
			RandomizeSkin = Type[2] == '1';
		}
		else if(Length == 4)
		{
			// Check for body, feet, skin and flag
			RandomizeBody = Type[0] == '1';
			RandomizeFeet = Type[1] == '1';
			RandomizeSkin = Type[2] == '1';
			RandomizeFlag = Type[3] == '1';
		}
	}

	if(RandomizeBody)
		RandomBodyColor();
	if(RandomizeFeet)
		RandomFeetColor();
	if(RandomizeSkin)
		RandomSkin(pUserData);
	if(RandomizeFlag)
		RandomFlag(pUserData);
	pThis->GameClient()->SendInfo(false);
}

void CTClient::ConchainAspectRatio(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CTClient *pThis = static_cast<CTClient *>(pUserData);
	pfnCallback(pResult, pCallbackUserData);
	pThis->SetForcedAspect();
}

void CTClient::OnInit()
{
	TextRender()->SetCustomFace(g_Config.m_TcCustomFont);
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	FetchTClientInfo();
	SetDDNetProcessPriority(g_Config.m_TcHighProcessPriority);
	if(g_Config.m_TcDiscordNormalProcessPriority)
		SetDiscordProcessesNormalPriority();

	char aError[512] = "";
	if(!Storage()->FileExists("tclient/gui_logo.png", IStorage::TYPE_ALL))
		str_format(aError, sizeof(aError), TCLocalize("%s not found", DATA_VERSION_PATH), "data/tclient/gui_logo.png");
	if(aError[0] == '\0')
		CheckDataVersion(aError, sizeof(aError), Storage()->OpenFile(DATA_VERSION_PATH, IOFLAG_READ, IStorage::TYPE_ALL));
	if(aError[0] != '\0')
	{
		SWarning Warning(aError, TCLocalize("You have probably only installed the TClient DDNet.exe which is not supported, please use the entire TClient folder", "data_version.h"));
		Client()->AddWarning(Warning);
	}
}

void CTClient::SetDDNetProcessPriority(bool Set)
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!SetPriorityClass(GetCurrentProcess(), Set ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS))
		return;
	if(!SetThreadPriority(GetCurrentThread(), Set ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_NORMAL))
		return;
#endif
}

void CTClient::SetDiscordProcessesNormalPriority()
{
#if defined(CONF_FAMILY_WINDOWS)
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hSnapshot == INVALID_HANDLE_VALUE)
		return;
	PROCESSENTRY32 Entry;
	mem_zero(&Entry, sizeof(Entry));
	Entry.dwSize = sizeof(Entry);
	if(Process32First(hSnapshot, &Entry))
	{
		do
		{
			char aExeName[128];
			if(WideCharToMultiByte(CP_UTF8, 0, Entry.szExeFile, -1, aExeName, sizeof(aExeName), nullptr, nullptr) > 0 && str_find_nocase(aExeName, "discord"))
			{
				HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Entry.th32ProcessID);
				if(hProcess)
				{
					SetPriorityClass(hProcess, NORMAL_PRIORITY_CLASS);
					CloseHandle(hProcess);
				}
			}
		} while(Process32Next(hSnapshot, &Entry));
	}
	CloseHandle(hSnapshot);
#endif
}

void CTClient::NotifyOnMove()
{
	if(!g_Config.m_TcNotifyOnMove)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	if(!m_pGraphics || m_pGraphics->WindowActive())
		return;
	int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !GameClient()->m_aClients[LocalId].m_Active)
		return;
	vec2 LocalPos = GameClient()->m_aClients[LocalId].m_RenderPos;
	if(distance(LocalPos, m_LastLocalPos) > 27.5f)
	{
		Client()->Notify("TClient", "current tile changed");
		m_pGraphics->NotifyWindow();
	}
	m_LastLocalPos = LocalPos;
}

void CTClient::NotifyOnSwitch()
{
	if(!g_Config.m_TcNotifyOnSwitch)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;
	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS || !GameClient()->m_aClients[LocalId].m_Active)
		return;

	const vec2 Pos = GameClient()->m_aClients[LocalId].m_Predicted.m_Pos;
	const int Index = Collision()->GetPureMapIndex(Pos);
	const int SwitchType = Collision()->GetSwitchType(Index);
	if(SwitchType < TILE_SWITCHTIMEDOPEN || SwitchType > TILE_SWITCHCLOSE)
	{
		m_LastSwitchTile = -1;
		return;
	}
	if(Index == m_LastSwitchTile)
		return;
	m_LastSwitchTile = Index;

	const char *pType = SwitchType == TILE_SWITCHTIMEDOPEN ? "activate, timed" :
			    SwitchType == TILE_SWITCHTIMEDCLOSE ? "deactivate, timed" :
			    SwitchType == TILE_SWITCHOPEN ? "activate" :
			    "deactivate";
	char aNum[16];
	str_format(aNum, sizeof(aNum), "%d", Collision()->GetSwitchNumber(Index));

	char aBuf[320];
	int Out = 0;
	const char *pScan = g_Config.m_TcNotifyOnSwitchText;
	while(*pScan != '\0' && Out < (int)sizeof(aBuf) - 1)
	{
		const char *pAppend = nullptr;
		if(str_comp_num(pScan, "@num", 4) == 0)
		{
			pAppend = aNum;
			pScan += 4;
		}
		else if(str_comp_num(pScan, "@type", 5) == 0)
		{
			pAppend = pType;
			pScan += 5;
		}
		if(pAppend != nullptr)
		{
			while(*pAppend != '\0' && Out < (int)sizeof(aBuf) - 1)
				aBuf[Out++] = *pAppend++;
		}
		else
			aBuf[Out++] = *pScan++;
	}
	aBuf[Out] = '\0';

	GameClient()->Echo(aBuf);
}

static bool LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_utf8_find_nocase(pLine, pName);
	if(pHL)
	{
		int Length = str_length(pName);
		if(Length > 0 && (pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}
	return false;
}

bool CTClient::SendNonDuplicateMessage(int Team, const char *pLine)
{
	if(str_comp(pLine, m_PreviousOwnMessage) != 0)
	{
		GameClient()->m_Chat.SendChat(Team, pLine);
		return true;
	}
	str_copy(m_PreviousOwnMessage, pLine);
	return false;
}

void CTClient::OnMessage(int MsgType, void *pRawMsg)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(MsgType == NETMSGTYPE_SV_RACEFINISH && (g_Config.m_TcSendFinishMessage || g_Config.m_TcScreenshotOnFinish))
	{
		const CNetMsg_Sv_RaceFinish *pMsg = (CNetMsg_Sv_RaceFinish *)pRawMsg;
		const bool IsLocal = pMsg->m_ClientId == GameClient()->m_aLocalIds[0] || (Client()->DummyConnected() && pMsg->m_ClientId == GameClient()->m_aLocalIds[1]);
		if(IsLocal && (pMsg->m_Time != m_LastFinishMessageTime || pMsg->m_ClientId != m_LastFinishMessageClientId))
		{
			m_LastFinishMessageTime = pMsg->m_Time;
			m_LastFinishMessageClientId = pMsg->m_ClientId;
			if(g_Config.m_TcScreenshotOnFinish)
				m_ScreenshotDelayLeft = 0.30f;
			if(g_Config.m_TcSendFinishMessage && g_Config.m_TcFinishMessage[0] != '\0')
			{
				m_FinishMessageDelayLeft = g_Config.m_TcFinishMessageDelay / 1000.0f;
				if(m_FinishMessageDelayLeft <= 0.0f)
				{
					GameClient()->m_Chat.SendChat(0, g_Config.m_TcFinishMessage);
					m_FinishMessageDelayLeft = -1.0f;
				}
			}
		}
	}

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		int ClientId = pMsg->m_ClientId;

		if(ClientId < 0 || ClientId > MAX_CLIENTS)
			return;
		int LocalId = GameClient()->m_Snap.m_LocalClientId;
		if(ClientId == LocalId)
			str_copy(m_PreviousOwnMessage, pMsg->m_pMessage);

		bool PingMessage = false;

		bool ValidIds = !(GameClient()->m_aLocalIds[0] < 0 || (GameClient()->Client()->DummyConnected() && GameClient()->m_aLocalIds[1] < 0));

		if(ValidIds && ClientId >= 0 && ClientId != GameClient()->m_aLocalIds[0] && (!GameClient()->Client()->DummyConnected() || ClientId != GameClient()->m_aLocalIds[1]))
		{
			PingMessage |= LineShouldHighlight(pMsg->m_pMessage, GameClient()->m_aClients[GameClient()->m_aLocalIds[0]].m_aName);
			PingMessage |= GameClient()->Client()->DummyConnected() && LineShouldHighlight(pMsg->m_pMessage, GameClient()->m_aClients[GameClient()->m_aLocalIds[1]].m_aName);
		}

		if(pMsg->m_Team == TEAM_WHISPER_RECV)
			PingMessage = true;

		if(!PingMessage)
			return;

		char aPlayerName[MAX_NAME_LENGTH];
		str_copy(aPlayerName, GameClient()->m_aClients[ClientId].m_aName, sizeof(aPlayerName));

		bool PlayerMuted = GameClient()->m_aClients[ClientId].m_Foe || GameClient()->m_aClients[ClientId].m_ChatIgnore;
		if(g_Config.m_TcAutoReplyMuted && PlayerMuted)
		{
			char aBuf[256];
			if(pMsg->m_Team == TEAM_WHISPER_RECV || ServerCommandExists("w"))
				str_format(aBuf, sizeof(aBuf), "/w %s %s", aPlayerName, g_Config.m_TcAutoReplyMutedMessage);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", aPlayerName, g_Config.m_TcAutoReplyMutedMessage);
			SendNonDuplicateMessage(0, aBuf);
			return;
		}

		bool WindowActive = m_pGraphics && m_pGraphics->WindowActive();
		if(g_Config.m_TcAutoReplyMinimized && !WindowActive && m_pGraphics)
		{
			char aBuf[256];
			if(pMsg->m_Team == TEAM_WHISPER_RECV || ServerCommandExists("w"))
				str_format(aBuf, sizeof(aBuf), "/w %s %s", aPlayerName, g_Config.m_TcAutoReplyMinimizedMessage);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", aPlayerName, g_Config.m_TcAutoReplyMinimizedMessage);
			SendNonDuplicateMessage(0, aBuf);
			return;
		}
	}

	if(MsgType == NETMSGTYPE_SV_VOTESET)
	{
		const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy]; // Do not care about spec behaviour
		const bool Afk = LocalId >= 0 && GameClient()->m_aClients[LocalId].m_Afk; // TODO Depends on server afk time
		CNetMsg_Sv_VoteSet *pMsg = (CNetMsg_Sv_VoteSet *)pRawMsg;
		if(pMsg->m_Timeout && !Afk)
		{
			char aDescription[VOTE_DESC_LENGTH];
			char aReason[VOTE_REASON_LENGTH];
			str_copy(aDescription, pMsg->m_pDescription);
			str_copy(aReason, pMsg->m_pReason);
			bool KickVote = str_startswith(aDescription, "Kick ") != nullptr ? true : false;
			bool SpecVote = str_startswith(aDescription, "Pause ") != nullptr ? true : false;
			bool SettingVote = !KickVote && !SpecVote;
			bool RandomMapVote = SettingVote && str_find_nocase(aDescription, "random");
			bool MapCoolDown = SettingVote && (str_find_nocase(aDescription, "change map") || str_find_nocase(aDescription, "no not change map"));
			bool CategoryVote = SettingVote && (str_find_nocase(aDescription, "☐") || str_find_nocase(aDescription, "☒"));
			bool FunVote = SettingVote && str_find_nocase(aDescription, "funvote");
			bool MapVote = SettingVote && !RandomMapVote && !MapCoolDown && !CategoryVote && !FunVote && (str_find_nocase(aDescription, "Map:") || str_find_nocase(aDescription, "★") || str_find_nocase(aDescription, "✰"));

			if(g_Config.m_TcAutoVoteWhenFar && (MapVote || RandomMapVote))
			{
				int RaceTime = 0;
				if(GameClient()->m_Snap.m_pGameInfoObj && GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
					RaceTime = (Client()->GameTick(g_Config.m_ClDummy) + GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer) / Client()->GameTickSpeed();

				if(RaceTime / 60 >= g_Config.m_TcAutoVoteWhenFarTime)
				{
					CGameClient::CClientData *pVoteCaller = nullptr;
					int CallerId = -1;
					for(int i = 0; i < MAX_CLIENTS; i++)
					{
						if(!GameClient()->m_aStats[i].IsActive())
							continue;

						char aBuf[MAX_NAME_LENGTH + 4];
						str_format(aBuf, sizeof(aBuf), "\'%s\'", GameClient()->m_aClients[i].m_aName);
						if(str_find_nocase(pMsg->m_pDescription, aBuf) != nullptr)
						{
							pVoteCaller = &GameClient()->m_aClients[i];
							CallerId = i;
							break;
						}
					}
					if(pVoteCaller)
					{
						bool Friend = pVoteCaller->m_Friend;
						bool SameTeam = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) == pVoteCaller->m_Team && pVoteCaller->m_Team != 0;
						bool MySelf = CallerId == LocalId;

						if(!Friend && !SameTeam && !MySelf)
						{
							GameClient()->m_Voting.Vote(-1);
							if(str_comp(g_Config.m_TcAutoVoteWhenFarMessage, "") != 0)
								SendNonDuplicateMessage(0, g_Config.m_TcAutoVoteWhenFarMessage);
						}
					}
				}
			}
		}
	}

	auto &vServerCommands = GameClient()->m_Chat.m_vServerCommands;
	auto AddSpecId = [&](bool Enable) {
		static const CChat::CCommand SpecId("specid", "v[id]", "Spectate a player");
		vServerCommands.erase(std::remove_if(vServerCommands.begin(), vServerCommands.end(), [](const CChat::CCommand &Command) { return Command == SpecId; }), vServerCommands.end());
		if(Enable)
			vServerCommands.push_back(SpecId);
		GameClient()->m_Chat.m_ServerCommandsNeedSorting = true;
	};
	if(MsgType == NETMSGTYPE_SV_COMMANDINFO)
	{
		CNetMsg_Sv_CommandInfo *pMsg = (CNetMsg_Sv_CommandInfo *)pRawMsg;
		if(str_comp_nocase(pMsg->m_pName, "spec") == 0)
			AddSpecId(!ServerCommandExists("specid"));
		else if(str_comp_nocase(pMsg->m_pName, "specid") == 0)
			AddSpecId(false);
		return;
	}
	if(MsgType == NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		CNetMsg_Sv_CommandInfoRemove *pMsg = (CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		if(str_comp_nocase(pMsg->m_pName, "spec") == 0)
			AddSpecId(false);
		else if(str_comp_nocase(pMsg->m_pName, "specid") == 0)
			AddSpecId(ServerCommandExists("spec"));
		return;
	}
}

void CTClient::ConSpecId(IConsole::IResult *pResult, void *pUserData)
{
	((CTClient *)pUserData)->SpecId(pResult->GetInteger(0));
}

bool CTClient::ChatDoSpecId(const char *pInput)
{
	const char *pNumber = str_startswith_nocase(pInput, "/specid ");
	if(!pNumber)
		return false;

	const int Length = str_length(pInput);
	CChat::CHistoryEntry *pEntry = GameClient()->m_Chat.m_History.Allocate(sizeof(CChat::CHistoryEntry) + Length);
	pEntry->m_Team = 0;
	str_copy(pEntry->m_aText, pInput, Length + 1);

	int ClientId = 0;
	if(!str_toint(pNumber, &ClientId))
		return true;

	SpecId(ClientId);
	return true;
}

bool CTClient::AntiUnSpec() const
{
	const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalClientId < 0)
		return false;
	if(!g_Config.m_TcAntiUnspectate)
		return false;

	const CGameClient::CClientData &Local = GameClient()->m_aClients[LocalClientId];
	if(!Local.m_Spec && !Local.m_Paused)
		return false;

	vec2 LocalPlayerPos = Local.m_RegularPredicted.m_Pos;
	const char *pPosSource = "regular";
	if(Local.m_SpecCharPresent)
	{
		LocalPlayerPos = Local.m_SpecChar;
		pPosSource = "specchar";
	}
	else if(GameClient()->m_Snap.m_aCharacters[LocalClientId].m_Active)
	{
		const CNetObj_Character LocalChar = GameClient()->m_Snap.m_aCharacters[LocalClientId].m_Cur;
		LocalPlayerPos = vec2(LocalChar.m_X, LocalChar.m_Y);
		pPosSource = "char";
	}

	bool CanCollidePhysical = false;
	int NearestId = -1;
	float NearestDist = -1.0f;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == GameClient()->m_aLocalIds[g_Config.m_ClDummy] || i == GameClient()->m_aLocalIds[!g_Config.m_ClDummy])
			continue;

		const CGameClient::CClientData &Other = GameClient()->m_aClients[i];
		if(!Other.m_Active || Other.m_Spec || Other.m_CollisionDisabled)
			continue;

		auto &CharInfo = GameClient()->m_Snap.m_aCharacters[i];
		if(!CharInfo.m_Active)
			continue;

		const vec2 CurPlayerPos = vec2(CharInfo.m_Cur.m_X, CharInfo.m_Cur.m_Y);
		const float Dist = distance(CurPlayerPos, LocalPlayerPos);
		if(NearestId < 0 || Dist < NearestDist)
		{
			NearestId = i;
			NearestDist = Dist;
		}
		if(Dist < CCharacterCore::PhysicalSize() * 1.25f)
		{
			CanCollidePhysical = true;
			break;
		}
	}

	if(g_Config.m_Debug)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "anti-unspec: %s, pos (%.1f, %.1f) from %s, nearest id %d dist %.1f, block %d",
			Local.m_Spec ? "spec" : "pause", LocalPlayerPos.x, LocalPlayerPos.y, pPosSource, NearestId, NearestDist, CanCollidePhysical ? 1 : 0);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "unspec", aBuf);
	}

	if(!CanCollidePhysical)
	{
		m_ConfirmUnSpec = false;
		return false;
	}

	if(m_ConfirmUnSpec)
	{
		m_ConfirmUnSpec = false;
		return false;
	}

	if(g_Config.m_TcShowChatClient)
		GameClient()->Echo("Are u sure want unspec? Press again to unspec");
	else
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "unspec", "Are u sure want unspec? Press again to unspec");
	m_ConfirmUnSpec = true;
	return true;
}

void CTClient::SpecId(int ClientId)
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK || GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		GameClient()->m_Spectator.Spectate(ClientId);
		return;
	}

	if(ClientId < 0 || ClientId > (int)std::size(GameClient()->m_aClients))
		return;
	const auto &Player = GameClient()->m_aClients[ClientId];
	if(!Player.m_Active)
		return;
	char aBuf[256];
	str_copy(aBuf, "/spec \"");
	char *pDst = aBuf + strlen(aBuf);
	str_escape(&pDst, Player.m_aName, aBuf + sizeof(aBuf));
	str_append(aBuf, "\"");
	GameClient()->m_Chat.SendChat(0, aBuf);
}

void CTClient::ConEmoteCycle(IConsole::IResult *pResult, void *pUserData)
{
	CTClient &This = *(CTClient *)pUserData;
	This.m_EmoteCycle += 1;
	if(This.m_EmoteCycle > 15)
		This.m_EmoteCycle = 0;
	This.GameClient()->m_Emoticon.Emote(This.m_EmoteCycle);
}

void CTClient::AirRescue()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const int ClientId = GameClient()->m_Snap.m_LocalClientId;
	if(ClientId < 0 || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		return;
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo && (GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_PRACTICE_MODE) == 0)
	{
		GameClient()->Echo("You are not in practice");
		return;
	}

	auto IsIndexAirLike = [&](int Index) {
		const auto Tile = Collision()->GetTileIndex(Index);
		return Tile == TILE_AIR || Tile == TILE_UNFREEZE || Tile == TILE_DUNFREEZE;
	};
	auto IsPosAirLike = [&](vec2 Pos) {
		const int Index = Collision()->GetPureMapIndex(Pos);
		return IsIndexAirLike(Index);
	};
	auto IsRadiusAirLike = [&](vec2 Pos, int Radius) {
		for(int y = -Radius; y <= Radius; ++y)
			for(int x = -Radius; x <= Radius; ++x)
				if(!IsPosAirLike(Pos + vec2(x, y) * 32.0f))
					return false;
		return true;
	};

	auto &AirRescuePositions = m_aAirRescuePositions[g_Config.m_ClDummy];
	while(!AirRescuePositions.empty())
	{
		// Get latest pos from positions
		const vec2 NewPos = AirRescuePositions.front();
		AirRescuePositions.pop_front();
		// Check for safety
		if(!IsRadiusAirLike(NewPos, 2))
			continue;
		// Do it
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "/tpxy %f %f", NewPos.x / 32.0f, NewPos.y / 32.0f);
		GameClient()->m_Chat.SendChat(0, aBuf);
		return;
	}

	GameClient()->Echo("No safe position found");
}

void CTClient::ConAirRescue(IConsole::IResult *pResult, void *pUserData)
{
	((CTClient *)pUserData)->AirRescue();
}

void CTClient::ConReloadTextures(IConsole::IResult *pResult, void *pUserData)
{
	((CTClient *)pUserData)->RequestReloadTextures();
}

void CTClient::RequestReloadTextures()
{
	m_ReloadTexturesRequested = true;
}

void CTClient::UpdateFinishMessage()
{
	const float FrameTime = Client()->RenderFrameTime();

	if(m_ScreenshotDelayLeft >= 0.0f)
	{
		m_ScreenshotDelayLeft -= FrameTime;
		if(m_ScreenshotDelayLeft <= 0.0f)
		{
			m_ScreenshotDelayLeft = -1.0f;
			if(g_Config.m_TcScreenshotOnFinish && Client()->State() == IClient::STATE_ONLINE)
				Console()->ExecuteLine("screenshot", IConsole::CLIENT_ID_UNSPECIFIED);
		}
	}

	if(m_FinishMessageDelayLeft < 0.0f)
		return;
	m_FinishMessageDelayLeft -= FrameTime;
	if(m_FinishMessageDelayLeft > 0.0f)
		return;
	m_FinishMessageDelayLeft = -1.0f;
	if(g_Config.m_TcSendFinishMessage && g_Config.m_TcFinishMessage[0] != '\0' && Client()->State() == IClient::STATE_ONLINE)
		GameClient()->m_Chat.SendChat(0, g_Config.m_TcFinishMessage);
}

void CTClient::OnUpdate()
{
	UpdateFinishMessage();

	if(!m_ReloadTexturesRequested)
		return;
	m_ReloadTexturesRequested = false;
	ReloadTextures();
}

void CTClient::ReloadTextures()
{
	CGameClient *pGameClient = GameClient();

	pGameClient->LoadGameSkin(g_Config.m_ClAssetGame);
	pGameClient->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
	pGameClient->LoadParticlesSkin(g_Config.m_ClAssetParticles);
	pGameClient->LoadHudSkin(g_Config.m_ClAssetHud);
	pGameClient->LoadExtrasSkin(g_Config.m_ClAssetExtras);

	for(int i = 0; i < g_pData->m_NumImages; i++)
	{
		if(i == IMAGE_GAME || i == IMAGE_EMOTICONS || i == IMAGE_PARTICLES || i == IMAGE_HUD || i == IMAGE_EXTRAS)
			continue;
		if(g_pData->m_aImages[i].m_pFilename[0] == '\0')
			continue;
		Graphics()->UnloadTexture(&g_pData->m_aImages[i].m_Id);
		g_pData->m_aImages[i].m_Id = Graphics()->LoadTexture(g_pData->m_aImages[i].m_pFilename, IStorage::TYPE_ALL);
	}

	pGameClient->m_NamePlates.RefreshTextureHandles();

	const bool MapLoaded = pGameClient->Map() != nullptr && pGameClient->Map()->IsLoaded();
	pGameClient->m_MapImages.ReloadAll();
	if(MapLoaded)
	{
		pGameClient->m_MapLayersBackground.OnMapLoad();
		pGameClient->m_MapLayersForeground.OnMapLoad();
	}
	pGameClient->m_Background.LoadBackground();

	pGameClient->RefreshSkins(CSkinDescriptor::FLAG_SIX | CSkinDescriptor::FLAG_SEVEN);

	char aBuf[192];
	str_format(aBuf, sizeof(aBuf), "Textures reloaded: textures %llu MiB, system RAM spill %llu MiB, VRAM limit %d MiB",
		(unsigned long long)(Graphics()->TextureMemoryUsage() / 1024 / 1024), (unsigned long long)(g_VramFallbackBytes.load() / 1024 / 1024), g_Config.m_TcVramLimit);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tclient", aBuf);
}

void CTClient::ConCalc(IConsole::IResult *pResult, void *pUserData)
{
	int Error = 0;
	double Out = te_interp(pResult->GetString(0), &Error);
	if(Out == NAN || Error != 0)
		log_info("tclient", "Calc error: %d", Error);
	else
		log_info("tclient", "Calc result: %lf", Out);
}

void CTClient::OnConsoleInit()
{
	Console()->Register("calc", "r[expression]", CFGFLAG_CLIENT, ConCalc, this, "Evaluate an expression");
	Console()->Register("airrescue", "", CFGFLAG_CLIENT, ConAirRescue, this, "Rescue to a nearby air tile");

	Console()->Register("tc_random_player", "s[type]", CFGFLAG_CLIENT, ConRandomTee, this, "Randomize player color (0 = all, 1 = body, 2 = feet, 3 = skin, 4 = flag) example: 0011 = randomize skin and flag [number is position]");
	Console()->Chain("tc_random_player", ConchainRandomColor, this);

	Console()->Register("spec_id", "v[id]", CFGFLAG_CLIENT, ConSpecId, this, "Spectate a player by Id");

	Console()->Register("emote_cycle", "", CFGFLAG_CLIENT, ConEmoteCycle, this, "Cycle through emotes");

	Console()->Register("tc_reload_textures", "", CFGFLAG_CLIENT, ConReloadTextures, this, "Reload all textures to apply the current memory settings");

	Console()->Chain(
		"tc_allow_any_res", [](IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData) {
			pfnCallback(pResult, pCallbackUserData);
			((CTClient *)pUserData)->SetForcedAspect();
		},
		this);

	Console()->Chain("tc_custom_aspect_ratio_mode", ConchainAspectRatio, this);
	Console()->Chain("tc_custom_aspect_ratio_apply_mode", ConchainAspectRatio, this);
	Console()->Chain("tc_custom_aspect_ratio", ConchainAspectRatio, this);
	Console()->Chain("tc_custom_aspect_ratio_num", ConchainAspectRatio, this);
	Console()->Chain("tc_custom_aspect_ratio_den", ConchainAspectRatio, this);

	Console()->Chain(
		"tc_regex_chat_ignore", [](IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData) {
			if(pResult->NumArguments() == 1)
			{
				auto Re = Regex(pResult->GetString(0));
				if(!Re.error().empty())
				{
					log_error("tclient", "Invalid regex: %s", Re.error().c_str());
					return;
				}
				((CTClient *)pUserData)->m_RegexChatIgnore = std::move(Re);
			}
			pfnCallback(pResult, pCallbackUserData);
		},
		this);
}

void CTClient::RandomBodyColor()
{
	g_Config.m_ClPlayerColorBody = ColorHSLA((std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, 1.0f).Pack(false);
}

void CTClient::RandomFeetColor()
{
	g_Config.m_ClPlayerColorFeet = ColorHSLA((std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, 1.0f).Pack(false);
}

void CTClient::RandomSkin(void *pUserData)
{
	CTClient *pThis = static_cast<CTClient *>(pUserData);
	const auto &Skins = pThis->GameClient()->m_Skins.SkinList().Skins();
	str_copy(g_Config.m_ClPlayerSkin, Skins[std::rand() % (int)Skins.size()].SkinContainer()->Name());
}

void CTClient::RandomFlag(void *pUserData)
{
	CTClient *pThis = static_cast<CTClient *>(pUserData);
	// get the flag count
	int FlagCount = pThis->GameClient()->m_CountryFlags.Num();

	// get a random flag number
	int FlagNumber = std::rand() % FlagCount;

	// get the flag name
	const CCountryFlags::CCountryFlag &Flag = pThis->GameClient()->m_CountryFlags.GetByIndex(FlagNumber);

	// set the flag code as number
	g_Config.m_PlayerCountry = Flag.m_CountryCode;
}

void CTClient::DoFinishCheck()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const bool DoRename = g_Config.m_TcChangeNameNearFinish > 0;
	const bool DoUnlock = g_Config.m_TcUnlockTeamNearFinish > 0;
	if(!DoRename && !DoUnlock)
		return;

	const float Passed = Client()->RenderFrameTime();
	m_FinishTextTimeout -= Passed;
	m_UnlockTeamTimeout -= Passed;

	const bool CheckRename = DoRename && m_FinishTextTimeout <= 0.0f;
	if(!CheckRename && !(DoUnlock && !m_UnlockTeamSent))
		return;
	// Check for finish tile
	const auto NearTile = [this](vec2 Pos, int RadiusInTiles, int Tile) -> bool {
		const CCollision *pCollision = GameClient()->Collision();
		for(int i = 0; i <= RadiusInTiles * 2; i++)
		{
			const float Angle = (float)i * pi / 2.0f / (float)RadiusInTiles;
			const float Height = std::ceil(std::pow(std::sin(Angle), 0.5f) * pi / 2.0f * (float)RadiusInTiles);
			const vec2 Pos1 = vec2(Pos.x + (float)(i - RadiusInTiles) * 32.0f, Pos.y - Height);
			const vec2 Pos2 = vec2(Pos.x + (float)(i - RadiusInTiles) * 32.0f, Pos.y + Height);
			std::vector<int> vIndices = pCollision->GetMapIndices(Pos1, Pos2);
			if(vIndices.empty())
				vIndices.push_back(pCollision->GetPureMapIndex(Pos1));
			for(int &Index : vIndices)
			{
				if(pCollision->GetTileIndex(Index) == Tile)
					return true;
				if(pCollision->GetFrontTileIndex(Index) == Tile)
					return true;
			}
		}
		return false;
	};
	const auto SendUrgentRename = [this](int Conn, const char *pNewName) {
		CNetMsg_Cl_ChangeInfo Msg;
		Msg.m_pName = pNewName;
		Msg.m_pClan = Conn == 0 ? g_Config.m_PlayerClan : g_Config.m_ClDummyClan;
		Msg.m_Country = Conn == 0 ? g_Config.m_PlayerCountry : g_Config.m_ClDummyCountry;
		Msg.m_pSkin = Conn == 0 ? g_Config.m_ClPlayerSkin : g_Config.m_ClDummySkin;
		Msg.m_UseCustomColor = Conn == 0 ? g_Config.m_ClPlayerUseCustomColor : g_Config.m_ClDummyUseCustomColor;
		Msg.m_ColorBody = Conn == 0 ? g_Config.m_ClPlayerColorBody : g_Config.m_ClDummyColorBody;
		Msg.m_ColorFeet = Conn == 0 ? g_Config.m_ClPlayerColorFeet : g_Config.m_ClDummyColorFeet;
		CMsgPacker Packer(&Msg);
		Msg.Pack(&Packer);
		Client()->SendMsg(Conn, &Packer, MSGFLAG_VITAL);
		GameClient()->m_aCheckInfo[Conn] = Client()->GameTickSpeed(); // 1 second
	};
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return;
	const auto &Player = GameClient()->m_aClients[LocalId];
	if(!Player.m_Active)
		return;
	if(!NearTile(Player.m_RenderPos, 10, TILE_FINISH))
	{
		if(CheckRename)
			m_FinishTextTimeout = 1.0f;
		m_WasNearFinish = false;
		m_UnlockTeamSent = false;
		return;
	}

	if(CheckRename)
	{
		m_FinishTextTimeout = 1.0f;
		const char *NewName = g_Config.m_TcFinishName;
		if(str_comp(Player.m_aName, NewName) != 0)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), TCLocalize("Changing name to %s near finish"), NewName);
			GameClient()->Echo(aBuf);
			SendUrgentRename(Dummy, NewName);
		}
	}

	if(DoUnlock && !m_WasNearFinish)
	{
		m_WasNearFinish = true;
		m_UnlockTeamTimeout = g_Config.m_TcUnlockTeamNearFinishDelay / 1000.0f;
	}
	if(DoUnlock && m_WasNearFinish && !m_UnlockTeamSent && m_UnlockTeamTimeout <= 0.0f)
	{
		m_UnlockTeamSent = true;
		GameClient()->m_Chat.SendChat(0, "/unlock");
		GameClient()->Echo("Unlocking team near finish");
	}
}

bool CTClient::ServerCommandExists(const char *pCommand)
{
	for(const auto &Command : GameClient()->m_Chat.m_vServerCommands)
		if(str_comp_nocase(pCommand, Command.m_aName) == 0)
			return true;
	return false;
}

void CTClient::OnRender()
{
	NotifyOnMove();
	NotifyOnSwitch();

	if(m_pTClientInfoTask)
	{
		if(m_pTClientInfoTask->State() == EHttpState::DONE)
		{
			FinishTClientInfo();
			ResetTClientInfoTask();
		}
	}

	DoFinishCheck();
}

static constexpr float TCLIENT_INFO_FONT_SIZE = 2.1f;
static constexpr float TCLIENT_INFO_LINE_HEIGHT = 2.3f;
static constexpr float TCLIENT_INFO_PADDING = 0.6f;

void CTClient::RenderInfoOverlay()
{
	m_InfoOverlayRect = CUIRect{0.0f, 0.0f, 0.0f, 0.0f};
	if((!g_Config.m_TcInfoVram && !g_Config.m_TcInfoTextures) || !HudLayout::IsEnabled(HudLayout::MODULE_VRAM_INFO))
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(GameClient()->m_Menus.IsActive() || GameClient()->m_GameConsole.IsActive())
		return;

	const float HudWidth = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
	const float HudHeight = HudLayout::CANVAS_HEIGHT;
	const auto Layout = HudLayout::Get(HudLayout::MODULE_VRAM_INFO, HudWidth, HudHeight);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);

	const CScreenRect PrevScreen = Graphics()->GetScreen();

	Graphics()->MapScreen(-Layout.m_X / Scale, -Layout.m_Y / Scale, -Layout.m_X / Scale + HudWidth / Scale, -Layout.m_Y / Scale + HudHeight / Scale);
	Graphics()->ClipDisable();
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	char aBuf[256];
	float OffsetY = 0.0f;
	float MaxWidth = 0.0f;
	auto RenderInfoLine = [&](const char *pText) {
		TextRender()->Text(0.0f, OffsetY, TCLIENT_INFO_FONT_SIZE, pText, -1.0f);
		MaxWidth = std::max(MaxWidth, TextRender()->TextWidth(TCLIENT_INFO_FONT_SIZE, pText, -1, -1.0f));
		OffsetY += TCLIENT_INFO_LINE_HEIGHT;
	};

	if(g_Config.m_TcInfoVram)
	{
		const uint64_t aUsageBytes[4] = {Graphics()->TextureMemoryUsage(), Graphics()->BufferMemoryUsage(), Graphics()->StreamedMemoryUsage(), Graphics()->StagingMemoryUsage()};
		const uint64_t aSpillBytes[4] = {g_VramFallbackBytesByUsage[0].load(), g_VramFallbackBytesByUsage[1].load(), g_VramFallbackBytesByUsage[2].load(), g_VramFallbackBytesByUsage[3].load()};
		str_format(aBuf, sizeof(aBuf), "VRAM: tex %llu MiB | buf %llu MiB | stream %llu MiB | staging %llu MiB",
			(unsigned long long)((aUsageBytes[0] > aSpillBytes[0] ? aUsageBytes[0] - aSpillBytes[0] : 0) / 1024 / 1024),
			(unsigned long long)((aUsageBytes[1] > aSpillBytes[1] ? aUsageBytes[1] - aSpillBytes[1] : 0) / 1024 / 1024),
			(unsigned long long)((aUsageBytes[2] > aSpillBytes[2] ? aUsageBytes[2] - aSpillBytes[2] : 0) / 1024 / 1024),
			(unsigned long long)((aUsageBytes[3] > aSpillBytes[3] ? aUsageBytes[3] - aSpillBytes[3] : 0) / 1024 / 1024));
		RenderInfoLine(aBuf);
		str_format(aBuf, sizeof(aBuf), "Limits: VRAM %d MiB | RAM %d MiB | tex max %d px | RAM fallback %s | skins limit %d",
			g_Config.m_TcVramLimit, g_Config.m_TcRamLimit, g_Config.m_TcTextureMaxSize, g_Config.m_TcVramFallback ? "on" : "off", g_Config.m_ClSkinsLoadedMax);
		RenderInfoLine(aBuf);
		str_format(aBuf, sizeof(aBuf), "RAM spill: %llu MiB in %llu allocations (tex %llu | buf %llu | stream %llu | staging %llu)",
			(unsigned long long)(g_VramFallbackBytes.load() / 1024 / 1024), (unsigned long long)g_VramFallbackCount.load(),
			(unsigned long long)(aSpillBytes[0] / 1024 / 1024), (unsigned long long)(aSpillBytes[1] / 1024 / 1024),
			(unsigned long long)(aSpillBytes[2] / 1024 / 1024), (unsigned long long)(aSpillBytes[3] / 1024 / 1024));
		RenderInfoLine(aBuf);
	}

	if(g_Config.m_TcInfoTextures)
	{
		uint64_t TotalBytes = 0;
		const int NumTextures = GameClient()->m_MapImages.NumLoadedTextures();
		for(int i = 0; i < NumTextures; i++)
		{
			const SLoadedMapTextureInfo *pInfo = GameClient()->m_MapImages.LoadedTexture(i);
			TotalBytes += (uint64_t)pInfo->m_Width * pInfo->m_Height * 4;
		}
		str_format(aBuf, sizeof(aBuf), "Map textures: %d, ~%llu MiB (RGBA, without mipmaps)", NumTextures, (unsigned long long)(TotalBytes / 1024 / 1024));
		RenderInfoLine(aBuf);
		for(int i = 0; i < NumTextures; i++)
		{
			const SLoadedMapTextureInfo *pInfo = GameClient()->m_MapImages.LoadedTexture(i);
			if(pInfo->m_Width <= 0 || pInfo->m_Height <= 0)
				continue;
			str_format(aBuf, sizeof(aBuf), "  %s %dx%d ~%llu KiB", pInfo->m_aName, pInfo->m_Width, pInfo->m_Height, (unsigned long long)((uint64_t)pInfo->m_Width * pInfo->m_Height * 4 / 1024));
			RenderInfoLine(aBuf);
		}
	}

	Graphics()->ClipDisable();
	Graphics()->MapScreen(PrevScreen);

	if(MaxWidth > 0.0f)
		m_InfoOverlayRect = CUIRect{Layout.m_X, Layout.m_Y, (MaxWidth + TCLIENT_INFO_PADDING) * Scale, (OffsetY - TCLIENT_INFO_LINE_HEIGHT + TCLIENT_INFO_FONT_SIZE) * Scale};
}

CUIRect CTClient::GetInfoOverlayHudEditorRect() const
{
	if(m_InfoOverlayRect.w > 0.0f && m_InfoOverlayRect.h > 0.0f)
		return m_InfoOverlayRect;

	const float HudWidth = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
	const auto Layout = HudLayout::Get(HudLayout::MODULE_VRAM_INFO, HudWidth, HudLayout::CANVAS_HEIGHT);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float Width = TextRender()->TextWidth(TCLIENT_INFO_FONT_SIZE, "VRAM: tex 12345 MiB | buf 12345 MiB | stream 12345 MiB | staging 12345 MiB", -1, -1.0f) + TCLIENT_INFO_PADDING;
	const float Height = TCLIENT_INFO_LINE_HEIGHT * 2.0f + TCLIENT_INFO_FONT_SIZE;
	return CUIRect{Layout.m_X, Layout.m_Y, Width * Scale, Height * Scale};
}

bool CTClient::NeedUpdate()

{
	return str_comp(m_aVersionStr, "0") != 0;
}

void CTClient::ResetTClientInfoTask()
{
	if(m_pTClientInfoTask)
	{
		m_pTClientInfoTask->Abort();
		m_pTClientInfoTask = nullptr;
	}
}


void CTClient::FetchTClientInfo()
{
	if(m_pTClientInfoTask && !m_pTClientInfoTask->Done())
		return;
	char aUrl[256];
	str_copy(aUrl, TCLIENT_INFO_URL);
	m_pTClientInfoTask = HttpGet(aUrl);
	m_pTClientInfoTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pTClientInfoTask->IpResolve(IPRESOLVE::V4);
	m_pTClientInfoTask->HeaderString("User-Agent", VCLIENT_NAME "/" VCLIENT_VERSION);
	m_pTClientInfoTask->HeaderString("Accept", "application/vnd.github+json");
	Http()->Run(m_pTClientInfoTask);
}

typedef std::tuple<int, int, int> TVersion;
static const TVersion gs_InvalidTCVersion = std::make_tuple(-1, -1, -1);

static TVersion ToTCVersion(char *pStr)
{
	int aVersion[3] = {0, 0, 0};
	const char *p = strtok(pStr, ".");

	for(int i = 0; i < 3 && p; ++i)
	{
		if(!str_isallnum(p))
			return gs_InvalidTCVersion;

		aVersion[i] = str_toint(p);
		p = strtok(nullptr, ".");
	}

	if(p)
		return gs_InvalidTCVersion;

	return std::make_tuple(aVersion[0], aVersion[1], aVersion[2]);
}

void CTClient::FinishTClientInfo()
{
	json_value *pJson = m_pTClientInfoTask->ResultJson();
	if(!pJson)
		return;
	const json_value &Json = *pJson;
	const json_value &TagName = Json["tag_name"];
	const json_value &LegacyVersion = Json["version"];
	const json_value &CurrentVersion = TagName.type == json_string ? TagName : LegacyVersion;

	if(CurrentVersion.type == json_string)
	{
		char aNewVersionStr[64];
		str_copy(aNewVersionStr, CurrentVersion);
		// strip a leading "v" from tags such as "v1.1"
		const char *pNewVersion = aNewVersionStr;
		while(*pNewVersion == 'v' || *pNewVersion == 'V' || *pNewVersion == ' ')
			pNewVersion++;
		char aNewVersion[64];
		str_copy(aNewVersion, pNewVersion, sizeof(aNewVersion));
		char aCurVersionStr[64];
		str_copy(aCurVersionStr, VCLIENT_VERSION);
		if(ToTCVersion(aNewVersion) > ToTCVersion(aCurVersionStr))
		{
			str_copy(m_aVersionStr, aNewVersion, sizeof(m_aVersionStr));
		}
		else
		{
			m_aVersionStr[0] = '0';
			m_aVersionStr[1] = '\0';
		}
		m_FetchedTClientInfo = true;
	}

	json_value_free(pJson);
}

void CTClient::SetForcedAspect()
{
	// TODO: Fix flashing on windows
	int State = Client()->State();
	bool Force = true;
	if(g_Config.m_TcAllowAnyRes == 0)
		;
	else if(State == CClient::EClientState::STATE_DEMOPLAYBACK)
		Force = false;
	else if(State == CClient::EClientState::STATE_ONLINE && GameClient()->m_GameInfo.m_AllowZoom && !GameClient()->m_Menus.IsActive())
		Force = false;

	const int AspectRatioMode = g_Config.m_TcCustomAspectRatioMode;
	if(AspectRatioMode == 0)
		Force = false;
	else if(AspectRatioMode > 0)
	{
		Force = true;
		if(g_Config.m_TcCustomAspectRatioApplyMode != 1 && State != CClient::EClientState::STATE_ONLINE && State != CClient::EClientState::STATE_DEMOPLAYBACK)
			Force = false;
	}

	if(IGraphics *pGraphics = Graphics())
		pGraphics->SetForcedAspect(Force);
}

void CTClient::OnStateChange(int OldState, int NewState)
{
	SetForcedAspect();
	m_FinishMessageDelayLeft = -1.0f;
	m_LastFinishMessageTime = -1;
	m_LastFinishMessageClientId = -1;
	for(auto &AirRescuePositions : m_aAirRescuePositions)
		AirRescuePositions = {};
}

void CTClient::OnNewSnapshot()
{
	SetForcedAspect();
	// Update volleyball
	bool IsVolleyBall = false;
	if(g_Config.m_TcVolleyBallBetterBall > 0 && g_Config.m_TcVolleyBallBetterBallSkin[0] != '\0')
	{
		if(g_Config.m_TcVolleyBallBetterBall > 1)
			IsVolleyBall = true;
		else
			IsVolleyBall = str_startswith_nocase(GameClient()->Map()->BaseName(), "volleyball");
	};
	for(auto &Client : GameClient()->m_aClients)
	{
		Client.m_IsVolleyBall = IsVolleyBall && Client.m_DeepFrozen;
	}
	// Update air rescue
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		for(int Dummy = 0; Dummy < NUM_DUMMIES; ++Dummy)
		{
			const int ClientId = GameClient()->m_aLocalIds[Dummy];
			if(ClientId == -1)
				continue;
			const auto &Char = GameClient()->m_Snap.m_aCharacters[ClientId];
			if(!Char.m_Active)
				continue;
			if(Client()->GameTick(Dummy) % 10 != 0) // Works for both 25tps and 50tps
				continue;
			const auto &Client = GameClient()->m_aClients[ClientId];
			if(Client.m_FreezeEnd == -1) // You aren't safe when frozen
				continue;
			const vec2 NewPos = vec2(Char.m_Cur.m_X, Char.m_Cur.m_Y);
			// If new pos is under 2 tiles from old pos, don't record a new position
			if(!m_aAirRescuePositions[Dummy].empty())
			{
				const vec2 OldPos = m_aAirRescuePositions[Dummy].front();
				if(distance(NewPos, OldPos) < 64.0f)
					continue;
			}
			if(m_aAirRescuePositions[Dummy].size() >= 256)
				m_aAirRescuePositions[Dummy].pop_back();
			m_aAirRescuePositions[Dummy].push_front(NewPos);
		}
	}
}

constexpr const char STRIP_CHARS[] = {'-', '=', '+', '_', ' '};
static bool IsStripChar(char c)
{
	return std::any_of(std::begin(STRIP_CHARS), std::end(STRIP_CHARS), [c](char s) {
		return s == c;
	});
}

static void StripStr(const char *pIn, char *pOut, const char *pEnd)
{
	if(!pIn)
	{
		*pOut = '\0';
		return;
	}

	while(*pIn && IsStripChar(*pIn))
		pIn++;

	// Special behaviour for empty checkbox
	if((unsigned char)*pIn == 0xE2 && (unsigned char)(*(pIn + 1)) == 0x98 && (unsigned char)(*(pIn + 2)) == 0x90)
	{
		pIn += 3;
		while(*pIn && IsStripChar(*pIn))
			pIn++;
	}

	char *pLastValid = nullptr;
	while(*pIn && pOut < pEnd - 1)
	{
		*pOut = *pIn;
		if(!IsStripChar(*pIn))
			pLastValid = pOut;
		pIn++;
		pOut++;
	}

	if(pLastValid)
		*(pLastValid + 1) = '\0';
	else
		*pOut = '\0';
}

void CTClient::RenderMiniVoteHud()
{
	const CUIRect Screen = *Ui()->Screen();
	const CScreenRect SaveScreen = Graphics()->GetScreen();
	const float HudWidth = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
	const auto VoteLayout = HudLayout::Get(HudLayout::MODULE_VOTE, HudWidth, HudLayout::CANVAS_HEIGHT);
	const float VoteScale = std::clamp(VoteLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float OffsetX = -(VoteLayout.m_X * 2.0f) / VoteScale;
	const float OffsetY = 60.0f - (VoteLayout.m_Y * 2.0f) / VoteScale;
	Graphics()->MapScreen(OffsetX, OffsetY, OffsetX + Screen.w / VoteScale, OffsetY + Screen.h / VoteScale);

	CUIRect View = {0.0f, 60.0f, 70.0f, 35.0f};
	const int VoteCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, VoteLayout.m_X, VoteLayout.m_Y, View.w * VoteScale, View.h * VoteScale, HudWidth, HudLayout::CANVAS_HEIGHT);
	View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), VoteCorners, 3.0f);
	View.Margin(3.0f, &View);

	SLabelProperties Props;
	Props.m_EllipsisAtEnd = true;
	Props.m_MaxWidth = View.w;

	CUIRect Row, LeftColumn, RightColumn, ProgressSpinner;
	char aBuf[256];

	// Vote description
	View.HSplitTop(6.0f, &Row, &View);
	StripStr(GameClient()->m_Voting.VoteDescription(), aBuf, aBuf + sizeof(aBuf));
	Ui()->DoLabel(&Row, aBuf, 6.0f, TEXTALIGN_ML, Props);

	// Vote reason
	View.HSplitTop(3.0f, nullptr, &View);
	View.HSplitTop(4.0f, &Row, &View);
	Ui()->DoLabel(&Row, GameClient()->m_Voting.VoteReason(), 4.0f, TEXTALIGN_ML, Props);

	// Time left
	str_format(aBuf, sizeof(aBuf), Localize("%ds left"), GameClient()->m_Voting.SecondsLeft());
	View.HSplitTop(3.0f, nullptr, &View);
	View.HSplitTop(3.0f, &Row, &View);
	Row.VSplitLeft(2.0f, nullptr, &Row);
	Row.VSplitLeft(3.0f, &ProgressSpinner, &Row);
	Row.VSplitLeft(2.0f, nullptr, &Row);

	SProgressSpinnerProperties ProgressProps;
	ProgressProps.m_Progress = std::clamp((time() - GameClient()->m_Voting.m_Opentime) / (float)(GameClient()->m_Voting.m_Closetime - GameClient()->m_Voting.m_Opentime), 0.0f, 1.0f);
	Ui()->RenderProgressSpinner(ProgressSpinner.Center(), ProgressSpinner.h / 2.0f, ProgressProps);

	Ui()->DoLabel(&Row, aBuf, 3.0f, TEXTALIGN_ML);

	// Bars
	View.HSplitTop(3.0f, nullptr, &View);
	View.HSplitTop(3.0f, &Row, &View);
	GameClient()->m_Voting.RenderBars(Row);

	// F3 / F4
	View.HSplitTop(3.0f, nullptr, &View);
	View.HSplitTop(0.5f, &Row, &View);
	Row.VSplitMid(&LeftColumn, &RightColumn, 4.0f);

	char aKey[64];
	GameClient()->m_Binds.GetKey("vote yes", aKey, sizeof(aKey));
	TextRender()->TextColor(GameClient()->m_Voting.TakenChoice() == 1 ? ColorRGBA(0.2f, 0.9f, 0.2f, 0.85f) : TextRender()->DefaultTextColor());
	Ui()->DoLabel(&LeftColumn, aKey[0] == '\0' ? "yes" : aKey, 0.5f, TEXTALIGN_ML);

	GameClient()->m_Binds.GetKey("vote no", aKey, sizeof(aKey));
	TextRender()->TextColor(GameClient()->m_Voting.TakenChoice() == -1 ? ColorRGBA(0.95f, 0.25f, 0.25f, 0.85f) : TextRender()->DefaultTextColor());
	Ui()->DoLabel(&RightColumn, aKey[0] == '\0' ? "no" : aKey, 0.5f, TEXTALIGN_MR);

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(SaveScreen);
}

void CTClient::RenderCenterLines()
{
	if(g_Config.m_TcShowCenter <= 0)
		return;

	if(GameClient()->m_Scoreboard.IsActive())
		return;

	Graphics()->TextureClear();

	const CScreenRect ScreenRect = Graphics()->GetScreen();
	const auto ScreenMid = (ScreenRect.m_BottomRight + ScreenRect.m_TopLeft) / 2.0f;

	if(g_Config.m_TcShowCenterWidth == 0)
	{
		Graphics()->LinesBegin();
		IGraphics::CLineItem aLines[2] = {
			{ScreenMid.x, ScreenRect.m_TopLeft.y, ScreenMid.x, ScreenRect.m_BottomRight.y},
			{ScreenRect.m_TopLeft.x, ScreenMid.y, ScreenRect.m_BottomRight.x, ScreenMid.y}};
		Graphics()->SetColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcShowCenterColor, true)));
		Graphics()->LinesDraw(aLines, std::size(aLines));
		Graphics()->LinesEnd();
	}
	else
	{
		const float W = g_Config.m_TcShowCenterWidth;
		const float X0 = ScreenRect.m_TopLeft.x;
		const float Y0 = ScreenRect.m_TopLeft.y;
		const float X1 = ScreenRect.m_BottomRight.x;
		const float Y1 = ScreenRect.m_BottomRight.y;
		Graphics()->QuadsBegin();
		IGraphics::CQuadItem aQuads[3] = {
			{ScreenMid.x, mix(Y0, Y1, 0.25f) - W / 4.0f, W, (Y1 - Y0 - W) / 2.0f},
			{ScreenMid.x, mix(Y0, Y1, 0.75f) + W / 4.0f, W, (Y1 - Y0 - W) / 2.0f},
			{ScreenMid.x, ScreenMid.y, X1 - X0, W}};
		Graphics()->SetColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcShowCenterColor, true)));
		Graphics()->QuadsDraw(aQuads, std::size(aQuads));
		Graphics()->QuadsEnd();
	}
}

void CTClient::RenderCtfFlag(vec2 Pos, float Alpha)
{
	// from CItems::RenderFlag
	float Size = 42.0f;
	int QuadOffset;
	if(g_Config.m_TcFakeCtfFlags == 1)
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);
		QuadOffset = GameClient()->m_Items.m_RedFlagOffset;
	}
	else
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);
		QuadOffset = GameClient()->m_Items.m_BlueFlagOffset;
	}
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->RenderQuadContainerAsSprite(GameClient()->m_Items.m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y - Size * 0.75f);
}
