#include "chatbinds.h"

#include <base/time.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "engine/client.h"
#include "engine/graphics.h"
#include "engine/shared/config.h"
#include "engine/textrender.h"

#include "game/client/gameclient.h"
#include "game/client/components/camera.h"
#include "game/client/components/controls.h"
#include "game/collision.h"
#include "game/mapitems.h"
#include "game/version.h"

static constexpr int MAX_MARKERS = 16;
static constexpr float MARKER_POINT_RADIUS = 4.5f;
static constexpr float MARKER_STEM_WIDTH = 2.0f;
static constexpr float MARKER_STEM_LENGTH = 22.0f;
static constexpr float MARKER_STEM_GAP = 4.0f;
static constexpr float MARKER_OUTLINE = 1.2f;
static constexpr float MARKER_LABEL_FONT_SIZE = 12.0f;
static constexpr float MARKER_LABEL_PADDING_X = 5.0f;
static constexpr float MARKER_LABEL_PADDING_Y = 2.5f;
static constexpr float MARKER_LABEL_ROUNDING = 4.0f;
static constexpr float MARKER_SCALE_MIN = 0.1f;
static constexpr float MARKER_SCALE_MAX = 8.0f;

static int FindPlayerClientId(CGameClient *pGameClient, const char *pNickname)
{
	int ClientId = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(str_comp_nocase(pGameClient->m_aClients[i].m_aName, pNickname) == 0)
		{
			ClientId = i;
			break;
		}
	}
	if(ClientId == -1)
	{
		ClientId = str_toint(pNickname);
		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			return -1;
		if(!pGameClient->m_aClients[ClientId].m_Active)
			ClientId = -1;
	}
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
		return ClientId;
	return -1;
}

static std::string TrimRight(const char *pInput)
{
	std::string Result(pInput);
	str_utf8_trim_right(Result.data());
	return Result;
}

static bool MatchChatCommand(const char *pLine, const char *pCommand, const char **ppRest)
{
	const int Length = str_length(pCommand);
	if(str_comp_nocase_num(pLine, pCommand, Length) != 0)
		return false;
	if(pLine[Length] != '\0' && pLine[Length] != ' ')
		return false;
	if(ppRest != nullptr)
		*ppRest = str_utf8_skip_whitespaces(pLine + Length);
	return true;
}

static bool ClientPosition(CGameClient *pGameClient, int ClientId, vec2 &Pos)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	const auto &Character = pGameClient->m_Snap.m_aCharacters[ClientId];
	if(!Character.m_Active)
		return false;
	Pos = pGameClient->m_aClients[ClientId].m_RenderPos;
	return true;
}

static int SharedPositionClientId(CGameClient *pGameClient)
{
	if(pGameClient->m_Snap.m_SpecInfo.m_Active && pGameClient->m_Snap.m_SpecInfo.m_SpectatorId >= 0)
		return pGameClient->m_Snap.m_SpecInfo.m_SpectatorId;
	return pGameClient->m_aLocalIds[g_Config.m_ClDummy];
}

static void FastPrint(CGameClient *pGameClient, const char *pName, const char *pFmt, ...)
{
	char aBuf[256];
	va_list Args;
	va_start(Args, pFmt);
	str_format_v(aBuf, sizeof(aBuf), pFmt, Args);
	va_end(Args);
	pGameClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, pName, aBuf);
}

static void FastEcho(CGameClient *pGameClient, const char *pFmt, ...)
{
	char aBuf[256];
	va_list Args;
	va_start(Args, pFmt);
	str_format_v(aBuf, sizeof(aBuf), pFmt, Args);
	va_end(Args);
	pGameClient->Echo(aBuf);
}

static void FastBoth(CGameClient *pGameClient, const char *pName, const char *pFmt, ...)
{
	char aBuf[256];
	va_list Args;
	va_start(Args, pFmt);
	str_format_v(aBuf, sizeof(aBuf), pFmt, Args);
	va_end(Args);
	pGameClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, pName, aBuf);
	pGameClient->Echo(aBuf);
}

static void PrintPlayerInfo(CGameClient *pGameClient, const char *pNickname, const char *pSkin, const char *pClan, const int Country, const int CustomColor, const int SkinColorBodyint, const int SkinColorFeetint)
{
	FastBoth(pGameClient, "Info", "- Nickname: %s", pNickname);
	FastBoth(pGameClient, "Info", "- Skin name: %s", pSkin);
	FastBoth(pGameClient, "Info", "- Clan: %s", pClan);
	FastBoth(pGameClient, "Info", "- Country: %d", Country);
	if(CustomColor)
	{
		FastBoth(pGameClient, "Info", "- Custom Color: 1");
		FastBoth(pGameClient, "Info", "- Body Color: %d", SkinColorBodyint);
		FastBoth(pGameClient, "Info", "- Feet Color: %d", SkinColorFeetint);
	}
	else
		FastBoth(pGameClient, "Info", "- Custom Color: 0");
}

