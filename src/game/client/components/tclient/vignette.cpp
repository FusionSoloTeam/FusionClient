#include "vignette.h"

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

enum
{
	VIGNETTE_SIDE_LEFT = 0,
	VIGNETTE_SIDE_RIGHT = 1,
	VIGNETTE_SIDE_TOP = 2,
	VIGNETTE_SIDE_BOTTOM = 3,
};

void CVignette::DrawBand(const CUIRect &Band, int Side, ColorRGBA Color, float Alpha) const
{
	ColorRGBA Edge = Color;
	Edge.a = Alpha;
	ColorRGBA Inner = Color;
	Inner.a = 0.0f;

	IGraphics::CQuadItem Quad(Band.x, Band.y, Band.w, Band.h);
	if(Side == VIGNETTE_SIDE_TOP)
		Graphics()->SetColor4(Edge, Edge, Inner, Inner);
	else if(Side == VIGNETTE_SIDE_BOTTOM)
		Graphics()->SetColor4(Inner, Inner, Edge, Edge);
	else if(Side == VIGNETTE_SIDE_LEFT)
		Graphics()->SetColor4(Edge, Inner, Edge, Inner);
	else
		Graphics()->SetColor4(Inner, Edge, Inner, Edge);
	Graphics()->QuadsDrawTL(&Quad, 1);
}

void CVignette::DrawSide(int Side, float Width, ColorRGBA Color, float Alpha) const
{
	const CUIRect *pScreen = Ui()->Screen();
	CUIRect Band;
	if(Side == VIGNETTE_SIDE_LEFT)
		Band = CUIRect(pScreen->x, pScreen->y, Width, pScreen->h);
	else if(Side == VIGNETTE_SIDE_RIGHT)
		Band = CUIRect(pScreen->x + pScreen->w - Width, pScreen->y, Width, pScreen->h);
	else if(Side == VIGNETTE_SIDE_TOP)
		Band = CUIRect(pScreen->x, pScreen->y, pScreen->w, Width);
	else
		Band = CUIRect(pScreen->x, pScreen->y + pScreen->h - Width, pScreen->w, Width);
	DrawBand(Band, Side, Color, Alpha);
}

void CVignette::OnRender()
{
	if(!g_Config.m_TcVignette)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const float Strength = g_Config.m_TcVignetteStrength / 100.0f;
	const float Darken = g_Config.m_TcVignetteDarken / 100.0f;
	if(Strength <= 0.0f || Darken <= 0.0f)
		return;

	Ui()->MapScreen();
	const CUIRect *pScreen = Ui()->Screen();

	const float Radius = g_Config.m_TcVignetteRadius / 100.0f;
	const float Smooth = g_Config.m_TcVignetteSmoothness / 100.0f;
	const float BandX = pScreen->w * Radius * 0.5f;
	const float BandY = pScreen->h * Radius * 0.5f;

	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcVignetteColor, true));
	Color.a = 1.0f;

	bool aSides[4] = {};
	if(g_Config.m_TcVignetteMode == 1)
	{
		aSides[VIGNETTE_SIDE_LEFT] = true;
		aSides[VIGNETTE_SIDE_RIGHT] = true;
	}
	else if(g_Config.m_TcVignetteMode == 2)
	{
		aSides[VIGNETTE_SIDE_TOP] = true;
		aSides[VIGNETTE_SIDE_BOTTOM] = true;
	}
	else
	{
		for(bool &Side : aSides)
			Side = true;
	}

	const float A0 = Strength * Darken * 0.55f;
	const float A1 = Strength * Darken * (0.25f + 0.15f * Smooth);
	const float A2 = Strength * Darken * (0.10f + 0.15f * Smooth);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	for(int Side = 0; Side < 4; Side++)
	{
		if(!aSides[Side])
			continue;
		const float Width = (Side == VIGNETTE_SIDE_LEFT || Side == VIGNETTE_SIDE_RIGHT) ? BandX : BandY;
		if(Width <= 0.0f)
			continue;
		DrawSide(Side, Width, Color, A0);
		DrawSide(Side, Width * (0.55f + 0.25f * Smooth), Color, A1);
		DrawSide(Side, Width * (0.28f + 0.20f * Smooth), Color, A2);
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
}
