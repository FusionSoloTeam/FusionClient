/* Copyright © 2026 BestProject Team */
#include "hookfrost.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include "own_tee.h"

#include <algorithm>
#include <cmath>

using FgfGlow::Whiten;

static constexpr float FADE_TIME = 0.4f;
static constexpr int NUM_TRAIL = 3;

static float FrostNoise(float Seed)
{
	const float x = std::sin(Seed * 12.9898f + 78.233f) * 43758.5453f;
	return x - std::floor(x);
}

static float FrostMist() { return g_Config.m_TcHookFrostMist / 100.0f; }
static float FrostSnow() { return g_Config.m_TcHookFrostSnow / 100.0f; }
static ColorRGBA FrostColor() { return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcHookFrostColor)); }

void CHookFrost::OnInit()
{
	m_GlowTexture = FgfGlow::CreateGlowTexture(Graphics());
	m_RingTexture = FgfGlow::CreateRingTexture(Graphics());
	m_BeamTexture = FgfGlow::CreateBeamTexture(Graphics());
}

void CHookFrost::OnReset()
{
	m_Tracker.Reset();
	for(CFrostState &State : m_aStates)
		State = CFrostState();
	m_Snow.Clear();
}

void CHookFrost::AddFlake(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Alpha, ColorRGBA Color)
{
	CGlowParticles::CSpark *pSpark = m_Snow.NewSpark();
	if(!pSpark)
		return;
	*pSpark = CGlowParticles::CSpark{};
	pSpark->m_Pos = Pos;
	pSpark->m_Vel = Vel;
	pSpark->m_Gravity = 22.0f;
	pSpark->m_Drag = 0.55f;
	pSpark->m_Stretch = 0.0f;
	pSpark->m_LifeSpan = LifeSpan;
	pSpark->m_StartSize = Size;
	pSpark->m_EndSize = Size * 0.35f;
	pSpark->m_StartAlpha = Alpha;
	pSpark->m_EndAlpha = 0.0f;
	pSpark->m_Color = Color;
}

void CHookFrost::AddShard(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Alpha, ColorRGBA Color)
{
	CGlowParticles::CSpark *pSpark = m_Snow.NewSpark();
	if(!pSpark)
		return;
	*pSpark = CGlowParticles::CSpark{};
	pSpark->m_Pos = Pos;
	pSpark->m_Vel = Vel;
	pSpark->m_Gravity = 260.0f;
	pSpark->m_Drag = 0.3f;
	pSpark->m_Stretch = 0.06f;
	pSpark->m_LifeSpan = LifeSpan;
	pSpark->m_StartSize = Size;
	pSpark->m_EndSize = Size * 0.12f;
	pSpark->m_StartAlpha = Alpha;
	pSpark->m_EndAlpha = 0.0f;
	pSpark->m_Color = Whiten(Color, 0.55f);
	pSpark->m_ShiftColor = true;
	pSpark->m_EndColor = Color;
}

void CHookFrost::Grow(int Owner, const CHookGripTracker::CGrip &Grip, float Scale)
{
	CFrostState &State = m_aStates[Owner];
	State = CFrostState();
	State.m_Seed = random_float(0.0f, 500.0f);
	State.m_Flash = 1.0f;

	const ColorRGBA Color = FrostColor();
	if(g_Config.m_TcHookRings)
		m_Snow.AddRing(Grip.m_Pos, 0.35f, 8.0f * Scale, 90.0f * Scale, 0.8f * Grip.m_Alpha, Whiten(Color, 0.4f));
	const int Count = std::max(0, round_to_int(18.0f * mix(0.5f, 1.0f, FrostSnow())));
	for(int i = 0; i < Count; i++)
	{
		const vec2 Dir = normalize(direction(random_angle()) + Grip.m_Dir * 0.6f);
		AddShard(Grip.m_Pos, Dir * random_float(120.0f, 340.0f) * Scale, random_float(3.0f, 6.0f) * Scale,
			random_float(0.3f, 0.6f), 0.9f * Grip.m_Alpha, Color);
	}
}

