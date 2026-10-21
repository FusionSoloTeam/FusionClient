#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_FX_FX_SCREEN_RECT_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_FX_FX_SCREEN_RECT_H

#include <base/math.h>
#include <base/vmath.h>

#include <engine/graphics.h>

class CFxScreenRect
{
public:
	CFxScreenRect(float Left, float Top, float Width, float Height) :
		m_TopLeft(Left, Top), m_BottomRight(Left + Width, Top + Height) {}

	CFxScreenRect(const vec2 &TopLeft, const vec2 &BottomRight) :
		m_TopLeft(TopLeft), m_BottomRight(BottomRight) {}

	CFxScreenRect Move(const vec2 &Position) const
	{
		CFxScreenRect Rect(*this);
		Rect.m_TopLeft += Position;
		Rect.m_BottomRight += Position;
		return Rect;
	}

	vec2 Size() const { return m_BottomRight - m_TopLeft; }
	float Width() const { return m_BottomRight.x - m_TopLeft.x; }
	float Height() const { return m_BottomRight.y - m_TopLeft.y; }
	bool Inside(const vec2 &Position) const { return !(!in_range(Position.x, m_TopLeft.x, m_BottomRight.x) || !in_range(Position.y, m_TopLeft.y, m_BottomRight.y)); }
	void Expand(float Width, float Height)
	{
		m_TopLeft.x -= Width;
		m_BottomRight.x += Width;
		m_TopLeft.y -= Height;
		m_BottomRight.y += Height;
	}
	void Expand(float Size) { Expand(Size, Size); }

	vec2 m_TopLeft;
	vec2 m_BottomRight;
};

inline CFxScreenRect FxGetScreen(IGraphics *pGraphics)
{
	const CScreenRect Rect = pGraphics->GetScreen();
	return CFxScreenRect(Rect.m_TopLeft, Rect.m_BottomRight);
}

inline CFxScreenRect FxMapScreenToWorld(IGraphics *pGraphics, float CenterX, float CenterY, float ParallaxX, float ParallaxY,
	float ParallaxZoom, float OffsetX, float OffsetY, float Zoom)
{
	const CScreenRect Rect = pGraphics->MapScreenToWorld(CenterX, CenterY, ParallaxX, ParallaxY, ParallaxZoom, OffsetX, OffsetY, pGraphics->WorldAspect(), Zoom);
	return CFxScreenRect(Rect.m_TopLeft, Rect.m_BottomRight);
}

#endif
