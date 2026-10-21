#include "react_bypass.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/friends.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/components/tclient/heartlist.h>

bool CReactBypass::IsMuteName(const char *pName) const
{
	if(pName == nullptr || pName[0] == '\0')
		return false;
	for(const std::string &Name : m_vMuteNames)
	{
		if(str_comp(Name.c_str(), pName) == 0)
			return true;
	}
	return false;
}

void CReactBypass::AddMuteName(const char *pName)
{
	if(pName == nullptr || pName[0] == '\0' || IsMuteName(pName))
		return;
	m_vMuteNames.emplace_back(pName);
}

void CReactBypass::AddTemp(const char *pOld, const char *pNew, int Kind, const char *pType, const char *pReason)
{
	for(const STempEntry &Entry : m_vTemp)
	{
		if(Entry.m_Kind == Kind && str_comp(Entry.m_aNewName, pNew) == 0)
			return;
	}
	STempEntry Entry;
	str_copy(Entry.m_aOldName, pOld);
	str_copy(Entry.m_aNewName, pNew);
	str_copy(Entry.m_aType, pType != nullptr ? pType : "");
	str_copy(Entry.m_aReason, pReason != nullptr ? pReason : "");
	Entry.m_Kind = Kind;
	m_vTemp.push_back(Entry);
}

void CReactBypass::RemoveTempFor(const char *pName)
{
	for(size_t i = 0; i < m_vTemp.size();)
	{
		STempEntry &Entry = m_vTemp[i];
		if(str_comp(Entry.m_aOldName, pName) != 0 && str_comp(Entry.m_aNewName, pName) != 0)
		{
			i++;
			continue;
		}
		switch(Entry.m_Kind)
		{
		case KIND_MUTE:
			for(size_t k = 0; k < m_vMuteNames.size(); k++)
			{
				if(str_comp(m_vMuteNames[k].c_str(), Entry.m_aNewName) == 0)
				{
					m_vMuteNames.erase(m_vMuteNames.begin() + k);
					break;
				}
			}
			break;
		case KIND_FOE:
			GameClient()->Foes()->RemoveFriend(Entry.m_aNewName, "");
			break;
		case KIND_WAR:
			GameClient()->m_WarList.RemoveWarEntry(Entry.m_aNewName, "", Entry.m_aType);
			break;
		case KIND_HEART:
			GameClient()->m_HeartList.RemoveFriend(Entry.m_aNewName);
			break;
		}
		m_vTemp.erase(m_vTemp.begin() + i);
	}
}

void CReactBypass::ReactToRename(int ClientId, const char *pOld, const char *pNew)
{
	if(pOld[0] == '\0' || pNew[0] == '\0')
		return;

	// "vibecode" ahhhh
	if(IsMuteName(pOld))
	{
		AddMuteName(pNew);
		AddTemp(pOld, pNew, KIND_MUTE, "", "");
		GameClient()->m_aClients[ClientId].m_ChatIgnore = true;
	}

	if(GameClient()->Foes()->GetFriendState(pOld, "") == IFriends::FRIEND_PLAYER)
	{
		GameClient()->Foes()->AddFriend(pNew, "");
		AddTemp(pOld, pNew, KIND_FOE, "", "");
	}

	for(const CWarEntry &Entry : GameClient()->m_WarList.m_vWarEntries)
	{
		if(Entry.m_aName[0] == '\0' || Entry.m_pWarType == nullptr || str_comp(Entry.m_aName, pOld) != 0)
			continue;
		GameClient()->m_WarList.AddWarEntry(pNew, "", Entry.m_aReason, Entry.m_pWarType->m_aWarName);
		AddTemp(pOld, pNew, KIND_WAR, Entry.m_pWarType->m_aWarName, Entry.m_aReason);
		break;
	}

	for(const CHeartEntry &Entry : GameClient()->m_HeartList.m_vHeartEntries)
	{
		if(Entry.m_aName[0] == '\0' || Entry.m_pHeartType == nullptr || str_comp(Entry.m_aName, pOld) != 0)
			continue;
		GameClient()->m_HeartList.AddFriend(pNew, Entry.m_pHeartType->m_aName);
		AddTemp(pOld, pNew, KIND_HEART, Entry.m_pHeartType->m_aName, "");
		break;
	}
}

void CReactBypass::OnReset()
{
	m_vTemp.clear();
	m_vMuteNames.clear();
	mem_zero(m_aaPrevNames, sizeof(m_aaPrevNames));
	mem_zero(m_aPrevIgnore, sizeof(m_aPrevIgnore));
}

void CReactBypass::OnNewSnapshot()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CGameClient::CClientData &Client = GameClient()->m_aClients[i];
		const bool Active = Client.m_Active && Client.m_aName[0] != '\0';

		if(!Active)
		{
			if(m_aaPrevNames[i][0] != '\0')
			{
				if(g_Config.m_TcReactBypass)
					RemoveTempFor(m_aaPrevNames[i]);
				m_aaPrevNames[i][0] = '\0';
			}
			m_aPrevIgnore[i] = 0;
			continue;
		}

		if(!g_Config.m_TcReactBypass)
		{
			str_copy(m_aaPrevNames[i], Client.m_aName);
			m_aPrevIgnore[i] = Client.m_ChatIgnore ? 1 : 0;
			continue;
		}

		if(str_comp(m_aaPrevNames[i], Client.m_aName) != 0)
		{
			if(m_aaPrevNames[i][0] != '\0')
				ReactToRename(i, m_aaPrevNames[i], Client.m_aName);
			str_copy(m_aaPrevNames[i], Client.m_aName);
		}

		const int Ignore = Client.m_ChatIgnore ? 1 : 0;
		if(Ignore && !m_aPrevIgnore[i])
			AddMuteName(Client.m_aName);
		m_aPrevIgnore[i] = Ignore;

		if(!Ignore && IsMuteName(Client.m_aName))
			GameClient()->m_aClients[i].m_ChatIgnore = true;
	}
}
