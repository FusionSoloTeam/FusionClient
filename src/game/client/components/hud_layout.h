/* Copyright © 2026 BestProject Team */
#ifndef GAME_CLIENT_COMPONENTS_HUD_LAYOUT_H
#define GAME_CLIENT_COMPONENTS_HUD_LAYOUT_H

#include <engine/graphics.h>

class IConsole;
class IConfigManager;

namespace HudLayout
{

	enum EModule
	{
		MODULE_SCORE = 0,
		MODULE_MOVEMENT_INFO,
		MODULE_GAME_TIMER,
		MODULE_HEALTH_ARMOR_AMMO,
		MODULE_PLAYER_STATE,
		MODULE_SPECTATOR_COUNT,
		MODULE_DUMMY_ACTIONS,
		MODULE_LOCAL_TIME,
		MODULE_FPS_PING,
		MODULE_FROZEN_TEXT,
		MODULE_SWAP_TIMER,
		MODULE_FROZEN_HUD,
		MODULE_KEYSTROKES,
		MODULE_FINISH_PREDICTION,
		MODULE_VOTE,
		MODULE_VRAM_INFO,
		MODULE_COUNT,
	};

	struct SModuleLayout
	{
		float m_X;
		float m_Y;
		int m_Scale;
		int m_Mode;
		bool m_Enabled;
		bool m_BackgroundEnabled;
		unsigned m_BackgroundColor;
	};

	struct SModuleRect
	{
		float m_X;
		float m_Y;
		float m_W;
		float m_H;
		float m_Rounding;
	};

	constexpr float CANVAS_WIDTH = 500.0f;
	constexpr float CANVAS_HEIGHT = 300.0f;

	enum EPositionMode
	{
		POSITION_MODE_TOP_LEFT = 0,
		POSITION_MODE_BOTTOM_RIGHT,
	};

	bool IsEditableModule(EModule Module);
	const char *Name(EModule Module);
	SModuleLayout Get(EModule Module, float HudWidth, float HudHeight);
	SModuleLayout GetDefault(EModule Module, float HudWidth, float HudHeight);
	bool HasRuntimeOverride(EModule Module);
	bool HasPositionOverride(EModule Module);
	void SetPosition(EModule Module, float X, float Y);
	void SetPosition(EModule Module, float X, float Y, EPositionMode PositionMode);
	void SetScale(EModule Module, int Scale);
	void SetEnabled(EModule Module, bool Enabled);
	bool IsEnabled(EModule Module);
	void ResetPosition(EModule Module);
	void ResetScale(EModule Module);
	void ResetSettings(EModule Module);
	void ResetEditableModules();
	SModuleRect ClampRectToScreen(const SModuleRect &Rect, float HudWidth, float HudHeight);
	float CanvasXToHud(float CanvasX, float HudWidth);
	int BackgroundCorners(int DefaultCorners, float RectX, float RectY, float RectW, float RectH, float CanvasWidth, float CanvasHeight);
	void OnConsoleInit(IConsole *pConsole, IConfigManager *pConfigManager);

}

#endif
