// my fav opus made this :happy:

#include <base/math.h>
#include <base/str.h>
#include <base/time.h>
#include <base/types.h>

#include <engine/client.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/components/console.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/components/tclient/menus_modern_ui.h>
#include <game/client/components/tclient/tclient.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/localization.h>
#include <game/version.h>

#include <algorithm>
#include <cmath>
#include <map>

const SModernUiPalette gs_ModernUiDarkPalette = {
	ColorRGBA(0.075f, 0.084f, 0.108f, 1.0f), // background top
	ColorRGBA(0.014f, 0.014f, 0.018f, 1.0f), // background bottom
	ColorRGBA(0.100f, 0.120f, 0.130f, 0.25f), // background accent
	ColorRGBA(0.078f, 0.078f, 0.086f, 1.0f), // panel
	ColorRGBA(0.110f, 0.110f, 0.122f, 1.0f), // button
	ColorRGBA(0.110f, 0.300f, 0.230f, 1.0f), // button running
	ColorRGBA(0.920f, 0.930f, 0.950f, 1.0f), // text
	ColorRGBA(0.550f, 0.570f, 0.620f, 1.0f), // text secondary
	ColorRGBA(1.000f, 1.000f, 1.000f, 0.10f), // shine
	ColorRGBA(0.180f, 0.780f, 0.620f, 1.0f), // accent
	ColorRGBA(0.078f, 0.078f, 0.094f, 1.0f), // card background
	ColorRGBA(0.000f, 0.000f, 0.000f, 0.55f), // card bar track
	ColorRGBA(0.850f, 0.870f, 1.000f, 0.95f), // card bar fill
};

const SModernUiPalette gs_ModernUiWhitePalette = {
	ColorRGBA(0.945f, 0.949f, 0.957f, 1.0f), // background top
	ColorRGBA(0.886f, 0.890f, 0.902f, 1.0f), // background bottom
	ColorRGBA(1.000f, 1.000f, 1.000f, 0.45f), // background accent
	ColorRGBA(1.000f, 1.000f, 1.000f, 1.0f), // panel
	ColorRGBA(0.925f, 0.929f, 0.937f, 1.0f), // button
	ColorRGBA(0.700f, 0.900f, 0.760f, 1.0f), // button running
	ColorRGBA(0.100f, 0.105f, 0.130f, 1.0f), // text
	ColorRGBA(0.420f, 0.440f, 0.470f, 1.0f), // text secondary
	ColorRGBA(1.000f, 1.000f, 1.000f, 0.55f), // shine
	ColorRGBA(0.100f, 0.600f, 0.480f, 1.0f), // accent
	ColorRGBA(0.937f, 0.937f, 0.953f, 1.0f), // card background
	ColorRGBA(0.000f, 0.000f, 0.000f, 0.12f), // card bar track
	ColorRGBA(0.150f, 0.160f, 0.180f, 0.85f), // card bar fill
};

static float EaseInOutCubic(float T)
{
	T = std::clamp(T, 0.0f, 1.0f);
	return T < 0.5f ? 4.0f * T * T * T : 1.0f - std::pow(-2.0f * T + 2.0f, 3.0f) / 2.0f;
}

static ColorRGBA MixColor(const ColorRGBA &A, const ColorRGBA &B, float T)
{
	return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, A.a + (B.a - A.a) * T);
}

bool CMenusModern::Enabled()
{
	return g_Config.m_FcModernUi != MODERN_UI_CLASSIC;
}

const SModernUiPalette &CMenusModern::Palette()
{
	return g_Config.m_FcModernUi == MODERN_UI_WHITE ? gs_ModernUiWhitePalette : gs_ModernUiDarkPalette;
}

float CMenusModern::HoverEase(const void *pId, bool bHot, float Dt)
{
	if(!g_Config.m_FcModernUiAnim)
		return bHot ? 1.0f : 0.0f;

	static std::map<const void *, float> s_Hover;
	float &Factor = s_Hover[pId];
	const float Target = bHot ? 1.0f : 0.0f;
	Factor += (Target - Factor) * std::clamp(Dt * 9.0f, 0.0f, 1.0f);
	if(Factor < 0.002f)
		Factor = 0.0f;
	else if(Factor > 0.998f)
		Factor = 1.0f;
	return Factor;
}

CTClient *CMenusModern::GetTClient()
{
	return GameClient() ? &GameClient()->m_TClient : nullptr;
}

void CMenusModern::DrawGlyph(const CUIRect &Rect, const char *pGlyph, float Size, ColorRGBA Color)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING |
		ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	const ColorRGBA Prev = TextRender()->DefaultTextColor();
	TextRender()->TextColor(Color);
	Ui()->DoLabel(const_cast<CUIRect *>(&Rect), pGlyph, Size, TEXTALIGN_MC);
	TextRender()->TextColor(Prev);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CMenusModern::DrawPanel(const CUIRect &Rect, float AlphaMul, float Rounding)
{
	const SModernUiPalette &Pal = Palette();
	const float PanelAlpha = std::clamp(g_Config.m_FcModernUiPanelAlpha, 10, 100) / 100.0f * AlphaMul;

	// subtle border ring, then a flat fill inset by 1px
	ColorRGBA Border = MixColor(Pal.m_Panel, Pal.m_Text, 0.10f);
	Border.a = PanelAlpha;
	Rect.Draw(Border, IGraphics::CORNER_ALL, Rounding);

	CUIRect FillRect;
	Rect.Margin(1.0f, &FillRect);
	ColorRGBA Base = Pal.m_Panel;
	Base.a *= PanelAlpha;
	FillRect.Draw(Base, IGraphics::CORNER_ALL, maximum(Rounding - 1.0f, 0.0f));
}

CUIRect CMenusModern::ExpandRect(const CUIRect &Rect, const void *pId)
{
	float Factor = 1.0f;
	for(const SExpandEntry &Entry : m_vExpandEntries)
	{
		if(Entry.m_pId == pId)
		{
			Factor = Entry.m_Factor;
			break;
		}
	}

	const float CenterX = Rect.x + Rect.w / 2.0f;
	const float CenterY = Rect.y + Rect.h / 2.0f;
	const float NewW = Rect.w * Factor;
	const float NewH = Rect.h * Factor;
	return CUIRect{CenterX - NewW / 2.0f, CenterY - NewH / 2.0f, NewW, NewH};
}

