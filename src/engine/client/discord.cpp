#include <base/dbg.h>
#include <base/hash.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/secure.h>
#include <base/str.h>
#include <base/dbg.h>
#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>
#include <base/types.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/discord.h>
#include <engine/shared/config.h>

#if defined(CONF_FAMILY_WINDOWS)

#define DISCORD_IPC
#endif

static constexpr int64_t DISCORD_CLIENT_ID = 1557654399717998622;

#if defined(CONF_DISCORD) && !defined(DISCORD_IPC)
#define IStorage IStorage_WindowsCOM
#include <discord_game_sdk.h>
#undef IStorage

typedef enum EDiscordResult(DISCORD_API *FDiscordCreate)(DiscordVersion, struct DiscordCreateParams *, struct IDiscordCore **);

#if defined(CONF_DISCORD_DYNAMIC)
#include <dlfcn.h>
static FDiscordCreate GetDiscordCreate()
{
	void *pSdk = dlopen("discord_game_sdk.so", RTLD_NOW);
	if(!pSdk)
	{
		return nullptr;
	}
	return (FDiscordCreate)dlsym(pSdk, "DiscordCreate");
}
#else
static FDiscordCreate GetDiscordCreate()
{
	return DiscordCreate;
}
#endif

class CDiscord : public IDiscord
{
	DiscordActivity m_Activity;
	bool m_UpdateActivity = false;
	int64_t m_LastActivityUpdate = 0;

	char m_aClientName[32] = "VClient";
	char m_aClientVersion[32] = "";

	IDiscordCore *m_pCore = nullptr;
	IDiscordActivityEvents m_ActivityEvents;
	IDiscordActivityManager *m_pActivityManager = nullptr;

	FDiscordCreate m_pfnDiscordCreate = nullptr;
	bool m_Enabled = false;

public:
	CDiscord()
	{
		m_pfnDiscordCreate = GetDiscordCreate();
	}

	bool InitDiscord()
	{
		if(m_pCore)
		{
			m_pCore->destroy(m_pCore);
			m_pCore = nullptr;
			m_pActivityManager = nullptr;
		}
		if(!m_Enabled)
			return false;
		if(m_pfnDiscordCreate == nullptr)
		{
			dbg_msg("discord", "discord sdk not available");
			return true;
		}

		mem_zero(&m_ActivityEvents, sizeof(m_ActivityEvents));

		m_ActivityEvents.on_activity_join = &CDiscord::OnActivityJoin;
		m_pActivityManager = nullptr;

		DiscordCreateParams Params;
		DiscordCreateParamsSetDefault(&Params);

		Params.client_id = DISCORD_CLIENT_ID;
		Params.flags = EDiscordCreateFlags::DiscordCreateFlags_NoRequireDiscord;
		Params.event_data = this;
		Params.activity_events = &m_ActivityEvents;

		int Error = m_pfnDiscordCreate(DISCORD_VERSION, &Params, &m_pCore);

		if(Error != DiscordResult_Ok)
		{
			dbg_msg("discord", "error initializing discord instance, error=%d", Error);
			return true;
		}

		m_pActivityManager = m_pCore->get_activity_manager(m_pCore);

		// which application to launch when joining activity
		m_pActivityManager->register_command(m_pActivityManager, CONNECTLINK_DOUBLE_SLASH);
		m_pActivityManager->register_steam(m_pActivityManager, 412220); // steam id

		ClearGameInfo();

		return false;
	}

	void SetClientInfo(const char *pName, const char *pVersion) override
	{
		str_copy(m_aClientName, pName, sizeof(m_aClientName));
		str_copy(m_aClientVersion, pVersion, sizeof(m_aClientVersion));
	}

	void FormatClientVersion(char *pOut, int Size) const
	{
		if(m_aClientVersion[0] != '\0')
			str_format(pOut, Size, "%s %s", m_aClientName, m_aClientVersion);
		else
			str_copy(pOut, m_aClientName, Size);
	}

	void UpdateLargeText()
	{
		char aText[64];
		FormatClientVersion(aText, sizeof(aText));
		str_copy(m_Activity.assets.large_text, aText, sizeof(m_Activity.assets.large_text));
	}

	void UpdateActivityText(const char *pServerName, const char *pMapName)
	{
		char aVersion[64];
		FormatClientVersion(aVersion, sizeof(aVersion));
		if(pMapName != nullptr && pMapName[0] != '\0')
			str_format(m_Activity.state, sizeof(m_Activity.state), "%s | %s", aVersion, pMapName);
		else
			str_copy(m_Activity.state, aVersion, sizeof(m_Activity.state));
		str_copy(m_Activity.details, pServerName, sizeof(m_Activity.details));
	}

