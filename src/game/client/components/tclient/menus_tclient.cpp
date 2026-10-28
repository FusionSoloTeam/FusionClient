#include <base/log.h>
#include <base/math.h>
#include <base/types.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/client/animstate.h>
#include <game/client/components/binds.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/components/tclient/bindchat.h>
#include <game/client/components/tclient/bindwheel.h>
#include <game/client/components/tclient/trails.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>
#include <game/version.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

enum
{
	TCLIENT_TAB_SETTINGS = 0,
	TCLIENT_TAB_BINDWHEEL,
	TCLIENT_TAB_WARLIST,
	TCLIENT_TAB_FRIENDLIST,
	TCLIENT_TAB_BINDCHAT,
	TCLIENT_TAB_STATUSBAR,
	TCLIENT_TAB_INFO,
	NUMBER_OF_TCLIENT_TABS
};

typedef struct
{
	const char *m_pName;
	const char *m_pCommand;
	int m_KeyId;
	int m_ModifierCombination;
} CKeyInfo;

static float s_Time = 0.0f;
static bool s_StartedTime = false;

const float FontSize = 14.0f;
const float EditBoxFontSize = 12.0f;
const float LineSize = 20.0f;
const float ColorPickerLineSize = 25.0f;
const float HeadlineFontSize = 20.0f;
const float StandardFontSize = 14.0f;

const float HeadlineHeight = HeadlineFontSize + 0.0f;
const float Margin = 10.0f;
const float MarginSmall = 5.0f;
const float MarginExtraSmall = 2.5f;
const float MarginBetweenSections = 30.0f;
const float MarginBetweenViews = 30.0f;

const float ColorPickerLabelSize = 13.0f;
const float ColorPickerLineSpacing = 5.0f;

static void SetFlag(int32_t &Flags, int n, bool Value)
{
	if(Value)
		Flags |= (1 << n);
	else
		Flags &= ~(1 << n);
}

bool CMenus::DoLine_KeyReader(CUIRect &View, CButtonContainer &ReaderButton, CButtonContainer &ClearButton, const char *pName, const char *pCommand)
{
	CBindSlot Bind(0, 0);
	for(int Mod = 0; Mod < KeyModifier::COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = GameClient()->m_Binds.Get(KeyId, Mod);
			if(!pBind[0])
				continue;

			if(str_comp(pBind, pCommand) == 0)
			{
				Bind.m_Key = KeyId;
				Bind.m_ModifierMask = Mod;
				break;
			}
		}
	}

	CUIRect KeyButton, KeyLabel;
	View.HSplitTop(LineSize, &KeyButton, &View);
	KeyButton.VSplitMid(&KeyLabel, &KeyButton);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", pName);
	Ui()->DoLabel(&KeyLabel, aBuf, FontSize, TEXTALIGN_ML);

	View.HSplitTop(MarginExtraSmall, nullptr, &View);

	const auto Result = GameClient()->m_KeyBinder.DoKeyReader(&ReaderButton, &ClearButton, &KeyButton, Bind, false);
	if(Result.m_Bind != Bind)
	{
		if(Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Bind.m_Key, "", false, Bind.m_ModifierMask);
		if(Result.m_Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Result.m_Bind.m_Key, pCommand, false, Result.m_Bind.m_ModifierMask);
		return true;
	}
	return false;
}

bool CMenus::DoSliderWithScaledValue(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int Scale, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;
	const bool StoreRealValue = Flags & SCROLLBAR_OPTION_REAL_VALUE;

	int Value = StoreRealValue ? *pOption / Scale : *pOption;
	Min /= Scale;
	Max /= Scale;
	// Allow adjustment of slider options when ctrl is pressed (to avoid scrolling, or accidentally adjusting the value)
	int Increment = std::max(1, (Max - Min) / 35);
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ui()->MouseInside(pRect))
	{
		Value += Increment;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = std::clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %i%s", pStr, Value * Scale, pSuffix);

	if(NoClampValue)
	{
		// Clamp the value internally for the scrollbar
		Value = std::clamp(Value, Min, Max);
	}

	CUIRect Label, ScrollBar;
	pRect->VSplitMid(&Label, &ScrollBar, std::min(10.0f, pRect->w * 0.05f));

	const float LabelFontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	Ui()->DoLabel(&Label, aBuf, LabelFontSize, TEXTALIGN_ML);

	const int OldStoredValue = StoreRealValue ? *pOption / Scale : *pOption;
	Value = pScale->ToAbsolute(Ui()->DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && OldStoredValue < Min) || (Value == Max && OldStoredValue > Max)))
	{
		Value = OldStoredValue;
	}

	if(Value != OldStoredValue)
	{
		*pOption = StoreRealValue ? Value * Scale : Value;
		return true;
	}
	return false;
}

bool CMenus::DoEditBoxWithLabel(CLineInput *LineInput, const CUIRect *pRect, const char *pLabel, const char *pDefault, char *pBuf, size_t BufSize)
{
	CUIRect Button, Label;
	pRect->VSplitLeft(210.0f, &Label, &Button);
	Ui()->DoLabel(&Label, pLabel, FontSize, TEXTALIGN_ML);
	LineInput->SetBuffer(pBuf, BufSize);
	LineInput->SetEmptyText(pDefault);
	return Ui()->DoEditBox(LineInput, &Button, EditBoxFontSize);
}

int CMenus::DoButtonLineSize_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, float ButtonLineSize, bool Fake, const char *pImageName, int Corners, float Rounding, float FontFactor, ColorRGBA Color)
{
	CUIRect Text = *pRect;

	if(Checked)
		Color = ColorRGBA(0.6f, 0.6f, 0.6f, 0.5f);
	Color.a *= Ui()->ButtonColorMul(pButtonContainer);

	if(Fake)
		Color.a *= 0.5f;

	pRect->Draw(Color, Corners, Rounding);

	Text.HMargin((Text.h - ButtonLineSize) / 2.0f, &Text);
	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);
	Ui()->DoLabel(&Text, pText, Text.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);

	if(Fake)
		return 0;

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_LEFT);
}

void CMenus::RenderDevSkin(vec2 RenderPos, float Size, const char *pSkinName, const char *pBackupSkin, bool CustomColors, int FeetColor, int BodyColor, int Emote, bool Rainbow, bool Cute, ColorRGBA ColorFeet, ColorRGBA ColorBody)
{
	bool WhiteFeetTemp = g_Config.m_TcWhiteFeet;
	g_Config.m_TcWhiteFeet = false;

	float DefTick = std::fmod(s_Time, 1.0f);

	CTeeRenderInfo SkinInfo;
	const CSkin *pSkin = GameClient()->m_Skins.Find(pSkinName);
	if(str_comp(pSkin->GetName(), pSkinName) != 0)
		pSkin = GameClient()->m_Skins.Find(pBackupSkin);

	SkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	SkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	SkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	SkinInfo.m_CustomColoredSkin = CustomColors;
	if(SkinInfo.m_CustomColoredSkin)
	{
		SkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		SkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		if(ColorFeet.a != 0.0f)
		{
			SkinInfo.m_ColorBody = ColorBody;
			SkinInfo.m_ColorFeet = ColorFeet;
		}
	}
	else
	{
		SkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		SkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	if(Rainbow)
	{
		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(DefTick, 1.0f, 0.5f));
		SkinInfo.m_ColorBody = Col;
		SkinInfo.m_ColorFeet = Col;
	}
	SkinInfo.m_Size = Size;
	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &SkinInfo, OffsetToMid);
	vec2 TeeRenderPos(RenderPos.x, RenderPos.y + OffsetToMid.y);
	if(Cute)
		RenderTeeCute(pIdleState, &SkinInfo, Emote, vec2(1.0f, 0.0f), TeeRenderPos, true);
	else
		RenderTools()->RenderTee(pIdleState, &SkinInfo, Emote, vec2(1.0f, 0.0f), TeeRenderPos);
	g_Config.m_TcWhiteFeet = WhiteFeetTemp;
}

void CMenus::RenderTeeCute(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, bool CuteEyes, float Alpha)
{
	Dir = Ui()->MousePos() - Pos;
	if(pInfo->m_Size > 0.0f)
		Dir /= pInfo->m_Size;
	const float Length = length(Dir);
	if(Length > 1.0f)
		Dir /= Length;
	if(CuteEyes && Length < 0.4f)
		Emote = 2;
	RenderTools()->RenderTee(pAnim, pInfo, Emote, Dir, Pos, Alpha);
}

void CMenus::RenderFontIcon(const CUIRect Rect, const char *pText, float Size, int Align)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(&Rect, pText, Size, Align);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

int CMenus::DoButtonNoRect_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextSelectionColor());
	if(Ui()->HotItem() == pButtonContainer)
	{
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	CUIRect Temp;
	pRect->HMargin(0.0f, &Temp);
	Ui()->DoLabel(&Temp, pText, Temp.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_LEFT);
}

void CMenus::PopupConfirmRemoveWarType()
{
	GameClient()->m_WarList.RemoveWarType(m_pRemoveWarType->m_aWarName);
	m_pRemoveWarType = nullptr;
}

void CMenus::RenderSettingsTClient(CUIRect MainView)
{
	s_Time += Client()->RenderFrameTime() * (1.0f / 100.0f);
	if(!s_StartedTime)
	{
		s_StartedTime = true;
		s_Time = (float)rand() / (float)RAND_MAX;
	}

	switch(g_Config.m_UiSettingsPage)
	{
	case SETTINGS_TCLIENT:
		RenderSettingsTClientSettings(MainView);
		break;
	case SETTINGS_TCLIENT_BINDWHEEL:
		RenderSettingsTClientBindWheel(MainView);
		break;
	case SETTINGS_TCLIENT_WARLIST:
		RenderSettingsTClientWarList(MainView);
		break;
	case SETTINGS_TCLIENT_FRIENDLIST:
		RenderSettingsTClientFriendList(MainView);
		break;
	case SETTINGS_TCLIENT_CHATBINDS:
		RenderSettingsTClientChatBinds(MainView);
		break;
	case SETTINGS_TCLIENT_STATUSBAR:
		RenderSettingsTClientStatusBar(MainView);
		break;
	case SETTINGS_TCLIENT_INFO:
		RenderSettingsTClientInfo(MainView);
		break;
	default:
		break;
	}
}

static bool SettingsSectionVisible(const char *pSearch, std::initializer_list<const char *> Keywords)
{
	if(pSearch[0] == '\0')
		return true;
	char aCombined[512] = "";
	for(const char *pKeyword : Keywords)
	{
		if(aCombined[0] != '\0')
			str_append(aCombined, " ", sizeof(aCombined));
		str_append(aCombined, pKeyword, sizeof(aCombined));
	}
	const char *pTok = pSearch;
	while(*pTok)
	{
		while(*pTok == ' ')
			pTok++;
		if(*pTok == '\0')
			break;
		const char *pEnd = pTok;
		while(*pEnd && *pEnd != ' ')
			pEnd++;
		char aWord[64];
		str_copy(aWord, pTok, minimum<int>((int)(pEnd - pTok) + 1, (int)sizeof(aWord)));
		if(!str_find_nocase(aCombined, aWord))
			return false;
		pTok = pEnd;
	}
	return true;
}

struct SSettingsSectionKeywords
{
	const char *const *apKeywords;
	int NumKeywords;
};

static const char *const s_apVisualKeywords[] = {"visual", "font", "custom", "color"};
static const char *const s_apInputKeywords[] = {"input", "mouse", "sens", "fast", "subtick"};
static const char *const s_apAntiLatencyKeywords[] = {"anti", "latency", "prediction", "margin", "freeze", "tp", "tiles", "laser"};
static const char *const s_apAntiPingKeywords[] = {"antiping", "smoothing", "prediction"};
static const char *const s_apAboveBelowKeywords[] = {"above", "below", "player", "position", "notify"};
static const char *const s_apHoverKeywords[] = {"hover", "notification"};
static const char *const s_apIdleAuraKeywords[] = {"aura", "idle"};
static const char *const s_apAutomationKeywords[] = {"automation", "notify", "join", "move", "name", "message"};
static const char *const s_apPerformanceKeywords[] = {"performance", "priority", "process", "discord"};
static const char *const s_apAutoExecuteKeywords[] = {"execute", "connect", "join", "auto"};
static const char *const s_apVotingKeywords[] = {"vote", "voting"};
static const char *const s_apAutoReplyKeywords[] = {"reply", "message"};
static const char *const s_apPlayerIndicatorKeywords[] = {"indicator", "player"};
static const char *const s_apPetKeywords[] = {"pet"};
static const char *const s_apSwapTimerKeywords[] = {"swap", "timer"};
static const char *const s_apNameplateGradientKeywords[] = {"gradient", "nameplate", "clan", "skin", "chat", "message"};
static const char *const s_apHudKeywords[] = {"hud", "checkpoint", "heart", "vote", "percent"};
static const char *const s_apFrozenKeywords[] = {"frozen", "freeze", "tee", "display"};
static const char *const s_apTileOutlinesKeywords[] = {"outline", "tile"};
static const char *const s_apGhostKeywords[] = {"ghost", "prediction", "swap"};
static const char *const s_apRainbowKeywords[] = {"rainbow", "color"};
static const char *const s_apTeeTrailsKeywords[] = {"trail", "tee"};
static const char *const s_apCursorTrailKeywords[] = {"cursor", "trail"};
static const char *const s_apBgDrawKeywords[] = {"background", "draw", "bg"};
static const char *const s_apPracticeTagKeywords[] = {"practice", "tags", "marker"};
static const char *const s_apFinishNameKeywords[] = {"finish", "name"};
static const char *const s_apNotifyOnSwitchKeywords[] = {"notify", "button", "switch", "press"};
static const char *const s_apSoundKeywords[] = {"sound", "3d", "raycast", "echo", "occlusion", "audio"};
static const char *const s_apCameraKeywords[] = {"camera", "respawn", "smooth camera"};
static const char *const s_apLyricsKeywords[] = {"lyrics", "song", "music", "karaoke", "lrc", "3d text", "sing"};
static const char *const s_apVignetteKeywords[] = {"vignette", "blur", "edge", "screen", "color"};
static const char *const s_apDiscordKeywords[] = {"discord", "rpc", "rich presence", "status"};

static const SSettingsSectionKeywords s_AllSettingsSections[] = {
	{s_apVisualKeywords, 4},
	{s_apInputKeywords, 5},
	{s_apAntiLatencyKeywords, 8},
	{s_apAntiPingKeywords, 3},
	{s_apAboveBelowKeywords, 5},
	{s_apHoverKeywords, 2},
	{s_apIdleAuraKeywords, 2},
	{s_apAutomationKeywords, 6},
	{s_apPerformanceKeywords, 4},
	{s_apAutoExecuteKeywords, 4},
	{s_apVotingKeywords, 2},
	{s_apAutoReplyKeywords, 2},
	{s_apPlayerIndicatorKeywords, 2},
	{s_apPetKeywords, 1},
	{s_apSwapTimerKeywords, 2},
	{s_apNameplateGradientKeywords, 6},
	{s_apHudKeywords, 4},
	{s_apFrozenKeywords, 4},
	{s_apTileOutlinesKeywords, 2},
	{s_apGhostKeywords, 3},
	{s_apRainbowKeywords, 2},
	{s_apTeeTrailsKeywords, 2},
	{s_apBgDrawKeywords, 3},
	{s_apPracticeTagKeywords, 3},
	{s_apFinishNameKeywords, 2},
	{s_apNotifyOnSwitchKeywords, 4},
	{s_apCursorTrailKeywords, 2},
	{s_apSoundKeywords, 6},
	{s_apCameraKeywords, 3},
	{s_apLyricsKeywords, 7},
	{s_apVignetteKeywords, 5},
	{s_apDiscordKeywords, 4},
};
static const int s_NumAllSettingsSections = (int)(sizeof(s_AllSettingsSections) / sizeof(s_AllSettingsSections[0]));

static bool SectionKeywordArrayMatches(const char *pSearch, const char *const *apKeywords, int NumKeywords)
{
	if(pSearch[0] == '\0')
		return true;
	char aCombined[512] = "";
	for(int i = 0; i < NumKeywords; i++)
	{
		if(aCombined[0] != '\0')
			str_append(aCombined, " ", sizeof(aCombined));
		str_append(aCombined, apKeywords[i], sizeof(aCombined));
	}
	const char *pTok = pSearch;
	while(*pTok)
	{
		while(*pTok == ' ')
			pTok++;
		if(*pTok == '\0')
			break;
		const char *pEnd = pTok;
		while(*pEnd && *pEnd != ' ')
			pEnd++;
		char aWord[64];
		str_copy(aWord, pTok, minimum<int>((int)(pEnd - pTok) + 1, (int)sizeof(aWord)));
		if(!str_find_nocase(aCombined, aWord))
			return false;
		pTok = pEnd;
	}
	return true;
}

static int CountVisibleSettingsSections(const char *pSearch)
{
	int Count = 0;
	for(int i = 0; i < s_NumAllSettingsSections; i++)
	{
		if(SectionKeywordArrayMatches(pSearch, s_AllSettingsSections[i].apKeywords, s_AllSettingsSections[i].NumKeywords))
			Count++;
	}
	return Count;
}

static int CountSettingsSearchMatches(const char *pSearch)
{
	if(pSearch[0] == '\0')
		return 0;
	int Count = 0;
	for(int i = 0; i < s_NumAllSettingsSections; i++)
	{
		for(int k = 0; k < s_AllSettingsSections[i].NumKeywords; k++)
		{
			if(str_find_nocase(s_AllSettingsSections[i].apKeywords[k], pSearch) != nullptr)
				Count++;
		}
	}
	return Count;
}

void CMenus::RenderSettingsTClientSettings(CUIRect MainView)
{
	RenderTClientSettingsPage(MainView, false);
}

void CMenus::RenderSettingsFusionSettings(CUIRect MainView)
{
	RenderTClientSettingsPage(MainView, true);
}

