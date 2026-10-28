#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_TCLIENT_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_TCLIENT_H

#include <base/types.h>
#include <engine/client/enums.h>
#include <engine/external/regex.h>
#include <engine/http.h>
#include <engine/shared/console.h>

#include <game/client/component.h>
#include <game/client/ui.h>

#include <deque>
#include <vector>

class CTClient : public CComponent
{
	struct STeleportEntry
	{
		char m_aText[64];
		bool m_Unpaired;
		std::vector<ivec2> m_vPositions;
	};

	std::deque<vec2> m_aAirRescuePositions[NUM_DUMMIES];
	void AirRescue();
	static void ConAirRescue(IConsole::IResult *pResult, void *pUserData);

	static void ConCalc(IConsole::IResult *pResult, void *pUserData);
	static void ConRandomTee(IConsole::IResult *pResult, void *pUserData);
	static void ConchainRandomColor(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAspectRatio(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void RandomBodyColor();
	static void RandomFeetColor();
	static void RandomSkin(void *pUserData);
	static void RandomFlag(void *pUserData);

	static void ConSpecId(IConsole::IResult *pResult, void *pUserData);
	void SpecId(int ClientId);

	int m_EmoteCycle = 0;
	static void ConEmoteCycle(IConsole::IResult *pResult, void *pUserData);

	class IEngineGraphics *m_pGraphics = nullptr;

	char m_PreviousOwnMessage[2048] = {};

	bool SendNonDuplicateMessage(int Team, const char *pLine);

	float m_FinishTextTimeout = 0.0f;
	float m_FinishMessageDelayLeft = -1.0f;
	float m_ScreenshotDelayLeft = -1.0f;
	int m_LastFinishMessageTime = -1;
	int m_LastFinishMessageClientId = -1;
	float m_UnlockTeamTimeout = 0.0f;
	bool m_UnlockTeamSent = false;
	bool m_WasNearFinish = false;
	void UpdateFinishMessage();
	void DoFinishCheck();

	bool ServerCommandExists(const char *pCommand);

	vec2 m_LastLocalPos = vec2(0, 0);
	int m_LastSwitchTile = -1;
	void NotifyOnSwitch();

public:
	void NotifyOnMove();
	void SetDDNetProcessPriority(bool Set);
	void SetDiscordProcessesNormalPriority();

public:
	CTClient();
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnConsoleInit() override;
	void OnRender() override;
	void OnUpdate() override;

	void OnStateChange(int OldState, int NewState) override;
	void OnNewSnapshot() override;
	void SetForcedAspect();

	std::shared_ptr<IHttpRequest> m_pTClientInfoTask = nullptr;
	void FetchTClientInfo();
	void FinishTClientInfo();
	void ResetTClientInfoTask();
	bool NeedUpdate();

	void RequestReloadTextures();
	void ReloadTextures();
	static void ConReloadTextures(IConsole::IResult *pResult, void *pUserData);
	bool m_ReloadTexturesRequested = false;

	void RenderMiniVoteHud();
	void RenderCenterLines();
	void RenderInfoOverlay();
	CUIRect GetInfoOverlayHudEditorRect() const;
	void RenderCtfFlag(vec2 Pos, float Alpha);

	bool ChatDoSpecId(const char *pInput);
	bool AntiUnSpec() const;
	mutable bool m_ConfirmUnSpec = false;
	bool InfoTaskDone() { return m_pTClientInfoTask && m_pTClientInfoTask->State() == EHttpState::DONE; }
	bool m_FetchedTClientInfo = false;
	char m_aVersionStr[10] = "0";

	CUIRect m_InfoOverlayRect{};

	Regex m_RegexChatIgnore;

	static void ConTeleportInfo(IConsole::IResult *pResult, void *pUserData);
	static CUi::EPopupMenuFunctionResult PopupTeleportInfo(void *pContext, CUIRect View, bool Active);
	void CollectTeleportData();
	SPopupMenuId m_TeleportInfoPopupId;
	std::vector<STeleportEntry> m_vTeleportEntries;

public:
	bool TeleportInfoActive() const;
	void RenderTeleportInfo();
};

#endif
