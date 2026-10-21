/* Copyright © 2026 BestProject Team */
#include "fgf_weather.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <base/time.h>

#include <generated/client_data.h>

#include <game/client/components/particles.h>
#include <game/client/gameclient.h>
#include <game/collision.h>

#include "own_tee.h"

#include <algorithm>
#include <cmath>

static constexpr float VIEW_MARGIN = 120.0f;

void CFgfWeather::OnInit()
{
	m_GlowTexture = FgfGlow::CreateGlowTexture(Graphics());
	m_LeafTexture = FgfGlow::CreateLeafTexture(Graphics());
	m_PetalTexture = FgfGlow::CreatePetalTexture(Graphics());
}

void CFgfWeather::OnReset()
{
	m_NumDrops = 0;
	m_NumSplashes = 0;
	m_SpawnAccumulator = 0.0f;
}

CFgfWeather::CDrop *CFgfWeather::NewDrop()
{
	if(m_NumDrops >= MAX_DROPS)
		return nullptr;
	CDrop *pDrop = &m_aDrops[m_NumDrops++];
	*pDrop = CDrop{};
	return pDrop;
}

void CFgfWeather::Splash(vec2 Pos)
{
	const int Count = 2 + (int)random_float(0.0f, 2.99f);
	for(int i = 0; i < Count && m_NumSplashes < MAX_SPLASHES; i++)
	{
		CSplash &Splash = m_aSplashes[m_NumSplashes++];
		Splash.m_Pos = Pos;
		Splash.m_Vel = vec2(random_float(-70.0f, 70.0f), random_float(-120.0f, -50.0f));
		Splash.m_Life = 0.0f;
		Splash.m_LifeSpan = random_float(0.2f, 0.35f);
		Splash.m_Size = random_float(2.0f, 3.2f);
	}
}

void CFgfWeather::Spawn(const CFxScreenRect &View, float Passed, float Time)
{
	const float Amount = g_Config.m_TcWeatherAmount / 100.0f;
	const float Width = View.m_BottomRight.x - View.m_TopLeft.x + VIEW_MARGIN * 2.0f;
	const float Height = View.m_BottomRight.y - View.m_TopLeft.y + VIEW_MARGIN * 2.0f;
	const float Area = Width / 1400.0f;

	if(m_Kind == FGF_WEATHER_FIREFLIES)
	{
		const int Wanted = std::min(MAX_DROPS, (int)(24.0f * Amount * Area * Height / 800.0f));
		for(int Tries = 0; m_NumDrops < Wanted && Tries < 8; Tries++)
		{
			const vec2 Pos(random_float(View.m_TopLeft.x - VIEW_MARGIN, View.m_BottomRight.x + VIEW_MARGIN),
				random_float(View.m_TopLeft.y - VIEW_MARGIN, View.m_BottomRight.y + VIEW_MARGIN));
			if(Collision()->CheckPoint(Pos))
				continue;
			CDrop *pDrop = NewDrop();
			pDrop->m_Pos = Pos;
			pDrop->m_Vel = direction(random_float(0.0f, 2.0f * pi)) * random_float(10.0f, 25.0f);
			pDrop->m_LifeSpan = random_float(5.0f, 10.0f);
			pDrop->m_Size = random_float(0.8f, 1.2f);
			pDrop->m_Phase = random_float(0.0f, 2.0f * pi);
		}
		return;
	}

	const bool Snow = m_Kind == FGF_WEATHER_SNOW;
	const bool Sakura = m_Kind == FGF_WEATHER_SAKURA;
	const bool Leafy = m_Kind == FGF_WEATHER_LEAVES || Sakura;
	float AverageLife = 0.45f;
	float WantedPerArea = 170.0f;
	if(Snow)
	{
		AverageLife = 5.0f;
		WantedPerArea = 110.0f;
	}
	else if(Leafy)
	{
		AverageLife = 6.5f;
		WantedPerArea = 85.0f;
	}
	const float Wanted = WantedPerArea * Amount * Area * Height / 800.0f;
	m_SpawnAccumulator += Passed * Wanted / AverageLife;
	int Tries = 0;
	while(m_SpawnAccumulator >= 1.0f && Tries < 400)
	{
		Tries++;
		const vec2 Pos(random_float(View.m_TopLeft.x - VIEW_MARGIN, View.m_BottomRight.x + VIEW_MARGIN),
			random_float(View.m_TopLeft.y - VIEW_MARGIN, View.m_BottomRight.y + VIEW_MARGIN));
		if(Collision()->CheckPoint(Pos))
			continue;
		m_SpawnAccumulator -= 1.0f;
		CDrop *pDrop = NewDrop();
		if(!pDrop)
		{
			m_SpawnAccumulator = 0.0f;
			break;
		}
		pDrop->m_Pos = Pos;
		pDrop->m_Phase = random_float(0.0f, 2.0f * pi);
		if(Leafy)
		{
			pDrop->m_Vel = vec2(random_float(-40.0f, 40.0f), random_float(28.0f, 62.0f));
			pDrop->m_LifeSpan = random_float(4.5f, 8.5f);
			pDrop->m_Size = Sakura ? random_float(1.1f, 1.9f) : random_float(1.5f, 2.6f);
		}
		else if(Snow)
		{
			pDrop->m_Vel = vec2(random_float(-15.0f, 15.0f), random_float(35.0f, 70.0f));
			pDrop->m_LifeSpan = random_float(3.0f, 7.0f);
			pDrop->m_Size = random_float(3.0f, 7.0f);
		}
		else
		{
			pDrop->m_Vel = vec2(90.0f + random_float(-20.0f, 20.0f), random_float(850.0f, 1050.0f));
			pDrop->m_LifeSpan = random_float(0.3f, 0.6f);
			pDrop->m_Size = random_float(0.8f, 1.2f);
		}
	}
	m_SpawnAccumulator = std::min(m_SpawnAccumulator, 5.0f);
}

