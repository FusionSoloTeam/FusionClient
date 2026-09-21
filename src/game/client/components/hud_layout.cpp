/* Copyright © 2026 BestProject Team */
#include "hud_layout.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/shared/config.h>

#include <game/client/components/hud.h>

#include <algorithm>

namespace HudLayout
{

	namespace
	{

		// Default layout per module, indexed by EModule. Positions are on the
		// CANVAS_WIDTH x CANVAS_HEIGHT canvas unless HasDynamicDefault() applies.
		static const SModuleLayout gs_aModuleLayouts[MODULE_COUNT] = {
			{460.0f, 229.0f, 100, 0, true, true, 0x40000000U}, // SCORE
			{470.0f, 199.0f, 100, 0, true, true, 0x66000000U}, // MOVEMENT_INFO
			{233.0f, 2.0f, 100, 0, true, false, 0x66000000U}, // GAME_TIMER
			{5.0f, 5.0f, 100, 0, true, false, 0x40000000U}, // HEALTH_ARMOR_AMMO
			{5.0f, 53.0f, 100, 0, true, false, 0x40000000U}, // PLAYER_STATE
			{483.0f, 181.0f, 100, 0, true, true, 0x66000000U}, // SPECTATOR_COUNT
			{484.0f, 166.0f, 100, 0, true, true, 0x66000000U}, // DUMMY_ACTIONS
			{202.0f, 0.0f, 100, 0, true, true, 0x66000000U}, // LOCAL_TIME
			{473.0f, 5.0f, 100, 0, true, false, 0x40000000U}, // FPS_PING
			{225.0f, 12.0f, 100, 0, true, false, 0x40000000U}, // FROZEN_TEXT
			{400.0f, 50.0f, 100, 0, true, true, 0x66000000U}, // SWAP_TIMER
			{233.0f, 0.0f, 100, 0, true, false, 0x40000000U}, // FROZEN_HUD
			{5.0f, 250.0f, 100, 0, true, true, 0x40000000U}, // KEYSTROKES
			{200.0f, 129.0f, 100, 0, true, true, 0x66000000U}, // FINISH_PREDICTION
			{0.0f, 30.0f, 100, 0, true, true, 0x40000000U}, // VOTE
			{2.1f, 8.3f, 100, 0, true, false, 0x40000000U}, // VRAM_INFO
		};

		static const char *gs_apModuleNames[MODULE_COUNT] = {
			"Score",
			"Movement Info",
			"Game Timer",
			"Health and Ammo",
			"Player State",
			"Spectator Count",
			"Dummy Actions",
			"Local Time",
			"FPS / Ping",
			"Frozen Text",
			"Swap Timer",
			"Frozen Tee HUD",
			"Keystrokes",
			"Finish Prediction",
			"Vote",
			"VRAM / RAM Info",
		};

		static SModuleLayout gs_aRuntimeModuleLayouts[MODULE_COUNT];
		static bool gs_RuntimeLayoutsInitialized = false;
		static bool gs_ConfigCallbackRegistered = false;
		static bool gs_ConsoleCommandRegistered = false;

		void EnsureRuntimeLayouts()
		{
			if(gs_RuntimeLayoutsInitialized)
				return;
			for(int i = 0; i < MODULE_COUNT; ++i)
				gs_aRuntimeModuleLayouts[i] = gs_aModuleLayouts[i];
			gs_RuntimeLayoutsInitialized = true;
		}

		bool HasRuntimeOverrideInternal(EModule Module)
		{
			EnsureRuntimeLayouts();
			const SModuleLayout &Runtime = gs_aRuntimeModuleLayouts[Module];
			const SModuleLayout &Default = gs_aModuleLayouts[Module];
			return Runtime.m_X != Default.m_X ||
			       Runtime.m_Y != Default.m_Y ||
			       Runtime.m_Scale != Default.m_Scale ||
			       Runtime.m_Mode != Default.m_Mode ||
			       Runtime.m_Enabled != Default.m_Enabled ||
			       Runtime.m_BackgroundEnabled != Default.m_BackgroundEnabled ||
			       Runtime.m_BackgroundColor != Default.m_BackgroundColor;
		}