void CMenus::RenderTClientSettingsPage(CUIRect MainView, bool FusionPage)
{
	const auto SectionEnabled = [FusionPage](bool IsFusionSection, const char *pSearch, std::initializer_list<const char *> Keywords) {
		return FusionPage == IsFusionSection && SettingsSectionVisible(pSearch, Keywords);
	};

	CUIRect Column, LeftView, RightView, Button, Label;

	static CLineInputBuffered<128> s_SettingsSearchInput;
	const char *pSettingsSearch = s_SettingsSearchInput.GetString();

	CUIRect SearchBar, SearchEdit, SearchStatus;
	constexpr float SearchStatusFontSize = 9.0f;
	MainView.HSplitTop(LineSize + Margin * 2.0f + SearchStatusFontSize * 0.5f, &SearchBar, &MainView);
	{
		const float SearchWidth = TextRender()->TextWidth(EditBoxFontSize, FontIcon::MAGNIFYING_GLASS) + 5.0f;
		SearchBar.Margin(Margin, &SearchEdit);
		SearchEdit.HSplitTop(EditBoxFontSize * 1.5f, &SearchEdit, nullptr);

		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		Ui()->DoLabel(&SearchEdit, FontIcon::MAGNIFYING_GLASS, EditBoxFontSize, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		SearchEdit.VSplitLeft(SearchWidth, nullptr, &SearchEdit);

		s_SettingsSearchInput.SetEmptyText(TCLocalize("Search"));
		Ui()->DoClearableEditBox(&s_SettingsSearchInput, &SearchEdit, EditBoxFontSize);

		SearchStatus = SearchEdit;
		SearchStatus.HSplitTop(EditBoxFontSize * 1.5f + SearchStatusFontSize * 1.5f + 1, nullptr, &SearchStatus);

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Modules: %d · search matches: %d", CountVisibleSettingsSections(pSettingsSearch), CountSettingsSearchMatches(pSettingsSearch));
		TextRender()->TextColor(ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f));
		Ui()->DoLabel(&SearchStatus, aBuf, SearchStatusFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_ForceShowScrollbar = true;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	const float ScrollOffset = MainView.y;
	static float s_PrevScrollOffset = 0.0f;

	MainView.VSplitRight(5.0f, &MainView, nullptr); // Padding for scrollbar
	MainView.VSplitLeft(5.0f, nullptr, &MainView); // Padding for scrollbar

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset - ScrollOffset;
		Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	// ***** LeftView ***** //
	Column = LeftView;

	// ***** Visual Miscellaneous ***** //
	if(SectionEnabled(false, pSettingsSearch, {"visual", "font", "custom", "color", "settings", "icon", "menu", "spectate", "prefix"}))
	{
	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Visual"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	static std::vector<const char *> s_FontDropDownNames = {};
	static CUi::SDropDownState s_FontDropDownState;
	static CScrollRegion s_FontDropDownScrollRegion;
	s_FontDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_FontDropDownScrollRegion;
	s_FontDropDownState.m_SelectionPopupContext.m_SpecialFontRenderMode = true;
	int FontSelectedOld = -1;
	for(size_t i = 0; i < TextRender()->GetCustomFaces()->size(); ++i)
	{
		if(s_FontDropDownNames.size() != TextRender()->GetCustomFaces()->size())
			s_FontDropDownNames.push_back(TextRender()->GetCustomFaces()->at(i).c_str());

		if(str_find_nocase(g_Config.m_TcCustomFont, TextRender()->GetCustomFaces()->at(i).c_str()))
			FontSelectedOld = i;
	}
	CUIRect FontDropDownRect, FontDirectory;
	Column.HSplitTop(LineSize, &FontDropDownRect, &Column);
	FontDropDownRect.VSplitLeft(100.0f, &Label, &FontDropDownRect);
	FontDropDownRect.VSplitRight(20.0f, &FontDropDownRect, &FontDirectory);
	FontDropDownRect.VSplitRight(MarginSmall, &FontDropDownRect, nullptr);

	Ui()->DoLabel(&Label, TCLocalize("Custom Font: "), FontSize, TEXTALIGN_ML);
	const int FontSelectedNew = Ui()->DoDropDown(&FontDropDownRect, FontSelectedOld, s_FontDropDownNames.data(), s_FontDropDownNames.size(), s_FontDropDownState);
	if(FontSelectedOld != FontSelectedNew)
	{
		str_copy(g_Config.m_TcCustomFont, s_FontDropDownNames[FontSelectedNew]);
		TextRender()->SetCustomFace(g_Config.m_TcCustomFont);

		// Attempt to reset all the containers
		TextRender()->OnPreWindowResize();
		GameClient()->OnWindowResize();
		GameClient()->Editor()->OnWindowResize();
		TextRender()->OnWindowResize();
		GameClient()->m_MapImages.SetTextureScale(101);
		GameClient()->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);
	}

	static CButtonContainer s_FontDirectoryId;
	if(Ui()->DoButton_FontIcon(&s_FontDirectoryId, FontIcon::FOLDER, 0, &FontDirectory, IGraphics::CORNER_ALL))
	{
		Storage()->CreateFolder("tclient", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("tclient/fonts", IStorage::TYPE_SAVE);
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "tclient/fonts", aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	CUIRect TinyTeeConfig;
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

	{
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		static std::vector<const char *> s_DropDownNames;
		s_DropDownNames = {TCLocalize("Normal", "Hammer Mode"), TCLocalize("Rotate with cursor", "Hammer Mode"), TCLocalize("Rotate with cursor like gun", "Hammer Mode")};
		static CUi::SDropDownState s_DropDownState;
		static CScrollRegion s_DropDownScrollRegion;
		s_DropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DropDownScrollRegion;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Hammer Mode: "), FontSize, TEXTALIGN_ML);
		g_Config.m_TcHammerRotatesWithCursor = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcHammerRotatesWithCursor, s_DropDownNames.data(), s_DropDownNames.size(), s_DropDownState);
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	}

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcCursorScale, &g_Config.m_TcCursorScale, &Button, TCLocalize("Ingame cursor scale"), 0, 500, &CUi::ms_LinearScrollbarScale, 0, "%");

	Column.HSplitTop(LineSize, &Button, &Column);
	if(g_Config.m_TcAnimateWheelTime > 0)
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimateWheelTime, &g_Config.m_TcAnimateWheelTime, &Button, TCLocalize("Wheel animate"), 0, 1000, &CUi::ms_LinearScrollbarScale, 0, "ms");
	else
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimateWheelTime, &g_Config.m_TcAnimateWheelTime, &Button, TCLocalize("Wheel animate"), 0, 1000, &CUi::ms_LinearScrollbarScale, 0, "ms (off)");

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplatePingCircle, TCLocalize("Show ping colored circle in nameplates"), &g_Config.m_TcNameplatePingCircle, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplateCountry, TCLocalize("Show country flags in nameplates"), &g_Config.m_TcNameplateCountry, &Column, LineSize);
	// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderNameplateSpec, TCLocalize("Hide nameplates in spec"), &g_Config.m_TcRenderNameplateSpec, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplateSkins, TCLocalize("Show skin names in nameplate"), &g_Config.m_TcNameplateSkins, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeStars, TCLocalize("Freeze stars"), &g_Config.m_ClFreezeStars, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcColorFreeze, TCLocalize("Colored frozen tee skins"), &g_Config.m_TcColorFreeze, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFrozenKatana, TCLocalize("Show katan on frozen players"), &g_Config.m_TcFrozenKatana, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderWeaponsAsGun, TCLocalize("Render weapons as the gun sprite"), &g_Config.m_TcRenderWeaponsAsGun, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWhiteFeet, TCLocalize("Render all custom colored feet as white feet skin"), &g_Config.m_TcWhiteFeet, &Column, LineSize);
	if(g_Config.m_TcWhiteFeet)
	{
		CUIRect FeetBox;
		Column.HSplitTop(LineSize + MarginExtraSmall, &FeetBox, &Column);
		FeetBox.HSplitTop(MarginExtraSmall, nullptr, &FeetBox);
		FeetBox.VSplitMid(&FeetBox, nullptr);
		static CLineInput s_WhiteFeet(g_Config.m_TcWhiteFeetSkin, sizeof(g_Config.m_TcWhiteFeetSkin));
		s_WhiteFeet.SetEmptyText("x_ninja");
		Ui()->DoEditBox(&s_WhiteFeet, &FeetBox, EditBoxFontSize);
	}

	{
		static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}};
		int Value = g_Config.m_TcTinyTees ? (g_Config.m_TcTinyTeesOthers ? 2 : 1) : 0;
		if(DoLine_RadioMenu(Column, TCLocalize("Tiny Tees"),
			   s_vButtonContainers,
			   {Localize("None"), Localize("Own"), Localize("All")},
			   {0, 1, 2},
			   Value))
		{
			g_Config.m_TcTinyTees = Value > 0 ? 1 : 0;
			g_Config.m_TcTinyTeesOthers = Value > 1 ? 1 : 0;
		}
		if(g_Config.m_TcTinyTees > 0)
		{
			Column.HSplitTop(LineSize, &TinyTeeConfig, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcTinyTeeSize, &g_Config.m_TcTinyTeeSize, &TinyTeeConfig, TCLocalize("Tiny Tee Size"), 85, 115);
		}
	}

	{
		static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}};
		int Value = g_Config.m_TcFakeCtfFlags;
		if(DoLine_RadioMenu(Column, TCLocalize("Fake CTF flags"),
			   s_vButtonContainers,
			   {Localize("None"), Localize("Red"), Localize("Blue")},
			   {0, 1, 2},
			   Value))
		{
			g_Config.m_TcFakeCtfFlags = Value;
		}
	}
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcMovingTilesEntities, TCLocalize("Show moving tiles in entities"), &g_Config.m_TcMovingTilesEntities, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowOthersInMenu, TCLocalize("Show settings icon when a tee is in a menu"), &g_Config.m_TcShowOthersInMenu, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSpecMenuPrefixes, TCLocalize("Player prefixes in the spectate menu"), &g_Config.m_TcSpecMenuPrefixes, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowhudPlayerCheckpoint, TCLocalize("Show checkpoint"), &g_Config.m_TcShowhudPlayerCheckpoint, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowhudSmallerHud, TCLocalize("Smaller hud (angle,checkpoint)"), &g_Config.m_TcShowhudSmallerHud, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	static int s_HeartSizeHudId = 0;
	Ui()->DoScrollbarOption(&s_HeartSizeHudId, &g_Config.m_TcSizeOfHeart, &Button, TCLocalize("Heart size"), 0, 200, &CUi::ms_LinearScrollbarScale, 0);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowhudVotingPercent, TCLocalize("Show percent in vote"), &g_Config.m_TcShowhudVotingPercent, &Column, LineSize);

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"custom clients", "client", "icon", "scoreboard", "nameplate", "identify"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Custom Clients"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcCustomClients, TCLocalize("Identify custom clients"), &g_Config.m_FcCustomClients, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoLabel(&Button, TCLocalize("Show which client other players use"), FontSize, TEXTALIGN_ML);

	if(g_Config.m_FcCustomClients)
	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcCustomClientsShare, TCLocalize("Share my client id (undetectable when off)"), &g_Config.m_FcCustomClientsShare, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcCustomClientsDetect, TCLocalize("Detect other clients"), &g_Config.m_FcCustomClientsDetect, &Column, LineSize);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcCustomClientsScoreboard, TCLocalize("Icons on the scoreboard"), &g_Config.m_FcCustomClientsScoreboard, &Column, LineSize);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcCustomClientsNamePlates, TCLocalize("Icons next to name plates"), &g_Config.m_FcCustomClientsNamePlates, &Column, LineSize);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcCustomClientsOwn, TCLocalize("Show the icon for your own tee"), &g_Config.m_FcCustomClientsOwn, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcCustomClientsBestClient, TCLocalize("Detect BestClient users (external server)"), &g_Config.m_FcCustomClientsBestClient, &Column, LineSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	if(SectionEnabled(true, pSettingsSearch, {"skin", "gradient", "glow", "enchant", "glint", "tee"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Skin Gradient"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcSkinGradient, TCLocalize("Animated glowing gradient on skins"), &g_Config.m_FcSkinGradient, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoLabel(&Button, TCLocalize("Colour sweep with a moving glint, like enchantments"), FontSize, TEXTALIGN_ML);

	if(g_Config.m_FcSkinGradient)
	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		static CButtonContainer s_SkinGradientColor1Button;
		static CButtonContainer s_SkinGradientColor2Button;
		DoLine_ColorPicker(&s_SkinGradientColor1Button, LineSize, FontSize, 5.0f, &Column, TCLocalize("Gradient colour 1"), &g_Config.m_FcSkinGradientColor1, ColorRGBA(0.54f, 0.17f, 0.89f), false);
		DoLine_ColorPicker(&s_SkinGradientColor2Button, LineSize, FontSize, 5.0f, &Column, TCLocalize("Gradient colour 2"), &g_Config.m_FcSkinGradientColor2, ColorRGBA(1.0f, 0.31f, 0.85f), false);

		{
			static std::vector<const char *> s_SkinGradientModeNames;
			s_SkinGradientModeNames = {Localize("Diagonal"), Localize("Vertical"), Localize("Horizontal"), Localize("Radial")};
			static CUi::SDropDownState s_SkinGradientModeState;
			static CScrollRegion s_SkinGradientModeScroll;
			s_SkinGradientModeState.m_SelectionPopupContext.m_pScrollRegion = &s_SkinGradientModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(100.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Direction"), FontSize, TEXTALIGN_ML);
			g_Config.m_FcSkinGradientMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_FcSkinGradientMode, s_SkinGradientModeNames.data(), s_SkinGradientModeNames.size(), s_SkinGradientModeState);
		}

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcSkinGradientSpeed, &g_Config.m_FcSkinGradientSpeed, &Button, TCLocalize("Animation speed"), 10, 400, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcSkinGradientBands, &g_Config.m_FcSkinGradientBands, &Button, TCLocalize("Gradient bands"), 1, 6, &CUi::ms_LinearScrollbarScale, 0, "");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcSkinGradientIntensity, &g_Config.m_FcSkinGradientIntensity, &Button, TCLocalize("Gradient strength"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcSkinGradientGlow, &g_Config.m_FcSkinGradientGlow, &Button, TCLocalize("Glow"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcSkinGradientOthers, TCLocalize("Apply to other players"), &g_Config.m_FcSkinGradientOthers, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcSkinGradientFeet, TCLocalize("Apply to feet"), &g_Config.m_FcSkinGradientFeet, &Column, LineSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	if(SectionEnabled(true, pSettingsSearch, {"animated", "assets", "gif", "entities", "animation", "frames"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Animated Assets"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcAnimatedAssets, TCLocalize("Animate .gif assets"), &g_Config.m_FcAnimatedAssets, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoLabel(&Button, TCLocalize("Drop a .gif, .mp4, .webm, .mkv, .mov or .avi into assets/skins, then pick it in Settings -> Assets"), FontSize, TEXTALIGN_ML);

	if(g_Config.m_FcAnimatedAssets)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoLabel(&Button, TCLocalize("game / hud / particles / emoticons / extras / entities"), FontSize, TEXTALIGN_ML);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoLabel(&Button, TCLocalize("Animated entries are marked [ANIM] in the Assets list"), FontSize, TEXTALIGN_ML);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcAnimatedAssetsFps, &g_Config.m_FcAnimatedAssetsFps, &Button, TCLocalize("Max upload rate"), 1, 60, &CUi::ms_LinearScrollbarScale, 0, " fps");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcAnimatedAssetsSpeed, &g_Config.m_FcAnimatedAssetsSpeed, &Button, TCLocalize("Playback speed"), 10, 400, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		{
			char aLabel[192];
			static const char *s_apVideoAlphaModes[] = {"opaque", "black", "white"};
			const int AlphaMode = g_Config.m_FcAnimatedAssetsVideoAlpha;
			str_format(aLabel, sizeof(aLabel), "%s: %s", TCLocalize("Video alpha key"), s_apVideoAlphaModes[AlphaMode >= 0 && AlphaMode < 3 ? AlphaMode : 1]);
			static CButtonContainer s_VideoAlphaButton;
			if(DoButton_Menu(&s_VideoAlphaButton, aLabel, 0, &Button))
			{
				g_Config.m_FcAnimatedAssetsVideoAlpha = (AlphaMode + 1) % 3;
				GameClient()->m_AnimatedAssets.Refresh();
			}
		}
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcAnimatedAssetsVideoFrames, &g_Config.m_FcAnimatedAssetsVideoFrames, &Button, TCLocalize("Max video frames"), 1, 240, &CUi::ms_LinearScrollbarScale, 0, "");
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcAnimatedAssetsGame, TCLocalize("Animate game assets"), &g_Config.m_FcAnimatedAssetsGame, &Column, LineSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	if(SectionEnabled(true, pSettingsSearch, {"aspect", "ratio", "stretch", "ultrawide", "custom"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Aspect Ratio"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		const bool CameraAllowed = GameClient()->IsCameraAllowedServer();
		if(!CameraAllowed)
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoLabel(&Button, TCLocalize("Only allowed on listed server types"), FontSize, TEXTALIGN_ML);
		}

		const auto ApplyAspectRatioSetting = [this](const char *pName, int Value) {
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s %d", pName, Value);
			Console()->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED);
		};
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	{
		static std::vector<const char *> s_AspectRatioModeNames;
		s_AspectRatioModeNames = {"Legacy Auto", "Off", "Preset", "Custom"};
		static CUi::SDropDownState s_AspectRatioModeState;
		static CScrollRegion s_AspectRatioModeScroll;
		s_AspectRatioModeState.m_SelectionPopupContext.m_pScrollRegion = &s_AspectRatioModeScroll;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(100.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, "Mode", FontSize, TEXTALIGN_ML);
		const int NewMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcCustomAspectRatioMode + 1, s_AspectRatioModeNames.data(), s_AspectRatioModeNames.size(), s_AspectRatioModeState) - 1;
			if(NewMode != g_Config.m_TcCustomAspectRatioMode)
				ApplyAspectRatioSetting("tc_custom_aspect_ratio_mode", NewMode);
	}

	if(g_Config.m_TcCustomAspectRatioMode == 1)
	{
		{
			Column.HSplitTop(MarginSmall, nullptr, &Column);
			static std::vector<const char *> s_AspectRatioPresetNames;
			s_AspectRatioPresetNames = {"5:4", "4:3", "3:2", "16:9", "16:10", "21:9"};
			static CUi::SDropDownState s_AspectRatioPresetState;
			static CScrollRegion s_AspectRatioPresetScroll;
			s_AspectRatioPresetState.m_SelectionPopupContext.m_pScrollRegion = &s_AspectRatioPresetScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(100.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, "Preset", FontSize, TEXTALIGN_ML);
			int PresetIndex = -1;
			if(g_Config.m_TcCustomAspectRatio == 125) PresetIndex = 0;
			else if(g_Config.m_TcCustomAspectRatio == 133) PresetIndex = 1;
			else if(g_Config.m_TcCustomAspectRatio == 150) PresetIndex = 2;
			else if(g_Config.m_TcCustomAspectRatio == 178) PresetIndex = 3;
			else if(g_Config.m_TcCustomAspectRatio == 160) PresetIndex = 4;
			else if(g_Config.m_TcCustomAspectRatio == 235) PresetIndex = 5;
			const int NewPresetIndex = Ui()->DoDropDown(&DropDownRect, PresetIndex, s_AspectRatioPresetNames.data(), s_AspectRatioPresetNames.size(), s_AspectRatioPresetState);
			if(NewPresetIndex >= 0 && NewPresetIndex != PresetIndex)
			{
				switch(NewPresetIndex)
				{
				case 0: ApplyAspectRatioSetting("tc_custom_aspect_ratio", 125); break;
				case 1: ApplyAspectRatioSetting("tc_custom_aspect_ratio", 133); break;
				case 2: ApplyAspectRatioSetting("tc_custom_aspect_ratio", 150); break;
				case 3: ApplyAspectRatioSetting("tc_custom_aspect_ratio", 178); break;
				case 4: ApplyAspectRatioSetting("tc_custom_aspect_ratio", 160); break;
				case 5: ApplyAspectRatioSetting("tc_custom_aspect_ratio", 235); break;
				}
			}
		}
	}
	else if(g_Config.m_TcCustomAspectRatioMode == 2)
	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		CUIRect NumRect, DenRect;
		Column.HSplitTop(LineSize, &NumRect, &Column);
		NumRect.VSplitMid(&NumRect, &DenRect, MarginSmall);
		NumRect.VSplitLeft(100.0f, &Label, &NumRect);
		Ui()->DoLabel(&Label, "Width (num)", FontSize, TEXTALIGN_ML);
		static CLineInput s_AspectRatioNumInput;
		s_AspectRatioNumInput.SetBuffer(g_Config.m_TcCustomAspectRatioNumStr, sizeof(g_Config.m_TcCustomAspectRatioNumStr));
		s_AspectRatioNumInput.SetEmptyText("16");
		if(Ui()->DoEditBox(&s_AspectRatioNumInput, &NumRect, EditBoxFontSize))
		{
			const int Num = str_toint(g_Config.m_TcCustomAspectRatioNumStr);
			if(Num > 0)
				ApplyAspectRatioSetting("tc_custom_aspect_ratio_num", Num);
		}

		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &DenRect, &Column);
		DenRect.VSplitLeft(100.0f, &Label, &DenRect);
		Ui()->DoLabel(&Label, "Height (den)", FontSize, TEXTALIGN_ML);
		static CLineInput s_AspectRatioDenInput;
		s_AspectRatioDenInput.SetBuffer(g_Config.m_TcCustomAspectRatioDenStr, sizeof(g_Config.m_TcCustomAspectRatioDenStr));
		s_AspectRatioDenInput.SetEmptyText("9");
		if(Ui()->DoEditBox(&s_AspectRatioDenInput, &DenRect, EditBoxFontSize))
		{
			const int Den = str_toint(g_Config.m_TcCustomAspectRatioDenStr);
			if(Den > 0)
				ApplyAspectRatioSetting("tc_custom_aspect_ratio_den", Den);
		}
	}

	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		static std::vector<const char *> s_AspectRatioApplyModeNames;
		s_AspectRatioApplyModeNames = {"Game only", "Full", "Game no HUD", "Stretch"};
		static CUi::SDropDownState s_AspectRatioApplyModeState;
		static CScrollRegion s_AspectRatioApplyModeScroll;
		s_AspectRatioApplyModeState.m_SelectionPopupContext.m_pScrollRegion = &s_AspectRatioApplyModeScroll;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(100.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, "Apply to", FontSize, TEXTALIGN_ML);
		const int NewApplyMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcCustomAspectRatioApplyMode, s_AspectRatioApplyModeNames.data(), s_AspectRatioApplyModeNames.size(), s_AspectRatioApplyModeState);
		if(NewApplyMode != g_Config.m_TcCustomAspectRatioApplyMode)
			ApplyAspectRatioSetting("tc_custom_aspect_ratio_apply_mode", NewApplyMode);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"flying", "nameplate", "kite"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Flying Name Plates"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFlyingNamePlates, TCLocalize("Flying name plates (kites)"), &g_Config.m_TcFlyingNamePlates, &Column, LineSize);
	if(g_Config.m_TcFlyingNamePlates)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFlyingNamePlatesHideLine, TCLocalize("Hide the kite line"), &g_Config.m_TcFlyingNamePlatesHideLine, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcFlyingNamePlatesLift, &g_Config.m_TcFlyingNamePlatesLift, &Button, TCLocalize("Lift above player"), 0, 120);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcFlyingNamePlatesDrag, &g_Config.m_TcFlyingNamePlatesDrag, &Button, TCLocalize("Movement drag"), 0, 200);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcFlyingNamePlatesFollow, &g_Config.m_TcFlyingNamePlatesFollow, &Button, TCLocalize("Follow speed"), 1, 100);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Input ***** //
	if(SectionEnabled(false, pSettingsSearch, {"input", "mouse", "sens", "fast", "subtick"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Input"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	{
		static std::vector<CButtonContainer> s_vInputModeButtons = {{}, {}, {}};
		int InputModeValue = g_Config.m_TcInputs == 2 ? 2 : (g_Config.m_TcFastInput || g_Config.m_TcInputs == 1 ? 1 : 0);
		if(DoLine_RadioMenu(Column, TCLocalize("Mode"), s_vInputModeButtons, {Localize("None"), Localize("Tater"), Localize("Best")}, {0, 1, 2}, InputModeValue))
		{
			g_Config.m_TcInputs = InputModeValue;
			g_Config.m_TcFastInput = InputModeValue == 1 ? 1 : 0;
		}
	}
	if(g_Config.m_TcInputs == 1 || g_Config.m_TcFastInput)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		DoSliderWithScaledValue(&g_Config.m_TcFastInputAmount, &g_Config.m_TcFastInputAmount, &Button, TCLocalize("Amount"), 1, 40, 1, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInputOthers, TCLocalize("Fast Input others"), &g_Config.m_TcFastInputOthers, &Column, LineSize);
	}
	if(g_Config.m_TcInputs == 2)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcBestInputAmount, &g_Config.m_TcBestInputAmount, &Button, TCLocalize("Amount"), 0, 500, &CUi::ms_LinearScrollbarScale, 0, "0.01t");
		{
			static std::vector<CButtonContainer> s_vBestInterpButtons = {{}, {}, {}};
			int BestInterp = std::clamp(g_Config.m_TcBestInputInterpolation, 1, 3);
			if(DoLine_RadioMenu(Column, TCLocalize("Interpolation"), s_vBestInterpButtons, {Localize("Linear"), Localize("Cubic"), Localize("Smooth")}, {1, 2, 3}, BestInterp))
				g_Config.m_TcBestInputInterpolation = BestInterp;
		}
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcBestInputSmoothing, &g_Config.m_TcBestInputSmoothing, &Button, TCLocalize("Smoothing"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcBestInputLatencyComp, &g_Config.m_TcBestInputLatencyComp, &Button, TCLocalize("Latency compensation"), 0, 50, &CUi::ms_LinearScrollbarScale, 0, "%");
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcBestInputOthers, TCLocalize("Best Input others"), &g_Config.m_TcBestInputOthers, &Column, LineSize);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSubTickAiming, TCLocalize("Sub-Tick aiming"), &g_Config.m_ClSubTickAiming, &Column, LineSize);
	if(!g_Config.m_ClSubTickAiming)
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInputRepredictHook, TCLocalize("Repredict on mid-tick hook direction change (may lag)"), &g_Config.m_TcFastInputRepredictHook, &Column, LineSize);

	// A little extra spacing because these are multi line
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Anti Latency Tools ***** //
	if(SectionEnabled(false, pSettingsSearch, {"anti", "latency", "prediction", "margin", "freeze", "tp", "tiles", "laser"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Anti Latency Tools"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, TCLocalize("Prediction Margin"), 10, 75, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPredictTpTiles, TCLocalize("Predict teleport tiles"), &g_Config.m_TcPredictTpTiles, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPredictSoloTiles, TCLocalize("Predict solo tiles"), &g_Config.m_TcPredictSoloTiles, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPredictLasers, TCLocalize("Predict map lasers"), &g_Config.m_TcPredictLasers, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPredictGuns, TCLocalize("Predict own weapon shots"), &g_Config.m_TcPredictGuns, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcPredictionMarginAdjust, &g_Config.m_TcPredictionMarginAdjust, &Button, TCLocalize("Prediction Margin Adjust"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "ms");
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowpredColor, TCLocalize("Color prediction time"), &g_Config.m_TcShowpredColor, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowpredShowPeak, TCLocalize("Show peak prediction time"), &g_Config.m_TcShowpredShowPeak, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRemoveAnti, TCLocalize("Remove prediction & antiping in freeze"), &g_Config.m_TcRemoveAnti, &Column, LineSize);
	if(g_Config.m_TcRemoveAnti)
	{
		if(g_Config.m_TcUnfreezeLagDelayTicks < g_Config.m_TcUnfreezeLagTicks)
			g_Config.m_TcUnfreezeLagDelayTicks = g_Config.m_TcUnfreezeLagTicks;
		Column.HSplitTop(LineSize, &Button, &Column);
		DoSliderWithScaledValue(&g_Config.m_TcUnfreezeLagTicks, &g_Config.m_TcUnfreezeLagTicks, &Button, TCLocalize("Amount"), 100, 300, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
		Column.HSplitTop(LineSize, &Button, &Column);
		DoSliderWithScaledValue(&g_Config.m_TcUnfreezeLagDelayTicks, &g_Config.m_TcUnfreezeLagDelayTicks, &Button, TCLocalize("Delay"), 100, 3000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
	}
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcUnpredOthersInFreeze, TCLocalize("Dont predict other players if you are frozen"), &g_Config.m_TcUnpredOthersInFreeze, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPredMarginInFreeze, TCLocalize("Adjust your prediction margin while frozen"), &g_Config.m_TcPredMarginInFreeze, &Column, LineSize);
	if(g_Config.m_TcPredMarginInFreeze)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcPredMarginInFreezeAmount, &g_Config.m_TcPredMarginInFreezeAmount, &Button, TCLocalize("Frozen Margin"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "ms");
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Improved Anti Ping ***** //
	if(SectionEnabled(false, pSettingsSearch, {"antiping", "smoothing", "prediction"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Anti Ping Smoothing"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingImproved, TCLocalize("Use new smoothing algorithm"), &g_Config.m_TcAntiPingImproved, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingStableDirection, TCLocalize("Optimistic prediction in stable direction"), &g_Config.m_TcAntiPingStableDirection, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingNegativeBuffer, TCLocalize("Remember instability for longer"), &g_Config.m_TcAntiPingNegativeBuffer, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcAntiPingUncertaintyScale, &g_Config.m_TcAntiPingUncertaintyScale, &Button, TCLocalize("Uncertainty duration"), 50, 400, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "%");
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingInteractionOnly, TCLocalize("Only smooth players near your view"), &g_Config.m_TcAntiPingInteractionOnly, &Column, LineSize);
	if(g_Config.m_TcAntiPingInteractionOnly)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		DoSliderWithScaledValue(&g_Config.m_TcAntiPingInteractionRadius, &g_Config.m_TcAntiPingInteractionRadius, &Button, TCLocalize("Interaction radius"), 100, 4000, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | SCROLLBAR_OPTION_REAL_VALUE, "u");
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"performance", "priority", "process", "discord"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Performance"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHighProcessPriority, TCLocalize("High DDNet Process Priority"), &g_Config.m_TcHighProcessPriority, &Column, LineSize);
	GameClient()->m_TClient.SetDDNetProcessPriority(g_Config.m_TcHighProcessPriority);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDiscordNormalProcessPriority, TCLocalize("Lower Discords Process Priority"), &g_Config.m_TcDiscordNormalProcessPriority, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcVramFallback, TCLocalize("Load textures into system RAM when VRAM is full"), &g_Config.m_TcVramFallback, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	DoSliderWithScaledValue(&g_Config.m_TcTextureMaxSize, &g_Config.m_TcTextureMaxSize, &Button, TCLocalize("Max texture size (0 = off)"), 0, 8192, 256, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | SCROLLBAR_OPTION_REAL_VALUE, "px");
	Column.HSplitTop(LineSize, &Button, &Column);
	DoSliderWithScaledValue(&g_Config.m_TcVramLimit, &g_Config.m_TcVramLimit, &Button, TCLocalize("VRAM limit (0 = off)"), 0, 32768, 512, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | SCROLLBAR_OPTION_REAL_VALUE, "MiB");
	Column.HSplitTop(LineSize, &Button, &Column);
	DoSliderWithScaledValue(&g_Config.m_TcRamLimit, &g_Config.m_TcRamLimit, &Button, TCLocalize("RAM limit (0 = off)"), 0, 65536, 512, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | SCROLLBAR_OPTION_REAL_VALUE, "MiB");

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"discord", "rpc", "rich presence", "status"}))
	{
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, TCLocalize("Discord Rich Presence"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDiscordRPC, TCLocalize("Show Fusion Client in your Discord profile"), &g_Config.m_TcDiscordRPC, &Column, LineSize);

		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	if(SectionEnabled(true, pSettingsSearch, {"info", "vram", "ram", "memory", "overlay"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Info"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcInfoVram, TCLocalize("Show VRAM and RAM usage"), &g_Config.m_TcInfoVram, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcInfoTextures, TCLocalize("Show loaded map textures"), &g_Config.m_TcInfoTextures, &Column, LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	// ***** Execute on join ***** //
	if(SectionEnabled(false, pSettingsSearch, {"execute", "connect", "join", "auto"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);

	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Auto execute"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	{
		CUIRect Box;
		Column.HSplitTop(LineSize + MarginExtraSmall, &Box, &Column);
		Box.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, Localize("Execute before connect"), FontSize, TEXTALIGN_ML);
		static CLineInput s_LineInput(g_Config.m_TcExecuteOnConnect, sizeof(g_Config.m_TcExecuteOnConnect));
		s_LineInput.SetEmptyText(TCLocalize("Run a console command before connect"));

		Ui()->DoEditBox(&s_LineInput, &Button, EditBoxFontSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	{
		CUIRect Box;
		Column.HSplitTop(LineSize + MarginExtraSmall, &Box, &Column);
		Box.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, Localize("Execute on join"), FontSize, TEXTALIGN_ML);
		static CLineInput s_LineInput(g_Config.m_TcExecuteOnJoin, sizeof(g_Config.m_TcExecuteOnJoin));
		s_LineInput.SetEmptyText(TCLocalize("Run a console command on join"));

		Ui()->DoEditBox(&s_LineInput, &Button, EditBoxFontSize);
	}

	Column.HSplitTop(LineSize, &Button, &Column);
	DoSliderWithScaledValue(&g_Config.m_TcExecuteOnJoinDelay, &g_Config.m_TcExecuteOnJoinDelay, &Button, TCLocalize("Delay"), 140, 2000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Voting ***** //
	if(SectionEnabled(false, pSettingsSearch, {"vote", "voting"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Voting"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoVoteWhenFar, TCLocalize("Auto vote no to map changes when far"), &g_Config.m_TcAutoVoteWhenFar, &Column, LineSize);
	if(g_Config.m_TcAutoVoteWhenFar)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAutoVoteWhenFarTime, &g_Config.m_TcAutoVoteWhenFarTime, &Button, TCLocalize("Minimum Time"), 1, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, " minutes");

		CUIRect VoteMessage;
		Column.HSplitTop(LineSize + MarginExtraSmall, &VoteMessage, &Column);
		VoteMessage.HSplitTop(MarginExtraSmall, nullptr, &VoteMessage);
		VoteMessage.VSplitMid(&Label, &VoteMessage);
		Ui()->DoLabel(&Label, TCLocalize("Message to send in chat:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_VoteMessage(g_Config.m_TcAutoVoteWhenFarMessage, sizeof(g_Config.m_TcAutoVoteWhenFarMessage));
		s_VoteMessage.SetEmptyText(TCLocalize("Leave empty to disable"));
		Ui()->DoEditBox(&s_VoteMessage, &VoteMessage, EditBoxFontSize);

		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Auto Reply ***** //
	if(SectionEnabled(false, pSettingsSearch, {"reply", "message"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Auto Reply"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReplyMuted, TCLocalize("Auto reply to muted players"), &g_Config.m_TcAutoReplyMuted, &Column, LineSize);
	if(g_Config.m_TcAutoReplyMuted)
	{
		CUIRect MutedReply;
		Column.HSplitTop(LineSize + MarginExtraSmall, &MutedReply, &Column);
		MutedReply.HSplitTop(MarginExtraSmall, nullptr, &MutedReply);
		static CLineInput s_MutedReply(g_Config.m_TcAutoReplyMutedMessage, sizeof(g_Config.m_TcAutoReplyMutedMessage));
		s_MutedReply.SetEmptyText("I have muted you");
		Ui()->DoEditBox(&s_MutedReply, &MutedReply, EditBoxFontSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReplyMinimized, TCLocalize("Auto reply when tabbed out"), &g_Config.m_TcAutoReplyMinimized, &Column, LineSize);
	if(g_Config.m_TcAutoReplyMinimized)
	{
		CUIRect MinimizedReply;
		Column.HSplitTop(LineSize + MarginExtraSmall, &MinimizedReply, &Column);
		MinimizedReply.HSplitTop(MarginExtraSmall, nullptr, &MinimizedReply);
		static CLineInput s_MinimizedReply(g_Config.m_TcAutoReplyMinimizedMessage, sizeof(g_Config.m_TcAutoReplyMinimizedMessage));
		s_MinimizedReply.SetEmptyText("I am not tabbed in");
		Ui()->DoEditBox(&s_MinimizedReply, &MinimizedReply, EditBoxFontSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"camera", "respawn", "smooth camera"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Camera"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);
	const bool CameraAllowed = GameClient()->IsCameraAllowedServer();
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSmoothRespawnCamera, TCLocalize("Smooth camera flight after respawn"), &g_Config.m_TcSmoothRespawnCamera, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSmoothCameraTeleport, TCLocalize("Smooth camera flight after teleport"), &g_Config.m_TcSmoothCameraTeleport, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSmoothCameraSpec, TCLocalize("Smooth camera flight on /spec and /pause"), &g_Config.m_TcSmoothCameraSpec, &Column, LineSize);
	if(!CameraAllowed)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoLabel(&Button, TCLocalize("Only allowed on listed server types"), FontSize, TEXTALIGN_ML);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	if(SectionEnabled(true, pSettingsSearch, {"above", "below", "player", "position", "notify"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Above/Below Player"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNotifyWhenAbovePosPlayer, TCLocalize("Notify when above player"), &g_Config.m_TcNotifyWhenAbovePosPlayer, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNotifyWhenSamePosPlayer, TCLocalize("Notify when same pos as player"), &g_Config.m_TcNotifyWhenSamePosPlayer, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNotifyWhenBelowPosPlayer, TCLocalize("Notify when below player"), &g_Config.m_TcNotifyWhenBelowPosPlayer, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	{
		CUIRect EditLabel, EditBox;
		Button.VSplitLeft(Button.w * 0.60f, &EditLabel, &EditBox);
		Ui()->DoLabel(&EditLabel, TCLocalize("Horizontal offset (1/100 tile):"), FontSize, TEXTALIGN_ML);
		static CLineInputNumber s_AboveBelowSkew;
		if(!s_AboveBelowSkew.IsActive())
			s_AboveBelowSkew.SetInteger(g_Config.m_TcAboveBelowSkew);
		if(Ui()->DoEditBox(&s_AboveBelowSkew, &EditBox, FontSize))
		{
			const int Value = std::clamp(s_AboveBelowSkew.GetInteger(), 0, 128);
			g_Config.m_TcAboveBelowSkew = Value;
		}
	}
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenPosPlayerPosX, &g_Config.m_TcNotifyWhenPosPlayerPosX, &Button, TCLocalize("Text pos x"), 0, 100, &CUi::ms_LinearScrollbarScale, 0);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenPosPlayerPosY, &g_Config.m_TcNotifyWhenPosPlayerPosY, &Button, TCLocalize("Text pos y"), 0, 100, &CUi::ms_LinearScrollbarScale, 0);

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Player Indicator ***** //
	if(SectionEnabled(false, pSettingsSearch, {"indicator", "player"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Player Indicator"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPlayerIndicator, TCLocalize("Show any enabled Indicators"), &g_Config.m_TcPlayerIndicator, &Column, LineSize);
	if(g_Config.m_TcPlayerIndicator)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorHideVisible, TCLocalize("Hide indicator for tees on your screen"), &g_Config.m_TcIndicatorHideVisible, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPlayerIndicatorFreeze, TCLocalize("Show only freeze Players"), &g_Config.m_TcPlayerIndicatorFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorTeamOnly, TCLocalize("Only show after joining a team"), &g_Config.m_TcIndicatorTeamOnly, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorTees, TCLocalize("Render tiny tees instead of circles"), &g_Config.m_TcIndicatorTees, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicator, TCLocalize("Use warlist groups for indicator"), &g_Config.m_TcWarListIndicator, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorRadius, &g_Config.m_TcIndicatorRadius, &Button, TCLocalize("Indicator size"), 1, 16);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOpacity, &g_Config.m_TcIndicatorOpacity, &Button, TCLocalize("Indicator opacity"), 0, 100);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorVariableDistance, TCLocalize("Change indicator offset based on distance to other tees"), &g_Config.m_TcIndicatorVariableDistance, &Column, LineSize);
		if(g_Config.m_TcIndicatorVariableDistance)
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOffset, &g_Config.m_TcIndicatorOffset, &Button, TCLocalize("Indicator min offset"), 16, 200);
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOffsetMax, &g_Config.m_TcIndicatorOffsetMax, &Button, TCLocalize("Indicator max offset"), 16, 200);
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorMaxDistance, &g_Config.m_TcIndicatorMaxDistance, &Button, TCLocalize("Indicator max distance"), 500, 7000);
		}
		else
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOffset, &g_Config.m_TcIndicatorOffset, &Button, TCLocalize("Indicator offset"), 16, 200);
		}
		if(g_Config.m_TcWarListIndicator)
		{
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorColors, TCLocalize("Use warlist colors instead of regular colors"), &g_Config.m_TcWarListIndicatorColors, &Column, LineSize);
			char aBuf[128];
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorAll, TCLocalize("Show all warlist groups"), &g_Config.m_TcWarListIndicatorAll, &Column, LineSize);
			str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(1)->m_aWarName);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorEnemy, aBuf, &g_Config.m_TcWarListIndicatorEnemy, &Column, LineSize);
			str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(2)->m_aWarName);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorTeam, aBuf, &g_Config.m_TcWarListIndicatorTeam, &Column, LineSize);
		}
		if(!g_Config.m_TcWarListIndicatorColors || !g_Config.m_TcWarListIndicator)
		{
			static CButtonContainer s_IndicatorAliveColorId, s_IndicatorDeadColorId, s_IndicatorSavedColorId;
			DoLine_ColorPicker(&s_IndicatorAliveColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Indicator alive color"), &g_Config.m_TcIndicatorAlive, ColorRGBA(0.0f, 0.0f, 0.0f), false);
			DoLine_ColorPicker(&s_IndicatorDeadColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Indicator in freeze color"), &g_Config.m_TcIndicatorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
			DoLine_ColorPicker(&s_IndicatorSavedColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Indicator safe color"), &g_Config.m_TcIndicatorSaved, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		}
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Pet ***** //
	if(SectionEnabled(false, pSettingsSearch, {"pet"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Pet"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPetShow, TCLocalize("Show the pet"), &g_Config.m_TcPetShow, &Column, LineSize);
	if(g_Config.m_TcPetShow)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcPetSize, &g_Config.m_TcPetSize, &Button, TCLocalize("Pet size"), 10, 500, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcPetAlpha, &g_Config.m_TcPetAlpha, &Button, TCLocalize("Pet alpha"), 10, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize + MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, TCLocalize("Pet Skin:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_PetSkin(g_Config.m_TcPetSkin, sizeof(g_Config.m_TcPetSkin));
		Ui()->DoEditBox(&s_PetSkin, &Button, EditBoxFontSize);

		// Pet Preview
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		CUIRect Preview;
		Column.HSplitTop(64.0f, &Preview, &Column);

		CTeeRenderInfo TeeInfo;
		const CSkin *pSkin = GameClient()->m_Skins.Find(g_Config.m_TcPetSkin);
		if(!pSkin || str_comp(pSkin->GetName(), g_Config.m_TcPetSkin) != 0)
			pSkin = GameClient()->m_Skins.Find("default");

		TeeInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
		TeeInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
		TeeInfo.m_SkinMetrics = pSkin->m_Metrics;
		TeeInfo.m_CustomColoredSkin = false;
		TeeInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		TeeInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
		TeeInfo.m_Size = 64.0f;

		const CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
		vec2 TeeRenderPos = Preview.Center();
		TeeRenderPos.y += OffsetToMid.y;

		vec2 Dir = Ui()->MousePos() - TeeRenderPos;
		const float Length = length(Dir);
		if(Length > 0.0f)
			Dir /= Length;
		if(Length < 0.4f * 64.0f)
		{
			Dir = vec2(1.0f, 0.0f);
		}

		int PetEmote = g_Config.m_ClPlayerDefaultEyes;
		RenderTools()->RenderTee(pIdleState, &TeeInfo, PetEmote, Dir, TeeRenderPos);

	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"cursor", "trail"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Cursor Trail"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcCursorTrail, TCLocalize("Enable cursor trail"), &g_Config.m_TcCursorTrail, &Column, LineSize);

	if(g_Config.m_TcCursorTrail)
	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		{
			static std::vector<const char *> s_TrailModeNames;
			s_TrailModeNames = {TCLocalize("Overlay"), TCLocalize("Background")};
			static CUi::SDropDownState s_TrailModeState;
			static CScrollRegion s_TrailModeScroll;
			s_TrailModeState.m_SelectionPopupContext.m_pScrollRegion = &s_TrailModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(150.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Trail mode"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcCursorTrailMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcCursorTrailMode, s_TrailModeNames.data(), s_TrailModeNames.size(), s_TrailModeState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		{
			static std::vector<const char *> s_ColorModeNames;
			s_ColorModeNames = {TCLocalize("Static"), TCLocalize("Rainbow"), TCLocalize("Gradient")};
			static CUi::SDropDownState s_ColorModeState;
			static CScrollRegion s_ColorModeScroll;
			s_ColorModeState.m_SelectionPopupContext.m_pScrollRegion = &s_ColorModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(150.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Trail color"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcCursorTrailColorMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcCursorTrailColorMode, s_ColorModeNames.data(), s_ColorModeNames.size(), s_ColorModeState);
		}
		if(g_Config.m_TcCursorTrailColorMode == 0)
		{
			Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
			static CButtonContainer s_TrailColorId;
			ColorRGBA TrailColorDefault(1.0f, 1.0f, 1.0f, 1.0f);
			DoLine_ColorPicker(&s_TrailColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Static color"), &g_Config.m_TcCursorTrailColor, TrailColorDefault, false, nullptr, true);
		}
		if(g_Config.m_TcCursorTrailColorMode == 2)
		{
			Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
			static CButtonContainer s_TrailColor1Id, s_TrailColor2Id;
			ColorRGBA TrailColorDefault(1.0f, 1.0f, 1.0f, 1.0f);
			DoLine_ColorPicker(&s_TrailColor1Id, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Gradient color 1"), &g_Config.m_TcCursorTrailColor, TrailColorDefault, false, nullptr, true);
			DoLine_ColorPicker(&s_TrailColor2Id, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Gradient color 2"), &g_Config.m_TcCursorTrailColor2, TrailColorDefault, false, nullptr, true);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcCursorTrailFade, TCLocalize("Fade trail alpha"), &g_Config.m_TcCursorTrailFade, &Column, LineSize);
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcCursorTrailLength, &g_Config.m_TcCursorTrailLength, &Button, TCLocalize("Trail length"), 50, 5000, &CUi::ms_LinearScrollbarScale, 0, "ms");
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"tile", "animation", "glitch", "glitched", "fade", "placing", "camera"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Tile Animations"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcTileAnimations, TCLocalize("Animate tiles entering the camera"), &g_Config.m_FcTileAnimations, &Column, LineSize);

	if(g_Config.m_FcTileAnimations)
	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		{
			static std::vector<const char *> s_AnimationModeNames;
			s_AnimationModeNames = {TCLocalize("Glitched (entities only)"), TCLocalize("Fade"), TCLocalize("Placing")};
			static CUi::SDropDownState s_AnimationModeState;
			static CScrollRegion s_AnimationModeScroll;
			s_AnimationModeState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimationModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(150.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Animation mode"), FontSize, TEXTALIGN_ML);
			g_Config.m_FcTileAnimationMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_FcTileAnimationMode, s_AnimationModeNames.data(), s_AnimationModeNames.size(), s_AnimationModeState);
		}

		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcTileAnimationSpeed, &g_Config.m_FcTileAnimationSpeed, &Button, TCLocalize("Animation speed"), 10, 400, &CUi::ms_LinearScrollbarScale, 0, "%");

		if(g_Config.m_FcTileAnimationMode == 0)
		{
			Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_FcTileAnimationGlitchInterval, &g_Config.m_FcTileAnimationGlitchInterval, &Button, TCLocalize("Glitch change interval"), 10, 250, &CUi::ms_LinearScrollbarScale, 0, "ms");
		}
		else if(g_Config.m_FcTileAnimationMode == 2)
		{
			Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_FcTileAnimationPlacingDistance, &g_Config.m_FcTileAnimationPlacingDistance, &Button, TCLocalize("Placing fall distance"), 1, 16, &CUi::ms_LinearScrollbarScale, 0, " tiles");
		}
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}
	if(SectionEnabled(true, pSettingsSearch, {"outlined", "outline", "tile", "block", "cursor", "crosshair"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Outlined Tile Under Cursor"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_FcOutlineUnderCursor, TCLocalize("Outline the tile the cursor is aiming at"), &g_Config.m_FcOutlineUnderCursor, &Column, LineSize);

	if(g_Config.m_FcOutlineUnderCursor)
	{
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		static CButtonContainer s_OutlineColorId;
		ColorRGBA OutlineColorDefault(1.0f, 1.0f, 1.0f, 1.0f);
		DoLine_ColorPicker(&s_OutlineColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Outline color"), &g_Config.m_FcOutlineUnderCursorColor, OutlineColorDefault, false, nullptr, true);
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_FcOutlineUnderCursorWidth, &g_Config.m_FcOutlineUnderCursorWidth, &Button, TCLocalize("Outline thickness"), 1, 8, &CUi::ms_LinearScrollbarScale, 0, "px");
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"keystrokes", "keys", "keyboard", "wasd"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Keystrokes"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcKeystrokes, TCLocalize("Show keystrokes"), &g_Config.m_TcKeystrokes, &Column, LineSize);
	if(g_Config.m_TcKeystrokes)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcKeystrokesOpacity, &g_Config.m_TcKeystrokesOpacity, &Button, TCLocalize("Pressed key opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcKeystrokesCps, TCLocalize("Show CPS (LMB / RMB)"), &g_Config.m_TcKeystrokesCps, &Column, LineSize);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"finish", "prediction", "time"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Finish Prediction"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFinishPrediction, TCLocalize("Show finish prediction"), &g_Config.m_TcFinishPrediction, &Column, LineSize);
	if(g_Config.m_TcFinishPrediction)
	{
		{
			static std::vector<const char *> s_FinishPredTimeModeNames;
			s_FinishPredTimeModeNames = {TCLocalize("Time remaining"), TCLocalize("Predicted total time")};
			static CUi::SDropDownState s_FinishPredTimeModeState;
			static CScrollRegion s_FinishPredTimeModeScroll;
			s_FinishPredTimeModeState.m_SelectionPopupContext.m_pScrollRegion = &s_FinishPredTimeModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Time mode"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcFinishPredictionTimeMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcFinishPredictionTimeMode, s_FinishPredTimeModeNames.data(), s_FinishPredTimeModeNames.size(), s_FinishPredTimeModeState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		{
			static std::vector<const char *> s_FinishPredSpeedNames;
			s_FinishPredSpeedNames = {TCLocalize("Current speed"), TCLocalize("Measured average")};
			static CUi::SDropDownState s_FinishPredSpeedState;
			static CScrollRegion s_FinishPredSpeedScroll;
			s_FinishPredSpeedState.m_SelectionPopupContext.m_pScrollRegion = &s_FinishPredSpeedScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Speed source"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcFinishPredictionSpeedMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcFinishPredictionSpeedMode, s_FinishPredSpeedNames.data(), s_FinishPredSpeedNames.size(), s_FinishPredSpeedState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcFinishPredictionSmooth, &g_Config.m_TcFinishPredictionSmooth, &Button, TCLocalize("Smoothing"), 0, 95, &CUi::ms_LinearScrollbarScale, 0, "%");
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFinishPredictionShowTime, TCLocalize("Show the time"), &g_Config.m_TcFinishPredictionShowTime, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFinishPredictionShowMillis, TCLocalize("Show milliseconds"), &g_Config.m_TcFinishPredictionShowMillis, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFinishPredictionShowPercent, TCLocalize("Show progress percentage"), &g_Config.m_TcFinishPredictionShowPercent, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFinishPredictionShowAlways, TCLocalize("Also before the race starts"), &g_Config.m_TcFinishPredictionShowAlways, &Column, LineSize);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"jelly", "tee", "deform"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Jelly Tee"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcJellyTee, TCLocalize("Enable jelly tee deformation"), &g_Config.m_TcJellyTee, &Column, LineSize);
	if(g_Config.m_TcJellyTee)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcJellyTeeStrength, &g_Config.m_TcJellyTeeStrength, &Button, TCLocalize("Strength"), 0, 1000, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcJellyTeeDuration, &g_Config.m_TcJellyTeeDuration, &Button, TCLocalize("Duration"), 1, 500);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcJellyTeeOthers, TCLocalize("Apply to other players"), &g_Config.m_TcJellyTeeOthers, &Column, LineSize);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"breath", "breathing", "idle", "sway", "tee"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Tee Breathing"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeBreathing, TCLocalize("Breathe while standing still"), &g_Config.m_TcTeeBreathing, &Column, LineSize);
	Column.HSplitTop(MarginSmall, nullptr, &Column);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeBreathingOthers, TCLocalize("Apply to other players"), &g_Config.m_TcTeeBreathingOthers, &Column, LineSize);

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"hitbox", "hit", "box", "hurtbox", "dot", "collision"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Tee Hitbox"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowHitBox, TCLocalize("Show my hitbox"), &g_Config.m_TcShowHitBox, &Column, LineSize);
	if(g_Config.m_TcShowHitBox)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowHitBoxOthers, TCLocalize("Show on other players"), &g_Config.m_TcShowHitBoxOthers, &Column, LineSize);
		static CButtonContainer s_ShowHitBoxColorId;
		DoLine_ColorPicker(&s_ShowHitBoxColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Hitbox color"), &g_Config.m_TcShowHitBoxColor, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"hook", "style", "custom", "effect"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Custom Hooks"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	{
		static std::vector<const char *> s_HookStyleNames;
		s_HookStyleNames = {TCLocalize("None"), TCLocalize("Whirlwind"), TCLocalize("Black hole"), TCLocalize("Magic"), TCLocalize("Fire"), TCLocalize("Rainbow"), TCLocalize("Lightning"), TCLocalize("Tentacle"), TCLocalize("Frost")};
		static const int s_aHookStyleValues[] = {0, 1, 2, 3, 4, 6, 7, 8, 9};
		static CUi::SDropDownState s_HookStyleState;
		static CScrollRegion s_HookStyleScroll;
		s_HookStyleState.m_SelectionPopupContext.m_pScrollRegion = &s_HookStyleScroll;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(110.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Hook"), FontSize, TEXTALIGN_ML);
		int HookStyleIndex = 0;
		for(int HookStyle = 0; HookStyle < (int)(sizeof(s_aHookStyleValues) / sizeof(s_aHookStyleValues[0])); HookStyle++)
		{
			if(s_aHookStyleValues[HookStyle] == g_Config.m_TcHookStyle)
				HookStyleIndex = HookStyle;
		}
		HookStyleIndex = Ui()->DoDropDown(&DropDownRect, HookStyleIndex, s_HookStyleNames.data(), s_HookStyleNames.size(), s_HookStyleState);
		g_Config.m_TcHookStyle = s_aHookStyleValues[std::clamp(HookStyleIndex, 0, (int)(sizeof(s_aHookStyleValues) / sizeof(s_aHookStyleValues[0])) - 1)];
	}

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcHookSize, &g_Config.m_TcHookSize, &Button, TCLocalize("Size"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcHookChargeTime, &g_Config.m_TcHookChargeTime, &Button, TCLocalize("Grow time"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookRope, TCLocalize("Style the hook rope"), &g_Config.m_TcHookRope, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookRings, TCLocalize("Ring effects"), &g_Config.m_TcHookRings, &Column, LineSize);

	if(g_Config.m_TcHookStyle == 1)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookWindEye, TCLocalize("Eye"), &g_Config.m_TcHookWindEye, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookWindSpin, &g_Config.m_TcHookWindSpin, &Button, TCLocalize("Spin speed"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookWindOpacity, &g_Config.m_TcHookWindOpacity, &Button, TCLocalize("Wind opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookWindThickness, &g_Config.m_TcHookWindThickness, &Button, TCLocalize("Line thickness"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookWindLeafAmount, &g_Config.m_TcHookWindLeafAmount, &Button, TCLocalize("Leaves"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookWindDust, &g_Config.m_TcHookWindDust, &Button, TCLocalize("Dust"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookWindBurst, TCLocalize("Grab gust"), &g_Config.m_TcHookWindBurst, &Column, LineSize);
		static CButtonContainer s_HookWindColorId;
		DoLine_ColorPicker(&s_HookWindColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Wind color"), &g_Config.m_TcHookWindColor, ColorRGBA(0.75f, 0.85f, 1.0f, 1.0f), false);
	}
	else if(g_Config.m_TcHookStyle == 2)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookHoleDisk, TCLocalize("Disk"), &g_Config.m_TcHookHoleDisk, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookHoleStreaks, &g_Config.m_TcHookHoleStreaks, &Button, TCLocalize("Matter"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookHoleSpin, &g_Config.m_TcHookHoleSpin, &Button, TCLocalize("Spin"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_HookHoleColorId;
		DoLine_ColorPicker(&s_HookHoleColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Disk color"), &g_Config.m_TcHookHoleColor, ColorRGBA(0.65f, 0.35f, 1.0f, 1.0f), false);
	}
	else if(g_Config.m_TcHookStyle == 3)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookMagicRunes, TCLocalize("Runes"), &g_Config.m_TcHookMagicRunes, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookMagicSparkles, &g_Config.m_TcHookMagicSparkles, &Button, TCLocalize("Sparkles"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookMagicSpin, &g_Config.m_TcHookMagicSpin, &Button, TCLocalize("Spin"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_HookMagicColorId;
		DoLine_ColorPicker(&s_HookMagicColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Spell color"), &g_Config.m_TcHookMagicColor, ColorRGBA(0.45f, 0.85f, 1.0f, 1.0f), false);
	}
	else if(g_Config.m_TcHookStyle == 4)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookFireSmoke, TCLocalize("Smoke"), &g_Config.m_TcHookFireSmoke, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookFireEmbers, &g_Config.m_TcHookFireEmbers, &Button, TCLocalize("Embers"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookFireIntensity, &g_Config.m_TcHookFireIntensity, &Button, TCLocalize("Power"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_HookFireColorId;
		DoLine_ColorPicker(&s_HookFireColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Fire color"), &g_Config.m_TcHookFireColor, ColorRGBA(1.0f, 0.5f, 0.15f, 1.0f), false);
	}
	else if(g_Config.m_TcHookStyle == 6)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookRainbowRays, TCLocalize("Rays"), &g_Config.m_TcHookRainbowRays, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookRainbowGlitter, &g_Config.m_TcHookRainbowGlitter, &Button, TCLocalize("Glitter"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookRainbowWidth, &g_Config.m_TcHookRainbowWidth, &Button, TCLocalize("Ribbon width"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookRainbowSpeed, &g_Config.m_TcHookRainbowSpeed, &Button, TCLocalize("Flow"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
	}
	else if(g_Config.m_TcHookStyle == 7)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookBoltArc, &g_Config.m_TcHookBoltArc, &Button, TCLocalize("Wall arcs"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookBoltBranches, &g_Config.m_TcHookBoltBranches, &Button, TCLocalize("Forks"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookBoltJitter, &g_Config.m_TcHookBoltJitter, &Button, TCLocalize("Jitter"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookBoltSparks, &g_Config.m_TcHookBoltSparks, &Button, TCLocalize("Sparks"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_HookBoltColorId;
		DoLine_ColorPicker(&s_HookBoltColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Lightning color"), &g_Config.m_TcHookBoltColor, ColorRGBA(0.6f, 0.8f, 1.0f, 1.0f), false);
	}
	else if(g_Config.m_TcHookStyle == 8)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookTentacleSuckers, TCLocalize("Suckers"), &g_Config.m_TcHookTentacleSuckers, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookTentacleWaves, &g_Config.m_TcHookTentacleWaves, &Button, TCLocalize("Writhing"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookTentacleThickness, &g_Config.m_TcHookTentacleThickness, &Button, TCLocalize("Thickness"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookTentacleSlime, &g_Config.m_TcHookTentacleSlime, &Button, TCLocalize("Slime"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_HookTentacleColorId;
		DoLine_ColorPicker(&s_HookTentacleColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Flesh color"), &g_Config.m_TcHookTentacleColor, ColorRGBA(0.85f, 0.4f, 0.55f, 1.0f), false);
	}
	else if(g_Config.m_TcHookStyle == 9)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookFrostCrystals, &g_Config.m_TcHookFrostCrystals, &Button, TCLocalize("Crystals"), 3, 24, &CUi::ms_LinearScrollbarScale, 0, "");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookFrostGrowth, &g_Config.m_TcHookFrostGrowth, &Button, TCLocalize("Growth"), 25, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookFrostSnow, &g_Config.m_TcHookFrostSnow, &Button, TCLocalize("Snowfall"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHookFrostMist, &g_Config.m_TcHookFrostMist, &Button, TCLocalize("Mist"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_HookFrostColorId;
		DoLine_ColorPicker(&s_HookFrostColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Frost color"), &g_Config.m_TcHookFrostColor, ColorRGBA(0.62f, 0.85f, 1.0f, 1.0f), false);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"hook", "effects", "jump", "hammer", "hold"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Hook Effects"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAirJump, TCLocalize("Air jump ring"), &g_Config.m_TcAirJump, &Column, LineSize);
	if(g_Config.m_TcAirJump)
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAirJumpHideParticles, TCLocalize("Disable jump particles"), &g_Config.m_TcAirJumpHideParticles, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHammerSlash, TCLocalize("Hammer slash"), &g_Config.m_TcHammerSlash, &Column, LineSize);
	if(g_Config.m_TcHammerSlash)
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHammerHideParticles, TCLocalize("Disable hammer particles"), &g_Config.m_TcHammerHideParticles, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookTether, TCLocalize("Hook lasso"), &g_Config.m_TcHookTether, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHookLassoBox, TCLocalize("Hook box"), &g_Config.m_TcHookLassoBox, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcGatheredEnergy, TCLocalize("Hook hold"), &g_Config.m_TcGatheredEnergy, &Column, LineSize);
	if(g_Config.m_TcGatheredEnergy)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcGatheredEnergyAura, TCLocalize("Hold aura"), &g_Config.m_TcGatheredEnergyAura, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcGatheredEnergyBurst, TCLocalize("Hold burst"), &g_Config.m_TcGatheredEnergyBurst, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcGatheredEnergyAmount, &g_Config.m_TcGatheredEnergyAmount, &Button, TCLocalize("Energy motes"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"weather", "snow", "rain", "fireflies", "dust", "leaves", "sakura", "petals"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Weather"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAtmosphere, TCLocalize("Enable"), &g_Config.m_TcAtmosphere, &Column, LineSize);
	if(g_Config.m_TcAtmosphere)
	{
		static std::vector<const char *> s_WeatherNames;
		s_WeatherNames = {TCLocalize("Snow"), TCLocalize("Rain"), TCLocalize("Fireflies"), TCLocalize("Leaves"), TCLocalize("Sakura")};
		static const int s_aWeatherValues[] = {FGF_WEATHER_SNOW, FGF_WEATHER_RAIN, FGF_WEATHER_FIREFLIES, FGF_WEATHER_LEAVES, FGF_WEATHER_SAKURA};
		static constexpr int NumWeatherKinds = (int)(sizeof(s_aWeatherValues) / sizeof(s_aWeatherValues[0]));
		static CUi::SDropDownState s_WeatherState;
		static CScrollRegion s_WeatherScroll;
		s_WeatherState.m_SelectionPopupContext.m_pScrollRegion = &s_WeatherScroll;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(110.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Type"), FontSize, TEXTALIGN_ML);
		int WeatherIndex = 0;
		for(int i = 0; i < NumWeatherKinds; i++)
		{
			if(s_aWeatherValues[i] == g_Config.m_TcWeather)
			{
				WeatherIndex = i;
				break;
			}
		}
		WeatherIndex = std::clamp(Ui()->DoDropDown(&DropDownRect, WeatherIndex, s_WeatherNames.data(), s_WeatherNames.size(), s_WeatherState), 0, NumWeatherKinds - 1);
		g_Config.m_TcWeather = s_aWeatherValues[WeatherIndex];

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherAmount, &g_Config.m_TcWeatherAmount, &Button, TCLocalize("Amount"), 25, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherSize, &g_Config.m_TcWeatherSize, &Button, TCLocalize("Size"), 25, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherOpacity, &g_Config.m_TcWeatherOpacity, &Button, TCLocalize("Opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		if(g_Config.m_TcWeather == FGF_WEATHER_SNOW)
		{
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWeatherSnowflakes, TCLocalize("Snowflakes"), &g_Config.m_TcWeatherSnowflakes, &Column, LineSize);
			static CButtonContainer s_WeatherSnowColorId;
			DoLine_ColorPicker(&s_WeatherSnowColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Snow color"), &g_Config.m_TcWeatherSnowColor, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false);
		}
		else if(g_Config.m_TcWeather == FGF_WEATHER_RAIN)
		{
			static CButtonContainer s_WeatherRainColorId;
			DoLine_ColorPicker(&s_WeatherRainColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Rain color"), &g_Config.m_TcWeatherRainColor, ColorRGBA(0.6f, 0.8f, 1.0f, 1.0f), false);
		}
		else if(g_Config.m_TcWeather == FGF_WEATHER_FIREFLIES)
		{
			static CButtonContainer s_WeatherFireflyColorId;
			DoLine_ColorPicker(&s_WeatherFireflyColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Firefly color"), &g_Config.m_TcWeatherFireflyColor, ColorRGBA(1.0f, 0.9f, 0.4f, 1.0f), false);
		}
		else if(g_Config.m_TcWeather == FGF_WEATHER_LEAVES)
		{
			static CButtonContainer s_WeatherLeafColorId;
			DoLine_ColorPicker(&s_WeatherLeafColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Leaf color"), &g_Config.m_TcWeatherLeafColor, ColorRGBA(0.26f, 0.68f, 0.16f, 1.0f), false);
		}
		else if(g_Config.m_TcWeather == FGF_WEATHER_SAKURA)
		{
			static CButtonContainer s_WeatherSakuraColorId;
			DoLine_ColorPicker(&s_WeatherSakuraColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Sakura petal color"), &g_Config.m_TcWeatherSakuraColor, ColorRGBA(0.96f, 0.75f, 0.81f, 1.0f), false);
		}
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAmbientDust, TCLocalize("Ambient dust"), &g_Config.m_TcAmbientDust, &Column, LineSize);
	if(g_Config.m_TcAmbientDust)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAmbientDustAmount, &g_Config.m_TcAmbientDustAmount, &Button, TCLocalize("Amount"), 25, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAmbientDustSize, &g_Config.m_TcAmbientDustSize, &Button, TCLocalize("Size"), 25, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAmbientDustOpacity, &g_Config.m_TcAmbientDustOpacity, &Button, TCLocalize("Opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_AmbientDustColorId;
		DoLine_ColorPicker(&s_AmbientDustColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Dust color"), &g_Config.m_TcAmbientDustColor, ColorRGBA(0.8f, 0.75f, 0.6f, 1.0f), false);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"sound", "3d", "raycast", "echo", "occlusion", "audio"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("3D Sound"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRaycastSounds, TCLocalize("Raycast 3D sounds"), &g_Config.m_TcRaycastSounds, &Column, LineSize);
	if(g_Config.m_TcRaycastSounds)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRaycastSoundsTeammates, TCLocalize("Mute teammates behind walls"), &g_Config.m_TcRaycastSoundsTeammates, &Column, LineSize);

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcRaycastSoundsOcclusion, &g_Config.m_TcRaycastSoundsOcclusion, &Button, TCLocalize("Volume through one wall"), 5, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRaycastSoundsEcho, TCLocalize("Echo in enclosed spaces"), &g_Config.m_TcRaycastSoundsEcho, &Column, LineSize);
		if(g_Config.m_TcRaycastSoundsEcho)
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcRaycastSoundsEchoVolume, &g_Config.m_TcRaycastSoundsEchoVolume, &Button, TCLocalize("Echo volume"), 5, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		}
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** RightView ***** //
	LeftView = Column;
	Column = RightView;

	// ***** HUD ***** //
	if(SectionEnabled(false, pSettingsSearch, {"hud", "fps", "chat", "scoreboard"}))
	{
	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("HUD"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcMiniVoteHud, TCLocalize("Show mini vote HUD"), &g_Config.m_TcMiniVoteHud, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcMiniDebug, TCLocalize("Show position and angle (mini debug)"), &g_Config.m_TcMiniDebug, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderCursorSpec, TCLocalize("Show your cursor when in free spectate"), &g_Config.m_TcRenderCursorSpec, &Column, LineSize);
	if(g_Config.m_TcRenderCursorSpec)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcRenderCursorSpecAlpha, &g_Config.m_TcRenderCursorSpecAlpha, &Button, TCLocalize("Spectate cursor alpha"), 0, 100);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNotifyWhenLast, TCLocalize("Show when you are the last alive"), &g_Config.m_TcNotifyWhenLast, &Column, LineSize);
	if(g_Config.m_TcNotifyWhenLast)
	{
		CUIRect NotificationConfig;
		Column.HSplitTop(LineSize + MarginSmall, &NotificationConfig, &Column);
		NotificationConfig.VSplitMid(&Button, &NotificationConfig);
		static CLineInput s_LastInput(g_Config.m_TcNotifyWhenLastText, sizeof(g_Config.m_TcNotifyWhenLastText));
		s_LastInput.SetEmptyText(TCLocalize("Last!"));
		Button.HSplitTop(MarginSmall, nullptr, &Button);
		Ui()->DoEditBox(&s_LastInput, &Button, EditBoxFontSize);
		static CButtonContainer s_ClientNotifyWhenLastColor;
		DoLine_ColorPicker(&s_ClientNotifyWhenLastColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &NotificationConfig, "", &g_Config.m_TcNotifyWhenLastColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenLastX, &g_Config.m_TcNotifyWhenLastX, &Button, TCLocalize("Horizontal Position"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenLastY, &g_Config.m_TcNotifyWhenLastY, &Button, TCLocalize("Vertical Position"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenLastSize, &g_Config.m_TcNotifyWhenLastSize, &Button, TCLocalize("Font Size"), 1, 50);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowCenter, TCLocalize("Show screen center lines"), &g_Config.m_TcShowCenter, &Column, LineSize);
	if(g_Config.m_TcShowCenter)
	{
		Column.HSplitTop(LineSize + MarginSmall, &Button, &Column);
		static CButtonContainer s_ShowCenterLineColor;
		DoLine_ColorPicker(&s_ShowCenterLineColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Button, TCLocalize("Screen center line color"), &g_Config.m_TcShowCenterColor, DefaultConfig::TcShowCenterColor, false, nullptr, true);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcShowCenterWidth, &g_Config.m_TcShowCenterWidth, &Button, TCLocalize("Screen center line width"), 0, 20);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Frozen Tee Display ***** //
	if(SectionEnabled(false, pSettingsSearch, {"frozen", "freeze", "tee", "display"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Frozen Tee Display"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowFrozenHud, TCLocalize("Show frozen tee display"), &g_Config.m_TcShowFrozenHud, &Column, LineSize);
	if(g_Config.m_TcShowFrozenHud)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowFrozenHudSkins, TCLocalize("Use skins instead of ninja tees"), &g_Config.m_TcShowFrozenHudSkins, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFrozenHudTeamOnly, TCLocalize("Only show after joining a team"), &g_Config.m_TcFrozenHudTeamOnly, &Column, LineSize);

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcFrozenMaxRows, &g_Config.m_TcFrozenMaxRows, &Button, TCLocalize("Max Rows"), 1, 6);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcFrozenHudTeeSize, &g_Config.m_TcFrozenHudTeeSize, &Button, TCLocalize("Tee Size"), 8, 27);

	}
	{
		CUIRect CheckBoxRect, CheckBoxRect2;
		Column.HSplitTop(LineSize, &CheckBoxRect, &Column);
		if(DoButton_CheckBox(&g_Config.m_TcShowFrozenText, TCLocalize("Tees left alive text"), g_Config.m_TcShowFrozenText >= 1, &CheckBoxRect))
			g_Config.m_TcShowFrozenText = g_Config.m_TcShowFrozenText >= 1 ? 0 : 1;
		if(g_Config.m_TcShowFrozenText)
		{
			Column.HSplitTop(LineSize, &CheckBoxRect2, &Column);
			static int s_CountFrozenText = 0;
			if(DoButton_CheckBox(&s_CountFrozenText, TCLocalize("Count frozen tees"), g_Config.m_TcShowFrozenText == 2, &CheckBoxRect2))
				g_Config.m_TcShowFrozenText = g_Config.m_TcShowFrozenText != 2 ? 2 : 1;
		}
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Tile Outlines ***** //
	if(SectionEnabled(false, pSettingsSearch, {"outline", "tile", "pulsate", "audio", "discord"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Tile Outlines"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutline, TCLocalize("Show any enabled outlines"), &g_Config.m_TcOutline, &Column, LineSize);
	if(g_Config.m_TcOutline)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlineEntities, TCLocalize("Only show outlines in entities"), &g_Config.m_TcOutlineEntities, &Column, LineSize);

		auto DoOutlineType = [&](CButtonContainer &ButtonContainer, const char *pName, int &Enable, int &Width, unsigned int &Color, const unsigned int &ColorDefault, int &Gradient, unsigned int &GradientColor2, CButtonContainer &GradientButton) {
			// Checkbox & Color
			DoLine_ColorPicker(&ButtonContainer, ColorPickerLineSize, ColorPickerLabelSize, 0, &Column, pName, &Color, ColorDefault, true, &Enable, true);
			if(!Enable)
			{
				Column.HSplitTop(ColorPickerLineSpacing, nullptr, &Column);
				return;
			}
			// Width
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&Width, &Width, &Button, TCLocalize("Width", "Outlines"), 1, 16);
			//
			DoLine_ColorPicker(&GradientButton, ColorPickerLineSize, ColorPickerLabelSize, 0, &Column, TCLocalize("Gradient"), &GradientColor2, ColorRGBA(0.0f, 0.0f, 1.0f, 1.0f), true, &Gradient, true);
			Column.HSplitTop(ColorPickerLineSpacing, nullptr, &Column);
		};
		Column.HSplitTop(ColorPickerLineSpacing, nullptr, &Column);
		static CButtonContainer s_aOutlineButtonContainers[5];
		static CButtonContainer s_aOutlineGradientButtonContainers[5];
		DoOutlineType(s_aOutlineButtonContainers[0], TCLocalize("Unhook & hook"), g_Config.m_TcOutlineSolid, g_Config.m_TcOutlineWidthSolid, g_Config.m_TcOutlineColorSolid, DefaultConfig::TcOutlineColorSolid, g_Config.m_TcOutlineGradientSolid, g_Config.m_TcOutlineGradientColor2Solid, s_aOutlineGradientButtonContainers[0]);
		DoOutlineType(s_aOutlineButtonContainers[1], TCLocalize("Freeze & deep"), g_Config.m_TcOutlineFreeze, g_Config.m_TcOutlineWidthFreeze, g_Config.m_TcOutlineColorFreeze, DefaultConfig::TcOutlineColorFreeze, g_Config.m_TcOutlineGradientFreeze, g_Config.m_TcOutlineGradientColor2Freeze, s_aOutlineGradientButtonContainers[1]);
		DoOutlineType(s_aOutlineButtonContainers[2], TCLocalize("Unfreeze & undeep"), g_Config.m_TcOutlineUnfreeze, g_Config.m_TcOutlineWidthUnfreeze, g_Config.m_TcOutlineColorUnfreeze, DefaultConfig::TcOutlineColorUnfreeze, g_Config.m_TcOutlineGradientUnfreeze, g_Config.m_TcOutlineGradientColor2Unfreeze, s_aOutlineGradientButtonContainers[2]);
		DoOutlineType(s_aOutlineButtonContainers[3], TCLocalize("Kill"), g_Config.m_TcOutlineKill, g_Config.m_TcOutlineWidthKill, g_Config.m_TcOutlineColorKill, DefaultConfig::TcOutlineColorKill, g_Config.m_TcOutlineGradientKill, g_Config.m_TcOutlineGradientColor2Kill, s_aOutlineGradientButtonContainers[3]);
		DoOutlineType(s_aOutlineButtonContainers[4], TCLocalize("Tele"), g_Config.m_TcOutlineTele, g_Config.m_TcOutlineWidthTele, g_Config.m_TcOutlineColorTele, DefaultConfig::TcOutlineColorTele, g_Config.m_TcOutlineGradientTele, g_Config.m_TcOutlineGradientColor2Tele, s_aOutlineGradientButtonContainers[4]);
		Column.h -= ColorPickerLineSpacing;

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcOutlineGradientSpeed, &g_Config.m_TcOutlineGradientSpeed, &Button, TCLocalize("Gradient scroll speed"), 0, 100);
		Column.HSplitTop(ColorPickerLineSpacing, nullptr, &Column);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlinePulsate, TCLocalize("Outline pulsation (system sound)"), &g_Config.m_TcOutlinePulsate, &Column, LineSize);
		if(g_Config.m_TcOutlinePulsate)
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcOutlinePulsateStrength, &g_Config.m_TcOutlinePulsateStrength, &Button, TCLocalize("Pulsation strength"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
			Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlinePulsateFilter, TCLocalize("Ignore own and listed processes"), &g_Config.m_TcOutlinePulsateFilter, &Column, LineSize);
			if(g_Config.m_TcOutlinePulsateFilter)
			{
				Column.HSplitTop(MarginSmall, nullptr, &Column);
				Column.HSplitTop(LineSize, &Button, &Column);
				static CLineInput s_FilterListInput;
				DoEditBoxWithLabel(&s_FilterListInput, &Button, TCLocalize("Ignored processes"), "discord.exe,ddnet.exe", g_Config.m_TcOutlinePulsateFilterList, sizeof(g_Config.m_TcOutlinePulsateFilterList));
			}
			Column.HSplitTop(ColorPickerLineSpacing, nullptr, &Column);
		}

		// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlineFreeze, TCLocalize("Outline freeze & deep"), &g_Config.m_TcOutlineFreeze, &Column, LineSize);
		// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlineSolid, TCLocalize("Outline walls"), &g_Config.m_TcOutlineSolid, &Column, LineSize);
		// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlineTele, TCLocalize("Outline teleporter"), &g_Config.m_TcOutlineTele, &Column, LineSize);
		// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlineUnfreeze, TCLocalize("Outline unfreeze & undeep"), &g_Config.m_TcOutlineUnfreeze, &Column, LineSize);
		// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlineKill, TCLocalize("Outline kill"), &g_Config.m_TcOutlineKill, &Column, LineSize);
		// Column.HSplitTop(LineSize, &Button, &Column);
		// Ui()->DoScrollbarOption(&g_Config.m_TcOutlineWidth, &g_Config.m_TcOutlineWidth, &Button, TCLocalize("Outline width"), 1, 16);
		// Column.HSplitTop(LineSize, &Button, &Column);
		// Ui()->DoScrollbarOption(&g_Config.m_TcOutlineAlpha, &g_Config.m_TcOutlineAlpha, &Button, TCLocalize("Outline alpha"), 0, 100);
		// Column.HSplitTop(LineSize, &Button, &Column);
		// Ui()->DoScrollbarOption(&g_Config.m_TcOutlineAlphaSolid, &g_Config.m_TcOutlineAlphaSolid, &Button, TCLocalize("Outline Alpha (walls)"), 0, 100);
		// static CButtonContainer s_OutlineColorFreezeId, s_OutlineColorSolidId, s_OutlineColorTeleId, s_OutlineColorUnfreezeId, s_OutlineColorKillId;
		// DoLine_ColorPicker(&s_OutlineColorFreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Freeze outline color"), &g_Config.m_TcOutlineColorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		// DoLine_ColorPicker(&s_OutlineColorSolidId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Walls outline color"), &g_Config.m_TcOutlineColorSolid, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		// DoLine_ColorPicker(&s_OutlineColorTeleId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Teleporter outline color"), &g_Config.m_TcOutlineColorTele, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		// DoLine_ColorPicker(&s_OutlineColorUnfreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Unfreeze outline color"), &g_Config.m_TcOutlineColorUnfreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		// DoLine_ColorPicker(&s_OutlineColorKillId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Kill outline color"), &g_Config.m_TcOutlineColorKill, ColorRGBA(0.0f, 0.0f, 0.0f), false);
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Ghost Tools ***** //
	if(SectionEnabled(false, pSettingsSearch, {"ghost", "prediction", "swap"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Ghost Tools"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowOthersGhosts, TCLocalize("Show unpredicted ghosts for other players"), &g_Config.m_TcShowOthersGhosts, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSwapGhosts, TCLocalize("Swap ghosts and normal players"), &g_Config.m_TcSwapGhosts, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcPredGhostsAlpha, &g_Config.m_TcPredGhostsAlpha, &Button, TCLocalize("Predicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcUnpredGhostsAlpha, &g_Config.m_TcUnpredGhostsAlpha, &Button, TCLocalize("Unpredicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHideFrozenGhosts, TCLocalize("Hide ghosts of frozen players"), &g_Config.m_TcHideFrozenGhosts, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderGhostAsCircle, TCLocalize("Render ghosts as circles"), &g_Config.m_TcRenderGhostAsCircle, &Column, LineSize);

	static CButtonContainer s_ReaderButtonGhost, s_ClearButtonGhost;
	DoLine_KeyReader(Column, s_ReaderButtonGhost, s_ClearButtonGhost, TCLocalize("Toggle ghosts key"), "toggle tc_show_others_ghosts 0 1");

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	// ***** Rainbow ***** //
	if(SectionEnabled(false, pSettingsSearch, {"rainbow", "color"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Rainbow"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowTees, TCLocalize("Rainbow Tees"), &g_Config.m_TcRainbowTees, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowWeapon, TCLocalize("Rainbow weapons"), &g_Config.m_TcRainbowWeapon, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowHook, TCLocalize("Rainbow hook"), &g_Config.m_TcRainbowHook, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowOthers, TCLocalize("Rainbow others"), &g_Config.m_TcRainbowOthers, &Column, LineSize);

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	static std::vector<const char *> s_RainbowDropDownNames;
	s_RainbowDropDownNames = {TCLocalize("Rainbow"), TCLocalize("Pulse"), TCLocalize("Black"), TCLocalize("Random")};
	static CUi::SDropDownState s_RainbowDropDownState;
	static CScrollRegion s_RainbowDropDownScrollRegion;
	s_RainbowDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_RainbowDropDownScrollRegion;
	int RainbowSelectedOld = g_Config.m_TcRainbowMode - 1;
	CUIRect RainbowDropDownRect;
	Column.HSplitTop(LineSize, &RainbowDropDownRect, &Column);
	const int RainbowSelectedNew = Ui()->DoDropDown(&RainbowDropDownRect, RainbowSelectedOld, s_RainbowDropDownNames.data(), s_RainbowDropDownNames.size(), s_RainbowDropDownState);
	if(RainbowSelectedOld != RainbowSelectedNew)
	{
		g_Config.m_TcRainbowMode = RainbowSelectedNew + 1;
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcRainbowSpeed, &g_Config.m_TcRainbowSpeed, &Button, TCLocalize("Rainbow speed"), 0, 5000, &CUi::ms_LogarithmicScrollbarScale, 0, "%");
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	}
	// ***** Tee Trails ***** //
	if(SectionEnabled(false, pSettingsSearch, {"trail", "tee"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Tee Trails"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrail, TCLocalize("Enable tee trails"), &g_Config.m_TcTeeTrail, &Column, LineSize);
	if(g_Config.m_TcTeeTrail)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailOthers, TCLocalize("Show other tees' trails"), &g_Config.m_TcTeeTrailOthers, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailFade, TCLocalize("Fade trail alpha"), &g_Config.m_TcTeeTrailFade, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailTaper, TCLocalize("Taper trail width"), &g_Config.m_TcTeeTrailTaper, &Column, LineSize);

		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		std::vector<const char *> vTrailDropDownNames;
		vTrailDropDownNames = {TCLocalize("Solid"), TCLocalize("Tee"), TCLocalize("Rainbow"), TCLocalize("Speed")};
		static CUi::SDropDownState s_TrailDropDownState;
		static CScrollRegion s_TrailDropDownScrollRegion;
		s_TrailDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_TrailDropDownScrollRegion;
		int TrailSelectedOld = g_Config.m_TcTeeTrailColorMode - 1;
		CUIRect TrailDropDownRect;
		Column.HSplitTop(LineSize, &TrailDropDownRect, &Column);
		const int TrailSelectedNew = Ui()->DoDropDown(&TrailDropDownRect, TrailSelectedOld, vTrailDropDownNames.data(), vTrailDropDownNames.size(), s_TrailDropDownState);
		if(TrailSelectedOld != TrailSelectedNew)
		{
			g_Config.m_TcTeeTrailColorMode = TrailSelectedNew + 1;
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		static CButtonContainer s_TeeTrailColor;
		if(g_Config.m_TcTeeTrailColorMode == CTrails::COLORMODE_SOLID)
			DoLine_ColorPicker(&s_TeeTrailColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Tee trail color"), &g_Config.m_TcTeeTrailColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
		else
			Column.HSplitTop(ColorPickerLineSize + ColorPickerLineSpacing, &Button, &Column);

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailWidth, &g_Config.m_TcTeeTrailWidth, &Button, TCLocalize("Trail width"), 0, 20);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailLength, &g_Config.m_TcTeeTrailLength, &Button, TCLocalize("Trail length"), 0, 200);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailAlpha, &g_Config.m_TcTeeTrailAlpha, &Button, TCLocalize("Trail alpha"), 0, 100);

	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	}
	// ***** BG Draw ***** //
	if(SectionEnabled(false, pSettingsSearch, {"background", "draw", "bg", "timecps", "checkpoint"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Background Draw"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	static CButtonContainer s_BgDrawColor;
	DoLine_ColorPicker(&s_BgDrawColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Color"), &g_Config.m_TcBgDrawColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);

	Column.HSplitTop(LineSize * 2.0f, &Button, &Column);
	if(g_Config.m_TcBgDrawFadeTime == 0)
		Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawFadeTime, &g_Config.m_TcBgDrawFadeTime, &Button, TCLocalize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, TCLocalize(" seconds (never)"));
	else
		Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawFadeTime, &g_Config.m_TcBgDrawFadeTime, &Button, TCLocalize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, TCLocalize(" seconds"));

	Column.HSplitTop(LineSize * 2.0f, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawWidth, &g_Config.m_TcBgDrawWidth, &Button, TCLocalize("Width"), 1, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

	static CButtonContainer s_ReaderButtonDraw, s_ClearButtonDraw;
	DoLine_KeyReader(Column, s_ReaderButtonDraw, s_ClearButtonDraw, TCLocalize("Draw where mouse is"), "+bg_draw");

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcBgDrawClientTimeCps, TCLocalize("Client sided timecps"), &g_Config.m_TcBgDrawClientTimeCps, &Column, LineSize);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	}
	if(SectionEnabled(true, pSettingsSearch, {"practice", "tags", "marker"}))
	{
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, TCLocalize("Practice tags"), HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPracticeTagEnabled, TCLocalize("Enable practice tags"), &g_Config.m_TcPracticeTagEnabled, &Column, LineSize);

		if(g_Config.m_TcPracticeTagEnabled)
		{
			static CUi::SDropDownState s_PracticeTagShapeDropDownState;
			static CScrollRegion s_PracticeTagShapeDropDownScrollRegion;
			s_PracticeTagShapeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_PracticeTagShapeDropDownScrollRegion;
			const char *apShapeNames[] = {TCLocalize("Circle"), TCLocalize("Square"), TCLocalize("Triangle"), TCLocalize("Cross")};
			Column.HSplitTop(LineSize, &Button, &Column);
			Button.VSplitLeft(120.0f, &Label, &Button);
			Ui()->DoLabel(&Label, TCLocalize("Shape:"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcPracticeTagShape = Ui()->DoDropDown(&Button, g_Config.m_TcPracticeTagShape, apShapeNames, std::size(apShapeNames), s_PracticeTagShapeDropDownState);
			Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

			static CButtonContainer s_PracticeTagColor;
			DoLine_ColorPicker(&s_PracticeTagColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Color"), &g_Config.m_TcPracticeTagColor, ColorRGBA(1.0f, 0.0f, 0.0f), false);

			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcPracticeTagSize, &g_Config.m_TcPracticeTagSize, &Button, TCLocalize("Size"), 4, 64);
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcPracticeTagMaxItems, &g_Config.m_TcPracticeTagMaxItems, &Button, TCLocalize("Maximum tags on map"), 0, 2048);
		}

		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
		Column.HSplitTop(MarginSmall, nullptr, &Column);
	}
	// ***** Finish Name ***** //
	if(SectionEnabled(false, pSettingsSearch, {"finish", "name", "message"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Finish Name"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcChangeNameNearFinish, TCLocalize("Attempt to change your name when near finish"), &g_Config.m_TcChangeNameNearFinish, &Column, LineSize);
	if(g_Config.m_TcChangeNameNearFinish)
	{
		Column.HSplitTop(LineSize + MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, TCLocalize("Finish Name:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_FinishName(g_Config.m_TcFinishName, sizeof(g_Config.m_TcFinishName));
		Ui()->DoEditBox(&s_FinishName, &Button, EditBoxFontSize);
	}
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcUnlockTeamNearFinish, TCLocalize("Send /unlock when near finish"), &g_Config.m_TcUnlockTeamNearFinish, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSendFinishMessage, TCLocalize("Send a message when you finish"), &g_Config.m_TcSendFinishMessage, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcScreenshotOnFinish, TCLocalize("Take a screenshot when you finish"), &g_Config.m_TcScreenshotOnFinish, &Column, LineSize);
	if(g_Config.m_TcSendFinishMessage)
	{
		Column.HSplitTop(LineSize + MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, TCLocalize("Finish message:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_FinishMessage(g_Config.m_TcFinishMessage, sizeof(g_Config.m_TcFinishMessage));
		Ui()->DoEditBox(&s_FinishMessage, &Button, EditBoxFontSize);
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"automation", "notify", "join", "move", "name", "message", "afk", "spectate", "spec", "unspectate", "unspec", "bypass", "nick", "mute", "war", "heart"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Automation"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoNotifyOnJoin, TCLocalize("Notify when a player on your list joins"), &g_Config.m_TcAutoNotifyOnJoin, &Column, LineSize);
	if(g_Config.m_TcAutoNotifyOnJoin)
	{
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		{
			CUIRect Box;
			Column.HSplitTop(LineSize + MarginExtraSmall, &Box, &Column);
			Box.VSplitMid(&Label, &Button);
			Ui()->DoLabel(&Label, Localize("Notify Names"), FontSize, TEXTALIGN_ML);
			static CLineInput s_NotifyName(g_Config.m_TcAutoNotifyName, sizeof(g_Config.m_TcAutoNotifyName));
			s_NotifyName.SetEmptyText(TCLocalize("Comma-separated names"));
			Ui()->DoEditBox(&s_NotifyName, &Button, EditBoxFontSize);
		}
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		{
			CUIRect Box;
			Column.HSplitTop(LineSize + MarginExtraSmall, &Box, &Column);
			Box.VSplitMid(&Label, &Button);
			Ui()->DoLabel(&Label, Localize("Notify Message"), FontSize, TEXTALIGN_ML);
			static CLineInput s_NotifyMsg(g_Config.m_TcAutoNotifyMsg, sizeof(g_Config.m_TcAutoNotifyMsg));
			s_NotifyMsg.SetEmptyText(TCLocalize("Message (@nick = player name)"));
			Ui()->DoEditBox(&s_NotifyMsg, &Button, EditBoxFontSize);
		}
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	}
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNotifyOnMove, TCLocalize("Notify when your player is moved while tabbed out"), &g_Config.m_TcNotifyOnMove, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNotifyOnMoveInSpec, TCLocalize("Notify when moved in spec"), &g_Config.m_TcNotifyOnMoveInSpec, &Column, LineSize);
	if(g_Config.m_TcNotifyOnMoveInSpec)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTextOnMoveInSpec, TCLocalize("Show text when moved in spec"), &g_Config.m_TcTextOnMoveInSpec, &Column, LineSize);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcReactBypass, TCLocalize("React to bypass (re-apply mute/war/heart on nick)"), &g_Config.m_TcReactBypass, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiUnspectate, TCLocalize("Anti-unspectate in player (confirm inside a tee)"), &g_Config.m_TcAntiUnspectate, &Column, LineSize);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"notify", "button", "switch", "press"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Notify on button press"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	{
		CUIRect Box, CheckBoxRect, MsgBox;
		Column.HSplitTop(LineSize + MarginExtraSmall, &Box, &Column);
		Box.VSplitLeft(140.0f, &CheckBoxRect, &MsgBox);
		if(DoButton_CheckBox(&g_Config.m_TcNotifyOnSwitch, TCLocalize("Print message"), g_Config.m_TcNotifyOnSwitch, &CheckBoxRect))
			g_Config.m_TcNotifyOnSwitch ^= 1;
		static CLineInput s_SwitchMsg(g_Config.m_TcNotifyOnSwitchText, sizeof(g_Config.m_TcNotifyOnSwitchText));
		s_SwitchMsg.SetEmptyText(TCLocalize("Pressed button @num (@type)"));
		Ui()->DoEditBox(&s_SwitchMsg, &MsgBox, EditBoxFontSize);
	}


	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"aura", "idle"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Idle Aura"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPlayerIdleAura, TCLocalize("Show an aura around your tee when idle"), &g_Config.m_TcPlayerIdleAura, &Column, LineSize);
	if(g_Config.m_TcPlayerIdleAura)
	{

		{
			static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}};
			int Value = g_Config.m_TcIdleAuraType;
			if(DoLine_RadioMenu(Column, TCLocalize("Idle aura style"),
				   s_vButtonContainers,
				   {Localize("Starfall"), Localize("Clock")},
				   {1, 3},
				   Value))
			{
				g_Config.m_TcIdleAuraType = Value;
			}
		}

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcPlayerIdleAuraTimer, &g_Config.m_TcPlayerIdleAuraTimer, &Button, TCLocalize("Idle delay"), 2, 30, &CUi::ms_LinearScrollbarScale, 0, "s");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcToggleAuraParticlesNum, &g_Config.m_TcToggleAuraParticlesNum, &Button, TCLocalize("Particle count"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcToggleAuraRadius, &g_Config.m_TcToggleAuraRadius, &Button, TCLocalize("Aura radius"), 0, 600, &CUi::ms_LinearScrollbarScale, 0, "");

	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"hover", "notification"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Hover Notifications"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHoverMessage, TCLocalize("Show system messages as hover notifications"), &g_Config.m_TcHoverMessage, &Column, LineSize);
	if(g_Config.m_TcHoverMessage)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHoverOnly, TCLocalize("Only show system messages as hover (hide from chat)"), &g_Config.m_TcHoverOnly, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHoverMessagesHistory, TCLocalize("Show hover notification history while chat is open"), &g_Config.m_TcHoverMessagesHistory, &Column, LineSize);

		{
			static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}, {}};
			int Value = g_Config.m_TcHoverPlacing;
			if(DoLine_RadioMenu(Column, TCLocalize("Notification position"),
				   s_vButtonContainers,
				   {Localize("Top right"), Localize("Top left"), Localize("Bottom right"), Localize("Bottom left")},
				   {0, 1, 2, 3},
				   Value))
			{
				g_Config.m_TcHoverPlacing = Value;
			}
		}

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHoverMessagesMaxNotifications, &g_Config.m_TcHoverMessagesMaxNotifications, &Button, TCLocalize("Max notifications"), 1, 40, &CUi::ms_LinearScrollbarScale, 0, "");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcHoverMessagesMaxHistory, &g_Config.m_TcHoverMessagesMaxHistory, &Button, TCLocalize("Max history"), 1, 40, &CUi::ms_LinearScrollbarScale, 0, "");

	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"better chat", "smooth chat", "typing", "caret", "chat animation", "chat input"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Better Chat"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSmoothChatTyping, TCLocalize("Smooth typing"), &g_Config.m_TcSmoothChatTyping, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSmoothChatSend, TCLocalize("Smooth send"), &g_Config.m_TcSmoothChatSend, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSmoothChatCaret, TCLocalize("Smooth caret"), &g_Config.m_TcSmoothChatCaret, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcBetterChatNoInputGap, TCLocalize("Hide the empty chat input space"), &g_Config.m_TcBetterChatNoInputGap, &Column, LineSize);

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	if(SectionEnabled(true, pSettingsSearch, {"lyrics", "song", "music", "karaoke", "lrc", "3d text", "sing"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Lyrics"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcLyrics, TCLocalize("Lyrics on the map"), &g_Config.m_TcLyrics, &Column, LineSize);
	if(g_Config.m_TcLyrics)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcLyricsTest, TCLocalize("Test pattern"), &g_Config.m_TcLyricsTest, &Column, LineSize);

		const char *pStatus = GameClient()->m_Lyrics.Status();
		if(pStatus[0] != '\0')
		{
			Column.HSplitTop(LineSize * 0.9f, &Label, &Column);
			Ui()->DoLabel(&Label, pStatus, FontSize * 0.85f, TEXTALIGN_ML);
		}
		if(GameClient()->m_Lyrics.HasLyrics())
		{
			char aInfo[128];
			str_format(aInfo, sizeof(aInfo), "%d lines | %d visible | %.1f s", GameClient()->m_Lyrics.NumLines(), GameClient()->m_Lyrics.NumVisible(), GameClient()->m_Lyrics.Position());
			Column.HSplitTop(LineSize * 0.9f, &Label, &Column);
			Ui()->DoLabel(&Label, aInfo, FontSize * 0.8f, TEXTALIGN_ML);
		}

		Column.HSplitTop(LineSize, &Button, &Column);
		static CLineInput s_LyricsFileInput;
		DoEditBoxWithLabel(&s_LyricsFileInput, &Button, TCLocalize("LRC file"), "auto", g_Config.m_TcLyricsFile, sizeof(g_Config.m_TcLyricsFile));

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcLyricsSize, &g_Config.m_TcLyricsSize, &Button, TCLocalize("Text size"), 12, 120, &CUi::ms_LinearScrollbarScale, 0, "u");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcLyricsLines, &g_Config.m_TcLyricsLines, &Button, TCLocalize("Lines on screen"), 1, 6, &CUi::ms_LinearScrollbarScale, 0, nullptr);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcLyricsExtrusion, &g_Config.m_TcLyricsExtrusion, &Button, TCLocalize("3D depth"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcLyricsTilt, &g_Config.m_TcLyricsTilt, &Button, TCLocalize("Random tilt"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcLyricsDrift, &g_Config.m_TcLyricsDrift, &Button, TCLocalize("Drift speed"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcLyricsClean, TCLocalize("Plain text (no effects)"), &g_Config.m_TcLyricsClean, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcLyricsGlitter, TCLocalize("Gold glitter"), &g_Config.m_TcLyricsGlitter, &Column, LineSize);
		static CButtonContainer s_LyricsColorId;
		DoLine_ColorPicker(&s_LyricsColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Gold color"), &g_Config.m_TcLyricsColor, ColorRGBA(1.0f, 0.78f, 0.25f, 1.0f), false);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcLyricsBeat, TCLocalize("Beat pulse"), &g_Config.m_TcLyricsBeat, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcLyricsOffset, &g_Config.m_TcLyricsOffset, &Button, TCLocalize("Timing offset"), -5000, 5000, &CUi::ms_LinearScrollbarScale, 0, "ms");
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	if(SectionEnabled(true, pSettingsSearch, {"vignette", "blur", "edge", "screen", "color"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Vignette"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcVignette, TCLocalize("Enable custom vignette"), &g_Config.m_TcVignette, &Column, LineSize);
	if(g_Config.m_TcVignette)
	{
		{
			static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}};
			int Value = g_Config.m_TcVignetteMode;
			if(DoLine_RadioMenu(Column, TCLocalize("Position"),
				   s_vButtonContainers,
				   {TCLocalize("All edges"), TCLocalize("Horizontal"), TCLocalize("Vertical")},
				   {0, 1, 2},
				   Value))
			{
				g_Config.m_TcVignetteMode = Value;
			}
		}

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcVignetteRadius, &g_Config.m_TcVignetteRadius, &Button, TCLocalize("Radius"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcVignetteStrength, &g_Config.m_TcVignetteStrength, &Button, TCLocalize("Strength"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcVignetteSmoothness, &g_Config.m_TcVignetteSmoothness, &Button, TCLocalize("Smoothness"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcVignetteDarken, &g_Config.m_TcVignetteDarken, &Button, TCLocalize("Darken zones"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		static CButtonContainer s_VignetteColorId;
		DoLine_ColorPicker(&s_VignetteColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Zone color"), &g_Config.m_TcVignetteColor, ColorRGBA(0.0f, 0.0f, 0.0f), false);
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"gradient", "nameplate", "clan", "skin", "chat", "message"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Gradient"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplateGradient, TCLocalize("Gradient player nicknames"), &g_Config.m_TcNameplateGradient, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplateGradientClan, TCLocalize("Gradient player clan tags"), &g_Config.m_TcNameplateGradientClan, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplateGradientEverything, TCLocalize("Apply gradient to all rendered text"), &g_Config.m_TcNameplateGradientEverything, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcNameplateGradientAnimateSpeed, &g_Config.m_TcNameplateGradientAnimateSpeed, &Button, TCLocalize("Gradient animate speed"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcGradientChat, TCLocalize("Gradient chat"), &g_Config.m_TcGradientChat, &Column, LineSize);
	if(g_Config.m_TcGradientChat)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcGradientChatNicknames, TCLocalize("Gradient nicknames"), &g_Config.m_TcGradientChatNicknames, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcGradientChatMessages, TCLocalize("Gradient messages"), &g_Config.m_TcGradientChatMessages, &Column, LineSize);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	if(SectionEnabled(true, pSettingsSearch, {"swap", "timer"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Swap Timer"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcEnableSwapTimer, TCLocalize("Enable swap timer"), &g_Config.m_TcEnableSwapTimer, &Column, LineSize);
	if(g_Config.m_TcEnableSwapTimer)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcEnableSwapTimerOnLeftSide, TCLocalize("Swap timer on left side"), &g_Config.m_TcEnableSwapTimerOnLeftSide, &Column, LineSize);

		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"dynamic", "camera", "zoom", "speed"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Dynamic Camera"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	const bool CameraAllowed = GameClient()->IsCameraAllowedServer();
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDynamicCamera, TCLocalize("Zoom based on your speed"), &g_Config.m_TcDynamicCamera, &Column, LineSize);
	if(!CameraAllowed)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoLabel(&Button, TCLocalize("Only allowed on listed server types"), FontSize, TEXTALIGN_ML);
	}
	if(g_Config.m_TcDynamicCamera)
	{
		{
			static std::vector<const char *> s_DynamicCameraModeNames;
			s_DynamicCameraModeNames = {TCLocalize("Zoom out with speed"), TCLocalize("Zoom in with speed")};
			static CUi::SDropDownState s_DynamicCameraModeState;
			static CScrollRegion s_DynamicCameraModeScroll;
			s_DynamicCameraModeState.m_SelectionPopupContext.m_pScrollRegion = &s_DynamicCameraModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Mode"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcDynamicCameraMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcDynamicCameraMode, s_DynamicCameraModeNames.data(), s_DynamicCameraModeNames.size(), s_DynamicCameraModeState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		{
			static std::vector<const char *> s_DynamicCameraCurveNames;
			s_DynamicCameraCurveNames = {TCLocalize("Linear"), TCLocalize("Smooth"), TCLocalize("Exponential")};
			static CUi::SDropDownState s_DynamicCameraCurveState;
			static CScrollRegion s_DynamicCameraCurveScroll;
			s_DynamicCameraCurveState.m_SelectionPopupContext.m_pScrollRegion = &s_DynamicCameraCurveScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Response curve"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcDynamicCameraCurve = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcDynamicCameraCurve, s_DynamicCameraCurveNames.data(), s_DynamicCameraCurveNames.size(), s_DynamicCameraCurveState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDynamicCameraStrength, &g_Config.m_TcDynamicCameraStrength, &Button, TCLocalize("Strength"), 0, 150, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDynamicCameraSpeedMin, &g_Config.m_TcDynamicCameraSpeedMin, &Button, TCLocalize("Reaction speed"), 0, 4000, &CUi::ms_LinearScrollbarScale, 0, " units/s");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDynamicCameraSpeedMax, &g_Config.m_TcDynamicCameraSpeedMax, &Button, TCLocalize("Full effect speed"), 10, 4000, &CUi::ms_LinearScrollbarScale, 0, " units/s");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDynamicCameraSmooth, &g_Config.m_TcDynamicCameraSmooth, &Button, TCLocalize("Smoothness"), 0, 100);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDynamicCameraLimit, &g_Config.m_TcDynamicCameraLimit, &Button, TCLocalize("Zoom out limit (0 = off)"), 0, 1000, &CUi::ms_LinearScrollbarScale, 0, "%");
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDynamicCameraOthers, TCLocalize("Also when spectating others"), &g_Config.m_TcDynamicCameraOthers, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDynamicCameraFreeze, TCLocalize("Also while frozen"), &g_Config.m_TcDynamicCameraFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDynamicCameraHook, TCLocalize("Also while hooking"), &g_Config.m_TcDynamicCameraHook, &Column, LineSize);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"drift", "camera", "speed"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Drift Camera"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	const bool CameraAllowed = GameClient()->IsCameraAllowedServer();
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDriftCamera, TCLocalize("Camera drifts with your speed"), &g_Config.m_TcDriftCamera, &Column, LineSize);
	if(!CameraAllowed)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoLabel(&Button, TCLocalize("Only allowed on listed server types"), FontSize, TEXTALIGN_ML);
	}
	if(g_Config.m_TcDriftCamera)
	{
		{
			static std::vector<const char *> s_DriftCameraModeNames;
			s_DriftCameraModeNames = {TCLocalize("Lead ahead of movement"), TCLocalize("Lag behind movement")};
			static CUi::SDropDownState s_DriftCameraModeState;
			static CScrollRegion s_DriftCameraModeScroll;
			s_DriftCameraModeState.m_SelectionPopupContext.m_pScrollRegion = &s_DriftCameraModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Direction"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcDriftCameraMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcDriftCameraMode, s_DriftCameraModeNames.data(), s_DriftCameraModeNames.size(), s_DriftCameraModeState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		{
			static std::vector<const char *> s_DriftCameraAxisNames;
			s_DriftCameraAxisNames = {TCLocalize("Movement vector"), TCLocalize("Horizontal only"), TCLocalize("Vertical only")};
			static CUi::SDropDownState s_DriftCameraAxisState;
			static CScrollRegion s_DriftCameraAxisScroll;
			s_DriftCameraAxisState.m_SelectionPopupContext.m_pScrollRegion = &s_DriftCameraAxisScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Axis"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcDriftCameraAxis = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcDriftCameraAxis, s_DriftCameraAxisNames.data(), s_DriftCameraAxisNames.size(), s_DriftCameraAxisState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDriftCameraStrength, &g_Config.m_TcDriftCameraStrength, &Button, TCLocalize("Strength"), 0, 400, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDriftCameraMaxOffset, &g_Config.m_TcDriftCameraMaxOffset, &Button, TCLocalize("Maximum offset"), 0, 600, &CUi::ms_LinearScrollbarScale, 0, " units");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDriftCameraSpeedRef, &g_Config.m_TcDriftCameraSpeedRef, &Button, TCLocalize("Speed for max offset"), 10, 4000, &CUi::ms_LinearScrollbarScale, 0, " units/s");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcDriftCameraSmooth, &g_Config.m_TcDriftCameraSmooth, &Button, TCLocalize("Smoothness"), 0, 100);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDriftCameraOthers, TCLocalize("Also when spectating others"), &g_Config.m_TcDriftCameraOthers, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDriftCameraFreeze, TCLocalize("Also while frozen"), &g_Config.m_TcDriftCameraFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDriftCameraHook, TCLocalize("Also while hooking"), &g_Config.m_TcDriftCameraHook, &Column, LineSize);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"tilt", "camera", "roll", "speed"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Camera Tilt"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	const bool CameraAllowed = GameClient()->IsCameraAllowedServer();
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcCameraTilt, TCLocalize("Tilt the view with your speed"), &g_Config.m_TcCameraTilt, &Column, LineSize);
	if(!CameraAllowed)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoLabel(&Button, TCLocalize("Only allowed on listed server types"), FontSize, TEXTALIGN_ML);
	}
	if(g_Config.m_TcCameraTilt)
	{
		{
			static std::vector<const char *> s_CameraTiltModeNames;
			s_CameraTiltModeNames = {TCLocalize("Horizontal speed"), TCLocalize("Movement direction"), TCLocalize("Movement direction inverted")};
			static CUi::SDropDownState s_CameraTiltModeState;
			static CScrollRegion s_CameraTiltModeScroll;
			s_CameraTiltModeState.m_SelectionPopupContext.m_pScrollRegion = &s_CameraTiltModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Axis"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcCameraTiltMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcCameraTiltMode, s_CameraTiltModeNames.data(), s_CameraTiltModeNames.size(), s_CameraTiltModeState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		{
			static std::vector<const char *> s_CameraTiltCurveNames;
			s_CameraTiltCurveNames = {TCLocalize("Linear"), TCLocalize("Smooth"), TCLocalize("Exponential")};
			static CUi::SDropDownState s_CameraTiltCurveState;
			static CScrollRegion s_CameraTiltCurveScroll;
			s_CameraTiltCurveState.m_SelectionPopupContext.m_pScrollRegion = &s_CameraTiltCurveScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(130.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Curve"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcCameraTiltCurve = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcCameraTiltCurve, s_CameraTiltCurveNames.data(), s_CameraTiltCurveNames.size(), s_CameraTiltCurveState);
		}
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcCameraTiltStrength, &g_Config.m_TcCameraTiltStrength, &Button, TCLocalize("Strength"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcCameraTiltMaxAngle, &g_Config.m_TcCameraTiltMaxAngle, &Button, TCLocalize("Maximum angle"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, " x0.1 deg");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcCameraTiltSpeedRef, &g_Config.m_TcCameraTiltSpeedRef, &Button, TCLocalize("Speed for max angle"), 10, 4000, &CUi::ms_LinearScrollbarScale, 0, " units/s");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcCameraTiltSmooth, &g_Config.m_TcCameraTiltSmooth, &Button, TCLocalize("Smoothness"), 0, 100);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcCameraTiltFit, TCLocalize("Zoom out so the screen stays covered"), &g_Config.m_TcCameraTiltFit, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcCameraTiltOthers, TCLocalize("Also when spectating others"), &g_Config.m_TcCameraTiltOthers, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcCameraTiltFreeze, TCLocalize("Also while frozen"), &g_Config.m_TcCameraTiltFreeze, &Column, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcCameraTiltHook, TCLocalize("Also while hooking"), &g_Config.m_TcCameraTiltHook, &Column, LineSize);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"laser", "glow", "rifle"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Laser Glow"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcLaserGlow, TCLocalize("Enable"), &g_Config.m_TcLaserGlow, &Column, LineSize);
	if(g_Config.m_TcLaserGlow)
	{
		{
			static std::vector<const char *> s_LaserGlowModeNames;
			s_LaserGlowModeNames = {TCLocalize("Classic"), TCLocalize("Prism"), TCLocalize("Pulse"), TCLocalize("Crystal"), TCLocalize("Plasma"), TCLocalize("Aurora")};
			static CUi::SDropDownState s_LaserGlowModeState;
			static CScrollRegion s_LaserGlowModeScroll;
			s_LaserGlowModeState.m_SelectionPopupContext.m_pScrollRegion = &s_LaserGlowModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(110.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Mode"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcLaserGlowMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcLaserGlowMode, s_LaserGlowModeNames.data(), s_LaserGlowModeNames.size(), s_LaserGlowModeState);
		}
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcLaserGlowPower, &g_Config.m_TcLaserGlowPower, &Button, TCLocalize("Power"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_LaserGlowColorId;
		DoLine_ColorPicker(&s_LaserGlowColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Color"), &g_Config.m_TcLaserGlowColor, ColorRGBA(0.55f, 0.45f, 1.0f, 1.0f), false);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"shotgun", "glow"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Shotgun Glow"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShotgunGlow, TCLocalize("Enable"), &g_Config.m_TcShotgunGlow, &Column, LineSize);
	if(g_Config.m_TcShotgunGlow)
	{
		{
			static std::vector<const char *> s_ShotgunGlowModeNames;
			s_ShotgunGlowModeNames = {TCLocalize("Classic"), TCLocalize("Prism"), TCLocalize("Pulse"), TCLocalize("Crystal"), TCLocalize("Plasma"), TCLocalize("Aurora")};
			static CUi::SDropDownState s_ShotgunGlowModeState;
			static CScrollRegion s_ShotgunGlowModeScroll;
			s_ShotgunGlowModeState.m_SelectionPopupContext.m_pScrollRegion = &s_ShotgunGlowModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(110.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Mode"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcShotgunGlowMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcShotgunGlowMode, s_ShotgunGlowModeNames.data(), s_ShotgunGlowModeNames.size(), s_ShotgunGlowModeState);
		}
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcShotgunGlowPower, &g_Config.m_TcShotgunGlowPower, &Button, TCLocalize("Power"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_ShotgunGlowColorId;
		DoLine_ColorPicker(&s_ShotgunGlowColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Color"), &g_Config.m_TcShotgunGlowColor, ColorRGBA(0.55f, 0.45f, 1.0f, 1.0f), false);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"rocket", "grenade", "glow"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Rocket Glow"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRocketGlow, TCLocalize("Enable"), &g_Config.m_TcRocketGlow, &Column, LineSize);
	if(g_Config.m_TcRocketGlow)
	{
		{
			static std::vector<const char *> s_RocketGlowModeNames;
			s_RocketGlowModeNames = {TCLocalize("Classic"), TCLocalize("Prism"), TCLocalize("Pulse"), TCLocalize("Crystal")};
			static CUi::SDropDownState s_RocketGlowModeState;
			static CScrollRegion s_RocketGlowModeScroll;
			s_RocketGlowModeState.m_SelectionPopupContext.m_pScrollRegion = &s_RocketGlowModeScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(110.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Mode"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcRocketGlowMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcRocketGlowMode, s_RocketGlowModeNames.data(), s_RocketGlowModeNames.size(), s_RocketGlowModeState);
		}
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcRocketGlowPower, &g_Config.m_TcRocketGlowPower, &Button, TCLocalize("Power"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_RocketGlowColorId;
		DoLine_ColorPicker(&s_RocketGlowColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Color"), &g_Config.m_TcRocketGlowColor, ColorRGBA(1.0f, 0.55f, 0.25f, 1.0f), false);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}
	if(SectionEnabled(true, pSettingsSearch, {"freeze", "lightning", "frost", "strike"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Freeze Lightning"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFreezeLightning, TCLocalize("Enable"), &g_Config.m_TcFreezeLightning, &Column, LineSize);
	if(g_Config.m_TcFreezeLightning)
	{
		{
			static std::vector<const char *> s_FreezeStyleNames;
			s_FreezeStyleNames = {TCLocalize("Lightning"), TCLocalize("Frost nova"), TCLocalize("Ice shards")};
			static CUi::SDropDownState s_FreezeStyleState;
			static CScrollRegion s_FreezeStyleScroll;
			s_FreezeStyleState.m_SelectionPopupContext.m_pScrollRegion = &s_FreezeStyleScroll;
			CUIRect DropDownRect;
			Column.HSplitTop(LineSize, &DropDownRect, &Column);
			DropDownRect.VSplitLeft(110.0f, &Label, &DropDownRect);
			Ui()->DoLabel(&Label, TCLocalize("Style"), FontSize, TEXTALIGN_ML);
			g_Config.m_TcFreezeLightningStyle = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcFreezeLightningStyle, s_FreezeStyleNames.data(), s_FreezeStyleNames.size(), s_FreezeStyleState);
		}
		if(g_Config.m_TcFreezeLightningStyle == 0)
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcFreezeLightningHeight, &g_Config.m_TcFreezeLightningHeight, &Button, TCLocalize("Height"), 100, 900, &CUi::ms_LinearScrollbarScale, 0, "");
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcFreezeLightningThickness, &g_Config.m_TcFreezeLightningThickness, &Button, TCLocalize("Thickness"), 50, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcFreezeLightningBranches, &g_Config.m_TcFreezeLightningBranches, &Button, TCLocalize("Branches"), 0, 8);
		}
		else
		{
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcFreezeLightningSize, &g_Config.m_TcFreezeLightningSize, &Button, TCLocalize("Size"), 50, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
			Column.HSplitTop(LineSize, &Button, &Column);
			Ui()->DoScrollbarOption(&g_Config.m_TcFreezeLightningBranches, &g_Config.m_TcFreezeLightningBranches, &Button, TCLocalize("Shards"), 0, 8);
		}
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFreezeLightningImpact, TCLocalize("Impact flash and sparks"), &g_Config.m_TcFreezeLightningImpact, &Column, LineSize);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcFreezeLightningShake, &g_Config.m_TcFreezeLightningShake, &Button, TCLocalize("Screen shake"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		static CButtonContainer s_FreezeLightningColorId;
		DoLine_ColorPicker(&s_FreezeLightningColorId, 25.0f, 13.0f, 5.0f, &Column, TCLocalize("Color"), &g_Config.m_TcFreezeLightningColor, ColorRGBA(0.35f, 0.65f, 1.0f, 1.0f), false);

		static CButtonContainer s_FreezeLightningTestId;
		Column.HSplitTop(LineSize, &Button, &Column);
		if(Ui()->DoButtonLogic(&s_FreezeLightningTestId, false, &Button, BUTTONFLAG_LEFT))
		{
			GameClient()->m_Lightning.Strike(GameClient()->m_LocalCharacterPos, 1.0f, true);
			if(g_Config.m_TcFreezeLightningShake > 0)
				GameClient()->m_Lightning.AddShake(g_Config.m_TcFreezeLightningShake / 100.0f * 14.0f);
		}
		Ui()->DoLabel(&Button, TCLocalize("Preview"), FontSize, TEXTALIGN_MC);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	}

	// ***** Translate ***** //
	if(SectionEnabled(false, pSettingsSearch, {"translate", "translation", "language", "backend", "libretranslate"}))
	{
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Translate"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	// Current backend's language table -- Google and LibreTranslate genuinely
	// support different language sets (see translate.h/.cpp's
	// g_aTranslateBackends), so the dropdowns below are backend-scoped
	// rather than one shared list.
	int CurrentBackendIndex = 0;
	for(size_t i = 0; i < g_aTranslateBackends.size(); i++)
	{
		if(str_comp_nocase(g_Config.m_TcTranslateBackend, g_aTranslateBackends[i].m_pValue) == 0)
			CurrentBackendIndex = (int)i;
	}
	const STranslateBackendInfo &CurrentBackend = g_aTranslateBackends[CurrentBackendIndex];

	{
		static std::vector<const char *> s_TranslateBackendNames;
		s_TranslateBackendNames.clear();
		for(const STranslateBackendInfo &Backend : g_aTranslateBackends)
			s_TranslateBackendNames.push_back(Backend.m_pName);

		static CUi::SDropDownState s_TranslateBackendDropDownState;
		static CScrollRegion s_TranslateBackendDropDownScrollRegion;
		s_TranslateBackendDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_TranslateBackendDropDownScrollRegion;
		CUIRect BackendDropDownRect;
		Column.HSplitTop(LineSize, &BackendDropDownRect, &Column);
		BackendDropDownRect.VSplitLeft(120.0f, &Label, &BackendDropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Backend: "), FontSize, TEXTALIGN_ML);
		const int BackendSelectedNew = Ui()->DoDropDown(&BackendDropDownRect, CurrentBackendIndex, s_TranslateBackendNames.data(), s_TranslateBackendNames.size(), s_TranslateBackendDropDownState);
		if(BackendSelectedNew != CurrentBackendIndex)
			str_copy(g_Config.m_TcTranslateBackend, g_aTranslateBackends[BackendSelectedNew].m_pValue);
	}
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTranslateAuto, TCLocalize("Automatically translate incoming messages"), &g_Config.m_TcTranslateAuto, &Column, LineSize);
	if(g_Config.m_TcTranslateAuto)
	{
		CUIRect IncomingTargetBox;
		Column.HSplitTop(LineSize + MarginExtraSmall, &IncomingTargetBox, &Column);
		IncomingTargetBox.HSplitTop(MarginExtraSmall, nullptr, &IncomingTargetBox);
		IncomingTargetBox.VSplitMid(&Label, &IncomingTargetBox);
		Ui()->DoLabel(&Label, TCLocalize("Translate incoming to:"), FontSize, TEXTALIGN_ML);
		static CUi::SDropDownState s_IncomingLangDropDownState;
		static CScrollRegion s_IncomingLangDropDownScrollRegion;
		s_IncomingLangDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_IncomingLangDropDownScrollRegion;
		static std::vector<const char *> s_IncomingLangNames;
		s_IncomingLangNames.clear();
		for(const STranslateLanguage &Language : CurrentBackend.m_vLanguages)
			s_IncomingLangNames.push_back(Language.m_pName);
		const int IncomingLangOld = TranslateLanguageIndex(CurrentBackend.m_vLanguages, g_Config.m_TcTranslateTarget);
		const int IncomingLangNew = Ui()->DoDropDown(&IncomingTargetBox, IncomingLangOld, s_IncomingLangNames.data(), s_IncomingLangNames.size(), s_IncomingLangDropDownState);
		if(IncomingLangOld != IncomingLangNew)
			str_copy(g_Config.m_TcTranslateTarget, CurrentBackend.m_vLanguages[IncomingLangNew].m_pCode);
	}
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTranslateOutgoing, TCLocalize("Translate your outgoing messages"), &g_Config.m_TcTranslateOutgoing, &Column, LineSize);
	if(g_Config.m_TcTranslateOutgoing)
	{
		CUIRect OutgoingTargetBox;
		Column.HSplitTop(LineSize + MarginExtraSmall, &OutgoingTargetBox, &Column);
		OutgoingTargetBox.HSplitTop(MarginExtraSmall, nullptr, &OutgoingTargetBox);
		OutgoingTargetBox.VSplitMid(&Label, &OutgoingTargetBox);
		Ui()->DoLabel(&Label, TCLocalize("Translate outgoing to:"), FontSize, TEXTALIGN_ML);
		static CUi::SDropDownState s_OutgoingLangDropDownState;
		static CScrollRegion s_OutgoingLangDropDownScrollRegion;
		s_OutgoingLangDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_OutgoingLangDropDownScrollRegion;
		static std::vector<const char *> s_OutgoingLangNames;
		s_OutgoingLangNames.clear();
		for(const STranslateLanguage &Language : CurrentBackend.m_vLanguages)
			s_OutgoingLangNames.push_back(Language.m_pName);
		const int OutgoingLangOld = TranslateLanguageIndex(CurrentBackend.m_vLanguages, g_Config.m_TcTranslateOutgoingTarget);
		const int OutgoingLangNew = Ui()->DoDropDown(&OutgoingTargetBox, OutgoingLangOld, s_OutgoingLangNames.data(), s_OutgoingLangNames.size(), s_OutgoingLangDropDownState);
		if(OutgoingLangOld != OutgoingLangNew)
			str_copy(g_Config.m_TcTranslateOutgoingTarget, CurrentBackend.m_vLanguages[OutgoingLangNew].m_pCode);
	}
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	if(CurrentBackend.m_NeedsEndpointConfig)
	{
		CUIRect EndpointBox;
		Column.HSplitTop(LineSize + MarginExtraSmall, &EndpointBox, &Column);
		EndpointBox.VSplitMid(&Label, &EndpointBox);
		Ui()->DoLabel(&Label, TCLocalize("LibreTranslate endpoint:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_TranslateEndpoint(g_Config.m_TcTranslateEndpoint, sizeof(g_Config.m_TcTranslateEndpoint));
		s_TranslateEndpoint.SetEmptyText("localhost:5000/translate");
		Ui()->DoEditBox(&s_TranslateEndpoint, &EndpointBox, EditBoxFontSize);
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

		CUIRect KeyBox;
		Column.HSplitTop(LineSize + MarginExtraSmall, &KeyBox, &Column);
		KeyBox.VSplitMid(&Label, &KeyBox);
		Ui()->DoLabel(&Label, TCLocalize("LibreTranslate API key:"), FontSize, TEXTALIGN_ML);
		static CLineInput s_TranslateKey(g_Config.m_TcTranslateKey, sizeof(g_Config.m_TcTranslateKey));
		s_TranslateKey.SetEmptyText(TCLocalize("Optional"));
		Ui()->DoEditBox(&s_TranslateKey, &KeyBox, EditBoxFontSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	}

	// ***** END OF PAGE 1 SETTINGS ***** //
	RightView = Column;

	// Scroll
	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = std::max(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsTClientBindWheel(CUIRect MainView)
{
	CUIRect LeftView, RightView, Label, Button;
	MainView.VSplitLeft(MainView.w / 2.1f, &LeftView, &RightView);

	const float Radius = std::min(RightView.w, RightView.h) / 2.0f;
	vec2 Center = RightView.Center();
	// Draw Circle
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	Graphics()->DrawCircle(Center.x, Center.y, Radius, 64);
	Graphics()->QuadsEnd();

	static char s_aBindName[BINDWHEEL_MAX_NAME];
	static char s_aBindCommand[BINDWHEEL_MAX_CMD];

	static int s_SelectedBindIndex = -1;
	int HoveringIndex = -1;

	float MouseDist = distance(Center, Ui()->MousePos());
	const int SegmentCount = GameClient()->m_BindWheel.m_vBinds.size();
	if(MouseDist < Radius && MouseDist > Radius * 0.25f && SegmentCount > 0)
	{
		float SegmentAngle = 2.0f * pi / SegmentCount;

		float HoveringAngle = angle(Ui()->MousePos() - Center) + SegmentAngle / 2.0f;
		if(HoveringAngle < 0.0f)
			HoveringAngle += 2.0f * pi;

		HoveringIndex = (int)(HoveringAngle / (2.0f * pi) * SegmentCount);
		HoveringIndex = std::clamp(HoveringIndex, 0, SegmentCount - 1);
		if(Ui()->MouseButtonClicked(0))
		{
			s_SelectedBindIndex = HoveringIndex;
			str_copy(s_aBindName, GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aName);
			str_copy(s_aBindCommand, GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(1) && s_SelectedBindIndex >= 0 && HoveringIndex >= 0 && HoveringIndex != s_SelectedBindIndex)
		{
			CBindWheel::CBind BindA = GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex];
			CBindWheel::CBind BindB = GameClient()->m_BindWheel.m_vBinds[HoveringIndex];
			str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aName, BindB.m_aName);
			str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aCommand, BindB.m_aCommand);
			str_copy(GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aName, BindA.m_aName);
			str_copy(GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aCommand, BindA.m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(2))
		{
			s_SelectedBindIndex = HoveringIndex;
		}
	}
	else if(MouseDist < Radius && Ui()->MouseButtonClicked(0))
	{
		s_SelectedBindIndex = -1;
		str_copy(s_aBindName, "");
		str_copy(s_aBindCommand, "");
	}

	const float Theta = pi * 2.0f / std::max<float>(1.0f, GameClient()->m_BindWheel.m_vBinds.size());
	for(int i = 0; i < static_cast<int>(GameClient()->m_BindWheel.m_vBinds.size()); i++)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

		float SegmentFontSize = FontSize * 1.1f;
		if(i == s_SelectedBindIndex)
		{
			SegmentFontSize = FontSize * 1.7f;
			TextRender()->TextColor(ColorRGBA(0.5f, 1.0f, 0.75f, 1.0f));
		}
		else if(i == HoveringIndex)
		{
			SegmentFontSize = FontSize * 1.35f;
		}

		const CBindWheel::CBind Bind = GameClient()->m_BindWheel.m_vBinds[i];
		const float Angle = Theta * i;

		const vec2 Pos = direction(Angle) * (Radius * 0.75f) + Center;
		const CUIRect Rect = CUIRect{Pos.x - 50.0f, Pos.y - 50.0f, 100.0f, 100.0f};
		Ui()->DoLabel(&Rect, Bind.m_aName, SegmentFontSize, TEXTALIGN_MC);
	}

	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Name:"), FontSize, TEXTALIGN_ML);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
	s_NameInput.SetEmptyText(TCLocalize("Name"));
	Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Command:"), FontSize, TEXTALIGN_ML);
	static CLineInput s_BindInput;
	s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
	s_BindInput.SetEmptyText(TCLocalize("Command"));
	Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize);

	static CButtonContainer s_AddButton, s_RemoveButton, s_OverrideButton;

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	if(DoButton_Menu(&s_OverrideButton, TCLocalize("Override Selected"), 0, &Button) && s_SelectedBindIndex >= 0 && s_SelectedBindIndex < static_cast<int>(GameClient()->m_BindWheel.m_vBinds.size()))
	{
		CBindWheel::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aName, TempBind.m_aName);
		str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aCommand, s_aBindCommand);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	CUIRect ButtonAdd, ButtonRemove;
	Button.VSplitMid(&ButtonRemove, &ButtonAdd, MarginSmall);
	if(DoButton_Menu(&s_AddButton, TCLocalize("Add Bind"), 0, &ButtonAdd))
	{
		CBindWheel::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		GameClient()->m_BindWheel.AddBind(TempBind.m_aName, s_aBindCommand);
		s_SelectedBindIndex = static_cast<int>(GameClient()->m_BindWheel.m_vBinds.size()) - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, TCLocalize("Remove Bind"), 0, &ButtonRemove) && s_SelectedBindIndex >= 0)
	{
		GameClient()->m_BindWheel.RemoveBind(s_SelectedBindIndex);
		s_SelectedBindIndex = -1;
	}

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("The command is ran in console not chat"), FontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize * 0.8f, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Use left mouse to select"), FontSize * 0.8f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize * 0.8f, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Use right mouse to swap with selected"), FontSize * 0.8f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize * 0.8f, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Use middle mouse select without copy"), FontSize * 0.8f, TEXTALIGN_ML);

	LeftView.HSplitBottom(LineSize, &LeftView, &Label);
	static CButtonContainer s_ReaderButtonWheel, s_ClearButtonWheel;
	DoLine_KeyReader(Label, s_ReaderButtonWheel, s_ClearButtonWheel, TCLocalize("Bind Wheel Key"), "+bindwheel");

	LeftView.HSplitBottom(LineSize, &LeftView, &Label);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcResetBindWheelMouse, TCLocalize("Reset position of mouse when opening bindwheel"), &g_Config.m_TcResetBindWheelMouse, &Label, LineSize);
}

void CMenus::RenderSettingsTClientChatBinds(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_ForceShowScrollbar = true;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	const float ScrollOffset = MainView.y;
	static float s_PrevScrollOffset = 0.0f;

	MainView.HSplitTop(Margin, nullptr, &MainView);
	MainView.VSplitRight(5.0f, &MainView, nullptr); // Padding for scrollbar
	MainView.VSplitLeft(5.0f, nullptr, &MainView); // Padding for scrollbar

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset - ScrollOffset;
		Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	// ***** All the stuff ***** //

	auto DoBindchatDefault = [&](CUIRect &Column, CBindChat::CBindDefault &BindDefault) {
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		CBindChat::CBind *pOldBind = GameClient()->m_BindChat.GetBind(BindDefault.m_Bind.m_aCommand);
		static char s_aTempName[BINDCHAT_MAX_NAME] = "";
		char *pName;
		if(pOldBind == nullptr)
			pName = s_aTempName;
		else
			pName = pOldBind->m_aName;
		if(DoEditBoxWithLabel(&BindDefault.m_LineInput, &Button, TCLocalize(BindDefault.m_pTitle), BindDefault.m_Bind.m_aName, pName, BINDCHAT_MAX_NAME) && BindDefault.m_LineInput.IsActive())
		{
			if(!pOldBind && pName[0] != '\0')
			{
				auto BindNew = BindDefault.m_Bind;
				str_copy(BindNew.m_aName, pName);
				GameClient()->m_BindChat.RemoveBind(pName); // Prevent duplicates
				GameClient()->m_BindChat.AddBind(BindNew);
				s_aTempName[0] = '\0';
			}
			if(pOldBind && pName[0] == '\0')
			{
				GameClient()->m_BindChat.RemoveBind(pName);
			}
		}
	};

	auto DoBindchatDefaults = [&](CUIRect &Column, const char *pTitle, std::vector<CBindChat::CBindDefault> &vBindchatDefaults) {
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, pTitle, HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		for(CBindChat::CBindDefault &BindchatDefault : vBindchatDefaults)
			DoBindchatDefault(Column, BindchatDefault);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	};

	float SizeL = 0.0f, SizeR = 0.0f;
	for(auto &[pTitle, vBindDefaults] : CBindChat::BIND_DEFAULTS)
	{
		float &Size = SizeL > SizeR ? SizeR : SizeL;
		CUIRect &Column = SizeL > SizeR ? RightView : LeftView;
		DoBindchatDefaults(Column, TCLocalize(pTitle), vBindDefaults);
		Size += vBindDefaults.size() * (MarginSmall + LineSize) + HeadlineHeight + HeadlineFontSize + MarginSmall * 2.0f;
	}

	// Scroll
	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = std::max(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsTClientWarList(CUIRect MainView)
{
	CUIRect RightView, LeftView, Column1, Column2, Column3, Column4, Button, ButtonL, ButtonR, Label;

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, Margin);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	// WAR LIST will have 4 columns
	//  [War entries] - [Entry Editing] - [Group Types] - [Recent Players]
	//									 [Group Editing]

	// putting this here so it can be updated by the entry list
	static char s_aEntryName[MAX_NAME_LENGTH];
	static char s_aEntryClan[MAX_CLAN_LENGTH];
	static char s_aEntryReason[MAX_WARLIST_REASON_LENGTH];
	static bool s_IsClan = false;
	static bool s_IsName = true;

	LeftView.VSplitMid(&Column1, &Column2, Margin);
	RightView.VSplitMid(&Column3, &Column4, Margin);

	// ======WAR ENTRIES======
	static CWarEntry *s_pSelectedEntry = nullptr;
	static CWarType *s_pSelectedType = GameClient()->m_WarList.m_WarTypes[0];
	{
		Column1.HSplitTop(HeadlineHeight, &Label, &Column1);
		Label.VSplitRight(25.0f, &Label, &Button);
		Ui()->DoLabel(&Label, TCLocalize("War Entries"), HeadlineFontSize, TEXTALIGN_ML);
		Column1.HSplitTop(MarginSmall, nullptr, &Column1);

		static CButtonContainer s_ReverseEntries;
		static bool s_Reversed = true;
		if(Ui()->DoButton_FontIcon(&s_ReverseEntries, s_Reversed ? FontIcon::CHEVRON_UP : FontIcon::CHEVRON_DOWN, 0, &Button, IGraphics::CORNER_ALL))
		{
			s_Reversed = !s_Reversed;
		}

		CUIRect EntriesSearch;
		Column1.HSplitBottom(25.0f, &Column1, &EntriesSearch);
		EntriesSearch.HSplitTop(MarginSmall, nullptr, &EntriesSearch);

		// Filter the list
		static CLineInputBuffered<128> s_EntriesFilterInput;
		std::vector<CWarEntry *> vpFilteredEntries;
		for(CWarEntry &Entry : GameClient()->m_WarList.m_vWarEntries)
		{
			if(str_find_nocase(Entry.m_aName, s_EntriesFilterInput.GetString()))
				vpFilteredEntries.push_back(&Entry);
			else if(str_find_nocase(Entry.m_aClan, s_EntriesFilterInput.GetString()))
				vpFilteredEntries.push_back(&Entry);
			else if(str_find_nocase(Entry.m_pWarType->m_aWarName, s_EntriesFilterInput.GetString()))
				vpFilteredEntries.push_back(&Entry);
		}
		if(s_Reversed)
			std::reverse(vpFilteredEntries.begin(), vpFilteredEntries.end());

		int SelectedOldEntry = -1;
		static CListBox s_EntriesListBox;
		s_EntriesListBox.DoStart(35.0f, vpFilteredEntries.size(), 1, 2, SelectedOldEntry, &Column1);

		static std::vector<unsigned char> s_vItemIds;
		static std::vector<CButtonContainer> s_vDeleteButtons;

		const int MaxEntries = GameClient()->m_WarList.m_vWarEntries.size();
		s_vItemIds.resize(MaxEntries);
		s_vDeleteButtons.resize(MaxEntries);

		for(size_t i = 0; i < vpFilteredEntries.size(); i++)
		{
			CWarEntry *pEntry = vpFilteredEntries[i];

			if(s_pSelectedEntry && pEntry == s_pSelectedEntry)
				SelectedOldEntry = i;

			const CListboxItem Item = s_EntriesListBox.DoNextItem(&s_vItemIds[i], SelectedOldEntry >= 0 && (size_t)SelectedOldEntry == i);
			if(!Item.m_Visible)
				continue;

			CUIRect EntryRect, DeleteButton, EntryTypeRect, WarType, ToolTip;
			Item.m_Rect.Margin(0.0f, &EntryRect);
			EntryRect.VSplitLeft(26.0f, &DeleteButton, &EntryRect);
			DeleteButton.HMargin(7.5f, &DeleteButton);
			DeleteButton.VSplitLeft(MarginSmall, nullptr, &DeleteButton);
			DeleteButton.VSplitRight(MarginExtraSmall, &DeleteButton, nullptr);

			if(Ui()->DoButton_FontIcon(&s_vDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				GameClient()->m_WarList.RemoveWarEntry(pEntry);

			bool IsClan = false;
			char aBuf[32];
			if(str_comp(pEntry->m_aClan, "") != 0)
			{
				str_copy(aBuf, pEntry->m_aClan);
				IsClan = true;
			}
			else
			{
				str_copy(aBuf, pEntry->m_aName);
			}
			EntryRect.VSplitLeft(35.0f, &EntryTypeRect, &EntryRect);

			if(IsClan)
			{
				RenderFontIcon(EntryTypeRect, FontIcon::ICON_USERS, 18.0f, TEXTALIGN_MC);
			}
			else
			{
				// TODO: stop misusing this function
				// TODO: render the real skin with skin remembering component (to be added)
				RenderDevSkin(EntryTypeRect.Center(), 35.0f, "default", "default", false, 0, 0, 0, false, false);
			}

			if(str_comp(pEntry->m_aReason, "") != 0)
			{
				EntryRect.VSplitRight(20.0f, &EntryRect, &ToolTip);
				RenderFontIcon(ToolTip, FontIcon::COMMENT, 18.0f, TEXTALIGN_MC);
				GameClient()->m_Tooltips.DoToolTip(&s_vItemIds[i], &ToolTip, pEntry->m_aReason);
				GameClient()->m_Tooltips.SetFadeTime(&s_vItemIds[i], 0.0f);
			}

			EntryRect.HMargin(MarginExtraSmall, &EntryRect);
			EntryRect.HSplitMid(&EntryRect, &WarType, MarginSmall);

			Ui()->DoLabel(&EntryRect, aBuf, StandardFontSize, TEXTALIGN_ML);
			TextRender()->TextColor(pEntry->m_pWarType->m_Color);
			Ui()->DoLabel(&WarType, pEntry->m_pWarType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		const int NewSelectedEntry = s_EntriesListBox.DoEnd();
		if(SelectedOldEntry != NewSelectedEntry || (SelectedOldEntry >= 0 && Ui()->HotItem() == &s_vItemIds[NewSelectedEntry] && Ui()->MouseButtonClicked(0)))
		{
			s_pSelectedEntry = vpFilteredEntries[NewSelectedEntry];
			if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
			{
				str_copy(s_aEntryName, s_pSelectedEntry->m_aName);
				str_copy(s_aEntryClan, s_pSelectedEntry->m_aClan);
				str_copy(s_aEntryReason, s_pSelectedEntry->m_aReason);
				if(str_comp(s_pSelectedEntry->m_aClan, "") != 0)
				{
					s_IsName = false;
					s_IsClan = true;
				}
				else
				{
					s_IsName = true;
					s_IsClan = false;
				}
				s_pSelectedType = s_pSelectedEntry->m_pWarType;
			}
		}

		Ui()->DoEditBox_Search(&s_EntriesFilterInput, &EntriesSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());
	}

	// ======WAR ENTRY EDITING======
	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Label.VSplitRight(25.0f, &Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Edit Entry"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);

	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aEntryName, sizeof(s_aEntryName));
	s_NameInput.SetEmptyText(TCLocalize("Name"));
	if(s_IsName)
	{
		Ui()->DoEditBox(&s_NameInput, &ButtonL, 12.0f);
	}
	else
	{
		ButtonL.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonL);
		ButtonL.VMargin(2.0f, &ButtonL);
		s_NameInput.Render(&ButtonL, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	static CLineInput s_ClanInput;
	s_ClanInput.SetBuffer(s_aEntryClan, sizeof(s_aEntryClan));
	s_ClanInput.SetEmptyText(TCLocalize("Clan"));
	if(s_IsClan)
		Ui()->DoEditBox(&s_ClanInput, &ButtonR, 12.0f);
	else
	{
		ButtonR.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonR);
		ButtonR.VMargin(2.0f, &ButtonR);
		s_ClanInput.Render(&ButtonR, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static unsigned char s_NameRadio, s_ClanRadio;
	if(DoButton_CheckBox_Common(&s_NameRadio, TCLocalize("Name"), s_IsName ? "X" : "", &ButtonL, BUTTONFLAG_LEFT))
	{
		s_IsName = true;
		s_IsClan = false;
	}
	if(DoButton_CheckBox_Common(&s_ClanRadio, TCLocalize("Clan"), s_IsClan ? "X" : "", &ButtonR, BUTTONFLAG_LEFT))
	{
		s_IsName = false;
		s_IsClan = true;
	}
	if(!s_IsName)
		str_copy(s_aEntryName, "");
	if(!s_IsClan)
		str_copy(s_aEntryClan, "");

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);
	static CLineInput s_ReasonInput;
	s_ReasonInput.SetBuffer(s_aEntryReason, sizeof(s_aEntryReason));
	s_ReasonInput.SetEmptyText(TCLocalize("Reason"));
	Ui()->DoEditBox(&s_ReasonInput, &Button, 12.0f);

	static CButtonContainer s_AddButton, s_OverrideButton;

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize * 2.0f, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);

	if(DoButtonLineSize_Menu(&s_OverrideButton, TCLocalize("Override Entry"), 0, &ButtonL, LineSize) && s_pSelectedEntry)
	{
		if(s_pSelectedEntry && s_pSelectedType && (str_comp(s_aEntryName, "") != 0 || str_comp(s_aEntryClan, "") != 0))
		{
			str_copy(s_pSelectedEntry->m_aName, s_aEntryName);
			str_copy(s_pSelectedEntry->m_aClan, s_aEntryClan);
			str_copy(s_pSelectedEntry->m_aReason, s_aEntryReason);
			s_pSelectedEntry->m_pWarType = s_pSelectedType;
		}
	}
	if(DoButtonLineSize_Menu(&s_AddButton, TCLocalize("Add Entry"), 0, &ButtonR, LineSize))
	{
		if(s_pSelectedType)
			GameClient()->m_WarList.AddWarEntry(s_aEntryName, s_aEntryClan, s_aEntryReason, s_pSelectedType->m_aWarName);
	}
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column2);
	if(s_pSelectedType)
	{
		float Shade = 0.0f;
		Button.Draw(ColorRGBA(Shade, Shade, Shade, 0.25f), 15, 3.0f);
		TextRender()->TextColor(s_pSelectedType->m_Color);
		Ui()->DoLabel(&Button, s_pSelectedType->m_aWarName, HeadlineFontSize, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	Column2.HSplitBottom(150.0f, nullptr, &Column2);

	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Ui()->DoLabel(&Label, TCLocalize("Settings"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListAllowDuplicates, TCLocalize("Allow Duplicate Entries"), &g_Config.m_TcWarListAllowDuplicates, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarList, TCLocalize("Enable warlist"), &g_Config.m_TcWarList, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListChat, TCLocalize("Colors in chat"), &g_Config.m_TcWarListChat, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListScoreboard, TCLocalize("Colors in scoreboard"), &g_Config.m_TcWarListScoreboard, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListSpectate, TCLocalize("Colors in spectate select"), &g_Config.m_TcWarListSpectate, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListShowClan, TCLocalize("Show clan if war"), &g_Config.m_TcWarListShowClan, &Column2, LineSize);

	// ======WAR TYPE EDITING======

	Column3.HSplitTop(HeadlineHeight, &Label, &Column3);
	Ui()->DoLabel(&Label, TCLocalize("War Groups"), HeadlineFontSize, TEXTALIGN_ML);
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);

	static char s_aTypeName[MAX_WARLIST_TYPE_LENGTH];
	static ColorRGBA s_GroupColor = ColorRGBA(1, 1, 1, 1);

	CUIRect WarTypeList;
	Column3.HSplitBottom(180.0f, &WarTypeList, &Column3);
	m_pRemoveWarType = nullptr;
	int SelectedOldType = -1;
	static CListBox s_WarTypeListBox;
	s_WarTypeListBox.DoStart(25.0f, GameClient()->m_WarList.m_WarTypes.size(), 1, 2, SelectedOldType, &WarTypeList, true, IGraphics::CORNER_ALL, true);

	static std::vector<unsigned char> s_vTypeItemIds;
	static std::vector<CButtonContainer> s_vTypeDeleteButtons;

	const int MaxTypes = GameClient()->m_WarList.m_WarTypes.size();
	s_vTypeItemIds.resize(MaxTypes);
	s_vTypeDeleteButtons.resize(MaxTypes);

	for(int i = 0; i < (int)GameClient()->m_WarList.m_WarTypes.size(); i++)
	{
		CWarType *pType = GameClient()->m_WarList.m_WarTypes[i];

		if(!pType)
			continue;

		if(s_pSelectedType && pType == s_pSelectedType)
			SelectedOldType = i;

		const CListboxItem Item = s_WarTypeListBox.DoNextItem(&s_vTypeItemIds[i], SelectedOldType >= 0 && SelectedOldType == i);
		if(!Item.m_Visible)
			continue;

		CUIRect TypeRect, DeleteButton;
		Item.m_Rect.Margin(0.0f, &TypeRect);

		if(pType->m_Removable)
		{
			TypeRect.VSplitRight(20.0f, &TypeRect, &DeleteButton);
			DeleteButton.HSplitTop(20.0f, &DeleteButton, nullptr);
			DeleteButton.Margin(2.0f, &DeleteButton);
			if(DoButtonNoRect_FontIcon(&s_vTypeDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				m_pRemoveWarType = pType;
		}
		TextRender()->TextColor(pType->m_Color);
		Ui()->DoLabel(&TypeRect, pType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelectedType = s_WarTypeListBox.DoEnd();
	if((SelectedOldType != NewSelectedType && NewSelectedType >= 0) || (NewSelectedType >= 0 && Ui()->HotItem() == &s_vTypeItemIds[NewSelectedType] && Ui()->MouseButtonClicked(0)))
	{
		s_pSelectedType = GameClient()->m_WarList.m_WarTypes[NewSelectedType];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aTypeName, s_pSelectedType->m_aWarName);
			s_GroupColor = s_pSelectedType->m_Color;
		}
	}
	if(m_pRemoveWarType != nullptr)
	{
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage),
			TCLocalize("Are you sure that you want to remove '%s' from your war groups?"),
			m_pRemoveWarType->m_aWarName);
		PopupConfirm(TCLocalize("Remove War Group"), aMessage, TCLocalize("Yes"), TCLocalize("No"), &CMenus::PopupConfirmRemoveWarType);
	}

	static CLineInput s_TypeNameInput;
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	Column3.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column3);
	s_TypeNameInput.SetBuffer(s_aTypeName, sizeof(s_aTypeName));
	s_TypeNameInput.SetEmptyText("Group Name");
	Ui()->DoEditBox(&s_TypeNameInput, &Button, 12.0f);
	static CButtonContainer s_AddGroupButton, s_OverrideGroupButton, s_GroupColorPicker;

	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	static unsigned int s_ColorValue = 0;
	s_ColorValue = color_cast<ColorHSLA>(s_GroupColor).Pack(false);
	ColorHSLA PickedColor = DoLine_ColorPicker(&s_GroupColorPicker, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column3, TCLocalize("Color"), &s_ColorValue, ColorRGBA(1.0f, 1.0f, 1.0f), true);
	s_GroupColor = color_cast<ColorRGBA>(PickedColor);

	Column3.HSplitTop(LineSize * 2.0f, &Button, &Column3);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	bool OverrideDisabled = NewSelectedType == 0;
	if(DoButtonLineSize_Menu(&s_OverrideGroupButton, TCLocalize("Override Group"), 0, &ButtonL, LineSize, OverrideDisabled) && s_pSelectedType)
	{
		if(s_pSelectedType && str_comp(s_aTypeName, "") != 0)
		{
			str_copy(s_pSelectedType->m_aWarName, s_aTypeName);
			s_pSelectedType->m_Color = s_GroupColor;
		}
	}
	bool AddDisabled = str_comp(GameClient()->m_WarList.FindWarType(s_aTypeName)->m_aWarName, "none") != 0 || str_comp(s_aTypeName, "none") == 0;
	if(DoButtonLineSize_Menu(&s_AddGroupButton, TCLocalize("Add Group"), 0, &ButtonR, LineSize, AddDisabled))
	{
		GameClient()->m_WarList.AddWarType(s_aTypeName, s_GroupColor);
	}

	// ======ONLINE PLAYER LIST======

	Column4.HSplitTop(HeadlineHeight, &Label, &Column4);
	Ui()->DoLabel(&Label, TCLocalize("Online Players"), HeadlineFontSize, TEXTALIGN_ML);
	Column4.HSplitTop(MarginSmall, nullptr, &Column4);

	CUIRect PlayerSearch;
	Column4.HSplitBottom(25.0f, &Column4, &PlayerSearch);
	PlayerSearch.HSplitTop(MarginSmall, nullptr, &PlayerSearch);
	static CLineInputBuffered<128> s_PlayerSearchInput;
	Ui()->DoEditBox_Search(&s_PlayerSearchInput, &PlayerSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	CUIRect PlayerList;
	Column4.HSplitBottom(0.0f, &PlayerList, &Column4);
	static CListBox s_PlayerListBox;
	s_PlayerListBox.DoStart(30.0f, MAX_CLIENTS, 1, 2, -1, &PlayerList, true, IGraphics::CORNER_ALL, true);

	static std::vector<unsigned char> s_vPlayerItemIds;
	static std::vector<CButtonContainer> s_vNameButtons;
	static std::vector<CButtonContainer> s_vClanButtons;

	s_vPlayerItemIds.resize(MAX_CLIENTS);
	s_vNameButtons.resize(MAX_CLIENTS);
	s_vClanButtons.resize(MAX_CLIENTS);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_Snap.m_apPlayerInfos[i])
			continue;

		const auto &Client = GameClient()->m_aClients[i];

		if(!str_find_nocase(Client.m_aName, s_PlayerSearchInput.GetString()) &&
			!str_find_nocase(Client.m_aClan, s_PlayerSearchInput.GetString()))
			continue;

		const CListboxItem Item = s_PlayerListBox.DoNextItem(&s_vPlayerItemIds[i], false);
		if(!Item.m_Visible)
			continue;

		CUIRect PlayerRect, TeeRect, NameRect, ClanRect;
		Item.m_Rect.Margin(0.0f, &PlayerRect);
		PlayerRect.VSplitLeft(25.0f, &TeeRect, &PlayerRect);

		PlayerRect.VSplitMid(&NameRect, &ClanRect);
		PlayerRect = NameRect;
		PlayerRect.x = TeeRect.x;
		PlayerRect.w += TeeRect.w;
		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_NameColor);
		ColorRGBA NameButtonColor = Ui()->CheckActiveItem(&s_vNameButtons[i]) ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f) :
											(Ui()->HotItem() == &s_vNameButtons[i] ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f));
		PlayerRect.Draw(NameButtonColor, IGraphics::CORNER_L, 5.0f);
		Ui()->DoLabel(&NameRect, Client.m_aName, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vNameButtons[i], false, &PlayerRect, BUTTONFLAG_LEFT))
		{
			s_IsName = true;
			s_IsClan = false;
			str_copy(s_aEntryName, Client.m_aName);
		}

		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_ClanColor);
		ColorRGBA ClanButtonColor = Ui()->CheckActiveItem(&s_vClanButtons[i]) ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f) :
											(Ui()->HotItem() == &s_vClanButtons[i] ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f));
		ClanRect.Draw(ClanButtonColor, IGraphics::CORNER_R, 5.0f);
		Ui()->DoLabel(&ClanRect, Client.m_aClan, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vClanButtons[i], false, &ClanRect, BUTTONFLAG_LEFT))
		{
			s_IsName = false;
			s_IsClan = true;
			str_copy(s_aEntryClan, Client.m_aClan);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		CTeeRenderInfo TeeInfo = Client.m_RenderInfo;
		TeeInfo.m_Size = 25.0f;
		RenderTeeCute(CAnimState::GetIdle(), &TeeInfo, 0, vec2(1.0f, 0.0f), TeeRect.Center() + vec2(-1.0f, 2.5f), true);
	}
	s_PlayerListBox.DoEnd();
}

void CMenus::RenderSettingsTClientFriendList(CUIRect MainView)
{
	CUIRect RightView, LeftView, Column1, Column2, Column3, Button, ButtonL, ButtonR, Label;

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, Margin);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	LeftView.VSplitMid(&Column1, &Column2, Margin);
	Column3 = RightView;

	static char s_aFriendName[MAX_NAME_LENGTH];
	static char s_aHeartName[MAX_HEART_TYPE_LENGTH];
	static int s_SelectedHeartIndex = 0;
	static ColorRGBA s_HeartColor = ColorRGBA(1, 1, 1, 1);

	Column1.HSplitTop(HeadlineHeight, &Label, &Column1);
	Ui()->DoLabel(&Label, TCLocalize("Friend Entries"), HeadlineFontSize, TEXTALIGN_ML);
	Column1.HSplitTop(MarginSmall, nullptr, &Column1);

	CUIRect EntriesSearch;
	Column1.HSplitBottom(25.0f, &Column1, &EntriesSearch);
	EntriesSearch.HSplitTop(MarginSmall, nullptr, &EntriesSearch);
	static CLineInputBuffered<128> s_EntriesFilterInput;

	std::vector<CHeartEntry *> vpFilteredEntries;
	for(CHeartEntry &Entry : GameClient()->m_HeartList.m_vHeartEntries)
	{
		if(str_find_nocase(Entry.m_aName, s_EntriesFilterInput.GetString()))
			vpFilteredEntries.push_back(&Entry);
		else if(str_find_nocase(Entry.m_pHeartType->m_aName, s_EntriesFilterInput.GetString()))
			vpFilteredEntries.push_back(&Entry);
	}

	int SelectedOldEntry = -1;
	static CListBox s_EntriesListBox;
	s_EntriesListBox.DoStart(35.0f, vpFilteredEntries.size(), 1, 2, SelectedOldEntry, &Column1);

	static std::vector<unsigned char> s_vItemIds;
	static std::vector<CButtonContainer> s_vDeleteButtons;
	s_vItemIds.resize(vpFilteredEntries.size());
	s_vDeleteButtons.resize(vpFilteredEntries.size());

	for(size_t i = 0; i < vpFilteredEntries.size(); i++)
	{
		CHeartEntry *pEntry = vpFilteredEntries[i];

		const CListboxItem Item = s_EntriesListBox.DoNextItem(&s_vItemIds[i], false);
		if(!Item.m_Visible)
			continue;

		CUIRect EntryRect, DeleteButton, HeartName;
		Item.m_Rect.Margin(0.0f, &EntryRect);
		EntryRect.VSplitLeft(26.0f, &DeleteButton, &EntryRect);
		DeleteButton.HMargin(7.5f, &DeleteButton);
		DeleteButton.VSplitLeft(MarginSmall, nullptr, &DeleteButton);
		DeleteButton.VSplitRight(MarginExtraSmall, &DeleteButton, nullptr);

		if(Ui()->DoButton_FontIcon(&s_vDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
			GameClient()->m_HeartList.RemoveFriend(pEntry->m_aName);

		EntryRect.HMargin(MarginExtraSmall, &EntryRect);
		EntryRect.HSplitMid(&EntryRect, &HeartName, MarginSmall);
		Ui()->DoLabel(&EntryRect, pEntry->m_aName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(pEntry->m_pHeartType->m_Color);
		Ui()->DoLabel(&HeartName, pEntry->m_pHeartType->m_aName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	s_EntriesListBox.DoEnd();
	Ui()->DoEditBox_Search(&s_EntriesFilterInput, &EntriesSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Ui()->DoLabel(&Label, TCLocalize("Add Friend"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);

	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aFriendName, sizeof(s_aFriendName));
	s_NameInput.SetEmptyText(TCLocalize("Nick"));
	Ui()->DoEditBox(&s_NameInput, &Button, 12.0f);

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize, &Button, &Column2);

	static std::vector<const char *> s_vHeartNames;
	static CUi::SDropDownState s_HeartDropDownState;
	static CScrollRegion s_HeartDropDownScrollRegion;
	s_HeartDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_HeartDropDownScrollRegion;
	if(s_vHeartNames.size() != GameClient()->m_HeartList.m_HeartTypes.size())
	{
		s_vHeartNames.clear();
		for(CHeartType *pType : GameClient()->m_HeartList.m_HeartTypes)
			s_vHeartNames.push_back(pType->m_aName);
	}
	if(s_SelectedHeartIndex >= (int)GameClient()->m_HeartList.m_HeartTypes.size())
		s_SelectedHeartIndex = 0;
	s_SelectedHeartIndex = Ui()->DoDropDown(&Button, s_SelectedHeartIndex, s_vHeartNames.data(), s_vHeartNames.size(), s_HeartDropDownState);

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize, &Button, &Column2);
	static CButtonContainer s_AddFriendButton;
	if(DoButtonLineSize_Menu(&s_AddFriendButton, TCLocalize("Add Friend"), 0, &Button, LineSize))
	{
		if(str_comp(s_aFriendName, "") != 0)
		{
			const char *pHeart = GameClient()->m_HeartList.m_HeartTypes[s_SelectedHeartIndex]->m_aName;
			GameClient()->m_HeartList.AddFriend(s_aFriendName, pHeart);
			s_aFriendName[0] = '\0';
		}
	}

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize, &Button, &Column2);
	Ui()->DoLabel(&Button, TCLocalize("Command: .friend <nick> <heart>"), StandardFontSize, TEXTALIGN_ML);

	Column3.HSplitTop(HeadlineHeight, &Label, &Column3);
	Ui()->DoLabel(&Label, TCLocalize("Hearts"), HeadlineFontSize, TEXTALIGN_ML);
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);

	CUIRect HeartList;
	Column3.HSplitBottom(220.0f, &HeartList, &Column3);

	int SelectedOldHeart = -1;
	static CListBox s_HeartListBox;
	s_HeartListBox.DoStart(25.0f, GameClient()->m_HeartList.m_HeartTypes.size(), 1, 2, SelectedOldHeart, &HeartList, true, IGraphics::CORNER_ALL, true);

	static std::vector<unsigned char> s_vHeartItemIds;
	static std::vector<CButtonContainer> s_vHeartDeleteButtons;
	static std::vector<CButtonContainer> s_vHeartPrioButtons;
	s_vHeartItemIds.resize(GameClient()->m_HeartList.m_HeartTypes.size());
	s_vHeartDeleteButtons.resize(GameClient()->m_HeartList.m_HeartTypes.size());
	s_vHeartPrioButtons.resize(GameClient()->m_HeartList.m_HeartTypes.size());

	static int s_DragHeartIndex = -1;
	static bool s_DragMovedHeart = false;
	static std::vector<CUIRect> s_vHeartRowRects;
	static std::vector<unsigned char> s_vHeartRowValid;
	s_vHeartRowRects.resize(GameClient()->m_HeartList.m_HeartTypes.size());
	s_vHeartRowValid.assign(GameClient()->m_HeartList.m_HeartTypes.size(), 0);

	CHeartType *pRemoveHeart = nullptr;
	for(int i = 0; i < (int)GameClient()->m_HeartList.m_HeartTypes.size(); i++)
	{
		CHeartType *pType = GameClient()->m_HeartList.m_HeartTypes[i];
		if(!pType)
			continue;
		if(i == s_SelectedHeartIndex)
			SelectedOldHeart = i;

		const CListboxItem Item = s_HeartListBox.DoNextItem(&s_vHeartItemIds[i], SelectedOldHeart >= 0 && SelectedOldHeart == i);
		if(!Item.m_Visible)
			continue;
		s_vHeartRowRects[i] = Item.m_Rect;
		s_vHeartRowValid[i] = 1;

		CUIRect TypeRect, DeleteButton, PrioButton;
		Item.m_Rect.Margin(0.0f, &TypeRect);
		bool HoverDelete = false, HoverPrio = false;
		if(pType->m_Removable)
		{
			TypeRect.VSplitRight(20.0f, &TypeRect, &DeleteButton);
			DeleteButton.HSplitTop(20.0f, &DeleteButton, nullptr);
			DeleteButton.Margin(2.0f, &DeleteButton);
			HoverDelete = Ui()->MouseInside(&DeleteButton);
			if(DoButtonNoRect_FontIcon(&s_vHeartDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				pRemoveHeart = pType;
		}
		TypeRect.VSplitRight(20.0f, &TypeRect, &PrioButton);
		PrioButton.HSplitTop(20.0f, &PrioButton, nullptr);
		PrioButton.Margin(2.0f, &PrioButton);
		HoverPrio = Ui()->MouseInside(&PrioButton);
		if(DoButton_CheckBox(&s_vHeartPrioButtons[i], "", pType->m_Prioritized, &PrioButton))
			pType->m_Prioritized = !pType->m_Prioritized;
		GameClient()->m_Tooltips.DoToolTip(&s_vHeartPrioButtons[i], &PrioButton, TCLocalize("Show friends with this heart in Prioritized Friends"));

		if(s_DragHeartIndex < 0 && Ui()->MouseButtonClicked(0) && Ui()->MouseInside(&Item.m_Rect) && !HoverDelete && !HoverPrio)
		{
			s_DragHeartIndex = i;
			s_DragMovedHeart = false;
		}
		if(s_DragHeartIndex == i)
			Item.m_Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.2f), IGraphics::CORNER_ALL, 5.0f);

		TextRender()->TextColor(pType->m_Color);
		Ui()->DoLabel(&TypeRect, pType->m_aName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelectedHeart = s_HeartListBox.DoEnd();

	bool DraggedHeart = false;
	if(s_DragHeartIndex >= 0)
	{
		if(!Ui()->MouseButton(0))
		{
			DraggedHeart = s_DragMovedHeart;
			s_DragHeartIndex = -1;
			s_DragMovedHeart = false;
		}
		else
		{
			int HoverIndex = -1;
			for(int i = 0; i < (int)s_vHeartRowValid.size(); i++)
			{
				if(s_vHeartRowValid[i] && Ui()->MouseInside(&s_vHeartRowRects[i]))
				{
					HoverIndex = i;
					break;
				}
			}
			if(HoverIndex >= 0 && HoverIndex != s_DragHeartIndex)
			{
				if(s_SelectedHeartIndex == s_DragHeartIndex)
					s_SelectedHeartIndex = HoverIndex;
				else if(s_DragHeartIndex < s_SelectedHeartIndex && s_SelectedHeartIndex <= HoverIndex)
					s_SelectedHeartIndex--;
				else if(HoverIndex <= s_SelectedHeartIndex && s_SelectedHeartIndex < s_DragHeartIndex)
					s_SelectedHeartIndex++;
				GameClient()->m_HeartList.MoveHeartType(s_DragHeartIndex, HoverIndex);
				s_DragHeartIndex = HoverIndex;
				s_DragMovedHeart = true;
			}
		}
	}

	if(!DraggedHeart && ((SelectedOldHeart != NewSelectedHeart && NewSelectedHeart >= 0) || (NewSelectedHeart >= 0 && Ui()->HotItem() == &s_vHeartItemIds[NewSelectedHeart] && Ui()->MouseButtonClicked(0))))
	{
		s_SelectedHeartIndex = NewSelectedHeart;
		str_copy(s_aHeartName, GameClient()->m_HeartList.m_HeartTypes[NewSelectedHeart]->m_aName);
		s_HeartColor = GameClient()->m_HeartList.m_HeartTypes[NewSelectedHeart]->m_Color;
	}
	if(pRemoveHeart != nullptr)
	{
		GameClient()->m_HeartList.RemoveHeartType(pRemoveHeart->m_aName);
		if(s_SelectedHeartIndex >= (int)GameClient()->m_HeartList.m_HeartTypes.size())
			s_SelectedHeartIndex = 0;
	}

	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	Column3.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column3);
	static CLineInput s_HeartNameInput;
	s_HeartNameInput.SetBuffer(s_aHeartName, sizeof(s_aHeartName));
	s_HeartNameInput.SetEmptyText("Heart Name");
	Ui()->DoEditBox(&s_HeartNameInput, &Button, 12.0f);

	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	static unsigned int s_ColorValue = 0;
	static CButtonContainer s_HeartColorPicker;
	s_ColorValue = color_cast<ColorHSLA>(s_HeartColor).Pack(false);
	ColorHSLA PickedColor = DoLine_ColorPicker(&s_HeartColorPicker, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column3, TCLocalize("Color"), &s_ColorValue, ColorRGBA(1.0f, 1.0f, 1.0f), true);
	s_HeartColor = color_cast<ColorRGBA>(PickedColor);

	Column3.HSplitTop(LineSize * 2.0f, &Button, &Column3);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static CButtonContainer s_HeartOverrideButton;
	bool OverrideDisabled = s_SelectedHeartIndex == 0;
	if(DoButtonLineSize_Menu(&s_HeartOverrideButton, TCLocalize("Override Heart"), 0, &ButtonL, LineSize, OverrideDisabled))
	{
		if(s_SelectedHeartIndex >= 0 && s_SelectedHeartIndex < (int)GameClient()->m_HeartList.m_HeartTypes.size() && str_comp(s_aHeartName, "") != 0)
		{
			str_copy(GameClient()->m_HeartList.m_HeartTypes[s_SelectedHeartIndex]->m_aName, s_aHeartName);
			GameClient()->m_HeartList.m_HeartTypes[s_SelectedHeartIndex]->m_Color = s_HeartColor;
		}
	}
	static CButtonContainer s_AddHeartButton;
	bool AddDisabled = str_comp(s_aHeartName, "") == 0;
	if(DoButtonLineSize_Menu(&s_AddHeartButton, TCLocalize("Add Heart"), 0, &ButtonR, LineSize, AddDisabled))
	{
		GameClient()->m_HeartList.AddHeartType(s_aHeartName, s_HeartColor);
	}
}

void CMenus::RenderSettingsTClientStatusBar(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label, StatusBar;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.HSplitBottom(100.0f, &MainView, &StatusBar);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Status Bar"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBar, TCLocalize("Show status bar"), &g_Config.m_TcStatusBar, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBarLabels, TCLocalize("Show labels on status bar items"), &g_Config.m_TcStatusBarLabels, &LeftView, LineSize);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_TcStatusBarHeight, &g_Config.m_TcStatusBarHeight, &Button, TCLocalize("Status bar height"), 1, 16);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Local Time"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBar12HourClock, TCLocalize("Use 12 hour clock"), &g_Config.m_TcStatusBar12HourClock, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBarLocalTimeSeconds, TCLocalize("Show seconds on clock"), &g_Config.m_TcStatusBarLocalTimeSeconds, &LeftView, LineSize);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Colors"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	static CButtonContainer s_StatusbarColor, s_StatusbarTextColor;

	DoLine_ColorPicker(&s_StatusbarColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Status bar color"), &g_Config.m_TcStatusBarColor, ColorRGBA(0.0f, 0.0f, 0.0f), false);
	DoLine_ColorPicker(&s_StatusbarTextColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Text color"), &g_Config.m_TcStatusBarTextColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_TcStatusBarAlpha, &g_Config.m_TcStatusBarAlpha, &Button, TCLocalize("Status bar alpha"), 0, 100);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_TcStatusBarTextAlpha, &g_Config.m_TcStatusBarTextAlpha, &Button, TCLocalize("Text alpha"), 0, 100);

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Status Bar Codes:"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("a = Angle"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("p = Ping"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("d = Prediction"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("c = Position"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("l = Local Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("r = Race Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("f = FPS"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("v = Velocity"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("z = Zoom"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("_ or ' ' = Space"), FontSize, TEXTALIGN_ML);
	static int s_SelectedItem = -1;
	static int s_TypeSelectedOld = -1;

	CUIRect StatusScheme, StatusButtons, ItemLabel;
	static CButtonContainer s_ApplyButton, s_AddButton, s_RemoveButton;
	StatusBar.HSplitBottom(LineSize + MarginSmall, &StatusBar, &StatusScheme);
	StatusBar.HSplitTop(LineSize + MarginSmall, &ItemLabel, &StatusBar);
	StatusScheme.HSplitTop(MarginSmall, nullptr, &StatusScheme);

	if(s_TypeSelectedOld >= 0)
		Ui()->DoLabel(&ItemLabel, GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld].m_aDesc, FontSize, TEXTALIGN_ML);

	StatusScheme.VSplitMid(&StatusButtons, &StatusScheme, MarginSmall);
	StatusScheme.VSplitMid(&Label, &StatusScheme, MarginSmall);
	StatusScheme.VSplitMid(&StatusScheme, &Button, MarginSmall);
	if(DoButton_Menu(&s_ApplyButton, TCLocalize("Apply"), 0, &Button))
	{
		GameClient()->m_StatusBar.ApplyStatusBarScheme(g_Config.m_TcStatusBarScheme);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		s_SelectedItem = -1;
	}
	Ui()->DoLabel(&Label, TCLocalize("Status Scheme:"), FontSize, TEXTALIGN_MR);
	static CLineInput s_StatusScheme(g_Config.m_TcStatusBarScheme, sizeof(g_Config.m_TcStatusBarScheme));
	s_StatusScheme.SetEmptyText("");
	Ui()->DoEditBox(&s_StatusScheme, &StatusScheme, EditBoxFontSize);

	static std::vector<const char *> s_DropDownNames = {};
	for(const CStatusItem &StatusItemType : GameClient()->m_StatusBar.m_StatusItemTypes)
		if(s_DropDownNames.size() != GameClient()->m_StatusBar.m_StatusItemTypes.size())
			s_DropDownNames.push_back(StatusItemType.m_aName);

	static CUi::SDropDownState s_DropDownState;
	static CScrollRegion s_DropDownScrollRegion;
	s_DropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DropDownScrollRegion;
	CUIRect DropDownRect;

	StatusButtons.VSplitMid(&DropDownRect, &StatusButtons, MarginSmall);
	const int TypeSelectedNew = Ui()->DoDropDown(&DropDownRect, s_TypeSelectedOld, s_DropDownNames.data(), s_DropDownNames.size(), s_DropDownState);
	if(s_TypeSelectedOld != TypeSelectedNew)
	{
		s_TypeSelectedOld = TypeSelectedNew;
		if(s_SelectedItem >= 0)
		{
			GameClient()->m_StatusBar.m_StatusBarItems[s_SelectedItem] = &GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld];
			GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		}
	}
	CUIRect ButtonL, ButtonR;
	StatusButtons.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	size_t NumItems = GameClient()->m_StatusBar.m_StatusBarItems.size();
	if(DoButton_Menu(&s_AddButton, TCLocalize("Add Item"), 0, &ButtonL) && s_TypeSelectedOld >= 0 && NumItems < 128)
	{
		GameClient()->m_StatusBar.m_StatusBarItems.push_back(&GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld]);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		s_SelectedItem = (int)GameClient()->m_StatusBar.m_StatusBarItems.size() - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, TCLocalize("Remove Item"), 0, &ButtonR) && s_SelectedItem >= 0)
	{
		GameClient()->m_StatusBar.m_StatusBarItems.erase(GameClient()->m_StatusBar.m_StatusBarItems.begin() + s_SelectedItem);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		s_SelectedItem = -1;
	}

	// color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcStatusBarColor)).WithAlpha(0.5f)
	StatusBar.Draw(ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, 5.0f);
	int ItemCount = GameClient()->m_StatusBar.m_StatusBarItems.size();
	float AvailableWidth = StatusBar.w;
	// AvailableWidth -= (ItemCount - 1) * MarginSmall;
	AvailableWidth -= MarginSmall;
	StatusBar.VSplitLeft(MarginExtraSmall, nullptr, &StatusBar);
	float ItemWidth = AvailableWidth / (float)ItemCount;
	CUIRect StatusItemButton;
	static std::vector<CButtonContainer *> s_pItemButtons;
	static std::vector<CButtonContainer> s_ItemButtons;
	static vec2 s_ActivePos = vec2(0.0f, 0.0f);
	class CSwapItem
	{
	public:
		vec2 m_InitialPosition = vec2(0.0f, 0.0f);
		float m_Duration = 0.0f;
	};

	static std::vector<CSwapItem> s_ItemSwaps;

	if((int)s_ItemButtons.size() != ItemCount)
	{
		s_ItemSwaps.resize(ItemCount);
		s_pItemButtons.resize(ItemCount);
		s_ItemButtons.resize(ItemCount);
		for(int i = 0; i < ItemCount; ++i)
		{
			s_pItemButtons[i] = &s_ItemButtons[i];
		}
	}
	bool StatusItemActive = false;
	int HotStatusIndex = 0;
	for(int i = 0; i < ItemCount; ++i)
	{
		if(Ui()->ActiveItem() == s_pItemButtons[i])
		{
			StatusItemActive = true;
			HotStatusIndex = i;
		}
	}

	for(int i = 0; i < ItemCount; ++i)
	{
		// if(i > 0)
		//	StatusBar.VSplitLeft(MarginSmall, nullptr, &StatusBar);
		StatusBar.VSplitLeft(ItemWidth, &StatusItemButton, &StatusBar);
		StatusItemButton.HMargin(MarginSmall, &StatusItemButton);
		StatusItemButton.VMargin(MarginExtraSmall, &StatusItemButton);
		CStatusItem *StatusItem = GameClient()->m_StatusBar.m_StatusBarItems[i];
		ColorRGBA Col = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
		if(s_SelectedItem == i)
			Col = ColorRGBA(1.0f, 0.35f, 0.35f, 0.75f);
		CUIRect TempItemButton = StatusItemButton;
		TempItemButton.y = 0, TempItemButton.h = 10000.0f;
		if(StatusItemActive && Ui()->ActiveItem() != s_pItemButtons[i] && Ui()->MouseInside(&TempItemButton))
		{
			std::swap(s_pItemButtons[i], s_pItemButtons[HotStatusIndex]);
			std::swap(GameClient()->m_StatusBar.m_StatusBarItems[i], GameClient()->m_StatusBar.m_StatusBarItems[HotStatusIndex]);
			s_SelectedItem = -2;
			s_ItemSwaps[HotStatusIndex].m_InitialPosition = vec2(StatusItemButton.x, StatusItemButton.y);
			s_ItemSwaps[HotStatusIndex].m_Duration = 0.15f;
			s_ItemSwaps[i].m_InitialPosition = vec2(s_ActivePos.x, s_ActivePos.y);
			s_ItemSwaps[i].m_Duration = 0.15f;
			GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		}
		TempItemButton = StatusItemButton;
		s_ItemSwaps[i].m_Duration = std::max(0.0f, s_ItemSwaps[i].m_Duration - Client()->RenderFrameTime());
		if(s_ItemSwaps[i].m_Duration > 0.0f)
		{
			float Progress = std::pow(2.0, -5.0 * (1.0 - s_ItemSwaps[i].m_Duration / 0.15f));
			TempItemButton.x = mix(TempItemButton.x, s_ItemSwaps[i].m_InitialPosition.x, Progress);
		}
		if(DoButtonLineSize_Menu(s_pItemButtons[i], StatusItem->m_aDisplayName, 0, &TempItemButton, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, Col))
		{
			if(s_SelectedItem == -2)
				s_SelectedItem++;
			else if(s_SelectedItem != i)
			{
				s_SelectedItem = i;
				for(int t = 0; t < (int)GameClient()->m_StatusBar.m_StatusItemTypes.size(); ++t)
					if(str_comp(GameClient()->m_StatusBar.m_StatusItemTypes[t].m_aName, StatusItem->m_aName) == 0)
						s_TypeSelectedOld = t;
			}
			else
			{
				s_SelectedItem = -1;
				s_TypeSelectedOld = -1;
			}
		}
		if(Ui()->ActiveItem() == s_pItemButtons[i])
			s_ActivePos = vec2(StatusItemButton.x, StatusItemButton.y);
	}
	if(!StatusItemActive)
		s_SelectedItem = std::max(-1, s_SelectedItem);
}

void CMenus::RenderSettingsTClientInfo(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label, LowerLeftView;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);
	LeftView.HSplitMid(&LeftView, &LowerLeftView, 0.0f);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("TClient Links"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	static CButtonContainer s_DiscordButton, s_WebsiteButton, s_GithubButton, s_SupportButton;
	CUIRect ButtonLeft, ButtonRight;

	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, MarginSmall);
	if(DoButtonLineSize_Menu(&s_DiscordButton, TCLocalize("Discord"), 0, &ButtonLeft, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/fBvhH93Bt6");
	if(DoButtonLineSize_Menu(&s_WebsiteButton, TCLocalize("Website"), 0, &ButtonRight, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://tclient.app/");

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, MarginSmall);

	if(DoButtonLineSize_Menu(&s_GithubButton, TCLocalize("Github"), 0, &ButtonLeft, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://github.com/sjrc6/TaterClient-ddnet");
	if(DoButtonLineSize_Menu(&s_SupportButton, TCLocalize("Support ♥"), 0, &ButtonRight, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://ko-fi.com/Totar");

	LeftView = LowerLeftView;
	LeftView.HSplitBottom(LineSize * 4.0f + MarginSmall * 2.0f + HeadlineFontSize, nullptr, &LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Config Files"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	char aBuf[128 + IO_MAX_PATH_LENGTH];
	CUIRect TClientConfig, ProfilesFile, WarlistFile, ChatbindsFile;

	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&TClientConfig, &ProfilesFile, MarginSmall);

	static CButtonContainer s_Config, s_Profiles, s_Warlist, s_Chatbinds;
	if(DoButtonLineSize_Menu(&s_Config, TCLocalize("TClient Settings"), 0, &TClientConfig, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENT].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	if(DoButtonLineSize_Menu(&s_Profiles, TCLocalize("Profiles"), 0, &ProfilesFile, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTPROFILES].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&WarlistFile, &ChatbindsFile, MarginSmall);

	if(DoButtonLineSize_Menu(&s_Warlist, TCLocalize("War List"), 0, &WarlistFile, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTWARLIST].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	if(DoButtonLineSize_Menu(&s_Chatbinds, TCLocalize("Chat Binds"), 0, &ChatbindsFile, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTCHATBINDS].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	// =======RIGHT VIEW========

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("TClient Developers"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	const float TeeSize = 50.0f;
	const float CardSize = TeeSize + MarginSmall;
	CUIRect TeeRect, DevCardRect;
	static CButtonContainer s_LinkButton1, s_LinkButton2, s_LinkButton3, s_LinkButton4, s_LinkButton5;
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "Tater"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "Tater", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton1, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/sjrc6");
		RenderDevSkin(TeeRect.Center(), 50.0f, "glow_mermyfox", "mermyfox", true, 0, 0, 0, false, true, ColorRGBA(0.92f, 0.29f, 0.48f, 1.0f), ColorRGBA(0.55f, 0.64f, 0.76f, 1.0f));
	}
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "SollyBunny / bun bun"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "SollyBunny / bun bun", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton3, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/SollyBunny");
		RenderDevSkin(TeeRect.Center(), 50.0f, "tuzi", "tuzi", false, 0, 0, 2, true, true, true);
	}
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "PeBox"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "PeBox", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton2, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/danielkempf");
		RenderDevSkin(TeeRect.Center(), 50.0f, "greyfox", "greyfox", true, 0, 0, 2, false, true, ColorRGBA(0.00f, 0.09f, 1.00f, 1.00f), ColorRGBA(1.00f, 0.92f, 0.00f, 1.00f));
	}
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "Teero"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "Teero", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton4, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/Teero888");
		RenderDevSkin(TeeRect.Center(), 50.0f, "glow_mermyfox", "mermyfox", true, 0, 0, 0, false, true, ColorRGBA(1.00f, 1.00f, 1.00f, 1.00f), ColorRGBA(1.00f, 0.02f, 0.13f, 1.00f));
	}
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "ChillerDragon"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "ChillerDragon", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton5, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/ChillerDragon");
		RenderDevSkin(TeeRect.Center(), 50.0f, "glow_greensward", "greensward", false, 0, 0, 0, false, true, ColorRGBA(1.00f, 1.00f, 1.00f, 1.00f), ColorRGBA(1.00f, 0.02f, 0.13f, 1.00f));
	}

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Hide Settings Tabs"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	CUIRect LeftSettings, RightSettings;

	RightView.VSplitMid(&LeftSettings, &RightSettings, MarginSmall);
	RightView.HSplitTop(LineSize * 3.5f, nullptr, &RightView);

	const char *apTabNames[] = {
		TCLocalize("Settings"),
		TCLocalize("Bind Wheel"),
		TCLocalize("War List"),
		TCLocalize("Friendlist"),
		TCLocalize("Chat Binds"),
		TCLocalize("Status Bar"),
		TCLocalize("Info")};
	static int s_aShowTabs[NUMBER_OF_TCLIENT_TABS] = {};
	for(int i = 0; i < NUMBER_OF_TCLIENT_TABS - 1; ++i)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&s_aShowTabs[i], apTabNames[i], &s_aShowTabs[i], i % 2 == 0 ? &LeftSettings : &RightSettings, LineSize);
		SetFlag(g_Config.m_TcTClientSettingsTabs, i, s_aShowTabs[i]);
	}

	// RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	// Ui()->DoLabel(&Label, TCLocalize("Integration"), HeadlineFontSize, TEXTALIGN_ML);
	// RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDiscordRPC, TCLocalize("Enable Discord Integration"), &g_Config.m_TcDiscordRPC, &RightView, LineSize);
}

void CMenus::RenderSettingsFusionInfo(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Fusion Links"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	static CButtonContainer s_DiscordButton, s_GithubButton;

	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	if(DoButtonLineSize_Menu(&s_DiscordButton, TCLocalize("Discord"), 0, &Button, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/AYeDKHk9f3");

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	if(DoButtonLineSize_Menu(&s_GithubButton, TCLocalize("Github"), 0, &Button, LineSize, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://github.com/FusionSoloTeam/FusionClient");
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Version"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %s  \u00b7  %s %s", FUSION_NAME, FUSION_VERSION, CLIENT_NAME, CLIENT_RELEASE_VERSION);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoLabel(&Button, aBuf, FontSize, TEXTALIGN_ML);

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Fusion Developer"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	const float TeeSize = 50.0f;
	const float CardSize = TeeSize + MarginSmall;
	CUIRect TeeRect, DevCardRect;
	static CButtonContainer s_DevLinkButton;
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "Arm1g3d0n"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "Arm1g3d0n", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_DevLinkButton, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/FusionSoloTeam");
		RenderDevSkin(TeeRect.Center(), 50.0f, "eptrnanami", "default", false, 0, 0, 0, false, true);
	}

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(LineSize, &Button, &RightView);
	Ui()->DoLabel(&Button, TCLocalize("Skin: eptrnanami"), FontSize, TEXTALIGN_ML);
}

void CMenus::RenderSettingsTClientProfiles(CUIRect MainView)
{
	int *pCurrentUseCustomColor = m_Dummy ? &g_Config.m_ClDummyUseCustomColor : &g_Config.m_ClPlayerUseCustomColor;

	const char *pCurrentSkinName = m_Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
	const unsigned CurrentColorBody = *pCurrentUseCustomColor == 1 ? (m_Dummy ? g_Config.m_ClDummyColorBody : g_Config.m_ClPlayerColorBody) : -1;
	const unsigned CurrentColorFeet = *pCurrentUseCustomColor == 1 ? (m_Dummy ? g_Config.m_ClDummyColorFeet : g_Config.m_ClPlayerColorFeet) : -1;
	const int CurrentFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;
	const int Emote = m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
	const char *pCurrentName = m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName;
	const char *pCurrentClan = m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan;

	const CProfile CurrentProfile(
		CurrentColorBody,
		CurrentColorFeet,
		CurrentFlag,
		Emote,
		pCurrentSkinName,
		pCurrentName,
		pCurrentClan);

	static int s_SelectedProfile = -1;

	CUIRect Label, Button;

	auto RenderProfile = [&](CUIRect Rect, const CProfile &Profile, bool Main) {
		auto RenderCross = [&](CUIRect Cross, float MaxSize = 0.0f) {
			float MaxExtent = std::max(Cross.w, Cross.h);
			if(MaxSize > 0.0f && MaxExtent > MaxSize)
				MaxExtent = MaxSize;
			TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f));
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			const auto TextBoundingBox = TextRender()->TextBoundingBox(MaxExtent * 0.8f, FontIcon::XMARK);
			TextRender()->Text(Cross.x + (Cross.w - TextBoundingBox.m_W) / 2.0f, Cross.y + (Cross.h - TextBoundingBox.m_H) / 2.0f, MaxExtent * 0.8f, FontIcon::XMARK);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		};
		{
			CUIRect Skin;
			Rect.VSplitLeft(50.0f, &Skin, &Rect);
			if(!Main && Profile.m_SkinName[0] == '\0')
			{
				RenderCross(Skin, 20.0f);
			}
			else
			{
				CTeeRenderInfo TeeRenderInfo;
				TeeRenderInfo.Apply(GameClient()->m_Skins.Find(Profile.m_SkinName));
				TeeRenderInfo.ApplyColors(Profile.m_BodyColor >= 0 && Profile.m_FeetColor > 0, Profile.m_BodyColor, Profile.m_FeetColor);
				TeeRenderInfo.m_Size = 50.0f;
				const vec2 Pos = Skin.Center() + vec2(0.0f, TeeRenderInfo.m_Size / 10.0f); // Prevent overflow from hats
				vec2 Dir = vec2(1.0f, 0.0f);
				if(Main)
					RenderTeeCute(CAnimState::GetIdle(), &TeeRenderInfo, std::max(0, Profile.m_Emote), Dir, Pos, false);
				else
					RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, std::max(0, Profile.m_Emote), Dir, Pos);
			}
		}
		Rect.VSplitLeft(5.0f, nullptr, &Rect);
		{
			CUIRect Colors;
			Rect.VSplitLeft(10.0f, &Colors, &Rect);
			CUIRect BodyColor{Colors.Center().x - 5.0f, Colors.Center().y - 11.0f, 10.0f, 10.0f};
			CUIRect FeetColor{Colors.Center().x - 5.0f, Colors.Center().y + 1.0f, 10.0f, 10.0f};
			if(Profile.m_BodyColor >= 0 && Profile.m_FeetColor > 0)
			{
				// Body Color
				Graphics()->DrawRect(BodyColor.x, BodyColor.y, BodyColor.w, BodyColor.h,
					color_cast<ColorRGBA>(ColorHSLA(Profile.m_BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT)).WithAlpha(1.0f),
					IGraphics::CORNER_ALL, 2.0f);
				// Feet Color;
				Graphics()->DrawRect(FeetColor.x, FeetColor.y, FeetColor.w, FeetColor.h,
					color_cast<ColorRGBA>(ColorHSLA(Profile.m_FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT)).WithAlpha(1.0f),
					IGraphics::CORNER_ALL, 2.0f);
			}
			else
			{
				RenderCross(BodyColor);
				RenderCross(FeetColor);
			}
		}
		Rect.VSplitLeft(5.0f, nullptr, &Rect);
		{
			CUIRect Flag;
			Rect.VSplitRight(50.0f, &Rect, &Flag);
			Flag = {Flag.x, Flag.y + (Flag.h - 25.0f) / 2.0f, Flag.w, 25.0f};
			if(Profile.m_CountryFlag == -2)
				RenderCross(Flag, 20.0f);
			else
				GameClient()->m_CountryFlags.Render(Profile.m_CountryFlag, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), Flag.x, Flag.y, Flag.w, Flag.h);
		}
		Rect.VSplitRight(5.0f, &Rect, nullptr);
		{
			const float Height = Rect.h / 3.0f;
			if(Main)
			{
				char aBuf[256];
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), TCLocalize("Name: %s"), Profile.m_Name);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), TCLocalize("Clan: %s"), Profile.m_Clan);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), TCLocalize("Skin: %s"), Profile.m_SkinName);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
			}
			else
			{
				Rect.HSplitTop(Height, &Label, &Rect);
				Ui()->DoLabel(&Label, Profile.m_Name, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				Ui()->DoLabel(&Label, Profile.m_Clan, Height / LineSize * FontSize, TEXTALIGN_ML);
			}
		}
	};

	{
		CUIRect Top;
		MainView.HSplitTop(160.0f, &Top, &MainView);
		CUIRect Profiles, Settings, Actions;
		Top.VSplitLeft(300.0f, &Profiles, &Top);
		{
			CUIRect Skin;
			Profiles.HSplitTop(LineSize, &Label, &Profiles);
			Ui()->DoLabel(&Label, TCLocalize("Your profile"), FontSize, TEXTALIGN_ML);
			Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
			Profiles.HSplitTop(50.0f, &Skin, &Profiles);
			RenderProfile(Skin, CurrentProfile, true);

			// After load
			if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
				Profiles.HSplitTop(LineSize, &Label, &Profiles);
				Ui()->DoLabel(&Label, TCLocalize("After Load"), FontSize, TEXTALIGN_ML);
				Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
				Profiles.HSplitTop(50.0f, &Skin, &Profiles);

				CProfile LoadProfile = CurrentProfile;
				const CProfile &Profile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
				if(g_Config.m_TcProfileSkin && strlen(Profile.m_SkinName) != 0)
					str_copy(LoadProfile.m_SkinName, Profile.m_SkinName);
				if(g_Config.m_TcProfileColors && Profile.m_BodyColor != -1 && Profile.m_FeetColor != -1)
				{
					LoadProfile.m_BodyColor = Profile.m_BodyColor;
					LoadProfile.m_FeetColor = Profile.m_FeetColor;
				}
				if(g_Config.m_TcProfileEmote && Profile.m_Emote != -1)
					LoadProfile.m_Emote = Profile.m_Emote;
				if(g_Config.m_TcProfileName && strlen(Profile.m_Name) != 0)
					str_copy(LoadProfile.m_Name, Profile.m_Name);
				if(g_Config.m_TcProfileClan && (strlen(Profile.m_Clan) != 0 || g_Config.m_TcProfileOverwriteClanWithEmpty))
					str_copy(LoadProfile.m_Clan, Profile.m_Clan);
				if(g_Config.m_TcProfileFlag && Profile.m_CountryFlag != -2)
					LoadProfile.m_CountryFlag = Profile.m_CountryFlag;

				RenderProfile(Skin, LoadProfile, true);
			}
		}
		Top.VSplitLeft(20.0f, nullptr, &Top);
		Top.VSplitMid(&Settings, &Actions, 20.0f);
		{
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileSkin, TCLocalize("Save/Load Skin"), &g_Config.m_TcProfileSkin, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileColors, TCLocalize("Save/Load Colors"), &g_Config.m_TcProfileColors, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileEmote, TCLocalize("Save/Load Emote"), &g_Config.m_TcProfileEmote, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileName, TCLocalize("Save/Load Name"), &g_Config.m_TcProfileName, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileClan, TCLocalize("Save/Load Clan"), &g_Config.m_TcProfileClan, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileFlag, TCLocalize("Save/Load Flag"), &g_Config.m_TcProfileFlag, &Settings, LineSize);
		}
		{
			Actions.HSplitTop(30.0f, &Button, &Actions);
			static CButtonContainer s_LoadButton;
			if(DoButton_Menu(&s_LoadButton, TCLocalize("Load"), 0, &Button))
			{
				if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
				{
					CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
					GameClient()->m_SkinProfiles.ApplyProfile(m_Dummy, LoadProfile);
				}
			}
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			Actions.HSplitTop(30.0f, &Button, &Actions);
			static CButtonContainer s_SaveButton;
			if(DoButton_Menu(&s_SaveButton, TCLocalize("Save"), 0, &Button))
			{
				GameClient()->m_SkinProfiles.AddProfile(
					g_Config.m_TcProfileColors ? CurrentColorBody : -1,
					g_Config.m_TcProfileColors ? CurrentColorFeet : -1,
					g_Config.m_TcProfileFlag ? CurrentFlag : -2,
					g_Config.m_TcProfileEmote ? Emote : -1,
					g_Config.m_TcProfileSkin ? pCurrentSkinName : "",
					g_Config.m_TcProfileName ? pCurrentName : "",
					g_Config.m_TcProfileClan ? pCurrentClan : "");
			}
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			static int s_AllowDelete;
			DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, TCLocalize("Enable Deleting"), &s_AllowDelete, &Actions, LineSize);
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			if(s_AllowDelete)
			{
				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_DeleteButton;
				if(DoButton_Menu(&s_DeleteButton, TCLocalize("Delete"), 0, &Button))
					if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
						GameClient()->m_SkinProfiles.m_Profiles.erase(GameClient()->m_SkinProfiles.m_Profiles.begin() + s_SelectedProfile);
				Actions.HSplitTop(5.0f, nullptr, &Actions);

				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_OverrideButton;
				if(DoButton_Menu(&s_OverrideButton, TCLocalize("Override"), 0, &Button))
				{
					if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
					{
						GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile] = CProfile(
							g_Config.m_TcProfileColors ? CurrentColorBody : -1,
							g_Config.m_TcProfileColors ? CurrentColorFeet : -1,
							g_Config.m_TcProfileFlag ? CurrentFlag : -2,
							g_Config.m_TcProfileEmote ? Emote : -1,
							g_Config.m_TcProfileSkin ? pCurrentSkinName : "",
							g_Config.m_TcProfileName ? pCurrentName : "",
							g_Config.m_TcProfileClan ? pCurrentClan : "");
					}
				}
			}
		}
	}
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	{
		CUIRect Options;
		MainView.HSplitTop(LineSize, &Options, &MainView);

		Options.VSplitLeft(150.0f, &Button, &Options);
		if(DoButton_CheckBox(&m_Dummy, TCLocalize("Dummy"), m_Dummy, &Button))
			m_Dummy = 1 - m_Dummy;

		Options.VSplitLeft(150.0f, &Button, &Options);
		static int s_CustomColorId = 0;
		if(DoButton_CheckBox(&s_CustomColorId, TCLocalize("Custom colors"), *pCurrentUseCustomColor, &Button))
		{
			*pCurrentUseCustomColor = *pCurrentUseCustomColor ? 0 : 1;
			SetNeedSendInfo();
		}

		Button = Options;
		if(DoButton_CheckBox(&g_Config.m_TcProfileOverwriteClanWithEmpty, TCLocalize("Overwrite clan even if empty"), g_Config.m_TcProfileOverwriteClanWithEmpty, &Button))
			g_Config.m_TcProfileOverwriteClanWithEmpty = 1 - g_Config.m_TcProfileOverwriteClanWithEmpty;
	}
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	{
		CUIRect SelectorRect;
		MainView.HSplitBottom(LineSize + MarginSmall, &MainView, &SelectorRect);
		SelectorRect.HSplitTop(MarginSmall, nullptr, &SelectorRect);

		static CButtonContainer s_ProfilesFile;
		SelectorRect.VSplitLeft(130.0f, &Button, &SelectorRect);
		if(DoButton_Menu(&s_ProfilesFile, TCLocalize("Profiles file"), 0, &Button))
		{
			char aBuf[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTPROFILES].m_aConfigPath, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
	}

	const std::vector<CProfile> &ProfileList = GameClient()->m_SkinProfiles.m_Profiles;
	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, ProfileList.size(), MainView.w / 200.0f, 3, s_SelectedProfile, &MainView, true, IGraphics::CORNER_ALL, true);

	static bool s_Indexes[1024];

	for(size_t i = 0; i < ProfileList.size(); ++i)
	{
		CListboxItem Item = s_ListBox.DoNextItem(&s_Indexes[i], s_SelectedProfile >= 0 && (size_t)s_SelectedProfile == i);
		if(!Item.m_Visible)
			continue;

		RenderProfile(Item.m_Rect, ProfileList[i], false);
	}

	s_SelectedProfile = s_ListBox.DoEnd();
}

void CMenus::RenderSettingsTClientConfigs(CUIRect MainView)
{
	// hi hello, this is a relatively self contained mess, sorry if you're forking or need to modify this -Tater

	struct SIntStage
	{
		int m_Value;
	};
	struct SStrStage
	{
		std::string m_Value;
	};
	struct SColStage
	{
		unsigned m_Value;
	};
	static std::unordered_map<const SConfigVariable *, SIntStage> s_StagedInts;
	static std::unordered_map<const SConfigVariable *, SStrStage> s_StagedStrs;
	static std::unordered_map<const SConfigVariable *, SColStage> s_StagedCols;

	struct SIntState
	{
		CLineInputNumber m_Input;
		int m_LastValue = 0;
		bool m_Inited = false;
	};
	struct SStrState
	{
		CLineInputBuffered<512> m_Input;
		bool m_Inited = false;
	};
	struct SColState
	{
		unsigned m_LastValue = 0;
		unsigned m_Working = 0;
		bool m_Inited = false;
	};
	static std::unordered_map<const SConfigVariable *, SIntState> s_IntInputs;
	static std::unordered_map<const SConfigVariable *, SStrState> s_StrInputs;
	static std::unordered_map<const SConfigVariable *, SColState> s_ColInputs;

	auto ClearStagedAndCaches = [&]() {
		s_StagedInts.clear();
		s_StagedStrs.clear();
		s_StagedCols.clear();
		s_IntInputs.clear();
		s_StrInputs.clear();
		s_ColInputs.clear();
	};

	size_t ChangesCount = 0;

	CUIRect ApplyBar, TopBar, ListArea;
	MainView.VSplitRight(5.0f, &MainView, nullptr); // padding for scrollbar
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.HSplitTop(LineSize + MarginSmall, &ApplyBar, &MainView);
	MainView.HSplitTop(LineSize + MarginSmall, &TopBar, &ListArea);
	ListArea.HSplitTop(MarginSmall, nullptr, &ListArea);

	static CLineInputBuffered<128> s_SearchInput;

	ChangesCount = s_StagedInts.size() + s_StagedStrs.size() + s_StagedCols.size();
	{
		CUIRect LeftHalf, RightHalf;
		ApplyBar.VSplitMid(&LeftHalf, &RightHalf, 0.0f);
		CUIRect Row = LeftHalf;
		Row.HMargin(MarginSmall, &Row);
		Row.h = LineSize;
		Row.y = ApplyBar.y + (ApplyBar.h - LineSize) / 2.0f;

		const float BtnWidth = 120.0f;
		CUIRect ApplyBtn, ClearBtn, Counter;
		Row.VSplitLeft(BtnWidth, &ApplyBtn, &Row);
		Row.VSplitLeft(MarginSmall, nullptr, &Row);
		Row.VSplitLeft(BtnWidth, &ClearBtn, &Row);
		Row.VSplitLeft(MarginSmall, nullptr, &Counter);

		static CButtonContainer s_ApplyBtn, s_ClearBtn;
		int DisabledStyle = ChangesCount > 0 ? 0 : -1;
		const bool ApplyClicked = DoButton_Menu(&s_ApplyBtn, Localize("Apply Changes"), DisabledStyle, &ApplyBtn);
		if(ChangesCount > 0 && ApplyClicked)
		{
			// NOLINTBEGIN(bugprone-nondeterministic-pointer-iteration-order)
			for(const auto &It : s_StagedInts)
			{
				const SConfigVariable *pVar = It.first;
				char aCmd[256];
				str_format(aCmd, sizeof(aCmd), "%s %d", pVar->m_pScriptName, It.second.m_Value);
				Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			for(const auto &It : s_StagedStrs)
			{
				const SConfigVariable *pVar = It.first;
				char aEsc[1024];
				aEsc[0] = '\0';
				char *pDst = aEsc;
				str_escape(&pDst, It.second.m_Value.c_str(), aEsc + sizeof(aEsc));
				char aCmd[1200];
				str_format(aCmd, sizeof(aCmd), "%s \"%s\"", pVar->m_pScriptName, aEsc);
				Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			for(const auto &It : s_StagedCols)
			{
				const SConfigVariable *pVar = It.first;
				char aCmd[256];
				str_format(aCmd, sizeof(aCmd), "%s %u", pVar->m_pScriptName, It.second.m_Value);
				Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			// NOLINTEND(bugprone-nondeterministic-pointer-iteration-order)
			ClearStagedAndCaches();
		}
		const bool ClearClicked = DoButton_Menu(&s_ClearBtn, Localize("Clear Changes"), DisabledStyle, &ClearBtn);
		if(ChangesCount > 0 && ClearClicked)
		{
			ClearStagedAndCaches();
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("Changes: %d"), (int)ChangesCount);
		Ui()->DoLabel(&Counter, aBuf, FontSize, TEXTALIGN_ML);

		CUIRect RightRow = RightHalf;
		RightRow.h = LineSize;
		RightRow.y = ApplyBar.y + (ApplyBar.h - LineSize) / 2.0f;
		const float RightInset = 24.0f;
		RightRow.VSplitLeft(RightInset, nullptr, &RightRow);
		CUIRect TopCol1, TopCol2;
		RightRow.VSplitMid(&TopCol1, &TopCol2, 0.0f);
		if(DoButton_CheckBox(&g_Config.m_TcUiShowTClient, Localize("TClient"), g_Config.m_TcUiShowTClient, &TopCol1))
			g_Config.m_TcUiShowTClient ^= 1;
		if(DoButton_CheckBox(&g_Config.m_TcUiCompactList, Localize("Compact List"), g_Config.m_TcUiCompactList, &TopCol2))
			g_Config.m_TcUiCompactList ^= 1;
	}

	const float SearchLabelW = 60.0f;
	{
		CUIRect SearchRow = TopBar;
		SearchRow.h = LineSize;
		SearchRow.y = TopBar.y + (TopBar.h - LineSize) / 2.0f;

		CUIRect LeftHalf, RightHalf;
		SearchRow.VSplitMid(&LeftHalf, &RightHalf, 0.0f);

		CUIRect SearchLabel, SearchEdit;
		LeftHalf.VSplitLeft(SearchLabelW, &SearchLabel, &SearchEdit);
		Ui()->DoLabel(&SearchLabel, Localize("Search"), FontSize, TEXTALIGN_ML);
		Ui()->DoClearableEditBox(&s_SearchInput, &SearchEdit, EditBoxFontSize);

		CUIRect RightCol1, RightCol2;
		const float RightInset2 = 24.0f;
		RightHalf.VSplitLeft(RightInset2, nullptr, &RightHalf);
		RightHalf.VSplitMid(&RightCol1, &RightCol2, 0.0f);
		if(DoButton_CheckBox(&g_Config.m_TcUiShowDDNet, Localize("DDNet"), g_Config.m_TcUiShowDDNet, &RightCol1))
			g_Config.m_TcUiShowDDNet ^= 1;
		if(DoButton_CheckBox(&g_Config.m_TcUiOnlyModified, Localize("Only modified"), g_Config.m_TcUiOnlyModified, &RightCol2))
			g_Config.m_TcUiOnlyModified ^= 1;
	}

	const int FlagMask = CFGFLAG_CLIENT;

	struct SEntry
	{
		const SConfigVariable *m_pVar;
	};
	std::vector<SEntry> vEntries;
	vEntries.reserve(256);

	auto Collector = [](const SConfigVariable *pVar, void *pUserData) {
		auto *pVec = static_cast<std::vector<SEntry> *>(pUserData);
		pVec->push_back({pVar});
	};
	ConfigManager()->PossibleConfigVariables("", FlagMask, Collector, &vEntries);

	auto DomainEnabled = [&](ConfigDomain Domain) {
		if(Domain == ConfigDomain::DDNET)
			return g_Config.m_TcUiShowDDNet != 0;
		if(Domain == ConfigDomain::TCLIENT)
			return g_Config.m_TcUiShowTClient != 0;
		// only show DDNet and TClient domains
		return false;
	};

	const char *pSearch = s_SearchInput.GetString();

	auto IsEffectiveDefaultVar = [&](const SConfigVariable *p) -> bool {
		if(p->m_Type == SConfigVariable::VAR_INT)
		{
			const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(p);
			auto It = s_StagedInts.find(p);
			int Value = It != s_StagedInts.end() ? It->second.m_Value : *pInt->m_pVariable;
			return Value == pInt->m_Default;
		}
		if(p->m_Type == SConfigVariable::VAR_STRING)
		{
			const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(p);
			auto It = s_StagedStrs.find(p);
			const char *pValue = It != s_StagedStrs.end() ? It->second.m_Value.c_str() : pStr->m_pStr;
			return str_comp(pValue, pStr->m_pDefault) == 0;
		}
		if(p->m_Type == SConfigVariable::VAR_COLOR)
		{
			const SColorConfigVariable *pColor = static_cast<const SColorConfigVariable *>(p);
			auto It = s_StagedCols.find(p);
			unsigned Value = It != s_StagedCols.end() ? It->second.m_Value : *pColor->m_pVariable;
			return Value == pColor->m_Default;
		}
		return true;
	};

	std::vector<const SConfigVariable *> vpFiltered;
	vpFiltered.reserve(vEntries.size());
	for(const auto &E : vEntries)
	{
		const SConfigVariable *pVar = E.m_pVar;
		if(!DomainEnabled(pVar->m_ConfigDomain))
			continue;
		if(g_Config.m_TcUiOnlyModified && IsEffectiveDefaultVar(pVar))
			continue;
		if(pSearch && pSearch[0])
		{
			const char *pName = pVar->m_pScriptName ? pVar->m_pScriptName : "";
			const char *pHelp = pVar->m_pHelp ? pVar->m_pHelp : "";
			if(!str_find_nocase(pName, pSearch) && !str_find_nocase(pHelp, pSearch))
				continue;
		}
		vpFiltered.push_back(pVar);
	}

	std::sort(vpFiltered.begin(), vpFiltered.end(), [](const SConfigVariable *a, const SConfigVariable *b) {
		if(a->m_ConfigDomain != b->m_ConfigDomain)
			return a->m_ConfigDomain < b->m_ConfigDomain;
		return str_comp(a->m_pScriptName, b->m_pScriptName) < 0;
	});

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_ForceShowScrollbar = true;
	s_ScrollRegion.Begin(&ListArea, &ScrollParams);

	ListArea.VSplitRight(5.0f, &ListArea, nullptr);
	CUIRect Content = ListArea;

	auto DomainName = [](ConfigDomain Domain) {
		switch(Domain)
		{
		case ConfigDomain::DDNET: return "DDNet";
		case ConfigDomain::TCLIENT: return "TClient";
		default: return "Other";
		}
	};

	ConfigDomain CurrentDomain = ConfigDomain::NUM;
	for(const SConfigVariable *pVar : vpFiltered)
	{
		if(pVar->m_ConfigDomain != CurrentDomain)
		{
			CurrentDomain = pVar->m_ConfigDomain;
			CUIRect Header;
			Content.HSplitTop(HeadlineHeight, &Header, &Content);
			if(s_ScrollRegion.AddRect(Header))
				Ui()->DoLabel(&Header, DomainName(CurrentDomain), HeadlineFontSize, TEXTALIGN_ML);
			Content.HSplitTop(MarginSmall, nullptr, &Content);
		}

		CUIRect RowItem;
		const float RowHeight = g_Config.m_TcUiCompactList ? (std::max(LineSize, ColorPickerLineSize) + 5.0f) : 55.0f;
		Content.HSplitTop(RowHeight, &RowItem, &Content);
		Content.HSplitTop(MarginExtraSmall, nullptr, &Content);
		const bool Visible = s_ScrollRegion.AddRect(RowItem);
		if(!Visible)
			continue;

		const bool Modified = !IsEffectiveDefaultVar(pVar);
		const ColorRGBA BgModified = ColorRGBA(1.0f, 0.8f, 0.0f, 0.15f);
		const ColorRGBA BgNormal = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
		RowItem.Draw(Modified ? BgModified : BgNormal, IGraphics::CORNER_ALL, 6.0f);

		CUIRect RowContent;
		RowItem.Margin(5.0f, &RowContent);

		CUIRect TopLine, Below;
		if(g_Config.m_TcUiCompactList)
		{
			const float UsedHeight = (pVar->m_Type == SConfigVariable::VAR_COLOR) ? ColorPickerLineSize : LineSize;
			TopLine = RowContent;
			TopLine.h = UsedHeight;
			TopLine.y = round_to_int(RowContent.y + (RowContent.h - UsedHeight) / 2.0f);
			Below = RowContent;
		}
		else
		{
			RowContent.HSplitTop(LineSize, &TopLine, &Below);
		}
		CUIRect NameLine, Right;
		TopLine.VSplitRight(320.0f, &NameLine, &Right);
		NameLine.VSplitLeft(10.0f, nullptr, &NameLine);

		Ui()->DoLabel(&NameLine, pVar->m_pScriptName, FontSize, TEXTALIGN_ML);

		CUIRect Controls, ResetRect;
		Right.VSplitRight(120.0f, &Controls, &ResetRect);
		Controls.h = LineSize;
		Controls.y = TopLine.y + (TopLine.h - LineSize) / 2.0f;
		ResetRect.h = LineSize;
		ResetRect.y = Controls.y;
		Controls.VSplitRight(MarginSmall, &Controls, nullptr);

		if(!g_Config.m_TcUiCompactList)
		{
			CUIRect Help;
			Below.HSplitTop(2.0f, nullptr, &Below);
			Help = Below;
			Help.VSplitLeft(10.0f, nullptr, &Help);
			Ui()->DoLabel(&Help, pVar->m_pHelp ? pVar->m_pHelp : "", 11.0f, TEXTALIGN_ML);
		}

		static std::unordered_map<const SConfigVariable *, CButtonContainer> s_ResetBtns;
		if(Modified && pVar->m_Type != SConfigVariable::VAR_COLOR)
		{
			CButtonContainer &ResetBtn = s_ResetBtns[pVar];
			if(DoButton_Menu(&ResetBtn, Localize("Reset"), 0, &ResetRect))
			{
				if(pVar->m_Type == SConfigVariable::VAR_INT)
				{
					const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(pVar);
					s_StagedInts[pVar] = {pInt->m_Default};
				}
				else if(pVar->m_Type == SConfigVariable::VAR_STRING)
				{
					const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(pVar);
					s_StagedStrs[pVar] = {std::string(pStr->m_pDefault)};
				}
			}
		}

		if(pVar->m_Type == SConfigVariable::VAR_INT)
		{
			const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(pVar);
			// treat 0 1 ints as checkboxes
			if(pInt->m_Min == 0 && pInt->m_Max == 1)
			{
				const int Effective = s_StagedInts.contains(pVar) ? s_StagedInts[pVar].m_Value : *pInt->m_pVariable;
				if(DoButton_CheckBox(pVar, "", Effective, &Controls))
				{
					const int NewVal = Effective ? 0 : 1;
					if(NewVal == *pInt->m_pVariable)
						s_StagedInts.erase(pVar);
					else
						s_StagedInts[pVar] = {NewVal};
				}
			}
			else
			{
				SIntState &State = s_IntInputs[pVar];
				const int Effective = s_StagedInts.contains(pVar) ? s_StagedInts[pVar].m_Value : *pInt->m_pVariable;
				if(!State.m_Inited)
				{
					State.m_Input.SetInteger(Effective);
					State.m_LastValue = Effective;
					State.m_Inited = true;
				}
				else if(!State.m_Input.IsActive() && State.m_LastValue != Effective)
				{
					State.m_Input.SetInteger(Effective);
					State.m_LastValue = Effective;
				}

				CUIRect InputBox, Dummy;
				Controls.VSplitLeft(60.0f, &InputBox, &Dummy);

				if(Ui()->DoEditBox(&State.m_Input, &InputBox, EditBoxFontSize))
				{
					int NewVal = State.m_Input.GetInteger();
					bool InRange = true;
					if(pInt->m_Min != pInt->m_Max)
					{
						if(NewVal < pInt->m_Min)
							InRange = false;
						if(pInt->m_Max != 0 && NewVal > pInt->m_Max)
							InRange = false;
					}
					if(InRange && NewVal != State.m_LastValue)
					{
						if(NewVal == *pInt->m_pVariable)
							s_StagedInts.erase(pVar);
						else
							s_StagedInts[pVar] = {NewVal};
						State.m_LastValue = NewVal;
					}
				}
			}
		}
		else if(pVar->m_Type == SConfigVariable::VAR_STRING)
		{
			const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(pVar);
			SStrState &State = s_StrInputs[pVar];
			const char *Effective = s_StagedStrs.contains(pVar) ? s_StagedStrs[pVar].m_Value.c_str() : pStr->m_pStr;
			if(!State.m_Inited)
			{
				State.m_Input.Set(Effective);
				State.m_Inited = true;
			}
			else if(!State.m_Input.IsActive())
			{
				if(str_comp(State.m_Input.GetString(), Effective) != 0)
					State.m_Input.Set(Effective);
			}

			if(Ui()->DoEditBox(&State.m_Input, &Controls, EditBoxFontSize))
			{
				const char *NewVal = State.m_Input.GetString();
				if(str_comp(NewVal, pStr->m_pStr) == 0)
					s_StagedStrs.erase(pVar);
				else
					s_StagedStrs[pVar] = {std::string(NewVal)};
			}
		}
		else if(pVar->m_Type == SConfigVariable::VAR_COLOR)
		{
			const SColorConfigVariable *pCol = static_cast<const SColorConfigVariable *>(pVar);
			CUIRect ColorRect;
			ColorRect.x = Controls.x;
			ColorRect.h = ColorPickerLineSize;
			ColorRect.y = TopLine.y + (TopLine.h - ColorPickerLineSize) / 2.0f;
			ColorRect.w = ColorPickerLineSize + 8.0f + 60.0f;
			const ColorRGBA DefaultColor = color_cast<ColorRGBA>(ColorHSLA(pCol->m_Default, true).UnclampLighting(pCol->m_DarkestLighting));
			static std::unordered_map<const SConfigVariable *, CButtonContainer> s_ColorResetIds;
			CButtonContainer &ResetId = s_ColorResetIds[pVar];

			SColState &ColState = s_ColInputs[pVar];
			unsigned Effective = s_StagedCols.contains(pVar) ? s_StagedCols[pVar].m_Value : *pCol->m_pVariable;
			if(!ColState.m_Inited)
			{
				ColState.m_Working = Effective;
				ColState.m_LastValue = Effective;
				ColState.m_Inited = true;
			}
			else
			{
				const bool EditingThis = Ui()->IsPopupOpen(&m_ColorPickerPopupContext) && m_ColorPickerPopupContext.m_pHslaColor == &ColState.m_Working;
				if(!EditingThis && ColState.m_Working != Effective)
				{
					ColState.m_Working = Effective;
					ColState.m_LastValue = Effective;
				}
			}

			DoLine_ColorPicker(&ResetId, ColorPickerLineSize, ColorPickerLabelSize, 0.0f, &ColorRect, "", &ColState.m_Working, DefaultColor, false, nullptr, pCol->m_Alpha);
			if(ColState.m_Working != Effective)
			{
				if(ColState.m_Working == *pCol->m_pVariable)
					s_StagedCols.erase(pVar);
				else
					s_StagedCols[pVar] = {ColState.m_Working};
				ColState.m_LastValue = ColState.m_Working;
			}
		}
	}

	CUIRect EndPad{Content.x, Content.y, Content.w, 5.0f};
	s_ScrollRegion.AddRect(EndPad);
	s_ScrollRegion.End();
}