static void PrintSkinInfo(CGameClient *pGameClient, const char *pSkin, const int CustomColor, const int SkinColorBodyint, const int SkinColorFeetint)
{
	FastBoth(pGameClient, "Info", "- Skin name: %s", pSkin);
	if(CustomColor)
	{
		FastBoth(pGameClient, "Info", "- Custom Color: 1");
		FastBoth(pGameClient, "Info", "- Body Color: %d", SkinColorBodyint);
		FastBoth(pGameClient, "Info", "- Feet Color: %d", SkinColorFeetint);
	}
	else
		FastBoth(pGameClient, "Info", "- Custom Color: 0");
}

static void PrintColorInfo(CGameClient *pGameClient, const int CustomColor, const int SkinColorBodyint, const int SkinColorFeetint)
{
	if(CustomColor)
	{
		FastBoth(pGameClient, "Info", "- Custom Color: 1");
		FastBoth(pGameClient, "Info", "- Body Color: %d", SkinColorBodyint);
		FastBoth(pGameClient, "Info", "- Feet Color: %d", SkinColorFeetint);
	}
	else
		FastBoth(pGameClient, "Info", "- Custom Color: 0");
}

void CChatBinds::ApplySkinToPlayer(const char *pSkin, const int CustomColor, const int SkinColorBodyint, const int SkinColorFeetint)
{
	if(g_Config.m_ClDummy == 1)
	{
		str_copy(m_aDummySkinBefore, g_Config.m_ClDummySkin, sizeof(m_aDummySkinBefore));
		m_DummyUseCustomColorBefore = g_Config.m_ClDummyUseCustomColor;
		m_DummyBodyColorBefore = g_Config.m_ClDummyColorBody;
		m_DummyFeetColorBefore = g_Config.m_ClDummyColorFeet;
		str_copy(g_Config.m_ClDummySkin, pSkin, sizeof(g_Config.m_ClDummySkin));
		g_Config.m_ClDummyUseCustomColor = CustomColor;
		g_Config.m_ClDummyColorBody = SkinColorBodyint;
		g_Config.m_ClDummyColorFeet = SkinColorFeetint;
		GameClient()->SendDummyInfo(false);
	}
	if(g_Config.m_ClDummy == 0)
	{
		str_copy(m_aPlayerSkinBefore, g_Config.m_ClPlayerSkin, sizeof(m_aPlayerSkinBefore));
		m_PlayerUseCustomColorBefore = g_Config.m_ClPlayerUseCustomColor;
		m_PlayerBodyColorBefore = g_Config.m_ClPlayerColorBody;
		m_PlayerFeetColorBefore = g_Config.m_ClPlayerColorFeet;
		str_copy(g_Config.m_ClPlayerSkin, pSkin, sizeof(g_Config.m_ClPlayerSkin));
		g_Config.m_ClPlayerUseCustomColor = CustomColor;
		g_Config.m_ClPlayerColorBody = SkinColorBodyint;
		g_Config.m_ClPlayerColorFeet = SkinColorFeetint;
		GameClient()->SendInfo(false);
	}
}

void CChatBinds::ApplyColorToPlayer(const int CustomColor, const int SkinColorBodyint, const int SkinColorFeetint)
{
	if(g_Config.m_ClDummy == 1)
	{
		str_copy(m_aDummySkinBefore, g_Config.m_ClDummySkin, sizeof(m_aDummySkinBefore));
		m_DummyUseCustomColorBefore = g_Config.m_ClDummyUseCustomColor;
		m_DummyBodyColorBefore = g_Config.m_ClDummyColorBody;
		m_DummyFeetColorBefore = g_Config.m_ClDummyColorFeet;
		g_Config.m_ClDummyUseCustomColor = CustomColor;
		g_Config.m_ClDummyColorBody = SkinColorBodyint;
		g_Config.m_ClDummyColorFeet = SkinColorFeetint;
		GameClient()->SendDummyInfo(false);
	}
	if(g_Config.m_ClDummy == 0)
	{
		str_copy(m_aPlayerSkinBefore, g_Config.m_ClPlayerSkin, sizeof(m_aPlayerSkinBefore));
		m_PlayerUseCustomColorBefore = g_Config.m_ClPlayerUseCustomColor;
		m_PlayerBodyColorBefore = g_Config.m_ClPlayerColorBody;
		m_PlayerFeetColorBefore = g_Config.m_ClPlayerColorFeet;
		g_Config.m_ClPlayerUseCustomColor = CustomColor;
		g_Config.m_ClPlayerColorBody = SkinColorBodyint;
		g_Config.m_ClPlayerColorFeet = SkinColorFeetint;
		GameClient()->SendInfo(false);
	}
}

void CChatBinds::OnConsoleInit()
{
	Console()->Register("tc_find_player", "r[player]", CFGFLAG_CLIENT, ConFindPlayer, this, "Find player info");
	Console()->Register("tc_find_skin", "r[player]", CFGFLAG_CLIENT, ConFindSkin, this, "Find player skin");
	Console()->Register("tc_copy_skin", "r[player]", CFGFLAG_CLIENT, ConCopySkin, this, "Copy skin");
	Console()->Register("tc_copy_color", "r[player]", CFGFLAG_CLIENT, ConCopyColor, this, "Copy color");
	Console()->Register("tc_backup_player_profile", "", CFGFLAG_CLIENT, ConBackupPlayerProfile, this, "Restore skin before copy");
	Console()->Register("tc_tracker_add", "r[player]", CFGFLAG_CLIENT, ConTrackerAdd, this, "Add player to tracker");
	Console()->Register("tc_tracker_remove", "r[player]", CFGFLAG_CLIENT, ConTrackerRemove, this, "Remove player from tracker");
	Console()->Register("tc_tracker_reset", "", CFGFLAG_CLIENT, ConTrackerReset, this, "Reset tracker");
	Console()->Register("tc_goto_tele_cursor", "", CFGFLAG_CLIENT, ConGotoTeleCursor, this, "Move cursor to connected teleporter");
	Console()->Register("tc_goto_finish_cursor", "", CFGFLAG_CLIENT, ConGotoFinishCursor, this, "Move cursor to finish (or start if near it)");
}

