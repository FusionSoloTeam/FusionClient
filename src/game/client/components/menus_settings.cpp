/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/dbg.h>
#include <base/math.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/components/menu_background.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::SetNeedSendInfo()
{
	if(m_Dummy)
		m_NeedSendDummyinfo = true;
	else
		m_NeedSendinfo = true;
}


int CMenus::SettingsGroupOfPage(int Page) const
{
	switch(Page)
	{
	case SETTINGS_LANGUAGE:
	case SETTINGS_GENERAL:
	case SETTINGS_PLAYER:
	case SETTINGS_TEE:
	case SETTINGS_APPEARANCE:
	case SETTINGS_CONTROLS:
	case SETTINGS_GRAPHICS:
	case SETTINGS_SOUND:
	case SETTINGS_DDNET:
	case SETTINGS_CREDITS:
		return SETTINGS_GROUP_DDNET;
	case SETTINGS_TCLIENT:
	case SETTINGS_TCLIENT_BINDWHEEL:
	case SETTINGS_TCLIENT_WARLIST:
	case SETTINGS_TCLIENT_FRIENDLIST:
	case SETTINGS_TCLIENT_CHATBINDS:
	case SETTINGS_TCLIENT_STATUSBAR:
	case SETTINGS_TCLIENT_INFO:
		return SETTINGS_GROUP_TCLIENT;
	case SETTINGS_FUSION:
	case SETTINGS_FUSION_INFO:
		return SETTINGS_GROUP_FUSION;
	default:
		return -1;
	}
}

bool CMenus::SettingsPageIsGroupHeader(int Page) const
{
	return Page == SETTINGS_PROFILES || Page == SETTINGS_CONFIGS || Page == SETTINGS_ASSETS;
}