float CMenusModern::ButtonExpandFactor(const void *pId, float Target)
{
	if(!g_Config.m_FcModernUiAnim)
		return 1.0f;

	for(SExpandEntry &Entry : m_vExpandEntries)
	{
		if(Entry.m_pId == pId)
		{
			const float Speed = 12.0f;
			Entry.m_Factor += std::clamp(Target - Entry.m_Factor, -Speed * m_ExpandDt, Speed * m_ExpandDt);
			return Entry.m_Factor;
		}
	}

	m_vExpandEntries.push_back({pId, 1.0f});
	return 1.0f;
}

float CMenusModern::ButtonHoverFactor(const void *pId, float Target)
{
	if(!g_Config.m_FcModernUiAnim)
		return Target;

	for(SExpandEntry &Entry : m_vHoverEntries)
	{
		if(Entry.m_pId == pId)
		{
			// exponential ease toward the target -> smooth light-up and fade-out
			const float Smoothing = std::clamp(m_ExpandDt * 9.0f, 0.0f, 1.0f);
			Entry.m_Factor += (Target - Entry.m_Factor) * Smoothing;
			if(Entry.m_Factor < 0.002f)
				Entry.m_Factor = 0.0f;
			return Entry.m_Factor;
		}
	}

	m_vHoverEntries.push_back({pId, Target});
	return Target;
}

bool CMenusModern::DoGlassButton(CButtonContainer *pButtonId, const char *pText, const CUIRect *pRect, ColorRGBA Color, bool AllowClick, const char *pIcon, bool Primary, float Rounding)
{
	const SModernUiPalette &Pal = Palette();
	const bool bHot = Ui()->HotItem() == pButtonId;
	const float Hover = ButtonHoverFactor(pButtonId, bHot ? 1.0f : 0.0f);
	const bool Active = Ui()->ActiveItem() == pButtonId;
	const float Reveal = Color.a;

	// flat fill; primary uses the accent color
	ColorRGBA Fill = Primary ? Pal.m_Accent : Color;
	Fill.a = Reveal;
	const bool bDarkSurface = (Fill.r + Fill.g + Fill.b) / 3.0f < 0.5f;
	Fill = MixColor(Fill, bDarkSurface ? ColorRGBA(1, 1, 1, Fill.a) : ColorRGBA(0, 0, 0, Fill.a), 0.10f * Hover);
	if(Active)
		Fill = MixColor(Fill, bDarkSurface ? ColorRGBA(0, 0, 0, Fill.a) : ColorRGBA(1, 1, 1, Fill.a), 0.12f);

	// subtle 1px border ring for definition
	ColorRGBA Border = MixColor(Fill, Pal.m_Text, 0.12f);
	Border.a = Reveal * (0.30f + 0.25f * Hover);
	pRect->Draw(Border, IGraphics::CORNER_ALL, Rounding);

	CUIRect FillRect;
	pRect->Margin(1.0f, &FillRect);
	FillRect.Draw(Fill, IGraphics::CORNER_ALL, maximum(Rounding - 1.0f, 0.0f));

	const ColorRGBA GlyphColor = Primary ? ColorRGBA(0.05f, 0.09f, 0.08f, Reveal) : Pal.m_Text;
	// explicit, clamped sizes: the row rects are tall, so ms_FontmodHeight (meant for ~24px
	// single-line rects) would blow the label up; derive a readable size from the row height.
	const float GlyphSize = std::clamp(pRect->h * 0.30f, 14.0f, 24.0f);
	const float FontSize = std::clamp(pRect->h * 0.28f, 13.0f, 22.0f);

	CUIRect Work = *pRect;
	Work.VSplitLeft(18.0f, nullptr, &Work);
	Work.VSplitRight(16.0f, &Work, nullptr);

	CUIRect ChevronArea;
	if(Primary || Hover > 0.0f)
		Work.VSplitRight(GlyphSize * 1.6f, &Work, &ChevronArea);

	CUIRect IconArea;
	if(pIcon)
	{
		Work.VSplitLeft(GlyphSize * 1.6f, &IconArea, &Work);
		Work.VSplitLeft(12.0f, nullptr, &Work);
	}

	if(IconArea.w > 0.0f)
		DrawGlyph(IconArea, pIcon, GlyphSize, GlyphColor);
	if(ChevronArea.w > 0.0f)
	{
		ColorRGBA ChevronColor = GlyphColor;
		if(!Primary)
			ChevronColor.a = Reveal * Hover;
		DrawGlyph(ChevronArea, FontIcon::CHEVRON_RIGHT, GlyphSize * 0.85f, ChevronColor);
	}

	const ColorRGBA PrevTextColor = TextRender()->DefaultTextColor();
	TextRender()->TextColor(GlyphColor);
	Ui()->DoLabel(&Work, pText, FontSize, TEXTALIGN_ML);
	TextRender()->TextColor(PrevTextColor);

	if(!AllowClick)
		return false;
	return Ui()->DoButtonLogic(pButtonId, 0, pRect, BUTTONFLAG_LEFT) != 0;
}

void CMenusModern::RenderBackgroundRect(const CUIRect &Rect, const SModernUiPalette &Pal)
{
	float Phase = m_GradientPhase;
	if(!g_Config.m_FcModernUiAnim)
		Phase = 0.15f;

	// diagonal base (bright top-left -> dark bottom-right) with a moving teal sheen band
	auto ColorAt = [&](float U, float V) {
		const float D = (U + (1.0f - V)) * 0.5f; // 0 at top-left .. 1 at bottom-right
		ColorRGBA Col = MixColor(Pal.m_BackgroundBottom, Pal.m_BackgroundTop, 1.0f - D);
		// persistent faint teal toward the top-left so the gradient itself reads
		Col = MixColor(Col, Pal.m_Accent, 0.06f * (1.0f - D));
		float Diff = D - Phase;
		Diff -= std::round(Diff); // wrap into [-0.5, 0.5] so the band loops seamlessly
		const float Band = std::pow(std::max(0.0f, 0.5f + 0.5f * std::cos(2.0f * pi * Diff)), 2.0f);
		Col = MixColor(Col, Pal.m_Accent, 0.22f * Band); // the moving highlight
		Col.a = 1.0f;
		return Col;
	};

	const int Cols = 12;
	const int Rows = 9;
	const float CellW = Rect.w / Cols;
	const float CellH = Rect.h / Rows;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(int j = 0; j < Rows; j++)
	{
		for(int i = 0; i < Cols; i++)
		{
			const float U0 = (float)i / Cols;
			const float U1 = (float)(i + 1) / Cols;
			const float V0 = (float)j / Rows;
			const float V1 = (float)(j + 1) / Rows;
			Graphics()->SetColor4(ColorAt(U0, V0), ColorAt(U1, V0), ColorAt(U0, V1), ColorAt(U1, V1));
			const IGraphics::CQuadItem Item(Rect.x + i * CellW, Rect.y + j * CellH, CellW + 1.0f, CellH + 1.0f);
			Graphics()->QuadsDrawTL(&Item, 1);
		}
	}
	Graphics()->QuadsEnd();
}

