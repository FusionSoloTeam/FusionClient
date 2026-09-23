#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_FX_OWN_TEE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_FX_OWN_TEE_H

class CGameClient;

bool BcFxSuppressed(const CGameClient *pGameClient);
bool BcIsOwnTee(const CGameClient *pGameClient, int ClientId);

#endif