void CChatBinds::OnReset()
{
	m_vPlayersInTracker.clear();
	m_vMarkers.clear();
}

void CChatBinds::OnRender()
{
	if(m_pProfileTask)
	{
		const EHttpState State = m_pProfileTask->State();
		if(State != EHttpState::QUEUED && State != EHttpState::RUNNING)
			FinishProfile();
	}

	if(!m_vMarkers.empty())
		RenderMarkers();

	if(m_vPlayersInTracker.empty())
		return;

	Ui()->MapScreen();
	const CUIRect Screen = *Ui()->Screen();
	const float LineSize = 12.0f;
	const float FontSize = 8.0f;
	const float Margin = 2.0f;
	float y = Screen.h * 0.2f;

	for(size_t i = 0; i < m_vPlayersInTracker.size(); i++)
	{
		const int TrackerClientId = m_vPlayersInTracker[i].m_ClientId;
		if(TrackerClientId < 0 || TrackerClientId >= MAX_CLIENTS || !GameClient()->m_aClients[TrackerClientId].m_Active)
		{
			TrackerClientIdRemove(TrackerClientId);
			continue;
		}
		const CNetObj_Character *pChar = &GameClient()->m_Snap.m_aCharacters[TrackerClientId].m_Cur;
		CUIRect Line;
		Line.x = 4.0f;
		Line.y = y;
		Line.w = Screen.w / 2.0f;
		Line.h = LineSize;
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s: %.2f, %.2f", m_vPlayersInTracker[i].m_Nickname.c_str(), pChar->m_X / 32.0f, pChar->m_Y / 32.0f);
		Ui()->DoLabel(&Line, aBuf, FontSize, TEXTALIGN_ML);
		y += LineSize + Margin;
	}
}

void CChatBinds::TrackerClientIdRemove(int ClientId)
{
	for(size_t i = 0; i < m_vPlayersInTracker.size(); i++)
	{
		if(ClientId == m_vPlayersInTracker[i].m_ClientId)
		{
			m_vPlayersInTracker.erase(m_vPlayersInTracker.begin() + i);
			return;
		}
	}
}

void CChatBinds::ConFindPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	int ClientId = FindPlayerClientId(pThis->GameClient(), TrimRight(pResult->GetString(0)).c_str());
	if(ClientId == -1)
	{
		FastBoth(pThis->GameClient(), "game", "Invalid client ID");
		pThis->GameClient()->Echo("No that player on server");
		return;
	}
	const CGameClient::CClientData &ClientData = pThis->GameClient()->m_aClients[ClientId];
	if(ClientData.m_aSkinName[0])
	{
		PrintPlayerInfo(pThis->GameClient(), ClientData.m_aName, ClientData.m_aSkinName, ClientData.m_aClan, ClientData.m_Country, ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);
	}
}

void CChatBinds::ConFindSkin(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	int ClientId = FindPlayerClientId(pThis->GameClient(), TrimRight(pResult->GetString(0)).c_str());
	if(ClientId == -1)
	{
		FastBoth(pThis->GameClient(), "game", "Invalid client ID");
		pThis->GameClient()->Echo("No that player on server");
		return;
	}
	const CGameClient::CClientData &ClientData = pThis->GameClient()->m_aClients[ClientId];
	if(ClientData.m_aSkinName[0])
	{
		PrintSkinInfo(pThis->GameClient(), ClientData.m_aSkinName, ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);
	}
}

void CChatBinds::ConCopySkin(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	int ClientId = FindPlayerClientId(pThis->GameClient(), TrimRight(pResult->GetString(0)).c_str());
	if(ClientId == -1)
	{
		FastBoth(pThis->GameClient(), "game", "Invalid client ID");
		pThis->GameClient()->Echo("No that player on server");
		return;
	}
	const CGameClient::CClientData &ClientData = pThis->GameClient()->m_aClients[ClientId];
	if(ClientData.m_aSkinName[0])
	{
		PrintSkinInfo(pThis->GameClient(), ClientData.m_aSkinName, ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);
		pThis->ApplySkinToPlayer(ClientData.m_aSkinName, ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);
	}
}

