#include "chatbinds.h"

#include <cstdarg>

#include "engine/shared/config.h"

#include "game/client/gameclient.h"
#include "game/client/components/camera.h"
#include "game/client/components/controls.h"
#include "game/collision.h"
#include "game/mapitems.h"

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
}

void CChatBinds::OnRender()
{
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