	void Update(bool Enabled) override
	{
		bool NeedsUpdate = m_Enabled != Enabled;
		m_Enabled = Enabled;

		if(NeedsUpdate)
			InitDiscord();

		if(m_pCore && m_Enabled)
		{
			// update every 5 seconds, rate limit is 5 updates per 20 seconds
			if(m_UpdateActivity && time_get() > m_LastActivityUpdate + time_freq() * 5)
			{
				m_UpdateActivity = false;
				m_LastActivityUpdate = time_get();

				m_pActivityManager->update_activity(m_pActivityManager, &m_Activity, nullptr, nullptr);
			}

			m_pCore->run_callbacks(m_pCore);
		}
	}

	void ClearGameInfo() override
	{
		mem_zero(&m_Activity, sizeof(DiscordActivity));

		str_copy(m_Activity.assets.large_image, "velocity", sizeof(m_Activity.assets.large_image));
		UpdateLargeText();
		m_Activity.timestamps.start = time_timestamp();
		str_copy(m_Activity.details, "Offline", sizeof(m_Activity.details));
		FormatClientVersion(m_Activity.state, sizeof(m_Activity.state));
		m_Activity.instance = false;

		m_UpdateActivity = true;
	}

	void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) override
	{
		mem_zero(&m_Activity, sizeof(DiscordActivity));

		str_copy(m_Activity.assets.large_image, "velocity", sizeof(m_Activity.assets.large_image));
		UpdateLargeText();
		m_Activity.timestamps.start = time_timestamp();
		str_copy(m_Activity.name, "Online");
		m_Activity.instance = true;

		UpdateActivityText(ServerInfo.m_aName, ServerInfo.m_aMap);
		m_Activity.party.size.current_size = ServerInfo.m_NumClients;
		m_Activity.party.size.max_size = ServerInfo.m_MaxClients;
		// private makes it so the game isn't public to join, but there's 'Ask to Join' button instead
		m_Activity.party.privacy = Registered ? DiscordActivityPartyPrivacy_Public : DiscordActivityPartyPrivacy_Private;

		if(!Registered)
		{
			// private parties have random id to not leak the server ip
			char aPartyId[sizeof(m_Activity.party.id)];
			secure_random_password(aPartyId, sizeof(aPartyId), 64);
			str_copy(m_Activity.party.id, aPartyId);
		}
		UpdateServerIp(ServerInfo);

		m_UpdateActivity = true;
	}

	void UpdateServerInfo(const CServerInfo &ServerInfo) override
	{
		if(!m_Activity.instance)
			return;

		UpdateServerIp(ServerInfo);

		UpdateActivityText(ServerInfo.m_aName, ServerInfo.m_aMap);
		m_Activity.party.size.max_size = ServerInfo.m_MaxClients;
		m_UpdateActivity = true;
	}

	void UpdatePlayerCount(int Count) override
	{
		if(!m_Activity.instance)
			return;

		if(m_Activity.party.size.current_size == Count)
			return;

		m_Activity.party.size.current_size = Count;
		m_UpdateActivity = true;
	}

	void UpdateServerIp(const CServerInfo &ServerInfo)
	{
		if(!m_Activity.instance)
			return;

		// secret is only shared when player is joining the game, or when they are invited for private games
		if(str_length(ServerInfo.m_aAddress) < (int)sizeof(m_Activity.secrets.join))
		{
			str_copy(m_Activity.secrets.join, ServerInfo.m_aAddress);
		}
		else
		{
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&ServerInfo.m_aAddresses[0], aAddr, sizeof(aAddr), true);
			str_copy(m_Activity.secrets.join, aAddr);
		}

		if(m_Activity.party.privacy == DiscordActivityPartyPrivacy_Public)
		{
			// id is sha256, because it didn't work with the ':' character
			char aPartyId[SHA256_MAXSTRSIZE];
			SHA256_DIGEST PartyIdSha256 = sha256(m_Activity.secrets.join, str_length(m_Activity.secrets.join));
			sha256_str(PartyIdSha256, aPartyId, sizeof(aPartyId));
			str_copy(m_Activity.party.id, aPartyId);
		}
	}

	static void DISCORD_CALLBACK OnActivityJoin(void *pEventData, const char *pSecret)
	{
		CDiscord *pSelf = static_cast<CDiscord *>(pEventData);
		IClient *pClient = pSelf->Kernel()->RequestInterface<IClient>();
		pClient->Connect(pSecret);
	}

	~CDiscord() override
	{
		if(m_pCore)
			m_pCore->destroy(m_pCore);
	}
};
#elif defined(DISCORD_IPC)
#define IStorage IStorage_WindowsCOM
#include <windows.h>
#undef IStorage

