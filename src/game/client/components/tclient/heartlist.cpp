#include "heartlist.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>

void CHeartList::OnNewSnapshot()
{
	UpdateFriendPlayers();

	if(m_NeedsChatRebuild)
	{
		m_NeedsChatRebuild = false;
		GameClient()->m_Chat.RebuildChat();
	}
}

void CHeartList::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::TCLIENTFRIENDLIST);

	Console()->Register("update_heart_type", "i[type_index] s[name] i[color] i[prioritized]", CFGFLAG_CLIENT, ConUpsertHeartType, this, "Update or add a specific heart type");
	Console()->Register("add_heart_entry", "s[type] s[name]", CFGFLAG_CLIENT, ConAddHeartEntry, this, "Adds a specific friend entry");

	Console()->Register("friend", "s[nick] s[heart]", CFGFLAG_CLIENT, ConFriend, this, "Add a friend with a custom heart");
	Console()->Register("unfriend", "s[nick]", CFGFLAG_CLIENT, ConUnfriend, this, "Remove a friend and their heart");
}

void CHeartList::ConFriend(IConsole::IResult *pResult, void *pUserData)
{
	const char *pName = pResult->GetString(0);
	const char *pHeart = pResult->GetString(1);
	CHeartList *pThis = static_cast<CHeartList *>(pUserData);

	if(str_comp(pName, "") == 0)
	{
		pThis->GameClient()->Echo("Usage: .friend <nick> <heart>");
		return;
	}

	pThis->AddFriend(pName, pHeart);
	char aBuf[128];
	CHeartType *pType = pThis->FindHeartType(pHeart);
	str_format(aBuf, sizeof(aBuf), "Added friend '%s' with heart '%s'", pName, pType ? pType->m_aName : pHeart);
	pThis->GameClient()->Echo(aBuf);
}

void CHeartList::ConUnfriend(IConsole::IResult *pResult, void *pUserData)
{
	const char *pName = pResult->GetString(0);
	CHeartList *pThis = static_cast<CHeartList *>(pUserData);

	if(str_comp(pName, "") == 0)
	{
		pThis->GameClient()->Echo("Usage: .unfriend <nick>");
		return;
	}

	bool Found = false;
	for(const CHeartEntry &Entry : pThis->m_vHeartEntries)
	{
		if(str_comp(Entry.m_aName, pName) == 0)
		{
			Found = true;
			break;
		}
	}
	if(!Found)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Not a friend: '%s'", pName);
		pThis->GameClient()->Echo(aBuf);
		return;
	}

	pThis->RemoveFriend(pName);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Removed friend '%s'", pName);
	pThis->GameClient()->Echo(aBuf);
}

void CHeartList::ConAddHeartEntry(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pName = pResult->GetString(1);
	CHeartList *pThis = static_cast<CHeartList *>(pUserData);
	pThis->AddFriend(pName, pType);
}

void CHeartList::ConUpsertHeartType(IConsole::IResult *pResult, void *pUserData)
{
	int Index = pResult->GetInteger(0);
	const char *pType = pResult->GetString(1);
	unsigned int ColorInt = pResult->GetInteger(2);
	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(ColorInt));
	bool Prioritized = pResult->NumArguments() <= 3 || pResult->GetInteger(3) != 0;
	CHeartList *pThis = static_cast<CHeartList *>(pUserData);
	pThis->UpsertHeartType(Index, pType, Color, Prioritized);
}

void CHeartList::AddFriend(const char *pName, const char *pHeart)
{
	if(str_comp(pName, "") == 0)
		return;

	CHeartType *pHeartType = FindHeartType(pHeart);
	if(pHeartType == m_pHeartTypeDefault && str_comp(pHeart, "default") != 0)
	{
		AddHeartType(pHeart, ColorRGBA(1.0f, 0.2f, 0.2f, 1.0f));
		pHeartType = FindHeartType(pHeart);
	}

	IFriends *pFriends = GameClient()->Friends();
	if(pFriends)
		pFriends->AddFriend(pName, "");
	for(auto It = m_vHeartEntries.begin(); It != m_vHeartEntries.end();)
	{
		if(str_comp(It->m_aName, pName) == 0)
			It = m_vHeartEntries.erase(It);
		else
			++It;
	}

	m_vHeartEntries.emplace_back(pHeartType, pName);

	UpdateFriendPlayers();
	m_NeedsChatRebuild = true;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Added friend '%s' with heart '%s'", pName, pHeartType->m_aName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "friendlist", aBuf);
}