void CFgfWeather::Update(const CFxScreenRect &View, float Passed, float Time)
{
	const float Left = View.m_TopLeft.x - VIEW_MARGIN * 2.0f;
	const float Right = View.m_BottomRight.x + VIEW_MARGIN * 2.0f;
	const float Top = View.m_TopLeft.y - VIEW_MARGIN * 2.0f;
	const float Bottom = View.m_BottomRight.y + VIEW_MARGIN * 2.0f;

	for(int i = 0; i < m_NumDrops;)
	{
		CDrop &Drop = m_aDrops[i];
		Drop.m_Life += Passed;
		bool Dead = Drop.m_Life >= Drop.m_LifeSpan ||
			    Drop.m_Pos.x < Left || Drop.m_Pos.x > Right || Drop.m_Pos.y < Top || Drop.m_Pos.y > Bottom;

		if(!Dead && !Drop.m_Settled)
		{
			vec2 Vel = Drop.m_Vel;
			if(m_Kind == FGF_WEATHER_SNOW)
			{
				Vel.x += std::sin(Time * 1.3f + Drop.m_Phase) * 25.0f;
			}
			else if(m_Kind == FGF_WEATHER_LEAVES || m_Kind == FGF_WEATHER_SAKURA)
			{
				Vel.x += std::sin(Time * 1.7f + Drop.m_Phase) * 45.0f + std::sin(Time * 3.9f + Drop.m_Phase * 2.3f) * 18.0f;
				Vel.y += std::sin(Time * 2.3f + Drop.m_Phase * 1.7f) * 10.0f;
			}
			else if(m_Kind == FGF_WEATHER_FIREFLIES)
			{
				const float Turn = std::sin(Time * 0.7f + Drop.m_Phase * 3.0f) * 1.6f;
				const float Speed = length(Drop.m_Vel);
				Drop.m_Vel = direction(angle(Drop.m_Vel) + Turn * Passed) * Speed;
				Vel = Drop.m_Vel + vec2(0.0f, std::sin(Time * 2.1f + Drop.m_Phase) * 8.0f);
			}

			const vec2 Next = Drop.m_Pos + Vel * Passed;
			if(Collision()->CheckPoint(Next))
			{
				if(m_Kind == FGF_WEATHER_RAIN)
				{
					vec2 Hit = Drop.m_Pos;
					Collision()->IntersectLine(Drop.m_Pos, Next, &Hit, nullptr);
					Splash(Hit - vec2(0.0f, 1.0f));
					Dead = true;
				}
				else if(m_Kind == FGF_WEATHER_SNOW)
				{
					Drop.m_Settled = true;
					Drop.m_LifeSpan = Drop.m_Life + random_float(1.5f, 3.0f);
				}
				else if(m_Kind == FGF_WEATHER_LEAVES || m_Kind == FGF_WEATHER_SAKURA)
				{
					Drop.m_Settled = true;
					Drop.m_LifeSpan = Drop.m_Life + random_float(0.9f, 1.9f);
				}
				else
				{
					Drop.m_Vel = -Drop.m_Vel;
				}
			}
			else
				Drop.m_Pos = Next;
		}

		if(Dead)
		{
			Drop = m_aDrops[--m_NumDrops];
			continue;
		}
		i++;
	}

	for(int i = 0; i < m_NumSplashes;)
	{
		CSplash &Splash = m_aSplashes[i];
		Splash.m_Life += Passed;
		if(Splash.m_Life >= Splash.m_LifeSpan)
		{
			Splash = m_aSplashes[--m_NumSplashes];
			continue;
		}
		Splash.m_Vel.y += 600.0f * Passed;
		Splash.m_Pos += Splash.m_Vel * Passed;
		i++;
	}
}

