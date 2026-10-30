#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_CHATBINDS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_CHATBINDS_H
#include "game/client/component.h"

#include <base/math.h>

#include <engine/console.h>
#include <engine/http.h>
#include <engine/shared/protocol.h>

#include <memory>
#include <string>
#include <vector>

class CChatBinds : public CComponent
{
	struct STrackedPlayer
	{
		int m_ClientId;
		std::string m_Nickname;
	};
	std::vector<STrackedPlayer> m_vPlayersInTracker;

	struct SMarker
	{
		vec2 m_Pos;
		char m_aName[MAX_NAME_LENGTH];
		int64_t m_Time;
	};
	std::vector<SMarker> m_vMarkers;

	std::shared_ptr<IHttpRequest> m_pProfileTask;
	char m_aProfileName[MAX_NAME_LENGTH] = "";

	char m_aPlayerSkinBefore[128] = "";
	int m_PlayerUseCustomColorBefore = 0;
	int m_PlayerBodyColorBefore = 0;
	int m_PlayerFeetColorBefore = 0;
	char m_aDummySkinBefore[128] = "";
	int m_DummyUseCustomColorBefore = 0;
	int m_DummyBodyColorBefore = 0;
	int m_DummyFeetColorBefore = 0;

	void ApplySkinToPlayer(const char *pSkin, int CustomColor, int SkinColorBodyint, int SkinColorFeetint);
	void ApplyColorToPlayer(int CustomColor, int SkinColorBodyint, int SkinColorFeetint);
	void TrackerClientIdRemove(int ClientId);
	void AddMarker(vec2 Pos, const char *pName);
	void RenderMarkers();
	void FetchProfile(const char *pName);
	void FinishProfile();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;

	bool OnChatCommand(int Team, const char *pLine);

	static void ConFindPlayer(IConsole::IResult *pResult, void *pUserData);
	static void ConFindSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConCopySkin(IConsole::IResult *pResult, void *pUserData);
	static void ConCopyColor(IConsole::IResult *pResult, void *pUserData);
	static void ConBackupPlayerProfile(IConsole::IResult *pResult, void *pUserData);
	static void ConTrackerAdd(IConsole::IResult *pResult, void *pUserData);
	static void ConTrackerRemove(IConsole::IResult *pResult, void *pUserData);
	static void ConTrackerReset(IConsole::IResult *pResult, void *pUserData);
	static void ConGotoTeleCursor(IConsole::IResult *pResult, void *pUserData);
	static void ConGotoFinishCursor(IConsole::IResult *pResult, void *pUserData);
};

#endif
