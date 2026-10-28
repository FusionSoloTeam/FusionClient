#include "custom_clients.h"

#include <base/net.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/external/json-parser/json.h>
#include <engine/http.h>
#include <engine/storage.h>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <iterator>
#include <memory>
#include <vector>

static const SCustomClientInfo s_aCustomClients[] = {
	{CUSTOM_CLIENT_ID_KAIZO_CLIENT, CCID_COLOR_BODY_KAIZO_CLIENT, CCID_COLOR_FEET_KAIZO_CLIENT, "Kaizo Client", "kaizoicon.png"},
	{CUSTOM_CLIENT_ID_DUCK_N_INFCLASS_CLIENT, CCID_COLOR_BODY_DUCK_N_INFCLASS_CLIENT, CCID_COLOR_FEET_DUCK_N_INFCLASS_CLIENT, "Duck & Infclass Client", "duckicon.png"},
	{CUSTOM_CLIENT_ID_GAMER_07, CCID_COLOR_BODY_NONE, CCID_COLOR_FEET_NONE, "Teeworlds Gamer (0.7)", "gamer.png"},
	{CUSTOM_CLIENT_ID_ZILLYWOODS_07, CCID_COLOR_BODY_NONE, CCID_COLOR_FEET_NONE, "ZillyWoods (0.7)", "zillywoods.png"},
	{CUSTOM_CLIENT_ID_FCLIENT_07, CCID_COLOR_BODY_NONE, CCID_COLOR_FEET_NONE, "F-Client (0.7)", "fclient.png"},
	{CUSTOM_CLIENT_ID_RUSHIECLIENT, CCID_COLOR_BODY_RUSHIECLIENT, CCID_COLOR_FEET_RUSHIECLIENT, "Rushie Client", "rushieicon.png"},
	{CUSTOM_CLIENT_ID_ASCENDCLIENT, CCID_COLOR_BODY_ASCENDCLIENT, CCID_COLOR_FEET_ASCENDCLIENT, "Ascend Client", "ascendclient.png"},
	{CUSTOM_CLIENT_ID_CHILLERBOTUX, CCID_COLOR_BODY_CHILLERBOTUX, CCID_COLOR_FEET_CHILLERBOTUX, "ChillerbotUX", "chillerbotux.png"},
	{CUSTOM_CLIENT_ID_BESTCLIENT, CCID_COLOR_BODY_NONE, CCID_COLOR_FEET_NONE, "BestClient", "bestclient.png"},
	{CUSTOM_CLIENT_ID_FUSION, CCID_COLOR_BODY_FUSION, CCID_COLOR_FEET_FUSION, "Fusion Client", "fusion.png"},
};

static_assert((int)std::size(s_aCustomClients) <= CUSTOM_CLIENT_ICON_MAX, "custom client registry exceeds icon texture capacity");

static const SCustomClientInfo *FindById(int Id)
{
	for(const SCustomClientInfo &Client : s_aCustomClients)
	{
		if(Client.m_Id == Id)
			return &Client;
	}
	return nullptr;
}

const SCustomClientInfo *CustomClientById(int Id)
{
	return FindById(Id);
}

const SCustomClientInfo *CustomClientByIcon(int Icon)
{
	return FindById(Icon);
}

int CustomClientCount()
{
	return (int)std::size(s_aCustomClients);
}

int CustomClientListId(int Index)
{
	if(Index < 0 || Index >= CustomClientCount())
		return CUSTOM_CLIENT_NONE;
	return s_aCustomClients[Index].m_Id;
}

const char *CustomClientName(int Id)
{
	const SCustomClientInfo *pInfo = FindById(Id);
	return pInfo ? pInfo->m_pName : "";
}

const char *CustomClientIconFile(int Id)
{
	const SCustomClientInfo *pInfo = FindById(Id);
	return pInfo ? pInfo->m_pIconFile : "";
}

int CustomClientDetect(int ColorBody, int ColorFeet, const char *pSkinName)
{
	if(pSkinName != nullptr)
	{
		if(str_comp(pSkinName, CCID_07_SKIN_PART_NAME_KAIZO_CLIENT_07_MODE) == 0)
			return CUSTOM_CLIENT_ID_KAIZO_CLIENT;
		if(str_comp(pSkinName, CCID_07_SKIN_PART_NAME_TEEWORLDS_GAMER) == 0)
			return CUSTOM_CLIENT_ID_GAMER_07;
		if(str_comp(pSkinName, CCID_07_SKIN_PART_NAME_ZILLYWOODS) == 0)
			return CUSTOM_CLIENT_ID_ZILLYWOODS_07;
		if(str_comp(pSkinName, CCID_07_SKIN_PART_NAME_FCLIENT) == 0)
			return CUSTOM_CLIENT_ID_FCLIENT_07;
	}

	const int BodyCCID = (ColorBody >> 24) & 0xFF;
	if(BodyCCID == CCID_COLOR_BODY_NONE)
		return CUSTOM_CLIENT_NONE;

	for(const SCustomClientInfo &Client : s_aCustomClients)
	{
		if(Client.m_ColorBody == CCID_COLOR_BODY_NONE)
			continue;
		if(Client.m_ColorBody == BodyCCID)
			return Client.m_Id;
	}

	return CUSTOM_CLIENT_NONE;
}