void CMenusModern::RenderBackground()
{
	RenderBackgroundRect(*Ui()->Screen(), Palette());
}

void CMenusModern::RenderStartupCard()
{
	const SModernUiPalette &Pal = Palette();
	CUIRect Screen = *Ui()->Screen();

	RenderBackgroundRect(Screen, Pal);

	CUIRect Card = Screen;
	Card.Draw(Pal.m_CardBackground, IGraphics::CORNER_NONE, 0.0f);

	CUIRect Bar;
	Card.HSplitBottom(4.0f, nullptr, &Bar);
	Bar.VMargin(Bar.h * 2.0f, &Bar);
	Bar.Draw(Pal.m_CardBarTrack, IGraphics::CORNER_ALL, Bar.h / 2.0f);

	CUIRect Fill = Bar;
	Fill.w *= std::clamp(m_Startup.m_Progress, 0.0f, 1.0f);
	if(Fill.w > Fill.h)
	{
		Fill.Draw(Pal.m_CardBarFill, IGraphics::CORNER_ALL, Fill.h / 2.0f);
	}
}

void CMenusModern::RenderStartupOverlay()
{
	RenderStartupCard();
}

void CMenusModern::UpdateStartup()
{
	if(m_Startup.m_Phase == SStartupAnim::IDLE)
		return;

	const std::chrono::nanoseconds Now = time_get_nanoseconds();
	if(m_Startup.m_StartTime.count() == 0)
		m_Startup.m_StartTime = Now;

	const auto Elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(Now - m_Startup.m_StartTime).count();

	if(m_Startup.m_Phase == SStartupAnim::LOADING)
	{
		m_Startup.m_Progress = std::clamp(Elapsed / STARTUP_LOADING_SECONDS, 0.0f, 0.99f);
		if(Elapsed >= STARTUP_LOADING_SECONDS)
		{
			m_Startup.m_Phase = SStartupAnim::GROWING;
			m_Startup.m_StartTime = Now;
		}
		return;
	}

	const float T = std::clamp(Elapsed / STARTUP_GROW_SECONDS, 0.0f, 1.0f);
	m_Startup.m_Progress = 1.0f;
	const float Eased = EaseInOutCubic(T);
	const int NewW = maximum((int)STARTUP_CARD_WIDTH, (int)mix((float)m_Startup.m_TargetWidth, (float)STARTUP_CARD_WIDTH, 1.0f - Eased));
	const int NewH = maximum((int)STARTUP_CARD_HEIGHT, (int)mix((float)m_Startup.m_TargetHeight, (float)STARTUP_CARD_HEIGHT, 1.0f - Eased));
	if(NewW != Graphics()->WindowWidth() || NewH != Graphics()->WindowHeight())
	{
		Graphics()->Resize(NewW, NewH, m_Startup.m_TargetRefreshRate);
		Graphics()->CenterWindow();
	}
	if(T >= 1.0f)
	{
		if(Graphics()->WindowWidth() != m_Startup.m_TargetWidth || Graphics()->WindowHeight() != m_Startup.m_TargetHeight)
		{
			Graphics()->Resize(m_Startup.m_TargetWidth, m_Startup.m_TargetHeight, m_Startup.m_TargetRefreshRate);
			Graphics()->CenterWindow();
		}
		m_Startup.m_Phase = SStartupAnim::IDLE;
		m_Startup.m_GrownTime = Now;
	}
}

void CMenusModern::MaybeBeginStartup()
{
	if(m_StartupBeginTried)
		return;
	m_StartupBeginTried = true;

	if(!Enabled() || !g_Config.m_FcModernUiAnim)
		return;
	if(g_Config.m_GfxFullscreen != 0)
		return;

	const int TargetW = Graphics()->WindowWidth();
	const int TargetH = Graphics()->WindowHeight();
	if(TargetW <= (int)STARTUP_CARD_WIDTH + 8 || TargetH <= (int)STARTUP_CARD_HEIGHT + 8)
		return;

	m_Startup.m_TargetWidth = TargetW;
	m_Startup.m_TargetHeight = TargetH;
	m_Startup.m_TargetRefreshRate = g_Config.m_GfxScreenRefreshRate;
	m_Startup.m_Phase = SStartupAnim::LOADING;
	m_Startup.m_Progress = 0.0f;
	m_Startup.m_StartTime = std::chrono::nanoseconds(0);
	m_Startup.m_GrownTime = std::chrono::nanoseconds(0);

	Graphics()->Resize((int)STARTUP_CARD_WIDTH, (int)STARTUP_CARD_HEIGHT, m_Startup.m_TargetRefreshRate);
	Graphics()->CenterWindow();
}

float CMenusModern::RevealAlphaForIndex(int Index) const
{
	if(!g_Config.m_FcModernUiAnim || m_Startup.m_GrownTime.count() == 0)
		return 1.0f;

	const auto Since = std::chrono::duration_cast<std::chrono::duration<float>>(time_get_nanoseconds() - m_Startup.m_GrownTime).count();
	const float Start = Index * 0.04f;
	const float Duration = 0.35f;
	return std::clamp((Since - Start) / Duration, 0.0f, 1.0f);
}

