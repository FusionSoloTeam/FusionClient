#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_CHATBINDS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_CHATBINDS_H
#include "game/client/component.h"

#include <engine/console.h>

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

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;

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