void CChatBinds::ConCopyColor(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	int ClientId = FindPlayerClientId(pThis->GameClient(), TrimRight(pResult->GetString(0)).c_str());
	if(ClientId == -1)
	{
		FastBoth(pThis->GameClient(), "game", "Invalid client ID");
		pThis->GameClient()->Echo("No that player on server");
		return;
	}
	const CGameClient::CClientData &ClientData = pThis->GameClient()->m_aClients[ClientId];
	if(ClientData.m_aSkinName[0])
	{
		PrintColorInfo(pThis->GameClient(), ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);
		pThis->ApplyColorToPlayer(ClientData.m_UseCustomColor, ClientData.m_ColorBody, ClientData.m_ColorFeet);
	}
}

void CChatBinds::ConBackupPlayerProfile(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	if(g_Config.m_ClDummy == 1)
	{
		if(str_length(pThis->m_aDummySkinBefore) > 0)
		{
			str_copy(g_Config.m_ClDummySkin, pThis->m_aDummySkinBefore, sizeof(g_Config.m_ClDummySkin));
			g_Config.m_ClDummyUseCustomColor = pThis->m_DummyUseCustomColorBefore;
			g_Config.m_ClDummyColorBody = pThis->m_DummyBodyColorBefore;
			g_Config.m_ClDummyColorFeet = pThis->m_DummyFeetColorBefore;
			pThis->GameClient()->SendDummyInfo(false);
		}
		else
			pThis->GameClient()->Echo("There no info of player/skin copy");
	}
	if(g_Config.m_ClDummy == 0)
	{
		if(str_length(pThis->m_aPlayerSkinBefore) > 0)
		{
			str_copy(g_Config.m_ClPlayerSkin, pThis->m_aPlayerSkinBefore, sizeof(g_Config.m_ClPlayerSkin));
			g_Config.m_ClPlayerUseCustomColor = pThis->m_PlayerUseCustomColorBefore;
			g_Config.m_ClPlayerColorBody = pThis->m_PlayerBodyColorBefore;
			g_Config.m_ClPlayerColorFeet = pThis->m_PlayerFeetColorBefore;
			pThis->GameClient()->SendInfo(false);
		}
		else
			pThis->GameClient()->Echo("There no info of player/skin copy");
	}
}

void CChatBinds::ConTrackerAdd(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	int ClientId = FindPlayerClientId(pThis->GameClient(), TrimRight(pResult->GetString(0)).c_str());
	if(ClientId == -1)
	{
		FastBoth(pThis->GameClient(), "Tracker", "Invalid client ID");
		pThis->GameClient()->Echo("No that player on server");
		return;
	}
	const CGameClient::CClientData &ClientData = pThis->GameClient()->m_aClients[ClientId];
	if(ClientData.m_aName[0] && ClientId >= 0 && ClientId < MAX_CLIENTS)
	{
		FastPrint(pThis->GameClient(), "Tracker", "Added player: %s", ClientData.m_aName);
		FastEcho(pThis->GameClient(), "[[green]]Tracker: Added player: %s", ClientData.m_aName);
		pThis->m_vPlayersInTracker.push_back({ClientData.ClientId(), ClientData.m_aName});
	}
}

void CChatBinds::ConTrackerRemove(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	int ClientId = FindPlayerClientId(pThis->GameClient(), TrimRight(pResult->GetString(0)).c_str());
	if(ClientId == -1)
	{
		FastBoth(pThis->GameClient(), "Tracker", "Invalid client ID");
		pThis->GameClient()->Echo("No that player on server");
		return;
	}
	const CGameClient::CClientData &ClientData = pThis->GameClient()->m_aClients[ClientId];
	if(ClientData.m_aName[0] && ClientId >= 0 && ClientId < MAX_CLIENTS)
	{
		for(size_t i = 0; i < pThis->m_vPlayersInTracker.size(); i++)
		{
			if(ClientData.ClientId() == pThis->m_vPlayersInTracker[i].m_ClientId)
			{
				FastPrint(pThis->GameClient(), "Tracker", "Removed player: %s", pThis->m_vPlayersInTracker[i].m_Nickname.c_str());
				FastEcho(pThis->GameClient(), "[[red]]Tracker: Removed player: %s", pThis->m_vPlayersInTracker[i].m_Nickname.c_str());
				pThis->m_vPlayersInTracker.erase(pThis->m_vPlayersInTracker.begin() + i);
				return;
			}
		}
		FastBoth(pThis->GameClient(), "Tracker", "Player not in tracker");
		pThis->GameClient()->Echo("Player not in tracker");
	}
}

void CChatBinds::ConTrackerReset(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pThis = static_cast<CChatBinds *>(pUserData);
	for(size_t i = 0; i < pThis->m_vPlayersInTracker.size(); i++)
	{
		FastPrint(pThis->GameClient(), "Tracker", "Removed player: %s", pThis->m_vPlayersInTracker[i].m_Nickname.c_str());
		FastEcho(pThis->GameClient(), "[[red]]Tracker: Removed player: %s", pThis->m_vPlayersInTracker[i].m_Nickname.c_str());
	}
	pThis->m_vPlayersInTracker.clear();
}