		// Modules whose default position is computed directly in HUD-pixel space (as
		// opposed to a fixed canvas coordinate that scales with aspect ratio via
		// CanvasXToHud()) until the user drags them for the first time.
		bool HasDynamicDefault(EModule Module)
		{
			switch(Module)
			{
			case MODULE_SCORE:
			case MODULE_MOVEMENT_INFO:
			case MODULE_GAME_TIMER:
			case MODULE_HEALTH_ARMOR_AMMO:
			case MODULE_PLAYER_STATE:
			case MODULE_SPECTATOR_COUNT:
			case MODULE_DUMMY_ACTIONS:
			case MODULE_LOCAL_TIME:
			case MODULE_FPS_PING:
			case MODULE_FROZEN_TEXT:
			case MODULE_SWAP_TIMER:
			case MODULE_FROZEN_HUD:
			case MODULE_KEYSTROKES:
			case MODULE_FINISH_PREDICTION:
			case MODULE_VRAM_INFO:
				return true;
			default:
				return false;
			}
		}

		// The default layout for a module at a given HUD size, as if it had never been
		// overridden - used both by ResolveBaseLayout() below and by the public GetDefault()
		// (see hud_layout.h), which the HUD editor uses as the animation target for
		// "reset position"/"reset scale".
		SModuleLayout DynamicDefaultLayout(EModule Module, float HudWidth, float HudHeight)
		{
			SModuleLayout Layout = gs_aModuleLayouts[Module];
			switch(Module)
			{
			case MODULE_SCORE:
				Layout.m_X = (float)round_to_int(HudWidth - 100.0f);
				Layout.m_Y = 229.0f;
				break;
			case MODULE_MOVEMENT_INFO:
				Layout.m_X = (float)round_to_int(HudWidth - 62.0f);
				Layout.m_Y = 199.0f;
				break;
			case MODULE_GAME_TIMER:
				Layout.m_X = (float)round_to_int(HudWidth * 0.5f);
				Layout.m_Y = 2.0f;
				break;
			case MODULE_HEALTH_ARMOR_AMMO:
				Layout.m_X = 5.0f;
				Layout.m_Y = 5.0f;
				break;
			case MODULE_PLAYER_STATE:
				Layout.m_X = 5.0f;
				Layout.m_Y = 53.0f;
				break;
			case MODULE_SPECTATOR_COUNT:
				Layout.m_X = (float)round_to_int(HudWidth - 17.0f);
				Layout.m_Y = 181.0f;
				break;
			case MODULE_DUMMY_ACTIONS:
				Layout.m_X = (float)round_to_int(HudWidth - 16.0f);
				Layout.m_Y = 166.0f;
				break;
			case MODULE_LOCAL_TIME:
				Layout.m_X = (float)round_to_int(HudWidth * 3.0f / 7.0f - 30.0f);
				Layout.m_Y = 0.0f;
				break;
			case MODULE_FPS_PING:
				Layout.m_X = (float)round_to_int(HudWidth - 10.0f);
				Layout.m_Y = 5.0f;
				break;
			case MODULE_FROZEN_TEXT:
				Layout.m_X = (float)round_to_int(HudWidth * 0.5f);
				Layout.m_Y = 12.0f;
				break;
			case MODULE_SWAP_TIMER:
				Layout.m_X = g_Config.m_TcEnableSwapTimerOnLeftSide ? 0.0f : (float)round_to_int(HudWidth - HudWidth / 4.0f);
				Layout.m_Y = (float)round_to_int((HudHeight - 12.0f) * g_Config.m_TcEnableSwapTimerPosY / 100.0f);
				break;
			case MODULE_FROZEN_HUD:
				Layout.m_X = (float)round_to_int(HudWidth / 2.0f + 38.0f * (HudWidth / HudHeight) / 1.78f - g_Config.m_TcFrozenHudTeeSize / 2.0f);
				Layout.m_Y = 0.0f;
				break;
			case MODULE_VRAM_INFO:
				Layout.m_X = 2.1f;
				Layout.m_Y = 8.3f;
				break;
			default:
				break;
			}
			return Layout;
		}