void CHookFrost::Flash(int Owner, const CHookGripTracker::CGrip &Grip, float Scale)
{
	CFrostState &State = m_aStates[Owner];
	State.m_Flash = 1.0f;
	const ColorRGBA Color = FrostColor();
	if(g_Config.m_TcHookRings)
	{
		m_Snow.AddRing(Grip.m_Pos, 0.5f, 14.0f * Scale, 150.0f * Scale, 0.9f * Grip.m_Alpha, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		m_Snow.AddRing(Grip.m_Pos, 0.3f, 8.0f * Scale, 60.0f * Scale, 0.7f * Grip.m_Alpha, Whiten(Color, 0.6f));
	}
}

void CHookFrost::Shatter(int Owner, const CHookGripTracker::CGrip &Grip, float Scale)
{
	const ColorRGBA Color = FrostColor();
	const int Count = std::max(0, round_to_int((14.0f + 22.0f * Grip.m_Charge) * mix(0.5f, 1.0f, FrostSnow())));
	for(int i = 0; i < Count; i++)
		AddShard(Grip.m_Pos + random_direction() * random_float(0.0f, 12.0f) * Scale,
			random_direction() * random_float(110.0f, mix(240.0f, 520.0f, Grip.m_Charge)) * Scale,
			random_float(2.5f, 5.0f) * Scale, random_float(0.3f, 0.6f), 0.9f * Grip.m_Alpha, Color);
	if(g_Config.m_TcHookRings)
		m_Snow.AddRing(Grip.m_Pos, 0.3f, 10.0f * Scale, mix(70.0f, 160.0f, Grip.m_Charge) * Scale, 0.65f * Grip.m_Alpha, Whiten(Color, 0.5f));
}

void CHookFrost::EmitSnow(int Owner, const CHookGripTracker::CGrip &Grip, float Passed, float Scale)
{
	CFrostState &State = m_aStates[Owner];
	State.m_SnowAccumulator += Passed * mix(3.0f, 11.0f, Grip.m_Charge) * FrostSnow();
	const float Drift = 26.0f * Scale;
	while(State.m_SnowAccumulator >= 1.0f)
	{
		State.m_SnowAccumulator -= 1.0f;
		AddFlake(Grip.m_Pos + random_direction() * random_float(4.0f, Drift),
			vec2(random_float(-16.0f, 16.0f), random_float(-10.0f, 14.0f)) * Scale,
			random_float(2.0f, 3.8f) * Scale, random_float(0.7f, 1.5f), 0.75f * Grip.m_Alpha,
			FgfGlow::Blend(FrostColor(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), 0.5f));
	}
}

void CHookFrost::Crystals(IGraphics *pGraphics, float Scale, const CHookGripTracker::CGrip &Grip, int Owner)
{
	const CFrostState &State = m_aStates[Owner];
	const ColorRGBA Color = FrostColor();
	const float Alpha = Grip.m_Fade * Grip.m_Alpha;
	const int Count = std::clamp(g_Config.m_TcHookFrostCrystals, 3, 24);
	const float Growth = g_Config.m_TcHookFrostGrowth / 100.0f;
	const float Length = mix(7.0f, 24.0f, Grip.m_Charge) * Growth * Scale;

	vec2 aPoints[NUM_TRAIL];
	float aWidths[NUM_TRAIL];
	for(int Arm = 0; Arm < Count; Arm++)
	{
		const float Base = (Arm / (float)Count) * 2.0f * pi + State.m_Seed * 0.37f;
		const float Jitter = FrostNoise(State.m_Seed + Arm * 5.13f) - 0.5f;
		const vec2 Dir = direction(Base + Jitter * 0.4f);
		const vec2 Side(-Dir.y, Dir.x);
		const float Len = Length * mix(0.55f, 1.25f, FrostNoise(State.m_Seed + Arm * 2.71f)) * State.m_Flicker;

		aPoints[0] = Grip.m_Pos;
		aPoints[1] = Grip.m_Pos + Dir * (Len * 0.55f) + Side * Jitter * Len * 0.22f;
		aPoints[2] = Grip.m_Pos + Dir * Len;
		aWidths[0] = mix(2.4f, 4.0f, Grip.m_Charge) * Scale;
		aWidths[1] = aWidths[0] * 0.6f;
		aWidths[2] = aWidths[0] * 0.1f;
		FgfGlow::DrawStrip(pGraphics, aPoints, aWidths, NUM_TRAIL, Color.WithAlpha((0.7f + 0.3f * State.m_Flicker) * Alpha));

		if(Len > 9.0f * Scale)
		{
			vec2 aBranch[NUM_TRAIL];
			float aBranchWidth[NUM_TRAIL];
			const vec2 BranchDir = normalize(Dir * 0.6f + Side * (Jitter > 0.0f ? 0.8f : -0.8f));
			aBranch[0] = aPoints[1];
			aBranch[1] = aPoints[1] + BranchDir * (Len * 0.3f);
			aBranch[2] = aPoints[1] + BranchDir * (Len * 0.45f);
			aBranchWidth[0] = aWidths[1] * 0.8f;
			aBranchWidth[1] = aWidths[1] * 0.45f;
			aBranchWidth[2] = aWidths[1] * 0.08f;
			FgfGlow::DrawStrip(pGraphics, aBranch, aBranchWidth, NUM_TRAIL, Whiten(Color, 0.45f).WithAlpha(0.8f * Alpha));
		}
	}
}

void CHookFrost::Draw(float Scale, float Time)
{
	bool AnyGrip = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
		AnyGrip |= m_Tracker.Grip(i).m_Fade > 0.0f;
	if(!AnyGrip && !m_Snow.HasAny())
		return;

	const ColorRGBA Color = FrostColor();
	const ColorRGBA Hot = Whiten(Color, 0.7f);
	const float Mist = FrostMist();

	Graphics()->BlendAdditive();

	Graphics()->TextureSet(m_GlowTexture);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0.0f);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CHookGripTracker::CGrip &Grip = m_Tracker.Grip(i);
		if(Grip.m_Fade <= 0.0f)
			continue;
		const CFrostState &State = m_aStates[i];
		const float Alpha = Grip.m_Fade * Grip.m_Alpha;

		const float Halo = mix(40.0f, 96.0f, Grip.m_Charge) * Scale * State.m_Flicker;
		Graphics()->SetColor(Color.WithAlpha(0.26f * Mist * Alpha));
		IGraphics::CQuadItem Glow(Grip.m_Pos.x, Grip.m_Pos.y, Halo, Halo);
		Graphics()->QuadsDraw(&Glow, 1);

		if(State.m_Flash > 0.0f)
		{
			const float Size = mix(26.0f, 160.0f, 1.0f - State.m_Flash) * Scale;
			Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f * State.m_Flash * Alpha));
			IGraphics::CQuadItem Quad(Grip.m_Pos.x, Grip.m_Pos.y, Size, Size);
			Graphics()->QuadsDraw(&Quad, 1);
		}

		const float Core = mix(7.0f, 15.0f, Grip.m_Charge) * Scale * State.m_Flicker;
		Graphics()->SetColor(Hot.WithAlpha(0.95f * Alpha));
		IGraphics::CQuadItem Quad(Grip.m_Pos.x, Grip.m_Pos.y, Core, Core);
		Graphics()->QuadsDraw(&Quad, 1);
	}
	Graphics()->QuadsEnd();

	m_Snow.Draw(Graphics(), m_GlowTexture, m_RingTexture);

	FgfGlow::BeginStrips(Graphics(), m_BeamTexture);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CHookGripTracker::CGrip &Grip = m_Tracker.Grip(i);
		if(Grip.m_Fade <= 0.0f)
			continue;
		Crystals(Graphics(), Scale, Grip, i);
	}
	Graphics()->QuadsEnd();

	Graphics()->BlendNormal();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CHookFrost::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(BcFxSuppressed(GameClient()))
		return;

	const float Time = LocalTime();
	const float Passed = std::clamp(Time - m_LastTime, 0.0f, 0.1f);
	m_LastTime = Time;

	if(g_Config.m_TcHookStyle != FGF_HOOK_STYLE_FROST)
	{
		OnReset();
		return;
	}

	const float Scale = g_Config.m_TcHookSize / 100.0f;
	m_Tracker.Update(GameClient(), Passed, g_Config.m_TcHookChargeTime / 1000.0f, FADE_TIME,
		[&](int ClientId, CHookGripTracker::EEvent Event, const CHookGripTracker::CGrip &Grip) {
			switch(Event)
			{
			case CHookGripTracker::EEvent::GRAB:
				Grow(ClientId, Grip, Scale);
				break;
			case CHookGripTracker::EEvent::CHARGED:
				Flash(ClientId, Grip, Scale);
				break;
			case CHookGripTracker::EEvent::RELEASE:
				Shatter(ClientId, Grip, Scale);
				break;
			}
		});

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CHookGripTracker::CGrip &Grip = m_Tracker.Grip(i);
		CFrostState &State = m_aStates[i];
		if(Grip.m_Holding)
			EmitSnow(i, Grip, Passed, Scale);
		State.m_Flicker = mix(0.85f, 1.15f, FrostNoise(State.m_Seed + std::floor(Time * 24.0f)));
		State.m_Flash = std::max(0.0f, State.m_Flash - Passed * 3.5f);
	}

	m_Snow.Update(Passed);
	Draw(Scale, Time);
}