void CHeartList::RemoveFriend(const char *pName)
{
	if(str_comp(pName, "") == 0)
		return;

	IFriends *pFriends = GameClient()->Friends();
	if(pFriends)
		pFriends->RemoveFriend(pName, "");

	for(auto It = m_vHeartEntries.begin(); It != m_vHeartEntries.end();)
	{
		if(str_comp(It->m_aName, pName) == 0)
			It = m_vHeartEntries.erase(It);
		else
			++It;
	}

	UpdateFriendPlayers();
	m_NeedsChatRebuild = true;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Removed friend '%s'", pName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "friendlist", aBuf);
}

void CHeartList::RemoveFriendBinding(const char *pName)
{
	if(str_comp(pName, "") == 0)
		return;

	bool Removed = false;
	for(auto It = m_vHeartEntries.begin(); It != m_vHeartEntries.end();)
	{
		if(str_comp(It->m_aName, pName) == 0)
		{
			It = m_vHeartEntries.erase(It);
			Removed = true;
		}
		else
			++It;
	}

	if(!Removed)
		return;

	UpdateFriendPlayers();
	m_NeedsChatRebuild = true;
}

bool CHeartList::OnChatInput(const char *pMessage)
{
	if(!pMessage)
		return false;

	const char *pMsg = str_utf8_skip_whitespaces(pMessage);

	{
		const char *pArgs = nullptr;
		const char *apCmds[] = { ".friend", "/friend" };
		for(const char *pCmd : apCmds)
		{
			const int CmdLen = str_length(pCmd);
			if(str_startswith_nocase(pMsg, pCmd) && (pMsg[CmdLen] == ' ' || pMsg[CmdLen] == '\0'))
			{
				pArgs = pMsg + CmdLen;
				break;
			}
		}
		if(pArgs != nullptr)
		{
			while(*pArgs == ' ')
				pArgs++;

			char aNick[MAX_NAME_LENGTH];
			int i = 0;
			while(*pArgs && *pArgs != ' ' && i < MAX_NAME_LENGTH - 1)
				aNick[i++] = *pArgs++;
			aNick[i] = '\0';
			while(*pArgs == ' ')
				pArgs++;

			char aHeart[MAX_HEART_TYPE_LENGTH];
			str_copy(aHeart, pArgs, sizeof(aHeart));

			if(str_comp(aNick, "") == 0)
			{
				GameClient()->Echo("Usage: .friend <nick> <heart>");
			}
			else
			{
				if(str_comp(aHeart, "") == 0)
					str_copy(aHeart, "default");
				AddFriend(aNick, aHeart);
			}
			return true;
		}
	}

	{
		const char *pArgs = nullptr;
		const char *apCmds[] = { ".unfriend", "/unfriend" };
		for(const char *pCmd : apCmds)
		{
			const int CmdLen = str_length(pCmd);
			if(str_startswith_nocase(pMsg, pCmd) && (pMsg[CmdLen] == ' ' || pMsg[CmdLen] == '\0'))
			{
				pArgs = pMsg + CmdLen;
				break;
			}
		}
		if(pArgs != nullptr)
		{
			while(*pArgs == ' ')
				pArgs++;

			char aNick[MAX_NAME_LENGTH];
			str_copy(aNick, pArgs, sizeof(aNick));
			if(str_comp(aNick, "") != 0)
				RemoveFriend(aNick);
			else
				GameClient()->Echo("Usage: .unfriend <nick>");
			return true;
		}
	}

	return false;
}

void CHeartList::UpsertHeartType(int Index, const char *pName, ColorRGBA Color, bool Prioritized)
{
	if(str_comp(pName, "") == 0)
		return;

	if(Index >= 0 && Index < static_cast<int>(m_HeartTypes.size()))
	{
		str_copy(m_HeartTypes[Index]->m_aName, pName);
		m_HeartTypes[Index]->m_Color = Color;
		m_HeartTypes[Index]->m_Prioritized = Prioritized;
	}
	else
	{
		AddHeartType(pName, Color, Prioritized);
	}

	UpdateFriendPlayers();
	m_NeedsChatRebuild = true;
}

