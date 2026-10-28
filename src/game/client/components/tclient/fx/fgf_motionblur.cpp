/* Copyright © 2026 BestProject Team */
#include "fgf_motionblur.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <algorithm>
#include <cmath>

static const float PATH_KEEP = 0.25f;

void CFgfMotionBlur::OnInit() {}

void CFgfMotionBlur::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_avTeePath[i].clear();
	m_vCrosshairPath.clear();
	m_LastCrosshairTime = -1.0f;
}

void CFgfMotionBlur::OnRender()
{
	const bool InGame = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
	if(!InGame || !g_Config.m_TcMotionBlur)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			m_avTeePath[i].clear();
		return;
	}

	const float Time = Client()->GlobalTime();

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		const auto &Char = GameClient()->m_Snap.m_aCharacters[ClientId];
		const float Alpha = Char.m_Active && ShowFor(ClientId, g_Config.m_TcMotionBlurSelf, g_Config.m_TcMotionBlurDummy, g_Config.m_TcMotionBlurOthers) ? PlayerAlpha(ClientId) : 0.0f;
		if(Alpha <= 0.0f)
		{
			m_avTeePath[ClientId].clear();
			continue;
		}

		const vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
		if(!m_avTeePath[ClientId].empty() && distance(m_avTeePath[ClientId].back().m_Pos, Pos) > 200.0f)
			m_avTeePath[ClientId].clear();
		if(m_avTeePath[ClientId].empty() || Time > m_avTeePath[ClientId].back().m_Time)
			m_avTeePath[ClientId].push_back({Time, Pos});
		Prune(m_avTeePath[ClientId], Time, PATH_KEEP);

		RenderTeeBlur(ClientId, Time, Alpha);
	}
}

void CFgfMotionBlur::RenderTeeBlur(int ClientId, float Time, float Alpha)
{
	const std::vector<SSample> &vPath = m_avTeePath[ClientId];
	if(vPath.size() < 2)
		return;

	const float Shutter = 0.015f + 0.065f * g_Config.m_TcMotionBlurStrength / 100.0f;
	const vec2 Pos = vPath.back().m_Pos;
	const float Moved = distance(Pos, PathAt(vPath, Time - Shutter));
	if(Moved < 6.0f)
		return;

	const auto &Data = GameClient()->m_aClients[ClientId];
	const vec2 Dir = direction(Data.m_RenderCur.m_Angle / 256.0f);
	const int Num = std::clamp((int)(Moved / 3.0f), 3, 18);
	const float SpeedFade = std::clamp((Moved - 6.0f) / 24.0f, 0.0f, 1.0f);

	for(int i = Num; i >= 1; i--)
	{
		const float k = (float)i / (float)Num;
		const vec2 GhostPos = PathAt(vPath, Time - Shutter * k);
		const float GhostAlpha = Alpha * SpeedFade * std::min(2.0f / (float)Num, 0.4f) * (1.0f - k * 0.9f);
		if(GhostAlpha <= 0.0f)
			continue;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &Data.m_RenderInfo, 0, Dir, GhostPos, GhostAlpha, false);
	}
}