void CChatBinds::ConGotoTeleCursor(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pSelf = static_cast<CChatBinds *>(pUserData);
	if(pSelf->GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW || !pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		pSelf->GameClient()->Echo("You're not in freeview spectating");
		return;
	}

	CCollision *pCollision = pSelf->GameClient()->Collision();
	if(pCollision->TeleLayer() == nullptr)
		return;

	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	const vec2 Center = pSelf->GameClient()->m_Camera.m_Center;
	const ivec2 CenterTile = ivec2(round_to_int(Center.x / 32.0f), round_to_int(Center.y / 32.0f));

	const CTeleTile *pTele = pCollision->TeleLayer();
	bool FoundTele = false;
	CTeleTile TeleTile{};
	float BestTeleDist = -1.0f;
	for(int y = CenterTile.y - 1; y <= CenterTile.y + 1; y++)
	{
		if(y < 0 || y >= Height)
			continue;
		for(int x = CenterTile.x - 1; x <= CenterTile.x + 1; x++)
		{
			if(x < 0 || x >= Width)
				continue;
			const int TileIndex = y * Width + x;
			const CTeleTile &Tile = pTele[TileIndex];
			if(Tile.m_Number <= 0 || Tile.m_Type <= 0)
				continue;
			const vec2 Pos = vec2(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
			const float Dist = distance(Pos, Center);
			if(BestTeleDist < 0.0f || Dist < BestTeleDist)
			{
				BestTeleDist = Dist;
				TeleTile = Tile;
				FoundTele = true;
			}
		}
	}

	if(!FoundTele)
	{
		pSelf->GameClient()->Echo("No teleporter near cursor");
		return;
	}

	const int Type = TeleTile.m_Type;

	std::vector<ivec2> Targets;

	auto IsTypeAny = [](int Value, const int *Types, size_t Count) {
		for(size_t i = 0; i < Count; i++)
		{
			if(Value == Types[i])
				return true;
		}
		return false;
	};

	auto CollectTargets = [&](const int *Types, size_t Count) {
		Targets.clear();
		for(int y = 0; y < Height; y++)
		{
			for(int x = 0; x < Width; x++)
			{
				const int TileIndex = y * Width + x;
				const CTeleTile &Tile = pTele[TileIndex];
				bool Match = false;
				for(size_t i = 0; i < Count; i++)
				{
					if(Tile.m_Type == Types[i]) { Match = true; break; }
				}
				if(Tile.m_Number == TeleTile.m_Number && Match)
					Targets.emplace_back(x, y);
			}
		}
	};

	const int TeleOutTypes[] = {TILE_TELEOUT};
	const int TeleCheckOut[] = {TILE_TELECHECKOUT};
	const int TeleIn[] = {TILE_TELEIN, TILE_TELEINEVIL, TILE_TELEINWEAPON, TILE_TELEINHOOK};
	const int TeleCheckIn[] = {TILE_TELECHECK, TILE_TELECHECKIN, TILE_TELECHECKINEVIL};

	const bool IsTeleOut = IsTypeAny(Type, TeleOutTypes, std::size(TeleOutTypes));
	const bool IsTeleCheckOut = IsTypeAny(Type, TeleCheckOut, std::size(TeleCheckOut));
	const bool IsTeleIn = IsTypeAny(Type, TeleIn, std::size(TeleIn));
	const bool IsTeleCheckIn = IsTypeAny(Type, TeleCheckIn, std::size(TeleCheckIn));

	if(IsTeleOut)
	{
		CollectTargets(TeleIn, std::size(TeleIn));
	}
	else if(IsTeleCheckOut)
	{
		CollectTargets(TeleCheckIn, std::size(TeleCheckIn));
		if(Targets.empty())
			CollectTargets(TeleIn, std::size(TeleIn));
	}
	else if(IsTeleCheckIn)
	{
		CollectTargets(TeleCheckOut, std::size(TeleCheckOut));
	}
	else if(IsTeleIn)
	{
		CollectTargets(TeleOutTypes, std::size(TeleOutTypes));
		if(Targets.empty())
			CollectTargets(TeleCheckOut, std::size(TeleCheckOut));
	}

	if(Targets.empty())
	{
		pSelf->GameClient()->Echo("No teleporter destination found");
		return;
	}

	int BestIndex = 0;
	float BestDist = -1.0f;
	for(int i = 0; i < (int)Targets.size(); i++)
	{
		const vec2 Pos = vec2(Targets[i].x * 32.0f + 16.0f, Targets[i].y * 32.0f + 16.0f);
		const float Dist = distance(Pos, Center);
		if(BestDist < 0.0f || Dist < BestDist)
		{
			BestDist = Dist;
			BestIndex = i;
		}
	}

	const vec2 TargetPos = vec2(Targets[BestIndex].x * 32.0f + 16.0f, Targets[BestIndex].y * 32.0f + 16.0f);
	pSelf->GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy] = TargetPos;
}

bool CChatBinds::OnChatCommand(int Team, const char *pLine)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return false;

	const char *pRest = nullptr;
	if(MatchChatCommand(pLine, "!sendpos", &pRest))
	{
		vec2 Pos;
		if(!ClientPosition(GameClient(), SharedPositionClientId(GameClient()), Pos))
			return true;

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "!pos %.2f %.2f", Pos.x / 32.0f, Pos.y / 32.0f);
		GameClient()->m_Chat.SendChat(Team, aBuf);
		return true;
	}

	if(MatchChatCommand(pLine, "!findhours", &pRest) || MatchChatCommand(pLine, "!hours", &pRest))
	{
		char aName[MAX_NAME_LENGTH];
		if(*pRest != '\0')
		{
			str_copy(aName, pRest);
		}
		else
		{
			const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
			if(LocalId < 0 || LocalId >= MAX_CLIENTS)
				return true;
			str_copy(aName, GameClient()->m_aClients[LocalId].m_aName);
		}
		FetchProfile(aName);
		return true;
	}

	return false;
}

