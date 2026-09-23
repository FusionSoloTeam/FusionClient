#include "own_tee.h"

#include <game/client/gameclient.h>

bool BcFxSuppressed(const CGameClient *pGameClient)
{
	return pGameClient != nullptr && pGameClient->m_SuppressEvents;
}

bool BcIsOwnTee(const CGameClient *pGameClient, int ClientId)
{
	if(pGameClient == nullptr)
		return false;
	return ClientId == pGameClient->m_aLocalIds[0] || ClientId == pGameClient->m_aLocalIds[1];
}