void CFgfMotionBlur::RenderCrosshair(vec2 Center, vec2 TargetPos, float Alpha, float Scale, const std::function<void(vec2, float, float)> &Draw)
{
	if(!g_Config.m_TcCursorMotionBlur)
	{
		m_vCrosshairPath.clear();
		Draw(TargetPos, Alpha, Scale);
		return;
	}

	const float Time = Client()->GlobalTime();
	const vec2 Offset = TargetPos - Center;
	if(!m_vCrosshairPath.empty() && distance(m_vCrosshairPath.back().m_Pos, Offset) > 1500.0f)
		m_vCrosshairPath.clear();
	if(Time > m_LastCrosshairTime)
	{
		m_vCrosshairPath.push_back({Time, Offset});
		m_LastCrosshairTime = Time;
	}
	Prune(m_vCrosshairPath, Time, PATH_KEEP);

	{
		const float TrailTime = 0.04f + 0.16f * g_Config.m_TcCursorMotionBlurStrength / 100.0f;
		Graphics()->QuadsBegin();
		vec2 Prev = Offset;
		for(int i = 1; i <= 14; i++)
		{
			const float k = (float)i / 14.0f;
			const vec2 Next = PathAt(m_vCrosshairPath, Time - TrailTime * k);
			if(distance(Prev, Next) > 0.5f)
			{
				const float Width = mix(3.5f, 0.6f, k) * Scale;
				const float a = Alpha * (1.0f - k) * 0.8f;
				if(a > 0.0f)
				{
					IGraphics::CQuadItem q(Center.x + Prev.x - Width, Center.y + Prev.y - Width, Width * 2.0f, Width * 2.0f);
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, a);
					Graphics()->QuadsDraw(&q, 1);
				}
			}
			Prev = Next;
		}
		Graphics()->QuadsEnd();
	}

	{
		const float Shutter = 0.01f + 0.05f * g_Config.m_TcCursorMotionBlurStrength / 100.0f;
		const float Moved = distance(Offset, PathAt(m_vCrosshairPath, Time - Shutter));
		if(Moved > 4.0f)
		{
			const int Num = std::clamp((int)(Moved / 3.0f), 3, 16);
			const float SpeedFade = std::clamp((Moved - 4.0f) / 20.0f, 0.0f, 1.0f);
			for(int i = Num; i >= 1; i--)
			{
				const float k = (float)i / (float)Num;
				const vec2 GhostOffset = PathAt(m_vCrosshairPath, Time - Shutter * k);
				const float GhostAlpha = Alpha * SpeedFade * std::min(2.4f / (float)Num, 0.4f) * (1.0f - k * 0.9f);
				if(Draw && GhostAlpha > 0.0f)
					Draw(Center + GhostOffset, GhostAlpha, Scale);
			}
		}
	}

	if(Draw)
		Draw(TargetPos, Alpha, Scale);
}

vec2 CFgfMotionBlur::PathAt(const std::vector<SSample> &vPath, float Time)
{
	if(vPath.empty())
		return vec2(0.0f, 0.0f);
	if(Time <= vPath.front().m_Time)
		return vPath.front().m_Pos;
	for(size_t i = vPath.size() - 1; i > 0; i--)
	{
		const SSample &a = vPath[i - 1];
		const SSample &b = vPath[i];
		if(Time >= a.m_Time)
		{
			const float Span = b.m_Time - a.m_Time;
			return Span > 0.0f ? mix(a.m_Pos, b.m_Pos, std::clamp((Time - a.m_Time) / Span, 0.0f, 1.0f)) : b.m_Pos;
		}
	}
	return vPath.front().m_Pos;
}

void CFgfMotionBlur::Prune(std::vector<SSample> &vPath, float Time, float Keep)
{
	size_t First = 0;
	while(First + 1 < vPath.size() && vPath[First + 1].m_Time < Time - Keep)
		First++;
	if(First > 0)
		vPath.erase(vPath.begin(), vPath.begin() + First);
}

bool CFgfMotionBlur::ShowFor(int ClientId, int Self, int Dummy, int Others)
{
	if(ClientId == GameClient()->m_aLocalIds[g_Config.m_ClDummy])
		return Self != 0;
	if(Client()->DummyConnected() && ClientId == GameClient()->m_aLocalIds[!g_Config.m_ClDummy])
		return Dummy != 0;
	return Others != 0;
}

float CFgfMotionBlur::PlayerAlpha(int ClientId)
{
	if(!GameClient()->IsOtherTeam(ClientId))
		return 1.0f;
	if(!g_Config.m_ClShowOthers)
		return 0.0f;
	return g_Config.m_ClShowOthersAlpha / 100.0f;
}