void CMenus::RenderSettings(CUIRect MainView)
{
	if(m_AssetsEditorState.m_VisualsEditorOpen && m_AssetsEditorState.m_FullscreenOpen)
	{
		RenderAssetsEditorScreen(*Ui()->Screen());
		return;
	}
	// render background
	CUIRect Button, TabBar, RestartBar;
	MainView.VSplitRight(120.0f, &MainView, &TabBar);
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(20.0f, &MainView);

	const bool NeedRestart = m_NeedRestartGraphics || m_NeedRestartSound || m_NeedRestartUpdate;
	if(NeedRestart)
	{
		MainView.HSplitBottom(20.0f, &MainView, &RestartBar);
		MainView.HSplitBottom(10.0f, &MainView, nullptr);
	}

	TabBar.HSplitTop(50.0f, &Button, &TabBar);
	Button.Draw(ms_ColorTabbarActive, IGraphics::CORNER_BR, 10.0f);

	struct SSubTab
	{
		int m_Page;
		const char *m_pLabel;
	};
	const SSubTab aDdnetTabs[] = {
		{SETTINGS_LANGUAGE, Localize("Language")},
		{SETTINGS_GENERAL, Localize("General")},
		{SETTINGS_PLAYER, Localize("Player")},
		{SETTINGS_TEE, Client()->IsSixup() ? "Tee 0.7" : Localize("Tee")},
		{SETTINGS_APPEARANCE, Localize("Appearance")},
		{SETTINGS_CONTROLS, Localize("Controls")},
		{SETTINGS_GRAPHICS, Localize("Graphics")},
		{SETTINGS_SOUND, Localize("Sound")},
		{SETTINGS_DDNET, Localize("DDNet")},
		{SETTINGS_CREDITS, Localize("Credits")},
	};
	const SSubTab aTclientTabs[] = {
		{SETTINGS_TCLIENT, TCLocalize("Settings")},
		{SETTINGS_TCLIENT_BINDWHEEL, TCLocalize("Bind Wheel")},
		{SETTINGS_TCLIENT_WARLIST, TCLocalize("War List")},
		{SETTINGS_TCLIENT_FRIENDLIST, TCLocalize("Friendlist")},
		{SETTINGS_TCLIENT_CHATBINDS, TCLocalize("Chat Binds")},
		{SETTINGS_TCLIENT_STATUSBAR, TCLocalize("Status Bar")},
		{SETTINGS_TCLIENT_INFO, TCLocalize("Info")},
	};
	const SSubTab aFusionTabs[] = {
		{SETTINGS_FUSION, TCLocalize("Settings")},
		{SETTINGS_FUSION_INFO, "Info"},
	};

	const SSubTab *apGroupTabs[SETTINGS_GROUP_LENGTH] = {aDdnetTabs, aTclientTabs, aFusionTabs};
	const int aGroupTabCount[SETTINGS_GROUP_LENGTH] = {
		(int)(sizeof(aDdnetTabs) / sizeof(aDdnetTabs[0])),
		(int)(sizeof(aTclientTabs) / sizeof(aTclientTabs[0])),
		(int)(sizeof(aFusionTabs) / sizeof(aFusionTabs[0])),
	};
	const char *apGroupNames[SETTINGS_GROUP_LENGTH] = {"DDNet", TCLocalize("TClient"), "Fusion"};

	const float Dt = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	const int PageGroup = SettingsGroupOfPage(g_Config.m_UiSettingsPage);
	const int OpenGroup = m_SettingsGroupCollapsedIndex == PageGroup ? -1 : PageGroup;
	for(int Group = 0; Group < SETTINGS_GROUP_LENGTH; Group++)
	{
		const float Target = Group == OpenGroup ? 1.0f : 0.0f;
		const float Step = Dt * 14.0f;
		if(m_aSettingsGroupExpand[Group] < Target)
			m_aSettingsGroupExpand[Group] = std::min(Target, m_aSettingsGroupExpand[Group] + Step);
		else
			m_aSettingsGroupExpand[Group] = std::max(Target, m_aSettingsGroupExpand[Group] - Step);
	}

	static CButtonContainer s_aGroupButtons[SETTINGS_GROUP_LENGTH];
	static CButtonContainer s_aSubTabButtons[SETTINGS_LENGTH];
	static CButtonContainer s_ProfilesButton, s_ConfigsButton, s_AssetsButton;

	constexpr float GroupTabHeight = 26.0f;
	constexpr float SubTabHeight = 19.0f;
	constexpr float SubTabGap = 3.0f;
	constexpr float GroupToSubGap = 6.0f;

	for(int Group = 0; Group < SETTINGS_GROUP_LENGTH; Group++)
	{
		TabBar.HSplitTop(10.0f, nullptr, &TabBar);
		TabBar.HSplitTop(GroupTabHeight, &Button, &TabBar);
		const bool GroupIsOpen = OpenGroup == Group;
		if(DoButton_MenuTab(&s_aGroupButtons[Group], apGroupNames[Group], PageGroup == Group, &Button, IGraphics::CORNER_R, &m_aAnimatorsSettingsTab[Group]))
		{
			if(GroupIsOpen)
				m_SettingsGroupCollapsedIndex = Group;
			else
			{
				m_SettingsGroupCollapsedIndex = -1;
				int Remembered = m_aSettingsGroupLastPage[Group];
				if(SettingsGroupOfPage(Remembered) != Group)
					Remembered = apGroupTabs[Group][0].m_Page;
				g_Config.m_UiSettingsPage = Remembered;
			}
		}

		const float ExpandRaw = m_aSettingsGroupExpand[Group];
		const float Expand = ExpandRaw * ExpandRaw * (3.0f - 2.0f * ExpandRaw);
		if(Expand <= 0.001f)
			continue;

		TabBar.HSplitTop(GroupToSubGap * Expand, nullptr, &TabBar);

		for(int Sub = 0; Sub < aGroupTabCount[Group]; Sub++)
		{
			const SSubTab &SubTab = apGroupTabs[Group][Sub];
			CUIRect SubRect;
			TabBar.HSplitTop(SubTabHeight * Expand, &SubRect, &TabBar);
			TabBar.HSplitTop(SubTabGap * Expand, nullptr, &TabBar);
			if(SubRect.h < 2.0f)
				continue;
			SubRect.VSplitLeft(10.0f, nullptr, &SubRect);
			SubRect.VSplitRight(4.0f, &SubRect, nullptr);
			if(DoButton_MenuTab(&s_aSubTabButtons[SubTab.m_Page], SubTab.m_pLabel, g_Config.m_UiSettingsPage == SubTab.m_Page, &SubRect, IGraphics::CORNER_ALL, nullptr, nullptr, nullptr, nullptr, 6.0f))
			{
				g_Config.m_UiSettingsPage = SubTab.m_Page;
				m_SettingsGroupCollapsedIndex = -1;
			}
		}
	}

	{
		TabBar.HSplitTop(10.0f, nullptr, &TabBar);
		TabBar.HSplitTop(GroupTabHeight, &Button, &TabBar);
		if(DoButton_MenuTab(&s_ProfilesButton, Localize("Profiles"), g_Config.m_UiSettingsPage == SETTINGS_PROFILES, &Button, IGraphics::CORNER_R))
			g_Config.m_UiSettingsPage = SETTINGS_PROFILES;
		TabBar.HSplitTop(10.0f, nullptr, &TabBar);
		TabBar.HSplitTop(GroupTabHeight, &Button, &TabBar);
		if(DoButton_MenuTab(&s_ConfigsButton, Localize("Configs"), g_Config.m_UiSettingsPage == SETTINGS_CONFIGS, &Button, IGraphics::CORNER_R))
			g_Config.m_UiSettingsPage = SETTINGS_CONFIGS;
		TabBar.HSplitTop(10.0f, nullptr, &TabBar);
		TabBar.HSplitTop(GroupTabHeight, &Button, &TabBar);
		if(DoButton_MenuTab(&s_AssetsButton, Localize("Assets"), g_Config.m_UiSettingsPage == SETTINGS_ASSETS, &Button, IGraphics::CORNER_R))
			g_Config.m_UiSettingsPage = SETTINGS_ASSETS;
	}

	const int NewPageGroup = SettingsGroupOfPage(g_Config.m_UiSettingsPage);
	if(NewPageGroup >= 0)
		m_aSettingsGroupLastPage[NewPageGroup] = g_Config.m_UiSettingsPage;

	switch(g_Config.m_UiSettingsPage)
	{
	case SETTINGS_LANGUAGE:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
		RenderLanguageSettings(MainView);
		break;
	case SETTINGS_GENERAL:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_GENERAL);
		RenderSettingsGeneral(MainView);
		break;
	case SETTINGS_PLAYER:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_PLAYER);
		RenderSettingsPlayer(MainView);
		break;
	case SETTINGS_TEE:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_TEE);
		if(Client()->IsSixup())
			RenderSettingsTee7(MainView);
		else
			RenderSettingsTee(MainView);
		break;
	case SETTINGS_APPEARANCE:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_APPEARANCE);
		RenderSettingsAppearance(MainView);
		break;
	case SETTINGS_CONTROLS:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_CONTROLS);
		m_MenusSettingsControls.Render(MainView);
		break;
	case SETTINGS_GRAPHICS:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_GRAPHICS);
		RenderSettingsGraphics(MainView);
		break;
	case SETTINGS_SOUND:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_SOUND);
		RenderSettingsSound(MainView);
		break;
	case SETTINGS_DDNET:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_DDNET);
		RenderSettingsDDNet(MainView);
		break;
	case SETTINGS_ASSETS:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_ASSETS);
		RenderSettingsAssets(MainView);
		break;
	case SETTINGS_TCLIENT:
	case SETTINGS_TCLIENT_BINDWHEEL:
	case SETTINGS_TCLIENT_WARLIST:
	case SETTINGS_TCLIENT_FRIENDLIST:
	case SETTINGS_TCLIENT_CHATBINDS:
	case SETTINGS_TCLIENT_STATUSBAR:
	case SETTINGS_TCLIENT_INFO:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
		RenderSettingsTClient(MainView);
		break;
	case SETTINGS_FUSION:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
		RenderSettingsFusionSettings(MainView);
		break;
	case SETTINGS_FUSION_INFO:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
		RenderSettingsFusionInfo(MainView);
		break;
	case SETTINGS_PROFILES:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_GENERAL);
		RenderSettingsTClientProfiles(MainView);
		break;
	case SETTINGS_CONFIGS:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_PLAYER);
		RenderSettingsTClientConfigs(MainView);
		break;
	case SETTINGS_CREDITS:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_CREDITS);
		RenderSettingsCredits(MainView);
		break;
	default:
		dbg_assert_failed("ui_settings_page invalid");
		break;
	}


	if(NeedRestart)
	{
		CUIRect RestartWarning, RestartButton;
		RestartBar.VSplitRight(125.0f, &RestartWarning, &RestartButton);
		RestartWarning.VSplitRight(10.0f, &RestartWarning, nullptr);
		if(m_NeedRestartUpdate)
		{
			TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
			Ui()->DoLabel(&RestartWarning, Localize("DDNet Client needs to be restarted to complete update!"), 14.0f, TEXTALIGN_ML);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			Ui()->DoLabel(&RestartWarning, Localize("You must restart the game for all settings to take effect."), 14.0f, TEXTALIGN_ML);
		}

		static CButtonContainer s_RestartButton;
		if(DoButton_Menu(&s_RestartButton, Localize("Restart"), 0, &RestartButton))
		{
			if(Client()->State() == IClient::STATE_ONLINE || GameClient()->Editor()->HasUnsavedData())
			{
				m_Popup = POPUP_RESTART;
			}
			else
			{
				Client()->Restart();
			}
		}
	}
}