void CChatBinds::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType != NETMSGTYPE_SV_CHAT || !g_Config.m_FcSendPosMarkers)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	if(pMsg->m_ClientId < 0 || pMsg->m_ClientId >= MAX_CLIENTS)
		return;

	float X, Y;
	if(std::sscanf(pMsg->m_pMessage, "!pos %f %f", &X, &Y) != 2)
		return;
	if(X < -10000.0f || X > 10000.0f || Y < -10000.0f || Y > 10000.0f)
		return;

	AddMarker(vec2(X * 32.0f, Y * 32.0f), GameClient()->m_aClients[pMsg->m_ClientId].m_aName);
}

void CChatBinds::OnStateChange(int NewState, int OldState)
{
	m_vMarkers.clear();
	if(NewState != IClient::STATE_ONLINE && NewState != IClient::STATE_DEMOPLAYBACK)
	{
		if(m_pProfileTask)
		{
			m_pProfileTask->Abort();
			m_pProfileTask = nullptr;
		}
	}
}

void CChatBinds::AddMarker(vec2 Pos, const char *pName)
{
	for(auto It = m_vMarkers.begin(); It != m_vMarkers.end();)
	{
		if(str_comp(It->m_aName, pName) == 0)
			It = m_vMarkers.erase(It);
		else
			++It;
	}
	if((int)m_vMarkers.size() >= MAX_MARKERS)
		m_vMarkers.erase(m_vMarkers.begin());

	SMarker Marker;
	Marker.m_Pos = Pos;
	str_copy(Marker.m_aName, pName);
	Marker.m_Time = time();
	m_vMarkers.push_back(Marker);
}

void CChatBinds::RenderMarkers()
{
	const int64_t Now = time();
	const int64_t Duration = (int64_t)g_Config.m_FcSendPosTime * time_freq();
	for(auto It = m_vMarkers.begin(); It != m_vMarkers.end();)
	{
		if(Now - It->m_Time >= Duration)
			It = m_vMarkers.erase(It);
		else
			++It;
	}
	if(m_vMarkers.empty())
		return;

	auto MarkerAlpha = [&](const SMarker &Marker) {
		const float Elapsed = (float)(Now - Marker.m_Time) / (float)time_freq();
		const float Left = (float)Duration / (float)time_freq() - Elapsed;
		return std::clamp(std::min(Left, Elapsed * 4.0f), 0.0f, 1.0f);
	};

	const CScreenRect PrevScreen = Graphics()->GetScreen();
	Graphics()->SetScreenRotation(GameClient()->m_Camera.TiltAngle());
	const float Zoom = std::max(GameClient()->m_Camera.m_RenderZoom, 0.01f);
	const float Scale = std::clamp(Zoom, MARKER_SCALE_MIN, MARKER_SCALE_MAX);
	Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, Zoom);

	const ColorRGBA Accent(0.45f, 0.9f, 0.95f, 1.0f);

	const float PointRadius = MARKER_POINT_RADIUS * Scale;
	const float Outline = MARKER_OUTLINE * Scale;
	const float StemWidth = MARKER_STEM_WIDTH * Scale;
	const float StemLength = MARKER_STEM_LENGTH * Scale;
	const float StemGap = MARKER_STEM_GAP * Scale;
	const float LabelPaddingX = MARKER_LABEL_PADDING_X * Scale;
	const float LabelPaddingY = MARKER_LABEL_PADDING_Y * Scale;
	const float LabelRounding = MARKER_LABEL_ROUNDING * Scale;
	const float LabelFontSize = MARKER_LABEL_FONT_SIZE * Scale;

	Graphics()->TextureClear();
	for(const SMarker &Marker : m_vMarkers)
	{
		const float Alpha = MarkerAlpha(Marker);
		if(Alpha <= 0.0f)
			continue;

		const vec2 Pos = Marker.m_Pos;
		const STextBoundingBox TextBox = TextRender()->TextBoundingBox(LabelFontSize, Marker.m_aName);
		const float LabelWidth = TextBox.m_W + LabelPaddingX * 2.0f;
		const float LabelHeight = TextBox.m_H + LabelPaddingY * 2.0f;
		const float LabelLeft = Pos.x - LabelWidth / 2.0f;
		const float StemTop = Pos.y - PointRadius - StemGap - StemLength;
		const float StemBottom = Pos.y - PointRadius - StemGap;
		const float LabelTop = StemTop - LabelHeight;

		Graphics()->DrawRect(Pos.x - StemWidth / 2.0f - Outline, StemTop - Outline, StemWidth + Outline * 2.0f, StemBottom - StemTop + Outline * 2.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f * Alpha), IGraphics::CORNER_ALL, StemWidth / 2.0f);
		Graphics()->DrawRect(Pos.x - StemWidth / 2.0f, StemTop, StemWidth, StemBottom - StemTop, ColorRGBA(Accent.r, Accent.g, Accent.b, 0.85f * Alpha), IGraphics::CORNER_ALL, StemWidth / 2.0f);

		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.5f * Alpha);
		Graphics()->DrawCircle(Pos.x, Pos.y, PointRadius + Outline, 24);
		Graphics()->SetColor(Accent.r, Accent.g, Accent.b, 0.95f * Alpha);
		Graphics()->DrawCircle(Pos.x, Pos.y, PointRadius, 24);
		Graphics()->QuadsEnd();

		Graphics()->DrawRect(LabelLeft - Outline, LabelTop - Outline, LabelWidth + Outline * 2.0f, LabelHeight + Outline * 2.0f, ColorRGBA(Accent.r, Accent.g, Accent.b, 0.3f * Alpha), IGraphics::CORNER_ALL, LabelRounding);
		Graphics()->DrawRect(LabelLeft, LabelTop, LabelWidth, LabelHeight, ColorRGBA(0.04f, 0.07f, 0.09f, 0.62f * Alpha), IGraphics::CORNER_ALL, LabelRounding - Outline);

		TextRender()->TextColor(0.82f, 0.97f, 1.0f, Alpha);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.35f * Alpha);
		TextRender()->Text(LabelLeft + LabelPaddingX - TextBox.m_X, LabelTop + LabelPaddingY - TextBox.m_Y, LabelFontSize, Marker.m_aName, -1.0f);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->MapScreen(PrevScreen);
	Graphics()->SetScreenRotation(0.0f);
}