static ColorRGBA WeatherColor(int Kind)
{
	unsigned Packed = g_Config.m_TcWeatherSnowColor;
	if(Kind == FGF_WEATHER_RAIN)
		Packed = g_Config.m_TcWeatherRainColor;
	else if(Kind == FGF_WEATHER_FIREFLIES)
		Packed = g_Config.m_TcWeatherFireflyColor;
	else if(Kind == FGF_WEATHER_LEAVES)
		Packed = g_Config.m_TcWeatherLeafColor;
	else if(Kind == FGF_WEATHER_SAKURA)
		Packed = g_Config.m_TcWeatherSakuraColor;
	return color_cast<ColorRGBA>(ColorHSLA(Packed));
}

void CFgfWeather::Draw(float Time)
{
	Graphics()->TextureSet(m_GlowTexture);
	const ColorRGBA Color = WeatherColor(m_Kind);

	if(m_Kind == FGF_WEATHER_FIREFLIES)
	{
		const ColorRGBA Core = FgfGlow::Whiten(Color, 0.55f);
		Graphics()->BlendAdditive();
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(0.0f);
		for(int i = 0; i < m_NumDrops; i++)
		{
			const CDrop &Drop = m_aDrops[i];
			const float Fade = std::min(1.0f, std::min(Drop.m_Life, Drop.m_LifeSpan - Drop.m_Life) / 1.0f);
			const float Blink = std::pow(0.5f + 0.5f * std::sin(Time * 2.2f + Drop.m_Phase), 3.0f);
			const float Alpha = Fade * mix(0.15f, 1.0f, Blink);
			Graphics()->SetColor(Color.r, Color.g, Color.b, 0.45f * Alpha);
			IGraphics::CQuadItem Glow(Drop.m_Pos.x, Drop.m_Pos.y, 30.0f * Drop.m_Size, 30.0f * Drop.m_Size);
			Graphics()->QuadsDraw(&Glow, 1);
			Graphics()->SetColor(Core.r, Core.g, Core.b, Alpha);
			IGraphics::CQuadItem CoreQuad(Drop.m_Pos.x, Drop.m_Pos.y, 6.0f * Drop.m_Size, 6.0f * Drop.m_Size);
			Graphics()->QuadsDraw(&CoreQuad, 1);
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
		return;
	}

	if(m_Kind == FGF_WEATHER_LEAVES || m_Kind == FGF_WEATHER_SAKURA)
	{
		const bool Sakura = m_Kind == FGF_WEATHER_SAKURA;
		const IGraphics::CTextureHandle ShapeTexture = Sakura ? m_PetalTexture : m_LeafTexture;
		Graphics()->TextureSet(ShapeTexture.IsValid() ? ShapeTexture : m_GlowTexture);
		Graphics()->BlendNormal();
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
		for(int i = 0; i < m_NumDrops; i++)
		{
			const CDrop &Drop = m_aDrops[i];
			const float Fade = std::clamp(std::min(Drop.m_Life / 0.7f, (Drop.m_LifeSpan - Drop.m_Life) / 0.9f), 0.0f, 1.0f);
			const float Facing = Drop.m_Phase < pi ? 1.0f : -1.0f;
			const float Spin = Drop.m_Settled ? Drop.m_Phase : Drop.m_Phase + Time * (Sakura ? 1.8f : 1.2f) * Facing;
			const float Flutter = Drop.m_Settled ? 1.0f : (0.28f + 0.72f * std::abs(std::sin(Time * 2.6f + Drop.m_Phase * 1.7f)));
			const float Size = Drop.m_Size * (Sakura ? 8.0f : 9.5f);
			Graphics()->QuadsSetRotation(Spin);
			Graphics()->SetColor(Color.r, Color.g, Color.b, 0.95f * Fade);
			IGraphics::CQuadItem Shape(Drop.m_Pos.x, Drop.m_Pos.y, Size * Flutter, Size);
			Graphics()->QuadsDraw(&Shape, 1);
		}
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();
		return;
	}

	if(m_Kind == FGF_WEATHER_SNOW)
	{
		const IGraphics::CTextureHandle FlakeTexture = GameClient()->m_ExtrasSkin.m_SpriteParticleSnowflake;
		const bool Sprite = g_Config.m_TcWeatherSnowflakes != 0 && FlakeTexture.IsValid();
		Graphics()->TextureSet(Sprite ? FlakeTexture : m_GlowTexture);
		Graphics()->BlendNormal();
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
		for(int i = 0; i < m_NumDrops; i++)
		{
			const CDrop &Drop = m_aDrops[i];
			const float Fade = std::clamp(std::min(Drop.m_Life / 0.5f, (Drop.m_LifeSpan - Drop.m_Life) / 1.0f), 0.0f, 1.0f);
			const float Spin = Drop.m_Phase + Time * (Drop.m_Phase < pi ? 0.4f : -0.4f);
			Graphics()->QuadsSetRotation(Sprite ? Spin : 0.0f);
			Graphics()->SetColor(Color.r, Color.g, Color.b, 0.9f * Fade);
			const float Size = Sprite ? Drop.m_Size * 3.4f : Drop.m_Size;
			IGraphics::CQuadItem Flake(Drop.m_Pos.x, Drop.m_Pos.y, Size, Size);
			Graphics()->QuadsDraw(&Flake, 1);
		}
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();
		return;
	}

	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	for(int i = 0; i < m_NumDrops; i++)
	{
		const CDrop &Drop = m_aDrops[i];
		Graphics()->QuadsSetRotation(angle(Drop.m_Vel));
		const float Fade = std::clamp(std::min(Drop.m_Life / 0.06f, (Drop.m_LifeSpan - Drop.m_Life) / 0.06f), 0.0f, 1.0f);
		Graphics()->SetColor(Color.r, Color.g, Color.b, 0.55f * Drop.m_Size * Fade);
		IGraphics::CQuadItem Streak(Drop.m_Pos.x, Drop.m_Pos.y, 26.0f * Drop.m_Size, 2.2f);
		Graphics()->QuadsDraw(&Streak, 1);
	}
	Graphics()->QuadsSetRotation(0.0f);
	const ColorRGBA SplashColor = FgfGlow::Whiten(Color, 0.45f);
	for(int i = 0; i < m_NumSplashes; i++)
	{
		const CSplash &Splash = m_aSplashes[i];
		const float Fade = 1.0f - Splash.m_Life / Splash.m_LifeSpan;
		Graphics()->SetColor(SplashColor.r, SplashColor.g, SplashColor.b, 0.7f * Fade);
		IGraphics::CQuadItem Droplet(Splash.m_Pos.x, Splash.m_Pos.y, Splash.m_Size, Splash.m_Size);
		Graphics()->QuadsDraw(&Droplet, 1);
	}
	Graphics()->QuadsEnd();
}

void CFgfWeather::AmbientDust()
{
	static int64_t s_LastUpdate = 0;
	const int64_t Now = time_get();
	const float Speed = 1.0f;
	if((Now - s_LastUpdate) / (float)time_freq() * Speed <= 1.0f / 5.0f)
		return;
	s_LastUpdate = Now;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CFxScreenRect View = FxMapScreenToWorld(Graphics(), Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0.0f, 0.0f, GameClient()->m_Camera.m_Zoom);
	const ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcAmbientDustColor));
	const int Amount = round_to_int(6.0f * g_Config.m_TcAmbientDustAmount / 100.0f);

	for(int i = 0; i < Amount; i++)
	{
		const float LifeSpan = random_float(4.0f, 8.0f);
		const bool Sparkle = (i % 4) == 0;

		CParticle p;
		p.SetDefault();
		p.m_Spr = Sparkle ? SPRITE_PART_SPARKLE : SPRITE_PART_BALL;
		p.m_Pos = vec2(random_float(View.m_TopLeft.x, View.m_BottomRight.x), random_float(View.m_TopLeft.y, View.m_BottomRight.y));
		p.m_Vel = vec2(random_float(-14.0f, 14.0f), random_float(-18.0f, 6.0f));
		p.m_LifeSpan = LifeSpan;
		p.m_StartSize = Sparkle ? random_float(8.0f, 16.0f) : random_float(5.0f, 14.0f);
		p.m_EndSize = 0.0f;
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float(-0.2f, 0.2f);
		p.m_Gravity = random_float(-4.0f, 2.0f);
		p.m_Friction = 1.0f;
		p.m_UseAlphaFading = true;
		p.m_StartAlpha = 0.5f;
		p.m_EndAlpha = 0.0f;
		p.m_Collides = false;
		p.m_Color = Color;
		GameClient()->m_Particles.Add(Sparkle ? CParticles::GROUP_TRAIL_EXTRA : CParticles::GROUP_PROJECTILE_TRAIL, &p, random_float(0.0f, LifeSpan));
	}
}

void CFgfWeather::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(BcFxSuppressed(GameClient()))
		return;

	const float Time = LocalTime();
	const float Passed = std::clamp(Time - m_LastTime, 0.0f, 0.1f);
	m_LastTime = Time;
	if(BcFxSuppressed(GameClient()))
		return;

	if(g_Config.m_TcAmbientDust)
		AmbientDust();

	if(!g_Config.m_TcAtmosphere)
	{
		if(m_NumDrops != 0 || m_NumSplashes != 0)
			OnReset();
		return;
	}

	const int Kind = std::clamp(g_Config.m_TcWeather, 0, (int)FGF_WEATHER_SAKURA);
	if(Kind != m_Kind)
	{
		m_Kind = Kind;
		OnReset();
	}
	if(m_Kind == FGF_WEATHER_DUST)
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CFxScreenRect View = FxMapScreenToWorld(Graphics(), Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0.0f, 0.0f, GameClient()->m_Camera.m_Zoom);

	Spawn(View, Passed, Time);
	Update(View, Passed, Time);
	Draw(Time);
}