void CMenusModern::RebuildReleaseLines()
{
	m_vReleaseLines.clear();

	CTClient *pTClient = GetTClient();
	if(pTClient == nullptr || pTClient->m_aReleaseNotesBody[0] == '\0')
		return;

	m_pReleaseSource = pTClient->m_aReleaseNotesBody;

	const char *pBody = pTClient->m_aReleaseNotesBody;

	std::string View;
	for(const char *p = pBody; *p; ++p)
	{
		if(*p == '\r')
			continue;
		if(*p != '\n')
		{
			View.push_back(*p);
			continue;
		}

		// process the finished line
		std::string Line = View;
		View.clear();

		size_t First = Line.find_first_not_of(" \t");
		if(First == std::string::npos)
			continue;
		Line = Line.substr(First);
		size_t Last = Line.find_last_not_of(" \t");
		Line = Line.substr(0, Last + 1);

		if(Line.empty() || Line[0] == '!')
			continue; // blank lines and images are skipped
		if(str_startswith(Line.c_str(), "```"))
			continue;

		// markdown table rows: drop separator rows (| --- | --- |) and collapse the rest
		// into a readable "cell \u00b7 cell" line instead of showing raw pipes.
		if(Line.find('|') != std::string::npos)
		{
			bool bSeparator = true;
			for(char C : Line)
			{
				if(C != '|' && C != '-' && C != ':' && C != ' ' && C != '\t')
				{
					bSeparator = false;
					break;
				}
			}
			if(bSeparator)
				continue;

			std::string Row;
			size_t Start = 0;
			while(true)
			{
				const size_t End = Line.find('|', Start);
				std::string Cell = (End == std::string::npos) ? Line.substr(Start) : Line.substr(Start, End - Start);
				const size_t A = Cell.find_first_not_of(" \t");
				const size_t B = Cell.find_last_not_of(" \t");
				Cell = (A == std::string::npos) ? std::string() : Cell.substr(A, B - A + 1);
				// drop markdown code-span backticks inside table cells
				Cell.erase(std::remove(Cell.begin(), Cell.end(), '`'), Cell.end());
				if(!Cell.empty())
				{
					if(!Row.empty())
						Row += "  \xc2\xb7  ";
					Row += Cell;
				}
				if(End == std::string::npos)
					break;
				Start = End + 1;
			}
			if(!Row.empty())
				m_vReleaseLines.push_back({Row, 0});
			continue;
		}

		int Kind = 0;
		while(!Line.empty() && Line[0] == '#')
		{
			Kind++;
			Line.erase(Line.begin());
		}
		if(Kind > 0)
		{
			First = Line.find_first_not_of(" \t");
			if(First != std::string::npos && First > 0)
				Line = Line.substr(First);
			Kind = Kind == 1 ? 1 : 2;
		}
		else if(str_startswith(Line.c_str(), "- ") || str_startswith(Line.c_str(), "* "))
		{
			Line = "\xe2\x80\xa2 " + Line.substr(2);
		}

		// strip markdown emphasis
		std::string Clean;
		Clean.reserve(Line.size());
		for(size_t i = 0; i < Line.size(); i++)
		{
			if(Line[i] == '\\')
			{
				Clean.push_back('\\');
				if(i + 1 < Line.size())
				{
					Clean.push_back(Line[i + 1]);
					i++;
				}
				continue;
			}
			if(Line[i] == '[')
			{
				const size_t Close = Line.find(']', i);
				if(Close != std::string::npos && Close + 1 < Line.size() && Line[Close + 1] == '(')
				{
					const size_t Paren = Line.find(')', Close + 1);
					if(Paren != std::string::npos)
					{
						const std::string Name = Line.substr(i + 1, Close - i - 1);
						const std::string Url = Line.substr(Close + 2, Paren - Close - 2);
						if(Name.empty() && str_startswith(Url.c_str(), "http"))
							Clean += Url;
						else
							Clean += Name;
						i = Paren;
						continue;
					}
				}
			}
			if(Line[i] == '*' || Line[i] == '`' || Line[i] == '~')
				continue;
			Clean.push_back(Line[i]);
		}

		if(!Clean.empty())
			m_vReleaseLines.push_back({Clean, Kind});
	}
}