void CustomClientRenderIcon(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, vec2 Position, float Size, float Alpha)
{
	if(!Texture.IsValid() || Size <= 0.0f)
		return;

	pGraphics->TextureSet(Texture);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	const IGraphics::CQuadItem Quad(Position.x, Position.y, Size, Size);
	pGraphics->QuadsDrawTL(&Quad, 1);
	pGraphics->QuadsEnd();
	pGraphics->TextureClear();
}

float CustomClientIconWidth(float IconSize)
{
	return IconSize + 2.0f;
}

void CCustomClientIcons::Init(IGraphics *pGraphics)
{
	m_pGraphics = pGraphics;
	if(m_Loaded)
		return;

	for(int i = 0; i < CustomClientCount(); i++)
	{
		const SCustomClientInfo *pInfo = &s_aCustomClients[i];
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "fusion/clients/%s", pInfo->m_pIconFile);
		m_aTextures[i] = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
	}
	m_Loaded = true;
}

void CCustomClientIcons::Unload()
{
	if(!m_Loaded)
		return;

	for(int i = 0; i < CustomClientCount(); i++)
	{
		if(m_aTextures[i].IsValid())
			m_pGraphics->UnloadTexture(&m_aTextures[i]);
	}
	m_Loaded = false;
}

void CCustomClientIcons::Reload()
{
	Unload();
	if(m_pGraphics != nullptr)
		Init(m_pGraphics);
}

IGraphics::CTextureHandle CCustomClientIcons::Texture(int Id) const
{
	for(int i = 0; i < CustomClientCount(); i++)
	{
		if(s_aCustomClients[i].m_Id == Id)
			return m_aTextures[i];
	}
	return IGraphics::CTextureHandle();
}

bool CCustomClientIcons::HasIcon(int Id) const
{
	return Texture(Id).IsValid();
}

static const char *const BESTCLIENT_USERS_URL = "https://150.241.70.188:8779/users.json";
static const int64_t BESTCLIENT_FETCH_INTERVAL_SECONDS = 30;

static std::shared_ptr<IHttpRequest> s_pBestClientRequest;
static std::vector<int> s_vBestClientIds;
static int64_t s_NextBestClientFetch = 0;

void CustomClientResetBestClient()
{
	if(s_pBestClientRequest)
	{
		s_pBestClientRequest->Abort();
		s_pBestClientRequest.reset();
	}
	s_vBestClientIds.clear();
	s_NextBestClientFetch = 0;
}

bool CustomClientIsBestClient(int ClientId)
{
	for(const int Id : s_vBestClientIds)
	{
		if(Id == ClientId)
			return true;
	}
	return false;
}

static void CustomClientParseBestClient(const NETADDR *pServerAddress)
{
	json_value *pJson = s_pBestClientRequest->ResultJson();
	if(pJson == nullptr)
		return;

	s_vBestClientIds.clear();

	char aAddress[NETADDR_MAXSTRSIZE];
	net_addr_str(pServerAddress, aAddress, sizeof(aAddress), true);

	if(pJson->type == json_array)
	{
		const json_value *pPlayers = nullptr;
		for(const json_value *pServer : pJson->u.array)
		{
			if(pServer == nullptr || pServer->type != json_object)
				continue;
			const json_value &Address = (*pServer)["server_address"];
			if(Address.type != json_string || str_comp(Address.u.string.ptr, aAddress) != 0)
				continue;
			pPlayers = &(*pServer)["players"];
			break;
		}

		if(pPlayers != nullptr && pPlayers->type == json_array)
		{
			for(const json_value *pPlayer : pPlayers->u.array)
			{
				if(pPlayer == nullptr || pPlayer->type != json_object)
					continue;
				const json_value &ClientIdValue = (*pPlayer)["client_id"];
				if(ClientIdValue.type != json_integer)
					continue;
				const int ClientId = (int)ClientIdValue.u.integer;
				if(ClientId >= 0 && ClientId < MAX_CLIENTS)
					s_vBestClientIds.push_back(ClientId);
			}
		}
	}

	json_value_free(pJson);
}

void CustomClientUpdateBestClient(IClient *pClient, IHttp *pHttp, bool Enabled)
{
	if(pClient == nullptr || pHttp == nullptr)
		return;

	if(!Enabled || pClient->State() != IClient::STATE_ONLINE)
	{
		CustomClientResetBestClient();
		return;
	}

	if(s_pBestClientRequest)
	{
		if(!s_pBestClientRequest->Done())
			return;
		if(s_pBestClientRequest->State() == EHttpState::DONE)
			CustomClientParseBestClient(&pClient->ServerAddress());
		s_pBestClientRequest.reset();
	}

	const int64_t Now = time_get();
	if(Now < s_NextBestClientFetch)
		return;
	s_NextBestClientFetch = Now + BESTCLIENT_FETCH_INTERVAL_SECONDS * time_freq();

	auto pRequest = HttpGet(BESTCLIENT_USERS_URL);
	pRequest->Timeout(CTimeout{10000, 0, 500, 10});
	pRequest->IpResolve(IPRESOLVE::V4);
	pRequest->InsecureSsl(true);
	pRequest->MaxResponseSize(4 * 1024 * 1024);
	pRequest->LogProgress(HTTPLOG::FAILURE);
	s_pBestClientRequest = std::shared_ptr<IHttpRequest>(std::move(pRequest));
	pHttp->Run(s_pBestClientRequest);
}