		SModuleLayout ResolveBaseLayout(EModule Module, float HudWidth, float HudHeight)
		{
			SModuleLayout Layout;
			if(HasDynamicDefault(Module) && !HasRuntimeOverrideInternal(Module))
			{
				Layout = DynamicDefaultLayout(Module, HudWidth, HudHeight);
			}
			else
			{
				EnsureRuntimeLayouts();
				Layout = gs_aRuntimeModuleLayouts[Module];
				Layout.m_X = CanvasXToHud(Layout.m_X, HudWidth);
			}
			return Layout;
		}

		void ConHudLayoutSet(IConsole::IResult *pResult, void *pUserData)
		{
			(void)pUserData;

			const int ModuleIndex = pResult->GetInteger(0);
			if(ModuleIndex < 0 || ModuleIndex >= MODULE_COUNT)
				return;

			EnsureRuntimeLayouts();
			const EModule Module = (EModule)ModuleIndex;
			SModuleLayout Layout = gs_aRuntimeModuleLayouts[Module];
			Layout.m_X = pResult->GetFloat(1);
			Layout.m_Y = pResult->GetFloat(2);
			Layout.m_Scale = std::clamp(pResult->GetInteger(3), 25, 300);
			Layout.m_Mode = pResult->GetInteger(4);
			Layout.m_BackgroundEnabled = pResult->GetInteger(5) != 0;
			Layout.m_BackgroundColor = (unsigned)pResult->GetInteger(6);
			if(pResult->NumArguments() > 7)
				Layout.m_Enabled = pResult->GetInteger(7) != 0;

			gs_aRuntimeModuleLayouts[Module] = Layout;
		}

		void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
		{
			(void)pUserData;
			EnsureRuntimeLayouts();

			char aLine[256];
			for(int Module = 0; Module < MODULE_COUNT; ++Module)
			{
				const SModuleLayout &Layout = gs_aRuntimeModuleLayouts[Module];
				str_format(
					aLine,
					sizeof(aLine),
					"hud_layout_set %d %.3f %.3f %d %d %d %u %d",
					Module,
					Layout.m_X,
					Layout.m_Y,
					Layout.m_Scale,
					Layout.m_Mode,
					Layout.m_BackgroundEnabled ? 1 : 0,
					Layout.m_BackgroundColor,
					Layout.m_Enabled ? 1 : 0);
				pConfigManager->WriteLine(aLine, ConfigDomain::HUDLAYOUT);
			}
		}

	} // namespace

	SModuleLayout Get(EModule Module, float HudWidth, float HudHeight)
	{
		return ResolveBaseLayout(Module, HudWidth, HudHeight);
	}

	SModuleLayout GetDefault(EModule Module, float HudWidth, float HudHeight)
	{
		SModuleLayout Layout = DynamicDefaultLayout(Module, HudWidth, HudHeight);
		if(!HasDynamicDefault(Module))
			Layout.m_X = CanvasXToHud(Layout.m_X, HudWidth);
		return Layout;
	}

	bool HasRuntimeOverride(EModule Module)
	{
		return Module >= 0 && Module < MODULE_COUNT && HasRuntimeOverrideInternal(Module);
	}

	bool HasPositionOverride(EModule Module)
	{
		if(Module < 0 || Module >= MODULE_COUNT)
			return false;

		EnsureRuntimeLayouts();
		const SModuleLayout &Layout = gs_aRuntimeModuleLayouts[Module];
		const SModuleLayout &Default = gs_aModuleLayouts[Module];
		return Layout.m_X != Default.m_X ||
		       Layout.m_Y != Default.m_Y ||
		       Layout.m_Mode != Default.m_Mode;
	}

	bool IsEditableModule(EModule Module)
	{
		return Module >= 0 && Module < MODULE_COUNT;
	}

	const char *Name(EModule Module)
	{
		return Module >= 0 && Module < MODULE_COUNT ? gs_apModuleNames[Module] : "HUD Module";
	}

	void SetPosition(EModule Module, float X, float Y)
	{
		EnsureRuntimeLayouts();
		gs_aRuntimeModuleLayouts[Module].m_X = X;
		gs_aRuntimeModuleLayouts[Module].m_Y = Y;
	}