float CMenusModern::RenderReleaseNotesPanel(CUIRect Panel, float RevealAlpha)
{
	const SModernUiPalette &Pal = Palette();
	DrawPanel(Panel, RevealAlpha);

	CTClient *pTClient = GetTClient();

	CUIRect Content = Panel;
	Content.Margin(18.0f, &Content);

	CUIRect Header, NotesArea, LinksArea;
	Content.HSplitTop(52.0f, &Header, &Content);
	Content.HSplitBottom(150.0f, &NotesArea, &LinksArea);

	const ColorRGBA LineColor = MixColor(Pal.m_Panel, Pal.m_Text, 0.14f);
	auto Hairline = [&](CUIRect R) {
		R.Draw(ColorRGBA(LineColor.r, LineColor.g, LineColor.b, 0.5f * RevealAlpha), IGraphics::CORNER_NONE, 0.0f);
	};

	// header: kicker + big version tag
	{
		char aVersion[64];
		if(pTClient && pTClient->m_aReleaseTag[0] != '\0')
			str_copy(aVersion, pTClient->m_aReleaseTag, sizeof(aVersion));
		else
			str_format(aVersion, sizeof(aVersion), "v%s", FUSION_VERSION);

		CUIRect Kicker, Ver;
		Header.HSplitTop(16.0f, &Kicker, &Ver);
		TextRender()->TextColor(Pal.m_TextSecondary);
		Ui()->DoLabel(&Kicker, TCLocalize("News"), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(Pal.m_Text);
		Ui()->DoLabel(&Ver, aVersion, 22.0f, TEXTALIGN_ML);
	}

	// release notes body with scroll
	{
		CUIRect Scrollbar, Viewport;
		NotesArea.VSplitRight(12.0f, &Viewport, &Scrollbar);
		Viewport.VSplitRight(6.0f, &Viewport, nullptr);

		const bool NotesAvailable = pTClient && pTClient->m_aReleaseNotesBody[0] != '\0';
		if(pTClient && m_pReleaseSource != pTClient->m_aReleaseNotesBody)
			RebuildReleaseLines();

		const float LineHeight = 17.0f;
		const float ContentHeight = NotesAvailable ? (float)m_vReleaseLines.size() * LineHeight : 0.0f;

		if(ContentHeight > Viewport.h)
		{
			const float MaxScroll = ContentHeight - Viewport.h;
			if(Ui()->MouseHovered(&Viewport))
			{
				if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
					m_ReleaseScroll -= 3.0f * LineHeight;
				else if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
					m_ReleaseScroll += 3.0f * LineHeight;
			}
			m_ReleaseScroll = Ui()->DoScrollbarV(&m_ReleaseScroll, &Scrollbar, m_ReleaseScroll);
			m_ReleaseScroll = std::clamp(m_ReleaseScroll, 0.0f, MaxScroll);
		}
		else
		{
			m_ReleaseScroll = 0.0f;
		}

		Ui()->ClipEnable(&Viewport);
		if(NotesAvailable)
		{
			float Y = Viewport.y - m_ReleaseScroll;
			for(const SReleaseLine &Line : m_vReleaseLines)
			{
				if(Y > Viewport.y + Viewport.h)
					break;
				if(Y + LineHeight >= Viewport.y)
				{
					CUIRect LineRect{Viewport.x, Y, Viewport.w, LineHeight};
					const float FontSize = Line.m_Kind == 1 ? 15.0f : (Line.m_Kind == 2 ? 13.5f : 11.5f);
					SLabelProperties Props;
					Props.m_MaxWidth = Viewport.w;
					if(Line.m_Kind != 0)
						TextRender()->TextColor(Pal.m_Text);
					else
						TextRender()->TextColor(MixColor(Pal.m_Text, Pal.m_TextSecondary, 0.25f));
					Ui()->DoLabel(&LineRect, Line.m_aText.c_str(), FontSize, TEXTALIGN_ML, Props);
				}
				Y += LineHeight;
			}
			TextRender()->TextColor(Pal.m_Text);
		}
		else
		{
			const char *pPlaceholder = pTClient && pTClient->m_FetchedTClientInfo ? TCLocalize("No release notes available") : TCLocalize("Fetching release notes\xe2\x80\xa6");
			TextRender()->TextColor(Pal.m_TextSecondary);
			CUIRect Placeholder = Viewport;
			Placeholder.HSplitTop(16.0f, &Placeholder, nullptr);
			Ui()->DoLabel(&Placeholder, pPlaceholder, 12.0f, TEXTALIGN_ML);
			TextRender()->TextColor(Pal.m_Text);
		}
		Ui()->ClipDisable();
	}

	// changelog link
	{
		CUIRect Changelog, Rest;
		LinksArea.HSplitTop(30.0f, &Changelog, &LinksArea);
		Changelog.VSplitLeft(120.0f, &Changelog, &Rest);
		static CButtonContainer s_ChangelogButton;
		const bool bHot = Ui()->HotItem() == &s_ChangelogButton;
		if(Ui()->DoButtonLogic(&s_ChangelogButton, 0, &Changelog, BUTTONFLAG_LEFT))
			Client()->ViewLink("https://github.com/FusionSoloTeam/FusionClient/releases/latest");

		CUIRect Label, Chev;
		Changelog.VSplitRight(20.0f, &Label, &Chev);
		const ColorRGBA Prev = TextRender()->DefaultTextColor();
		TextRender()->TextColor(Pal.m_Accent);
		Ui()->DoLabel(&Label, TCLocalize("Changelog"), Changelog.h * CUi::ms_FontmodHeight * 0.9f, TEXTALIGN_ML);
		DrawGlyph(Chev, FontIcon::CHEVRON_RIGHT, Changelog.h * 0.4f, Pal.m_Accent);
		TextRender()->TextColor(Prev);
		(void)bHot;
	}

	// hairline + 2x2 link grid
	{
		CUIRect Sep;
		LinksArea.HSplitTop(1.0f, &Sep, &LinksArea);
		Hairline(Sep);

		CUIRect Row1, Row2;
		LinksArea.HSplitTop(LinksArea.h / 2.0f, &Row1, &Row2);

		CUIRect HSep;
		Row1.HSplitBottom(1.0f, &Row1, &HSep);
		Hairline(HSep);

		CUIRect R1C1, R1VSep, R1C2;
		Row1.VSplitLeft((Row1.w - 1.0f) / 2.0f, &R1C1, &R1VSep);
		R1VSep.VSplitLeft(1.0f, &R1VSep, &R1C2);
		Hairline(R1VSep);

		CUIRect R2C1, R2VSep, R2C2;
		Row2.VSplitLeft((Row2.w - 1.0f) / 2.0f, &R2C1, &R2VSep);
		R2VSep.VSplitLeft(1.0f, &R2VSep, &R2C2);
		Hairline(R2VSep);

		static CButtonContainer s_aLinkButtons[4];
		auto Cell = [&](const CUIRect &R, const char *pGlyph, const char *pText, CButtonContainer *pId) -> bool {
			const bool bClicked = Ui()->DoButtonLogic(pId, 0, &R, BUTTONFLAG_LEFT) != 0;
			const bool bHot = Ui()->HotItem() == pId;
			if(bHot)
			{
				CUIRect Hl = R;
				Hl.Margin(2.0f, &Hl);
				Hl.Draw(MixColor(Pal.m_Panel, Pal.m_Text, 0.06f), IGraphics::CORNER_ALL, 6.0f);
			}
			CUIRect Work = R;
			Work.VSplitLeft(18.0f, nullptr, &Work);
			CUIRect Icon, Label;
			Work.VSplitLeft(R.h * 0.7f, &Icon, &Label);
			DrawGlyph(Icon, pGlyph, R.h * 0.40f, bHot ? Pal.m_Text : Pal.m_TextSecondary);
			const ColorRGBA Prev = TextRender()->DefaultTextColor();
			TextRender()->TextColor(bHot ? Pal.m_Text : Pal.m_TextSecondary);
			Ui()->DoLabel(&Label, pText, R.h * 0.34f, TEXTALIGN_ML);
			TextRender()->TextColor(Prev);
			return bClicked;
		};

		if(Cell(R1C1, FontIcon::EARTH_AMERICAS, Localize("Website"), &s_aLinkButtons[0]))
			Client()->ViewLink("https://ddnet.org/");
		if(Cell(R1C2, FontIcon::MAP, Localize("Tutorial"), &s_aLinkButtons[1]))
			GameClient()->m_Menus.JoinTutorial();
		if(Cell(R2C1, FontIcon::BOOKMARK, Localize("Learn"), &s_aLinkButtons[2]))
			Client()->ViewLink(Localize("https://wiki.ddnet.org/"));
		if(Cell(R2C2, FontIcon::COMMENT, Localize("Discord"), &s_aLinkButtons[3]))
			Client()->ViewLink("https://discord.gg/AYeDKHk9f3");
	}

	return 1.0f;
}

bool CMenusModern::ModernCheckHotKey(int Key) const
{
	return !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() &&
	       Input()->KeyPress(Key) &&
	       !GameClient()->m_GameConsole.IsActive();
}

void CMenusModern::Update()
{
	const std::chrono::nanoseconds Now = time_get_nanoseconds();
	float Dt = m_LastUpdate.count() == 0 ? 0.016f : std::chrono::duration_cast<std::chrono::duration<float>>(Now - m_LastUpdate).count();
	Dt = std::clamp(Dt, 0.001f, 0.1f);
	m_LastUpdate = Now;
	m_ExpandDt = Dt;

	UpdateStartup();

	if(g_Config.m_FcModernUiAnim)
		m_GradientPhase = std::fmod(m_GradientPhase + Dt * 0.065f, 1.0f);

	for(SExpandEntry &Entry : m_vExpandEntries)
	{
		if(Ui()->HotItem() != Entry.m_pId && Entry.m_Factor > 1.0f)
			Entry.m_Factor = maximum(1.0f, Entry.m_Factor - 12.0f * Dt);
	}

	const bool ServerRunning = GameClient()->m_LocalServer.IsServerRunning();
	if(ServerRunning)
		m_SplitProgress = std::clamp(m_SplitProgress + Dt / 0.25f, 0.0f, 1.0f);
	else
		m_SplitProgress = 0.0f;
	if(!g_Config.m_FcModernUiAnim)
		m_SplitProgress = ServerRunning ? 1.0f : 0.0f;

	CMenus &Menus = GameClient()->m_Menus;

	if(m_Flash.m_Active)
	{
		m_Flash.m_Progress = std::clamp(m_Flash.m_Progress + Dt / FLASH_SECONDS, 0.0f, 1.0f);
		if(!m_Flash.m_Applied && m_Flash.m_Progress >= 0.5f)
		{
			m_Flash.m_Applied = true;
			if(m_Flash.m_Editor)
			{
				g_Config.m_ClEditor = 1;
				Input()->MouseModeRelative();
				m_ReturnedFromEditor = true; // set only now that the editor is actually open, so a later return plays the incoming flash
			}
			else if(m_Flash.m_ShowStartTarget)
			{
				Menus.SetShowStart(true);
			}
			else
			{
				Menus.SetShowStart(false);
				Menus.SetMenuPage(m_Flash.m_TargetPage);
			}
		}
		if(m_Flash.m_Progress >= 1.0f)
			m_Flash.m_Active = false;
	}

	// Returning from the editor: the editor can't be dipped to the flash midpoint (it has its own render path),
	// so replay only the fade-out half over the menu: start the flash at its opaque midpoint to reveal the menu.
	if(m_ReturnedFromEditor && !g_Config.m_ClEditor)
	{
		m_ReturnedFromEditor = false;
		if(g_Config.m_FcModernUiAnim)
		{
			m_Flash.m_Active = true;
			m_Flash.m_Applied = true; // no deferred swap; the menu is already showing
			m_Flash.m_Editor = false;
			m_Flash.m_ShowStartTarget = false;
			m_Flash.m_Progress = 0.5f; // sin(0.5*pi)=1 (opaque) fading to sin(1.0*pi)=0
			m_Flash.m_TargetPage = -1;
		}
	}

	// View transition detection: entering the start menu (page -> menu) replays the staggered reveal.
	// start -> page and page -> page are driven by the flash, so the reveal is skipped while a flash runs.
	const int ViewKey = Menus.m_ShowStart ? -1 : Menus.m_MenuPage;
	if(ViewKey != m_LastViewKey)
	{
		const int OldKey = m_LastViewKey;
		m_LastViewKey = ViewKey;
		if(OldKey != -2 && OldKey != -1 && ViewKey == -1 && g_Config.m_FcModernUiAnim && !m_Flash.m_Active)
			m_Startup.m_GrownTime = time_get_nanoseconds(); // -> menu: replay the staggered reveal
	}
}

void CMenusModern::RenderOverlay()
{
	if(!m_Flash.m_Active)
		return;

	// solid full-screen dip to the background colour, opaque at the midpoint where the page swaps
	const SModernUiPalette &Pal = Palette();
	const CUIRect Screen = *Ui()->Screen();
	const float Alpha = std::sin(m_Flash.m_Progress * pi);
	if(Alpha <= 0.0f)
		return;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Pal.m_BackgroundBottom.r, Pal.m_BackgroundBottom.g, Pal.m_BackgroundBottom.b, Alpha);
	const IGraphics::CQuadItem Item(Screen.x, Screen.y, Screen.w, Screen.h);
	Graphics()->QuadsDrawTL(&Item, 1);
	Graphics()->QuadsEnd();
}