bool CMenus::RenderHslaScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha, float DarkestLight)
{
	const unsigned PrevPackedColor = *pColor;
	ColorHSLA Color(*pColor, Alpha);
	const ColorHSLA OriginalColor = Color;
	const char *apLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht."), Localize("Alpha")};
	const float SizePerEntry = 20.0f;
	const float MarginPerEntry = 5.0f;
	const float PreviewMargin = 2.5f;
	const float PreviewHeight = 40.0f + 2 * PreviewMargin;
	const float OffY = (SizePerEntry + MarginPerEntry) * (3 + (Alpha ? 1 : 0)) - PreviewHeight;

	CUIRect Preview;
	pRect->VSplitLeft(PreviewHeight, &Preview, pRect);
	Preview.HSplitTop(OffY / 2.0f, nullptr, &Preview);
	Preview.HSplitTop(PreviewHeight, &Preview, nullptr);

	Preview.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 4.0f + PreviewMargin);
	Preview.Margin(PreviewMargin, &Preview);
	Preview.Draw(color_cast<ColorRGBA>(Color.UnclampLighting(DarkestLight)), IGraphics::CORNER_ALL, 4.0f + PreviewMargin);

	auto &&RenderHueRect = [&](CUIRect *pColorRect) {
		float CurXOff = pColorRect->x;
		const float SizeColor = pColorRect->w / 6;

		// red to yellow
		{
			Graphics()->SetColor4(
				ColorRGBA(1, 0, 0, 1),
				ColorRGBA(1, 1, 0, 1),
				ColorRGBA(1, 1, 0, 1),
				ColorRGBA(1, 0, 0, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		// yellow to green
		CurXOff += SizeColor;
		{
			Graphics()->SetColor4(
				ColorRGBA(1, 1, 0, 1),
				ColorRGBA(0, 1, 0, 1),
				ColorRGBA(0, 1, 0, 1),
				ColorRGBA(1, 1, 0, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// green to turquoise
		{
			Graphics()->SetColor4(
				ColorRGBA(0, 1, 0, 1),
				ColorRGBA(0, 1, 1, 1),
				ColorRGBA(0, 1, 1, 1),
				ColorRGBA(0, 1, 0, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// turquoise to blue
		{
			Graphics()->SetColor4(
				ColorRGBA(0, 1, 1, 1),
				ColorRGBA(0, 0, 1, 1),
				ColorRGBA(0, 0, 1, 1),
				ColorRGBA(0, 1, 1, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// blue to purple
		{
			Graphics()->SetColor4(
				ColorRGBA(0, 0, 1, 1),
				ColorRGBA(1, 0, 1, 1),
				ColorRGBA(1, 0, 1, 1),
				ColorRGBA(0, 0, 1, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// purple to red
		{
			Graphics()->SetColor4(
				ColorRGBA(1, 0, 1, 1),
				ColorRGBA(1, 0, 0, 1),
				ColorRGBA(1, 0, 0, 1),
				ColorRGBA(1, 0, 1, 1));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}
	};

	auto &&RenderSaturationRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

		LeftColor.s = 0.0f;
		RightColor.s = 1.0f;

		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	auto &&RenderLightingRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

		LeftColor.l = DarkestLight;
		RightColor.l = 1.0f;

		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	auto &&RenderAlphaRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColorFull) {
		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(color_cast<ColorHSLA>(CurColorFull).WithAlpha(0.0f));
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(color_cast<ColorHSLA>(CurColorFull).WithAlpha(1.0f));

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	for(int i = 0; i < 3 + Alpha; i++)
	{
		CUIRect Button, Label;
		pRect->HSplitTop(SizePerEntry, &Button, pRect);
		pRect->HSplitTop(MarginPerEntry, nullptr, pRect);
		Button.VSplitLeft(140.0f, &Label, &Button);
		Label.VMargin(10.0f, &Label);

		Button.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 1.0f);

		CUIRect Rail;
		Button.Margin(2.0f, &Rail);

		char aBuf[32];

		// Hue
		if(i == 0)
			str_format(aBuf, sizeof(aBuf), "%s: %.1f° (%03d)", apLabels[i], Color[i] * 360.0f, round_to_int(Color[i] * 255.0f));
		// Lht
		else if(i == 2)
		{
			// handle internal light clamping, see `UnclampLighting`
			float Lht = DarkestLight + Color[i] * (1.0f - DarkestLight);
			str_format(aBuf, sizeof(aBuf), "%s: %.1f%% (%03d)", apLabels[i], Lht * 100.0f, round_to_int(Color[i] * 255.0f));
		}
		// Sat and Alpha
		else
			str_format(aBuf, sizeof(aBuf), "%s: %.1f%% (%03d)", apLabels[i], Color[i] * 100.0f, round_to_int(Color[i] * 255.0f));
		Ui()->DoLabel(&Label, aBuf, 12.0f, TEXTALIGN_ML);

		ColorRGBA HandleColor;
		Graphics()->TextureClear();
		Graphics()->TrianglesBegin();
		if(i == 0)
		{
			RenderHueRect(&Rail);
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f));
		}
		else if(i == 1)
		{
			RenderSaturationRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f)));
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f));
		}
		else if(i == 2)
		{
			RenderLightingRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f)));
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, Color.l, 1.0f).UnclampLighting(DarkestLight));
		}
		else if(i == 3)
		{
			RenderAlphaRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, Color.l, 1.0f).UnclampLighting(DarkestLight)));
			HandleColor = color_cast<ColorRGBA>(Color.UnclampLighting(DarkestLight));
		}
		Graphics()->TrianglesEnd();

		Color[i] = Ui()->DoScrollbarH(&((char *)pColor)[i], &Button, Color[i], &HandleColor);
	}

	if(OriginalColor != Color)
	{
		*pColor = Color.Pack(Alpha);
	}
	return PrevPackedColor != *pColor;
}