void CChatBinds::ConGotoFinishCursor(IConsole::IResult *pResult, void *pUserData)
{
	CChatBinds *pSelf = static_cast<CChatBinds *>(pUserData);
	if(pSelf->GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW || !pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		pSelf->GameClient()->Echo("You're not in freeview spectating");
		return;
	}

	CCollision *pCollision = pSelf->GameClient()->Collision();
	if(!pCollision)
		return;

	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	const vec2 Center = pSelf->GameClient()->m_Camera.m_Center;
	const ivec2 CenterTile = ivec2(round_to_int(Center.x / 32.0f), round_to_int(Center.y / 32.0f));

	auto HasTile = [&](int Index, int Tile) -> bool {
		return pCollision->GetTileIndex(Index) == Tile || pCollision->GetFrontTileIndex(Index) == Tile;
	};

	bool NearFinish = false;
	for(int y = CenterTile.y - 1; y <= CenterTile.y + 1; y++)
	{
		if(y < 0 || y >= Height)
			continue;
		for(int x = CenterTile.x - 1; x <= CenterTile.x + 1; x++)
		{
			if(x < 0 || x >= Width)
				continue;
			const int TileIndex = y * Width + x;
			if(HasTile(TileIndex, TILE_FINISH))
			{
				NearFinish = true;
				break;
			}
		}
	}

	std::vector<ivec2> Targets;

	auto CollectTargets = [&](const int SearchType) {
		Targets.clear();
		for(int y = 0; y < Height; y++)
		{
			for(int x = 0; x < Width; x++)
			{
				const int TileIndex = y * Width + x;
				if(HasTile(TileIndex, SearchType))
					Targets.emplace_back(x, y);
			}
		}
	};

	CollectTargets(NearFinish ? TILE_START : TILE_FINISH);

	if(Targets.empty())
	{
		pSelf->GameClient()->Echo(NearFinish ? "No Start found" : "No Finish found");
		return;
	}

	int BestIndex = 0;
	float BestDist = -1.0f;
	for(int i = 0; i < (int)Targets.size(); i++)
	{
		const vec2 Pos = vec2(Targets[i].x * 32.0f + 16.0f, Targets[i].y * 32.0f + 16.0f);
		const float Dist = distance(Pos, Center);
		if(BestDist < 0.0f || Dist < BestDist)
		{
			BestDist = Dist;
			BestIndex = i;
		}
	}

	const vec2 TargetPos = vec2(Targets[BestIndex].x * 32.0f + 16.0f, Targets[BestIndex].y * 32.0f + 16.0f);
	pSelf->GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy] = TargetPos;
}

static std::string HtmlToText(const unsigned char *pData, size_t Length)
{
	std::string Plain;
	Plain.reserve(Length);
	bool InTag = false;
	bool LastSpace = true;
	for(size_t i = 0; i < Length; i++)
	{
		const char c = (char)pData[i];
		if(c == '<')
		{
			InTag = true;
			continue;
		}
		if(i + 6 <= Length && str_comp_num((const char *)pData + i, "&nbsp;", 6) == 0)
		{
			if(!LastSpace)
			{
				Plain.push_back(' ');
				LastSpace = true;
			}
			i += 5;
			continue;
		}
		if(InTag)
		{
			if(c == '>')
				InTag = false;
			continue;
		}
		if(c == ' ' || c == '\n' || c == '\r' || c == '\t')
		{
			if(!LastSpace)
			{
				Plain.push_back(' ');
				LastSpace = true;
			}
			continue;
		}
		Plain.push_back(c);
		LastSpace = false;
	}
	return Plain;
}