void CMenusModern::StartFlash(int TargetPage)
{
	if(!g_Config.m_FcModernUiAnim)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(TargetPage);
		return;
	}
	m_Flash.m_Active = true;
	m_Flash.m_Applied = false;
	m_Flash.m_Editor = false;
	m_Flash.m_ShowStartTarget = false;
	m_Flash.m_Progress = 0.0f;
	m_Flash.m_TargetPage = TargetPage;
}

void CMenusModern::StartFlashEditor()
{
	if(!g_Config.m_FcModernUiAnim)
	{
		g_Config.m_ClEditor = 1;
		Input()->MouseModeRelative();
		return;
	}
	m_Flash.m_Active = true;
	m_Flash.m_Applied = false;
	m_Flash.m_Editor = true;
	m_Flash.m_ShowStartTarget = false;
	m_Flash.m_Progress = 0.0f;
	m_Flash.m_TargetPage = -1;
}

void CMenusModern::StartFlashToMenu()
{
	// ignore while a flash is already running; the in-flight one will land the menu
	if(m_Flash.m_Active)
		return;
	if(!g_Config.m_FcModernUiAnim)
	{
		GameClient()->m_Menus.SetShowStart(true);
		return;
	}
	m_Flash.m_Active = true;
	m_Flash.m_Applied = false;
	m_Flash.m_Editor = false;
	m_Flash.m_ShowStartTarget = true;
	m_Flash.m_Progress = 0.0f;
	m_Flash.m_TargetPage = -1;
}

