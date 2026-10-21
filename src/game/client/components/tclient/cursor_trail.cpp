#include "cursor_trail.h"

#include <base/color.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	struct STrailPoint
	{
		vec2 m_Pos;
		double m_Time;
	};

	std::vector<STrailPoint> s_vPoints;
}

CCursorTrail::CCursorTrail(bool Above)
{
	m_Above = Above;
}

void CCursorTrail::OnReset()
{
	s_vPoints.clear();
}

void CCursorTrail::SampleCurrentPos()
{
	if(!g_Config.m_TcCursorTrail)
	{
		s_vPoints.clear();
		return;
	}
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		s_vPoints.clear();
		return;
	}
	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	vec2 Pos;
	bool Valid = false;
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_pLocalCharacter)
	{
		Pos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
		Valid = true;
	}
	else if(g_Config.m_ClSpecCursor && GameClient()->m_CursorInfo.IsAvailable())
	{
		const bool RenderSpecCursor = Client()->State() == IClient::STATE_DEMOPLAYBACK ||
					      (GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW);
		if(RenderSpecCursor)
		{
			const vec2 Center = GameClient()->m_Camera.m_Center;
			float aPoints[4];
			Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->WorldAspect(), 1.0f, aPoints);
			const vec2 HalfSize = vec2(Center.x - aPoints[0], Center.y - aPoints[1]);
			const vec2 ScreenPos = (GameClient()->m_CursorInfo.WorldTarget() - Center) / GameClient()->m_Camera.m_RenderZoom;
			const float ClampFactor = maximum(1.0f, absolute(ScreenPos.x / HalfSize.x), absolute(ScreenPos.y / HalfSize.y));
			Pos = ScreenPos / ClampFactor + Center;
			Valid = true;
		}
	}
	if(!Valid)
		return;

	if(!s_vPoints.empty() && distance(s_vPoints.back().m_Pos, Pos) < 0.05f)
		return;

	s_vPoints.push_back({Pos, time_get() / (double)time_freq()});
	constexpr size_t MaxPoints = 1024;
	if(s_vPoints.size() > MaxPoints)
		s_vPoints.erase(s_vPoints.begin(), s_vPoints.begin() + (s_vPoints.size() - MaxPoints));
}

void CCursorTrail::RenderTrail() const
{
	if(s_vPoints.size() < 3)
		return;

	const double Now = time_get() / (double)time_freq();
	const float Lifetime = std::clamp(g_Config.m_TcCursorTrailLength, 10, 10000) / 1000.0f;
	while(!s_vPoints.empty() && (Now - s_vPoints.front().m_Time) > Lifetime)
		s_vPoints.erase(s_vPoints.begin());
	if(s_vPoints.size() < 3)
		return;

	const int Num = (int)s_vPoints.size();

	std::vector<ColorRGBA> vCols(Num);
	for(int i = 0; i < Num; i++)
	{
		const float Progress = Num > 1 ? (float)(Num - 1 - i) / (float)(Num - 1) : 0.0f;
		ColorRGBA Col;
		switch(g_Config.m_TcCursorTrailColorMode)
		{
		case 1:
		{
			const float Hue = std::fmod((float)(s_vPoints[i].m_Time * 0.35) + (1.0f - Progress) * 0.4f, 1.0f);
			Col = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
			break;
		}
		case 2:
		{
			const ColorRGBA Col1 = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcCursorTrailColor, true));
			const ColorRGBA Col2 = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcCursorTrailColor2, true));
			Col = ColorRGBA(mix(Col1.r, Col2.r, Progress), mix(Col1.g, Col2.g, Progress), mix(Col1.b, Col2.b, Progress), mix(Col1.a, Col2.a, Progress));
			break;
		}
		default:
			Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcCursorTrailColor, true));
			break;
		}
		Col.a = 1.0f;
		if(g_Config.m_TcCursorTrailFade)
			Col.a *= 1.0f - Progress;
		vCols[i] = Col;
	}

	const vec2 Center = GameClient()->m_Camera.m_Center;
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	float aPoints[4];
	Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->WorldAspect(), 1.0f, aPoints);
	Graphics()->TextureClear();
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);

	Graphics()->LinesBegin();
	for(int i = 0; i < Num - 1; i++)
	{
		Graphics()->SetColor(vCols[i + 1]);
		const IGraphics::CLineItem LineItem(s_vPoints[i].m_Pos.x, s_vPoints[i].m_Pos.y, s_vPoints[i + 1].m_Pos.x, s_vPoints[i + 1].m_Pos.y);
		Graphics()->LinesDraw(&LineItem, 1);
	}
	Graphics()->LinesEnd();

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CCursorTrail::OnRender()
{
	if(!m_Above)
		SampleCurrentPos();

	if(!g_Config.m_TcCursorTrail)
		return;
	if((g_Config.m_TcCursorTrailMode == 1) != !m_Above)
		return;
	RenderTrail();
}