	void SetPosition(EModule Module, float X, float Y, EPositionMode PositionMode)
	{
		EnsureRuntimeLayouts();
		gs_aRuntimeModuleLayouts[Module].m_X = X;
		gs_aRuntimeModuleLayouts[Module].m_Y = Y;
		gs_aRuntimeModuleLayouts[Module].m_Mode = PositionMode;
	}

	void SetScale(EModule Module, int Scale)
	{
		EnsureRuntimeLayouts();
		gs_aRuntimeModuleLayouts[Module].m_Scale = std::clamp(Scale, 25, 300);
	}

	void SetEnabled(EModule Module, bool Enabled)
	{
		EnsureRuntimeLayouts();
		gs_aRuntimeModuleLayouts[Module].m_Enabled = Enabled;
	}

	bool IsEnabled(EModule Module)
	{
		EnsureRuntimeLayouts();
		return gs_aRuntimeModuleLayouts[Module].m_Enabled;
	}

	void ResetPosition(EModule Module)
	{
		EnsureRuntimeLayouts();
		gs_aRuntimeModuleLayouts[Module].m_X = gs_aModuleLayouts[Module].m_X;
		gs_aRuntimeModuleLayouts[Module].m_Y = gs_aModuleLayouts[Module].m_Y;
		gs_aRuntimeModuleLayouts[Module].m_Mode = gs_aModuleLayouts[Module].m_Mode;
	}

	void ResetScale(EModule Module)
	{
		EnsureRuntimeLayouts();
		gs_aRuntimeModuleLayouts[Module].m_Scale = gs_aModuleLayouts[Module].m_Scale;
	}

	void ResetSettings(EModule Module)
	{
		EnsureRuntimeLayouts();
		gs_aRuntimeModuleLayouts[Module] = gs_aModuleLayouts[Module];
	}

	void ResetEditableModules()
	{
		for(int Module = 0; Module < MODULE_COUNT; ++Module)
		{
			if(IsEditableModule((EModule)Module))
				ResetSettings((EModule)Module);
		}
	}

	SModuleRect ClampRectToScreen(const SModuleRect &Rect, float HudWidth, float HudHeight)
	{
		SModuleRect Result = Rect;
		Result.m_X = std::clamp(Result.m_X, 0.0f, maximum(0.0f, HudWidth - Result.m_W));
		Result.m_Y = std::clamp(Result.m_Y, 0.0f, maximum(0.0f, HudHeight - Result.m_H));
		return Result;
	}

	float CanvasXToHud(float CanvasX, float HudWidth)
	{
		return CanvasX * (HudWidth / CANVAS_WIDTH);
	}

	int BackgroundCorners(int DefaultCorners, float RectX, float RectY, float RectW, float RectH, float CanvasWidth, float CanvasHeight)
	{
		int Corners = DefaultCorners;
		const float Eps = 0.01f;
		if(RectW <= 0.0f || RectH <= 0.0f)
			return Corners;

		if(RectX <= Eps)
			Corners &= ~IGraphics::CORNER_L;
		if(RectX + RectW >= CanvasWidth - Eps)
			Corners &= ~IGraphics::CORNER_R;
		if(RectY <= Eps)
			Corners &= ~IGraphics::CORNER_T;
		if(RectY + RectH >= CanvasHeight - Eps)
			Corners &= ~IGraphics::CORNER_B;
		return Corners;
	}

	void OnConsoleInit(IConsole *pConsole, IConfigManager *pConfigManager)
	{
		if(!gs_ConsoleCommandRegistered && pConsole)
		{
			pConsole->Register(
				"hud_layout_set",
				"i[module] f[x] f[y] i[scale] i[mode] i[background_enabled] i[background_color] ?i[enabled]",
				CFGFLAG_CLIENT,
				ConHudLayoutSet,
				nullptr,
				"Set HUD module layout entry");
			gs_ConsoleCommandRegistered = true;
		}

		if(!gs_ConfigCallbackRegistered && pConfigManager)
		{
			pConfigManager->RegisterCallback(ConfigSaveCallback, nullptr, ConfigDomain::HUDLAYOUT);
			gs_ConfigCallbackRegistered = true;
		}
	}

} // namespace HudLayout