void CMenusModern::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

	const SModernUiPalette &Pal = Palette();
	const bool Animated = g_Config.m_FcModernUiAnim;

	float ColumnAlpha = 1.0f;
	float PanelAlpha = 1.0f;
	if(Animated && m_Startup.m_GrownTime.count() != 0)
	{
		ColumnAlpha = RevealAlphaForIndex(1);
		PanelAlpha = RevealAlphaForIndex(6);
	}

	// outer margin
	CUIRect Root;
	MainView.Margin(std::clamp(MainView.h * 0.05f, 16.0f, 48.0f), &Root);

	CUIRect TopBar, Footer;
	Root.HSplitTop(std::clamp(Root.h * 0.13f, 52.0f, 84.0f), &TopBar, &Root);
	Root.HSplitBottom(std::clamp(Root.h * 0.08f, 30.0f, 44.0f), &Root, &Footer);
	Root.HSplitTop(std::clamp(Root.h * 0.04f, 8.0f, 20.0f), nullptr, &Root);

	// top bar: wordmark + version (fades in with the column reveal)
	{
		const float FontSize = std::clamp(TopBar.h * 0.46f, 18.0f, 30.0f);
		const ColorRGBA PrevTextColor = TextRender()->DefaultTextColor();

		const ColorRGBA WmColor = ColorRGBA(Pal.m_Text.r, Pal.m_Text.g, Pal.m_Text.b, Pal.m_Text.a * ColumnAlpha);
		const ColorRGBA AccentColor = ColorRGBA(Pal.m_Accent.r, Pal.m_Accent.g, Pal.m_Accent.b, Pal.m_Accent.a * ColumnAlpha);
		const ColorRGBA VerColor = ColorRGBA(Pal.m_TextSecondary.r, Pal.m_TextSecondary.g, Pal.m_TextSecondary.b, Pal.m_TextSecondary.a * ColumnAlpha);

		// center the wordmark over the button column (LeftWidth mirrors the body split formula)
		const float WordmarkCenter = TopBar.x + std::clamp(TopBar.w * 0.34f, 280.0f, 440.0f) / 2.0f;
		const float NameW = TextRender()->TextWidth(FontSize, "Fusion");
		const float TagW = TextRender()->TextWidth(FontSize, "Client");
		const float WordGap = 10.0f;
		const float StartX = WordmarkCenter - (NameW + WordGap + TagW) / 2.0f;
		CUIRect NamePart{StartX, TopBar.y, NameW, TopBar.h};
		CUIRect TagPart{StartX + NameW + WordGap, TopBar.y, TagW, TopBar.h};
		TextRender()->TextColor(WmColor);
		Ui()->DoLabel(&NamePart, "Fusion", FontSize, TEXTALIGN_ML);
		TextRender()->TextColor(AccentColor);
		Ui()->DoLabel(&TagPart, "Client", FontSize, TEXTALIGN_ML);

		CUIRect VerArea;
		TopBar.VSplitRight(200.0f, nullptr, &VerArea);
		char aVersion[64];
		str_format(aVersion, sizeof(aVersion), "v%s", FUSION_VERSION);
		TextRender()->TextColor(VerColor);
		Ui()->DoLabel(&VerArea, aVersion, std::clamp(TopBar.h * 0.28f, 12.0f, 16.0f), TEXTALIGN_MR);

		TextRender()->TextColor(PrevTextColor);
	}

	// body: left button column + right release notes panel
	CUIRect LeftColumn, RightPanel;
	const float LeftWidth = std::clamp(Root.w * 0.34f, 280.0f, 440.0f);
	Root.VSplitLeft(LeftWidth, &LeftColumn, &RightPanel);
	RightPanel.VSplitLeft(24.0f, nullptr, &RightPanel);

	RenderReleaseNotesPanel(RightPanel, PanelAlpha);

	static CButtonContainer s_PlayButton;
	static CButtonContainer s_DemoButton;
	static CButtonContainer s_EditorButton;
	static CButtonContainer s_LocalServerButton;
	static CButtonContainer s_StopServerButton;
	static CButtonContainer s_JoinServerButton;
	static CButtonContainer s_SettingsButton;
	static CButtonContainer s_QuitButton;
	static CButtonContainer s_ConsoleButton;

	int NewPage = -1;
	int ButtonIndex = 0;

	CUIRect Buttons = LeftColumn;
	const int NumRows = 5;
	const float Spacing = std::clamp(Buttons.h * 0.035f, 8.0f, 18.0f);
	float RowH = (Buttons.h - Spacing * (NumRows - 1)) / NumRows;
	RowH = std::clamp(RowH, 40.0f, 50.0f);
	// vertically center the (now shorter) button stack within the column
	const float StackH = RowH * NumRows + Spacing * (NumRows - 1);
	Buttons.HSplitTop(std::clamp((LeftColumn.h - StackH) / 2.0f, 0.0f, LeftColumn.h), nullptr, &Buttons);

	auto NextRow = [&](float Height, float Gap) {
		CUIRect Row;
		Buttons.HSplitTop(Height, &Row, &Buttons);
		Buttons.HSplitTop(Gap, nullptr, &Buttons);
		return Row;
	};

	auto RenderMainButton = [&](CButtonContainer *pId, const char *pText, const char *pIcon, ColorRGBA Color, bool bPrimary, bool &Clicked) {
		const CUIRect Base = NextRow(RowH, Spacing);
		const float Alpha = Animated ? RevealAlphaForIndex(2 + ButtonIndex) : 1.0f;
		ButtonIndex++;
		if(Alpha <= 0.0f)
			return;
		const CUIRect Rect = ExpandRect(Base, pId);
		ButtonExpandFactor(pId, Ui()->MouseHovered(&Rect) ? 1.02f : 1.0f);
		Color.a *= Alpha;
		if(DoGlassButton(pId, pText, &Rect, Color, true, pIcon, bPrimary))
			Clicked = true;
	};

	bool Clicked = false;
	RenderMainButton(&s_PlayButton, Localize("Play", "Start menu"), FontIcon::PLAY, Pal.m_Button, true, Clicked);
	const bool PlayClicked = Clicked;
	Clicked = false;

	RenderMainButton(&s_DemoButton, Localize("Demos"), FontIcon::FILM, Pal.m_Button, false, Clicked);
	const bool DemosClicked = Clicked;
	Clicked = false;

	RenderMainButton(&s_EditorButton, Localize("Editor"), FontIcon::PEN_TO_SQUARE, GameClient()->Editor()->HasUnsavedData() ? Pal.m_ButtonRunning : Pal.m_Button, false, Clicked);
	const bool EditorClicked = Clicked;
	Clicked = false;

	// run / stop / join server row with split animation
	{
		const CUIRect Base = NextRow(RowH, Spacing);
		const float Alpha = Animated ? RevealAlphaForIndex(2 + ButtonIndex) : 1.0f;
		ButtonIndex++;
		if(Alpha > 0.0f)
		{
			const float Split = m_SplitProgress;
			const float HalfW = (Base.w - 6.0f) / 2.0f;
			const CUIRect StopRect{Base.x, Base.y, HalfW * Split, Base.h};
			const CUIRect JoinRect{Base.x + Base.w - HalfW * Split, Base.y, HalfW * Split, Base.h};
			const CUIRect RunRect{Base.x + (Base.w - Base.w * (1.0f - Split)) / 2.0f, Base.y, Base.w * (1.0f - Split), Base.h};

			if(Split < 0.99f)
			{
				const CUIRect Rect = ExpandRect(RunRect, &s_LocalServerButton);
				ButtonExpandFactor(&s_LocalServerButton, Ui()->MouseHovered(&Rect) ? 1.02f : 1.0f);
				ColorRGBA Color = Pal.m_Button;
				Color.a *= Alpha;
				if(DoGlassButton(&s_LocalServerButton, Localize("Run server"), &Rect, Color, Split <= 0.01f, FontIcon::LAYER_GROUP))
					GameClient()->m_LocalServer.RunServer({});
			}
			if(Split > 0.01f)
			{
				const CUIRect Rect = ExpandRect(StopRect, &s_StopServerButton);
				ButtonExpandFactor(&s_StopServerButton, Ui()->MouseHovered(&Rect) ? 1.02f : 1.0f);
				ColorRGBA Color = Pal.m_ButtonRunning;
				Color.a *= Alpha;
				if(DoGlassButton(&s_StopServerButton, Localize("Stop server"), &Rect, Color, Split >= 0.99f, FontIcon::STOP))
				{
					GameClient()->m_LocalServer.KillServer();
					Ui()->SetHotItem(nullptr);
				}

				const CUIRect Join = ExpandRect(JoinRect, &s_JoinServerButton);
				ButtonExpandFactor(&s_JoinServerButton, Ui()->MouseHovered(&Join) ? 1.02f : 1.0f);
				ColorRGBA JoinColor = Pal.m_Button;
				JoinColor.a *= Alpha;
				if(DoGlassButton(&s_JoinServerButton, Localize("Join server"), &Join, JoinColor, Split >= 0.99f, FontIcon::NETWORK_WIRED))
					Client()->Connect("localhost:8303");
			}
		}
	}

	Clicked = false;
	RenderMainButton(&s_SettingsButton, Localize("Settings"), FontIcon::GEAR, Pal.m_Button, false, Clicked);
	const bool SettingsClicked = Clicked;

	// footer: quit (left), update (center), console (right)
	{
		auto FlatItem = [&](const CUIRect &R, const char *pGlyph, const char *pText, bool bRight, CButtonContainer *pId) -> bool {
			const bool bClicked = Ui()->DoButtonLogic(pId, 0, &R, BUTTONFLAG_LEFT) != 0;
			const bool bHot = Ui()->HotItem() == pId;
			const ColorRGBA Col = bHot ? Pal.m_Text : Pal.m_TextSecondary;
			CUIRect Work = R;
			CUIRect Icon, Label;
			if(bRight)
			{
				Work.VSplitRight(Work.h * 0.7f, &Work, &Icon);
				Work.VSplitRight(8.0f, &Work, nullptr);
				Label = Work;
			}
			else
			{
				Work.VSplitLeft(Work.h * 0.7f, &Icon, &Work);
				Work.VSplitLeft(8.0f, nullptr, &Work);
				Label = Work;
			}
			DrawGlyph(Icon, pGlyph, R.h * 0.42f, Col);
			const ColorRGBA PrevTextColor = TextRender()->DefaultTextColor();
			TextRender()->TextColor(Col);
			Ui()->DoLabel(&Label, pText, R.h * CUi::ms_FontmodHeight * 0.85f, bRight ? TEXTALIGN_MR : TEXTALIGN_ML);
			TextRender()->TextColor(PrevTextColor);
			return bClicked;
		};

		CUIRect QuitArea, Mid, ConsoleArea, UpdateArea;
		Footer.VSplitLeft(200.0f, &QuitArea, &Mid);
		Mid.VSplitRight(200.0f, &Mid, &ConsoleArea);
		Mid.VSplitLeft(240.0f, &UpdateArea, &Mid);

		const bool QuitClicked = FlatItem(QuitArea, FontIcon::RIGHT_FROM_BRACKET, Localize("Quit"), false, &s_QuitButton) || Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE) || ModernCheckHotKey(KEY_Q);
		if(QuitClicked)
		{
			if(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE) || GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
				GameClient()->m_Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}

		if(FlatItem(ConsoleArea, FontIcon::TERMINAL, Localize("Console"), true, &s_ConsoleButton))
			GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);