class CDiscord : public IDiscord
{
	static constexpr const char *LARGE_IMAGE = "velocity";

	static constexpr const char *BUTTON1_LABEL = "Discord";
	static constexpr const char *BUTTON1_URL = "https://discord.gg/4BhPbEuYeS";
	static constexpr const char *BUTTON2_LABEL = "Website";
	static constexpr const char *BUTTON2_URL = "https://velocity-soft.vercel.app/";

	HANDLE m_Pipe = INVALID_HANDLE_VALUE;
	int64_t m_NextConnectAttempt = 0;
	int64_t m_LastActivityUpdate = 0;
	bool m_UpdateActivity = false;
	bool m_Online = false;
	bool m_WasEnabled = false;
	int m_PartyCurrent = 0;
	int m_PartyMax = 0;
	bool m_PartyPublic = false;
	time_t m_StartTimestamp = 0;

	char m_aClientName[32] = "VClient";
	char m_aClientVersion[32] = "";
	char m_aDetails[128] = "Main menu";
	char m_aState[128] = "";
	char m_aServerName[128] = "";
	char m_aMapName[128] = "";
	char m_aJoinSecret[128] = "";
	char m_aPartyId[128] = "";

	char m_aReadBuffer[4096] = "";
	size_t m_ReadBufferSize = 0;

