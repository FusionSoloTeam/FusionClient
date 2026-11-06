#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_MENUS_MODERN_UI_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_MENUS_MODERN_UI_H

#include <game/client/component.h>
#include <game/client/ui.h>

#include <chrono>
#include <string>
#include <vector>

enum
{
	MODERN_UI_CLASSIC = 0,
	MODERN_UI_DARK = 1,
	MODERN_UI_WHITE = 2,
};

struct SModernUiPalette
{
	ColorRGBA m_BackgroundTop;
	ColorRGBA m_BackgroundBottom;
	ColorRGBA m_BackgroundAccent;
	ColorRGBA m_Panel;
	ColorRGBA m_Button;
	ColorRGBA m_ButtonRunning;
	ColorRGBA m_Text;
	ColorRGBA m_TextSecondary;
	ColorRGBA m_Shine;
	ColorRGBA m_Accent;
	ColorRGBA m_CardBackground;
	ColorRGBA m_CardBarTrack;
	ColorRGBA m_CardBarFill;
};

extern const SModernUiPalette gs_ModernUiDarkPalette;
extern const SModernUiPalette gs_ModernUiWhitePalette;

class CTClient;

class CMenusModern : public CComponentInterfaces
{
	struct SReleaseLine
	{
		std::string m_aText;
		int m_Kind; // 0 = text, 1 = heading, 2 = subheading
	};

	struct SExpandEntry
	{
		const void *m_pId;
		float m_Factor;
	};

	struct SFlash
	{
		bool m_Active = false;
		bool m_Applied = false;
		bool m_Editor = false;
		bool m_ShowStartTarget = false;
		float m_Progress = 0.0f;
		int m_TargetPage = -1;
	};

	struct SStartupAnim
	{
		enum
		{
			IDLE = 0,
			LOADING,
			GROWING,
		};

		int m_Phase = IDLE;
		float m_Progress = 0.0f;
		int m_TargetWidth = 0;
		int m_TargetHeight = 0;
		int m_TargetRefreshRate = 0;
		std::chrono::nanoseconds m_StartTime{0};
		std::chrono::nanoseconds m_GrownTime{0};
	};

	std::vector<SReleaseLine> m_vReleaseLines;
	const char *m_pReleaseSource = nullptr;
	float m_ReleaseContentHeight = 0.0f;
	float m_ReleaseScroll = 0.0f;

	std::vector<SExpandEntry> m_vExpandEntries;
	std::vector<SExpandEntry> m_vHoverEntries;
	float m_GradientPhase = 0.0f;
	float m_SplitProgress = 0.0f;
	float m_ExpandDt = 0.016f;
	bool m_StartupBeginTried = false;
	std::chrono::nanoseconds m_LastUpdate{0};
	SFlash m_Flash;
	SStartupAnim m_Startup;

	// view transition tracking: -1 = start menu, otherwise the active CMenus page (-2 = unset)
	int m_LastViewKey = -2;
	bool m_ReturnedFromEditor = false;

	static constexpr float STARTUP_CARD_WIDTH = 420.0f;
	static constexpr float STARTUP_CARD_HEIGHT = 96.0f;
	static constexpr float STARTUP_LOADING_SECONDS = 1.1f;
	static constexpr float STARTUP_GROW_SECONDS = 0.9f;
	static constexpr float STARTUP_REVEAL_SECONDS = 0.8f;
	static constexpr float FLASH_SECONDS = 0.6f;

	void RebuildReleaseLines();
	float RenderReleaseNotesPanel(CUIRect Panel, float RevealAlpha);
	void RenderBackgroundRect(const CUIRect &Rect, const SModernUiPalette &Palette);
	void RenderStartupCard();
	void UpdateStartup();
	float ButtonExpandFactor(const void *pId, float Target);
	float ButtonHoverFactor(const void *pId, float Target);
	float RevealAlphaForIndex(int Index) const;
	bool ModernCheckHotKey(int Key) const;
	void DrawGlyph(const CUIRect &Rect, const char *pGlyph, float Size, ColorRGBA Color);

public:
	static bool Enabled();
	static const SModernUiPalette &Palette();
	// shared per-button hover easing (0..1) for modern-styled controls outside the start menu
	static float HoverEase(const void *pId, bool bHot, float Dt);
	CTClient *GetTClient();

	void Update();
	void MaybeBeginStartup();
	void RenderBackground();
	void RenderStartMenu(CUIRect MainView);
	void RenderOverlay();
	bool StartupActive() const { return m_Startup.m_Phase != SStartupAnim::IDLE; }
	void RenderStartupOverlay();

	// Requests a page transition (with the flash animation) to the given menu page.
	void StartFlash(int TargetPage);
	// Requests a transition into the map editor (with the flash animation).
	void StartFlashEditor();
	// Requests a transition back to the start menu (with the flash animation).
	void StartFlashToMenu();

	void DrawPanel(const CUIRect &Rect, float AlphaMul = 1.0f, float Rounding = 12.0f);
	bool DoGlassButton(CButtonContainer *pButtonId, const char *pText, const CUIRect *pRect, ColorRGBA Color, bool AllowClick = true, const char *pIcon = nullptr, bool Primary = false, float Rounding = 10.0f);
	CUIRect ExpandRect(const CUIRect &Rect, const void *pId);
};

#endif