#if defined(CONF_AUTOUPDATE)
		{
			static CButtonContainer s_VersionUpdateButton;
			const IUpdater::EUpdaterState State = Updater()->GetCurrentState();
			const bool NeedUpdate = GameClient()->m_TClient.NeedUpdate();
			if(State == IUpdater::CLEAN && NeedUpdate)
			{
				if(FlatItem(UpdateArea, FontIcon::PLUS, Localize("Update now"), false, &s_VersionUpdateButton))
					Updater()->InitiateUpdate();
			}
			else if(State == IUpdater::NEED_RESTART)
			{
				if(FlatItem(UpdateArea, FontIcon::ARROW_ROTATE_RIGHT, Localize("Restart"), false, &s_VersionUpdateButton))
					Client()->Restart();
			}
			else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
			{
				Ui()->RenderProgressBar(UpdateArea, Updater()->GetCurrentPercent() / 100.0f);
			}
		}
#endif
	}

	if(ModernCheckHotKey(KEY_N))
		NewPage = CMenus::PAGE_NEWS;

	// hotkeys and page changes
	if(ModernCheckHotKey(KEY_P) || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || PlayClicked)
		StartFlash(g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET);
	else if(ModernCheckHotKey(KEY_D) || DemosClicked)
		StartFlash(CMenus::PAGE_DEMOS);
	else if(ModernCheckHotKey(KEY_S) || SettingsClicked)
		StartFlash(CMenus::PAGE_SETTINGS);
	else if(ModernCheckHotKey(KEY_E) || EditorClicked)
		StartFlashEditor();

	if(NewPage != -1)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}

	// keep the local server hotkey working
	if(ModernCheckHotKey(KEY_R) && Input()->KeyPress(KEY_R) && m_SplitProgress <= 0.01f)
		GameClient()->m_LocalServer.RunServer({});
	else if(ModernCheckHotKey(KEY_R) && m_SplitProgress >= 0.99f)
		GameClient()->m_LocalServer.KillServer();
}