static bool ReadNumberBefore(const std::string &Plain, size_t Pos, std::string &Out)
{
	size_t Start = Pos;
	while(Start > 0 && (std::isdigit((unsigned char)Plain[Start - 1]) || Plain[Start - 1] == ','))
		Start--;
	if(Start == Pos)
		return false;
	Out.clear();
	for(size_t i = Start; i < Pos; i++)
	{
		if(Plain[i] != ',')
			Out.push_back(Plain[i]);
	}
	return true;
}

static bool ReadPoints(const std::string &Plain, std::string &Out)
{
	const size_t Pos = Plain.find(" total)");
	return Pos != std::string::npos && ReadNumberBefore(Plain, Pos, Out);
}

static bool ReadRank(const std::string &Plain, std::string &Out)
{
	for(size_t Pos = Plain.find("Rank "); Pos != std::string::npos; Pos = Plain.find("Rank ", Pos + 5))
	{
		if(Pos >= 5 && Plain.compare(Pos - 5, 5, "Team ") == 0)
			continue;
		const size_t Start = Pos + 5;
		size_t End = Start;
		while(End < Plain.size() && std::isdigit((unsigned char)Plain[End]))
			End++;
		if(End == Start || End >= Plain.size() || Plain[End] != '.')
			continue;
		Out.assign(Plain, Start, End - Start);
		return true;
	}
	return false;
}

static bool ReadFirstFinish(const std::string &Plain, std::string &Date, std::string &MapName)
{
	const char *pHeading = "First Finish ";
	const size_t Pos = Plain.find(pHeading);
	if(Pos == std::string::npos)
		return false;

	const size_t Start = Pos + str_length(pHeading);
	if(Start + 10 >= Plain.size())
		return false;
	for(int i = 0; i < 10; i++)
	{
		const char c = Plain[Start + i];
		const bool Digit = std::isdigit((unsigned char)c) != 0;
		if((i == 4 || i == 7) ? c != '-' : !Digit)
			return false;
	}
	Date.assign(Plain, Start, 10);

	size_t NameStart = Start + 10;
	if(NameStart + 7 <= Plain.size() && Plain[NameStart] == ' ')
		NameStart += 7;
	while(NameStart < Plain.size() && Plain[NameStart] == ' ')
		NameStart++;
	size_t NameEnd = Plain.find(" (", NameStart);
	if(NameEnd == std::string::npos)
		NameEnd = Plain.size();
	MapName.assign(Plain, NameStart, NameEnd - NameStart);
	return true;
}

void CChatBinds::FetchProfile(const char *pName)
{
	if(m_pProfileTask && !m_pProfileTask->Done())
	{
		GameClient()->Echo("A ddnet.org lookup is already running");
		return;
	}

	char aEscaped[MAX_NAME_LENGTH * 4];
	EscapeUrl(aEscaped, sizeof(aEscaped), pName);
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "https://ddnet.org/players/%s/", aEscaped);

	m_pProfileTask = HttpGet(aUrl);
	m_pProfileTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pProfileTask->IpResolve(IPRESOLVE::V4);
	m_pProfileTask->HeaderString("User-Agent", FUSION_NAME "/" FUSION_VERSION);
	Http()->Run(m_pProfileTask);

	str_copy(m_aProfileName, pName);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Looking up the DDNet profile of %s", pName);
	GameClient()->Echo(aBuf);
}

void CChatBinds::FinishProfile()
{
	std::shared_ptr<IHttpRequest> pTask = m_pProfileTask;
	m_pProfileTask = nullptr;
	if(!pTask)
		return;

	if(pTask->State() != EHttpState::DONE || pTask->StatusCode() != 200)
	{
		FastEcho(GameClient(), "[[red]]%s: could not load the DDNet profile", m_aProfileName);
		return;
	}

	unsigned char *pResult = nullptr;
	size_t ResultLength = 0;
	pTask->Result(&pResult, &ResultLength);
	if(pResult == nullptr || ResultLength == 0)
	{
		FastEcho(GameClient(), "[[red]]%s: could not load the DDNet profile", m_aProfileName);
		return;
	}

	const std::string Plain = HtmlToText(pResult, ResultLength);

	std::string Hours;
	const size_t HoursPos = Plain.find(" hours played");
	if(HoursPos == std::string::npos || !ReadNumberBefore(Plain, HoursPos, Hours))
	{
		FastEcho(GameClient(), "[[red]]%s: no DDNet profile found", m_aProfileName);
		return;
	}
	FastEcho(GameClient(), "[[green]]%s: %s hours played in the past 365 days", m_aProfileName, Hours.c_str());

	std::string Points, Rank;
	if(ReadPoints(Plain, Points))
	{
		if(ReadRank(Plain, Rank))
			FastEcho(GameClient(), "[[green]]%s: %s points, global rank %s", m_aProfileName, Points.c_str(), Rank.c_str());
		else
			FastEcho(GameClient(), "[[green]]%s: %s points", m_aProfileName, Points.c_str());
	}

	std::string FirstFinish, FirstFinishMap;
	if(ReadFirstFinish(Plain, FirstFinish, FirstFinishMap))
		FastEcho(GameClient(), "[[green]]%s: first finish %s on %s", m_aProfileName, FirstFinish.c_str(), FirstFinishMap.c_str());
}