	void Disconnect()
	{
		if(m_Pipe != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_Pipe);
			m_Pipe = INVALID_HANDLE_VALUE;
		}
		m_ReadBufferSize = 0;
	}

	bool WriteAll(const void *pData, size_t Size)
	{
		const char *pCursor = (const char *)pData;
		while(Size > 0)
		{
			DWORD Written = 0;
			if(!WriteFile(m_Pipe, pCursor, (DWORD)Size, &Written, nullptr) || Written == 0)
				return false;
			pCursor += Written;
			Size -= Written;
		}
		return true;
	}

	bool SendFrame(int Opcode, const char *pJson)
	{
		if(m_Pipe == INVALID_HANDLE_VALUE)
			return false;

		uint32_t Header[2];
		Header[0] = (uint32_t)Opcode;
		Header[1] = (uint32_t)str_length(pJson);
		if(!WriteAll(Header, sizeof(Header)))
			return false;
		if(!WriteAll(pJson, Header[1]))
			return false;
		return true;
	}

	void Drain()
	{
		if(m_Pipe == INVALID_HANDLE_VALUE)
			return;

		DWORD Available = 0;
		while(PeekNamedPipe(m_Pipe, nullptr, 0, nullptr, &Available, nullptr) && Available > 0)
		{
			if(m_ReadBufferSize >= sizeof(m_aReadBuffer))
				m_ReadBufferSize = 0;

			DWORD Read = 0;
			if(!ReadFile(m_Pipe, m_aReadBuffer + m_ReadBufferSize, (DWORD)(sizeof(m_aReadBuffer) - m_ReadBufferSize), &Read, nullptr) || Read == 0)
				break;

			m_ReadBufferSize += Read;
			ProcessFrames();
		}
	}

	void ProcessFrames()
	{
		size_t Offset = 0;
		while(m_ReadBufferSize - Offset >= 8)
		{
			uint32_t Opcode = 0;
			uint32_t Length = 0;
			mem_copy(&Opcode, m_aReadBuffer + Offset, sizeof(Opcode));
			mem_copy(&Length, m_aReadBuffer + Offset + 4, sizeof(Length));
			if(Length > sizeof(m_aReadBuffer) - 8)
			{
				Disconnect();
				return;
			}
			if(m_ReadBufferSize - Offset - 8 < Length)
				break;

			char aPayload[1024];
			if(Length < sizeof(aPayload))
			{
				mem_copy(aPayload, m_aReadBuffer + Offset + 8, Length);
				aPayload[Length] = '\0';
				if(Opcode == 2) // close
				{
					Disconnect();
					m_NextConnectAttempt = time_get() + time_freq() * 5;
					return;
				}
				else if(Opcode == 3) // ping
					SendFrame(4, aPayload);
				else if(Opcode == 1) // frame
					HandleEvent(aPayload);
			}

			Offset += 8 + Length;
		}

		if(Offset > 0)
		{
			mem_move(m_aReadBuffer, m_aReadBuffer + Offset, m_ReadBufferSize - Offset);
			m_ReadBufferSize -= Offset;
		}
	}

	static bool ExtractJsonString(const char *pJson, const char *pKey, char *pOut, size_t OutSize)
	{
		char aPattern[64];
		aPattern[0] = '"';
		str_copy(aPattern + 1, pKey, sizeof(aPattern) - 2);
		const int PatternLen = str_length(aPattern);
		aPattern[PatternLen] = '"';
		aPattern[PatternLen + 1] = 0;
	
		const char *pPos = str_find(pJson, aPattern);
		if(pPos == nullptr)
			return false;
		pPos += PatternLen + 1;
	
		while(*pPos != 0 && *pPos != ':')
			pPos++;
		if(*pPos != ':')
			return false;
		pPos++;
		while(*pPos != 0 && *pPos != '"')
			pPos++;
		if(*pPos != '"')
			return false;
		pPos++;
	
		size_t Out = 0;
		while(*pPos != 0 && *pPos != '"' && Out + 1 < OutSize)
		{
			if(*pPos == 92 && pPos[1] != 0)
				pPos++;
			pOut[Out++] = *pPos;
			pPos++;
		}
		pOut[Out] = 0;
		return Out > 0;
	}

	void HandleEvent(const char *pPayload)
	{
		if(str_find_nocase(pPayload, "ACTIVITY_JOIN") == nullptr)
			return;

		char aSecret[128];
		if(!ExtractJsonString(pPayload, "secret", aSecret, sizeof(aSecret)))
			return;

		IClient *pClient = Kernel()->RequestInterface<IClient>();
		if(pClient != nullptr)
			pClient->Connect(aSecret);
	}

	bool Connect()
	{
		for(int i = 0; i < 10; i++)
		{
			char aPath[64];
			str_format(aPath, sizeof(aPath), "\\\\.\\pipe\\discord-ipc-%d", i);
			HANDLE hPipe = CreateFileA(aPath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if(hPipe == INVALID_HANDLE_VALUE)
				continue;

			m_Pipe = hPipe;

			char aHandshake[128];
			str_format(aHandshake, sizeof(aHandshake), "{\"v\":1,\"client_id\":\"%lld\"}", (long long)DISCORD_CLIENT_ID);
			if(!SendFrame(0, aHandshake))
			{
				Disconnect();
				continue;
			}

			m_LastActivityUpdate = 0;
			m_UpdateActivity = true;
			return true;
		}

		return false;
	}

	static void JsonEscape(const char *pText, char *pOut, size_t OutSize)
	{
		size_t Out = 0;
		for(const char *p = pText; p != nullptr && *p != '\0' && Out + 2 < OutSize; p++)
		{
			const unsigned char c = (unsigned char)*p;
			if(c == '"' || c == '\\')
			{
				pOut[Out++] = '\\';
				pOut[Out++] = (char)c;
			}
			else if(c < 0x20)
			{
				if(Out + 7 >= OutSize)
					break;
				str_format(pOut + Out, OutSize - Out, "\\u%04x", c);
				Out += 6;
			}
			else
			{
				pOut[Out++] = (char)c;
			}
		}
		pOut[Out] = '\0';
	}

	static bool BuildButtonJson(const char *pLabel, const char *pUrl, char *pOut, size_t OutSize)
	{
		if(pLabel[0] == 0 || pUrl[0] == 0)
			return false;
		if(!str_startswith_nocase(pUrl, "http://") && !str_startswith_nocase(pUrl, "https://"))
			return false;

		char aLabel[160];
		char aUrl[640];
		JsonEscape(pLabel, aLabel, sizeof(aLabel));
		JsonEscape(pUrl, aUrl, sizeof(aUrl));
		str_format(pOut, OutSize, "{\"label\":\"%s\",\"url\":\"%s\"}", aLabel, aUrl);
		return true;
	}

	void SendActivity()
	{
		char aDetails[400];
		char aState[400];
		char aLargeText[160];
		char aLargeTextRaw[128];
		char aButtonsRaw[1024] = "";
		char aButtonsPart[1100] = "";
		char aPartyPart[768] = "";

		JsonEscape(m_aDetails, aDetails, sizeof(aDetails));
		JsonEscape(m_aState, aState, sizeof(aState));
		str_format(aLargeTextRaw, sizeof(aLargeTextRaw), "%s %s", m_aClientName, m_aClientVersion);
		JsonEscape(aLargeTextRaw, aLargeText, sizeof(aLargeText));

		char aButton[640];
		if(BuildButtonJson(BUTTON1_LABEL, BUTTON1_URL, aButton, sizeof(aButton)))
			str_append(aButtonsRaw, aButton, sizeof(aButtonsRaw));
		if(BuildButtonJson(BUTTON2_LABEL, BUTTON2_URL, aButton, sizeof(aButton)))
		{
			if(aButtonsRaw[0] != 0)
				str_append(aButtonsRaw, ",", sizeof(aButtonsRaw));
			str_append(aButtonsRaw, aButton, sizeof(aButtonsRaw));
		}
		if(aButtonsRaw[0] != 0)
		{
			str_append(aButtonsPart, ",\"buttons\":[", sizeof(aButtonsPart));
			str_append(aButtonsPart, aButtonsRaw, sizeof(aButtonsPart));
			str_append(aButtonsPart, "]", sizeof(aButtonsPart));
		}

		if(m_Online && m_PartyMax > 0 && m_aPartyId[0] != 0)
		{
			char aPartyId[320];
			JsonEscape(m_aPartyId, aPartyId, sizeof(aPartyId));
			str_format(aPartyPart, sizeof(aPartyPart), ",\"party\":{\"id\":\"%s\",\"size\":[%d,%d],\"privacy\":%d}",
				aPartyId, m_PartyCurrent, m_PartyMax, m_PartyPublic ? 0 : 1);
		}
		// низя юзать join secrets и кнопочки вместе *плачет* :((
		if(aButtonsRaw[0] == 0 && m_Online && m_aJoinSecret[0] != 0)
		{
			char aJoinSecret[320];
			JsonEscape(m_aJoinSecret, aJoinSecret, sizeof(aJoinSecret));
			str_append(aPartyPart, ",\"secrets\":{\"join\":\"", sizeof(aPartyPart));
			str_append(aPartyPart, aJoinSecret, sizeof(aPartyPart));
			str_append(aPartyPart, "\"}", sizeof(aPartyPart));
		}

		char aActivity[4096];
		str_format(aActivity, sizeof(aActivity),
			"{\"details\":\"%s\",\"state\":\"%s\",\"timestamps\":{\"start\":%lld},"
			"\"assets\":{\"large_image\":\"%s\",\"large_text\":\"%s\"},\"instance\":%s%s%s}",
			aDetails, aState, (long long)m_StartTimestamp, LARGE_IMAGE, aLargeText,
			m_Online ? "true" : "false", aPartyPart, aButtonsPart);

		char aFrame[6144];
		str_format(aFrame, sizeof(aFrame),
			"{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%d,\"activity\":%s},\"nonce\":\"%lld\"}",
			(int)GetCurrentProcessId(), aActivity, (long long)time_get());

		if(!SendFrame(1, aFrame))
		{
			Disconnect();
			m_NextConnectAttempt = 0;
			return;
		}

		m_LastActivityUpdate = time_get();
		m_UpdateActivity = false;
	}

	void RefreshStrings()
	{
		if(m_Online)
		{
			str_copy(m_aDetails, m_aMapName[0] != '\0' ? m_aMapName : "Unknown map", sizeof(m_aDetails));
			if(m_aServerName[0] != '\0')
				str_format(m_aState, sizeof(m_aState), "%s %s | %s", m_aClientName, m_aClientVersion, m_aServerName);
			else
				str_format(m_aState, sizeof(m_aState), "%s %s", m_aClientName, m_aClientVersion);
		}
		else
		{
			str_copy(m_aDetails, "Main menu", sizeof(m_aDetails));
			str_format(m_aState, sizeof(m_aState), "%s %s", m_aClientName, m_aClientVersion);
		}
		m_UpdateActivity = true;
	}

public:
	CDiscord()
	{
		m_StartTimestamp = time_timestamp();
	}

	void SetClientInfo(const char *pName, const char *pVersion) override
	{
		str_copy(m_aClientName, pName != nullptr && pName[0] != '\0' ? pName : "VClient", sizeof(m_aClientName));
		str_copy(m_aClientVersion, pVersion != nullptr ? pVersion : "", sizeof(m_aClientVersion));
		RefreshStrings();
	}

	void Update(bool Enabled) override
	{
		if(!Enabled)
		{
			if(m_WasEnabled)
			{
				Disconnect();
				m_WasEnabled = false;
			}
			return;
		}

		m_WasEnabled = true;

		if(m_Pipe == INVALID_HANDLE_VALUE)
		{
			if(time_get() < m_NextConnectAttempt)
				return;
			if(!Connect())
			{
				m_NextConnectAttempt = time_get() + time_freq() * 5;
				return;
			}
			RefreshStrings();
		}

		Drain();

		// 5 upd per 20 sec
		if(m_UpdateActivity && time_get() >= m_LastActivityUpdate + time_freq() * 4)
			SendActivity();
	}

	void UpdateJoinInfo(const CServerInfo &ServerInfo, bool Registered)
	{
		m_PartyPublic = Registered;
		m_aJoinSecret[0] = 0;
		m_aPartyId[0] = 0;

		if(str_length(ServerInfo.m_aAddress) < (int)sizeof(m_aJoinSecret))
			str_copy(m_aJoinSecret, ServerInfo.m_aAddress, sizeof(m_aJoinSecret));
		else
		{
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&ServerInfo.m_aAddresses[0], aAddr, sizeof(aAddr), true);
			str_copy(m_aJoinSecret, aAddr, sizeof(m_aJoinSecret));
		}

		if(m_aJoinSecret[0] == 0)
			return;

		if(Registered)
		{
			char aPartyId[SHA256_MAXSTRSIZE];
			SHA256_DIGEST PartyIdSha256 = sha256(m_aJoinSecret, str_length(m_aJoinSecret));
			sha256_str(PartyIdSha256, aPartyId, sizeof(aPartyId));
			str_copy(m_aPartyId, aPartyId, sizeof(m_aPartyId));
		}
		else
		{
			char aPartyId[128];
			secure_random_password(aPartyId, sizeof(aPartyId), 64);
			str_copy(m_aPartyId, aPartyId, sizeof(m_aPartyId));
		}
	}

	void ClearGameInfo() override
	{
		m_Online = false;
		m_aServerName[0] = '\0';
		m_aMapName[0] = '\0';
		m_aJoinSecret[0] = '\0';
		m_aPartyId[0] = '\0';
		m_PartyPublic = false;
		m_PartyCurrent = 0;
		m_PartyMax = 0;
		m_StartTimestamp = time_timestamp();
		RefreshStrings();
	}

	void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) override
	{
		m_Online = true;
		str_copy(m_aServerName, ServerInfo.m_aName, sizeof(m_aServerName));
		str_copy(m_aMapName, ServerInfo.m_aMap, sizeof(m_aMapName));
		m_PartyCurrent = ServerInfo.m_NumClients;
		m_PartyMax = ServerInfo.m_MaxClients;
		m_StartTimestamp = time_timestamp();
		UpdateJoinInfo(ServerInfo, Registered);
		RefreshStrings();
	}

	void UpdateServerInfo(const CServerInfo &ServerInfo) override
	{
		if(!m_Online)
			return;
		str_copy(m_aServerName, ServerInfo.m_aName, sizeof(m_aServerName));
		str_copy(m_aMapName, ServerInfo.m_aMap, sizeof(m_aMapName));
		m_PartyMax = ServerInfo.m_MaxClients;
		UpdateJoinInfo(ServerInfo, m_PartyPublic);
		RefreshStrings();
	}

	void UpdatePlayerCount(int Count) override
	{
		if(!m_Online)
			return;
		if(m_PartyCurrent == Count)
			return;
		m_PartyCurrent = Count;
		m_UpdateActivity = true;
	}

	~CDiscord() override
	{
		Disconnect();
	}
};
#else
class CDiscord : public IDiscord
{
public:
	void SetClientInfo(const char *pName, const char *pVersion) override
	{
		(void)pName;
		(void)pVersion;
	}
	void Update(bool Enabled) override { (void)Enabled; }
	void ClearGameInfo() override {}
	void SetGameInfo(const CServerInfo &ServerInfo, bool Registered) override
	{
		(void)ServerInfo;
		(void)Registered;
	}
	void UpdateServerInfo(const CServerInfo &ServerInfo) override { (void)ServerInfo; }
	void UpdatePlayerCount(int Count) override { (void)Count; }
};
#endif

IDiscord *CreateDiscord()
{
	return new CDiscord();
}