void CHeartList::AddHeartType(const char *pName, ColorRGBA Color, bool Prioritized)
{
	if(str_comp(pName, "") == 0)
		return;

	CHeartType *pType = FindHeartType(pName);
	if(pType == m_pHeartTypeDefault)
	{
		m_HeartTypes.push_back(new CHeartType(pName, Color));
		m_HeartTypes.back()->m_Prioritized = Prioritized;
	}
	else
	{
		pType->m_Color = Color;
	}

	UpdateFriendPlayers();
	m_NeedsChatRebuild = true;
}

void CHeartList::RemoveHeartType(const char *pName)
{
	CHeartType Type(pName);
	for(auto It = m_HeartTypes.begin(); It != m_HeartTypes.end(); ++It)
	{
		if(**It == Type && (*It)->m_Removable)
		{
			for(CHeartEntry &Entry : m_vHeartEntries)
			{
				if(*Entry.m_pHeartType == **It)
					Entry.m_pHeartType = m_pHeartTypeDefault;
			}
			m_HeartTypes.erase(It);

			UpdateFriendPlayers();
			GameClient()->m_Chat.RebuildChat();
			return;
		}
	}
}

CHeartType *CHeartList::FindHeartType(const char *pName)
{
	CHeartType Type(pName);
	auto It = std::find_if(m_HeartTypes.begin(), m_HeartTypes.end(),
		[&Type](CHeartType *pHeartTypePtr) { return *pHeartTypePtr == Type; });
	if(It != m_HeartTypes.end())
		return *It;
	else
		return m_pHeartTypeDefault;
}

void CHeartList::MoveHeartType(int From, int To)
{
	if(From == To || From < 0 || To < 0 || From >= static_cast<int>(m_HeartTypes.size()) || To >= static_cast<int>(m_HeartTypes.size()))
		return;

	CHeartType *pType = m_HeartTypes[From];
	m_HeartTypes.erase(m_HeartTypes.begin() + From);
	m_HeartTypes.insert(m_HeartTypes.begin() + To, pType);
}

void CHeartList::UpdateFriendPlayers()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aFriendData[i].m_IsFriend = false;
		m_aFriendData[i].m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		for(CHeartEntry &Entry : m_vHeartEntries)
		{
			if(str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aName) == 0 && str_comp(Entry.m_aName, "") != 0)
			{
				m_aFriendData[i].m_IsFriend = true;
				m_aFriendData[i].m_Color = Entry.m_pHeartType->m_Color;
			}
		}
	}
}

CHeartList::~CHeartList()
{
	for(CHeartType *pHeartType : m_HeartTypes)
		delete pHeartType;
	m_HeartTypes.clear();
}

CHeartList::CHeartList()
{
	str_copy(m_HeartTypes[0]->m_aName, "default");
	m_HeartTypes[0]->m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
}

static inline void EscapeParam(char *pDst, const char *pSrc, int Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

void CHeartList::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CHeartList *pThis = (CHeartList *)pUserData;

	char aBuf[512];
	for(int i = 0; i < static_cast<int>(pThis->m_HeartTypes.size()); i++)
	{
		CHeartType &HeartType = *pThis->m_HeartTypes[i];

		char aEscapeType[MAX_HEART_TYPE_LENGTH * 2];
		EscapeParam(aEscapeType, HeartType.m_aName, sizeof(aEscapeType));
		ColorHSLA Color = color_cast<ColorHSLA>(HeartType.m_Color);

		str_format(aBuf, sizeof(aBuf), "update_heart_type %d \"%s\" %d %d", i, aEscapeType, Color.Pack(false), HeartType.m_Prioritized ? 1 : 0);
		pConfigManager->WriteLine(aBuf, ConfigDomain::TCLIENTFRIENDLIST);
	}
	for(CHeartEntry &Entry : pThis->m_vHeartEntries)
	{
		char aEscapeType[MAX_HEART_TYPE_LENGTH * 2];
		char aEscapeName[MAX_NAME_LENGTH * 2];
		EscapeParam(aEscapeType, Entry.m_pHeartType->m_aName, sizeof(aEscapeType));
		EscapeParam(aEscapeName, Entry.m_aName, sizeof(aEscapeName));

		str_format(aBuf, sizeof(aBuf), "add_heart_entry \"%s\" \"%s\"", aEscapeType, aEscapeName);
		pConfigManager->WriteLine(aBuf, ConfigDomain::TCLIENTFRIENDLIST);
	}
}
