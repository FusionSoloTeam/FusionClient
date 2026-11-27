#include "black_frost.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/shared/config.h>

#include <game/client/components/tclient/fx/fgf_glow.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
constexpr float BF_PI = 3.14159265358979323846f;

const ColorRGBA ICE_VOID(0.020f, 0.035f, 0.090f, 1.0f);
const ColorRGBA ICE_DEEP(0.060f, 0.150f, 0.330f, 1.0f);
const ColorRGBA ICE_TEAL(0.115f, 0.410f, 0.520f, 1.0f);
const ColorRGBA ICE_CYAN(0.290f, 0.760f, 0.950f, 1.0f);
const ColorRGBA ICE_PALE(0.640f, 0.920f, 1.000f, 1.0f);
const ColorRGBA ICE_WHITE(0.870f, 0.975f, 1.000f, 1.0f);

float Hash01(uint32_t Seed)
{
	Seed = Seed * 0x9E3779B1u + 0x7F4A7C15u;
	Seed ^= Seed >> 15;
	Seed *= 0x85EBCA77u;
	Seed ^= Seed >> 13;
	Seed *= 0xC2B2AE3Du;
	Seed ^= Seed >> 16;
	return (float)(Seed & 0xFFFFFFu) / (float)0x1000000u;
}

float ValueNoise(float X, int Seed)
{
	const int Index = (int)std::floor(X);
	const float Frac = X - (float)Index;
	const float Smooth = Frac * Frac * (3.0f - 2.0f * Frac);
	const float A = Hash01((uint32_t)Index * 374761393u + (uint32_t)Seed * 668265263u);
	const float B = Hash01((uint32_t)(Index + 1) * 374761393u + (uint32_t)Seed * 668265263u);
	return A + (B - A) * Smooth;
}

float NoiseSigned(float X, int Seed)
{
	return ValueNoise(X, Seed) * 2.0f - 1.0f;
}

ColorRGBA LerpColor(const ColorRGBA &From, const ColorRGBA &To, float Amount)
{
	Amount = std::clamp(Amount, 0.0f, 1.0f);
	return ColorRGBA(
		From.r + (To.r - From.r) * Amount,
		From.g + (To.g - From.g) * Amount,
		From.b + (To.b - From.b) * Amount,
		From.a + (To.a - From.a) * Amount);
}

ColorRGBA WithAlpha(const ColorRGBA &Color, float Alpha)
{
	return ColorRGBA(Color.r, Color.g, Color.b, std::clamp(Alpha, 0.0f, 1.0f));
}

void FrostRadiusRange(float &Min, float &Max)
{
	Min = (float)g_Config.m_FcBlackFrostRadiusMin;
	Max = (float)g_Config.m_FcBlackFrostRadiusMax;
	if(Min > Max)
		std::swap(Min, Max);
	if(Max - Min < 1.0f)
		Max = Min + 1.0f;
}
}

void CBlackFrost::OnInit()
{
	EnsureTextures();
	OnReset();
}

void CBlackFrost::OnShutdown()
{
	if(m_GlowTexture.IsValid())
		Graphics()->UnloadTexture(&m_GlowTexture);
	m_TexturesReady = false;
}

void CBlackFrost::EnsureTextures()
{
	if(m_TexturesReady)
		return;
	m_GlowTexture = FgfGlow::CreateGlowTexture(Graphics());
	m_TexturesReady = true;
}

void CBlackFrost::OnReset()
{
	for(int Tee = 0; Tee < 2; Tee++)
		ResetShards(Tee);
	m_Fade = 0.0f;
}

void CBlackFrost::ResetShards(int Tee)
{
	for(auto &Shard : m_aaShards[Tee])
		Shard.m_Active = false;
	for(auto &Mote : m_aaMotes[Tee])
		Mote.m_Active = false;
	m_aMoteAccum[Tee] = 0.0f;
}

void CBlackFrost::IceQuad(const vec2 &P0, const vec2 &P1, const vec2 &P2, const vec2 &P3,
	const ColorRGBA &C0, const ColorRGBA &C1, const ColorRGBA &C2, const ColorRGBA &C3)
{
	Graphics()->SetColor4(C0, C1, C2, C3);
	const IGraphics::CFreeformItem Item(P0, P1, P2, P3);
	Graphics()->QuadsDrawFreeform(&Item, 1);
}

void CBlackFrost::SpawnShard(int Tee, int Index, const vec2 &Origin, float Scale)
{
	SShard &Shard = m_aaShards[Tee][Index];
	const uint32_t Seed = (uint32_t)(Index * 7919 + Tee * 104729) ^ (uint32_t)(LocalTime() * 4096.0f);
	const int NoiseSeed = (int)(Seed & 0x7FFFFFFFu);

	float RadiusMin, RadiusMax;
	FrostRadiusRange(RadiusMin, RadiusMax);

	Shard = SShard{};
	Shard.m_Active = true;
	Shard.m_Seed = NoiseSeed;
	Shard.m_Age = 0.0f;
	Shard.m_LifeSpan = 3.4f + Hash01(Seed + 1u) * 4.6f;

	const float Angle = Hash01(Seed + 2u) * 2.0f * BF_PI;
	const float Dist = RadiusMin + Hash01(Seed + 3u) * (RadiusMax - RadiusMin);
	Shard.m_Pos = Origin + direction(Angle) * (Dist * Scale);

	const float MoveAngle = Angle + (Hash01(Seed + 4u) < 0.5f ? -1.0f : 1.0f) * (0.5f + Hash01(Seed + 5u) * 2.1f);
	Shard.m_Vel = direction(MoveAngle) * (18.0f + Hash01(Seed + 6u) * 42.0f) * Scale;

	Shard.m_Size = (1.5f + Hash01(Seed + 7u) * 2.6f) * Scale;
	Shard.m_Twist = Hash01(Seed + 8u) * 2.0f * BF_PI;
	Shard.m_TwistRate = (Hash01(Seed + 9u) * 2.0f - 1.0f) * 1.9f;
	Shard.m_Wander = 0.55f + Hash01(Seed + 10u) * 1.05f;
	Shard.m_Alpha = 0.55f + Hash01(Seed + 11u) * 0.45f;
}

void CBlackFrost::UpdateShards(int Tee, const vec2 &Origin, float Dt, float Scale, float Anim)
{
	float RadiusMin, RadiusMax;
	FrostRadiusRange(RadiusMin, RadiusMax);
	RadiusMin *= Scale;
	RadiusMax *= Scale;

	const float Spring = 10.0f * Anim;
	const float Damping = 3.1f;
	const float SpeedCap = 150.0f * Scale;

	for(int i = 0; i < MAX_SHARDS; i++)
	{
		SShard &Shard = m_aaShards[Tee][i];
		if(!Shard.m_Active)
			continue;

		Shard.m_Age += Dt;
		if(Shard.m_Age >= Shard.m_LifeSpan)
		{
			SpawnShard(Tee, i, Origin, Scale);
			continue;
		}

		const float NoiseX = NoiseSigned(Shard.m_Age * 0.57f, Shard.m_Seed + 11);
		const float NoiseY = NoiseSigned(Shard.m_Age * 0.43f, Shard.m_Seed + 977);
		vec2 Accel = vec2(NoiseX, NoiseY) * (78.0f * Shard.m_Wander * Anim);

		const vec2 Delta = Shard.m_Pos - Origin;
		const float Dist = length(Delta);
		if(Dist > 0.001f)
		{
			const vec2 Dir = Delta / Dist;
			if(Dist > RadiusMax)
				Accel -= Dir * (Spring * (Dist - RadiusMax));
			else if(Dist < RadiusMin)
				Accel += Dir * (Spring * (RadiusMin - Dist));
		}

		Shard.m_Vel += Accel * Dt;
		Shard.m_Vel *= 1.0f - std::clamp(Damping * Dt, 0.0f, 0.85f);

		const float Speed = length(Shard.m_Vel);
		if(Speed > SpeedCap)
			Shard.m_Vel *= SpeedCap / Speed;

		Shard.m_Pos += Shard.m_Vel * Dt;
		Shard.m_Twist += Shard.m_TwistRate * Anim * Dt;

		if(length(Shard.m_Pos - Origin) > RadiusMax * 3.0f + 160.0f * Scale)
			SpawnShard(Tee, i, Origin, Scale);
	}
}

void CBlackFrost::RenderShards(int Tee, float Scale, float Glow)
{
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0.0f);

	for(const auto &Shard : m_aaShards[Tee])
	{
		if(!Shard.m_Active)
			continue;

		const float T = Shard.m_Age / Shard.m_LifeSpan;
		const float Fade = std::min(1.0f, T / 0.14f) * std::min(1.0f, (1.0f - T) / 0.20f);
		if(Fade <= 0.0f)
			continue;

		const float Speed = length(Shard.m_Vel);
		const float BaseAngle = Speed > 3.0f ? std::atan2(Shard.m_Vel.y, Shard.m_Vel.x) : Shard.m_Twist;
		const vec2 Dir = direction(BaseAngle + Shard.m_Twist);
		const vec2 Perp = vec2(-Dir.y, Dir.x);

		const float Stretch = 1.0f + std::min(0.85f, Speed / (190.0f * Scale));
		const float Len = Shard.m_Size * 3.2f * Stretch;
		const float Wide = Shard.m_Size * 0.72f;

		const vec2 Tip = Shard.m_Pos + Dir * Len;
		const vec2 SideA = Shard.m_Pos + Perp * Wide;
		const vec2 Tail = Shard.m_Pos - Dir * (Len * 0.55f);
		const vec2 SideB = Shard.m_Pos - Perp * Wide;

		const float Alpha = Shard.m_Alpha * Fade * m_Fade;
		const ColorRGBA TipColor = WithAlpha(LerpColor(ICE_PALE, ICE_WHITE, 0.35f + 0.35f * Glow), Alpha * 0.95f);
		const ColorRGBA SideColor = WithAlpha(LerpColor(ICE_DEEP, ICE_CYAN, 0.30f + 0.30f * Glow), Alpha * 0.70f);
		const ColorRGBA TailColor = WithAlpha(ICE_VOID, Alpha * 0.15f);

		IceQuad(Tip, SideA, SideB, Tail, TipColor, SideColor, SideColor, TailColor);

		const vec2 CoreTip = Shard.m_Pos + Dir * (Len * 0.42f);
		const vec2 CoreTail = Shard.m_Pos - Dir * (Len * 0.30f);
		const vec2 CoreA = Shard.m_Pos + Perp * (Wide * 0.42f);
		const vec2 CoreB = Shard.m_Pos - Perp * (Wide * 0.42f);
		const ColorRGBA CoreColor = WithAlpha(ICE_WHITE, Alpha * 0.55f * Glow);
		IceQuad(CoreTip, CoreA, CoreB, CoreTail, CoreColor,
			WithAlpha(ICE_CYAN, Alpha * 0.35f * Glow),
			WithAlpha(ICE_CYAN, Alpha * 0.35f * Glow),
			WithAlpha(ICE_TEAL, 0.0f));
	}

	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
	Graphics()->BlendNormal();
}

void CBlackFrost::SpawnMote(int Tee, const vec2 &Origin, float Scale)
{
	SMote *pMote = nullptr;
	for(auto &Mote : m_aaMotes[Tee])
	{
		if(!Mote.m_Active)
		{
			pMote = &Mote;
			break;
		}
	}
	if(pMote == nullptr)
		return;

	float RadiusMin, RadiusMax;
	FrostRadiusRange(RadiusMin, RadiusMax);

	const float Angle = random_angle();
	const float Dist = random_float(RadiusMin, RadiusMax) * Scale;
	pMote->m_Active = true;
	pMote->m_Pos = Origin + direction(Angle) * Dist;
	pMote->m_Vel = direction(Angle + random_float(-1.2f, 1.2f)) * random_float(8.0f, 30.0f) * Scale;
	pMote->m_LifeSpan = random_float(1.1f, 2.8f);
	pMote->m_Life = pMote->m_LifeSpan;
	pMote->m_Size = random_float(2.0f, 5.0f) * Scale;
	pMote->m_Phase = random_angle();
}

void CBlackFrost::UpdateMotes(int Tee, const vec2 &Origin, float Dt, float Scale, float Anim)
{
	float RadiusMin, RadiusMax;
	FrostRadiusRange(RadiusMin, RadiusMax);
	RadiusMin *= Scale;
	RadiusMax *= Scale;

	const float Density = g_Config.m_FcBlackFrostDensity / 100.0f;
	m_aMoteAccum[Tee] += Dt * 11.0f * Density * Anim;
	while(m_aMoteAccum[Tee] >= 1.0f)
	{
		m_aMoteAccum[Tee] -= 1.0f;
		SpawnMote(Tee, Origin, Scale);
	}

	for(auto &Mote : m_aaMotes[Tee])
	{
		if(!Mote.m_Active)
			continue;
		Mote.m_Life -= Dt;
		if(Mote.m_Life <= 0.0f)
		{
			Mote.m_Active = false;
			continue;
		}

		Mote.m_Vel.x += std::sin(Mote.m_Life * 3.1f + Mote.m_Phase) * 14.0f * Dt;
		Mote.m_Vel.y += std::cos(Mote.m_Life * 2.3f + Mote.m_Phase) * 11.0f * Dt;

		const vec2 Delta = Mote.m_Pos - Origin;
		const float Dist = length(Delta);
		if(Dist > 0.001f)
		{
			const vec2 Dir = Delta / Dist;
			if(Dist > RadiusMax)
				Mote.m_Vel -= Dir * (14.0f * (Dist - RadiusMax) * Dt);
		}

		Mote.m_Pos += Mote.m_Vel * Dt;
	}
}

void CBlackFrost::RenderMotes(int Tee, float Scale, float Glow)
{
	if(!m_GlowTexture.IsValid())
		return;

	Graphics()->BlendAdditive();
	Graphics()->TextureSet(m_GlowTexture);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0.0f);

	for(const auto &Mote : m_aaMotes[Tee])
	{
		if(!Mote.m_Active)
			continue;

		const float T = 1.0f - Mote.m_Life / Mote.m_LifeSpan;
		const float Fade = std::min(1.0f, T / 0.22f) * std::min(1.0f, (1.0f - T) / 0.35f);
		const float Twinkle = 0.55f + 0.45f * std::sin(Mote.m_Life * 6.2f + Mote.m_Phase);
		const float Alpha = Fade * Twinkle * 0.55f * Glow * m_Fade;
		const float Size = Mote.m_Size * (0.85f + 0.4f * T);

		Graphics()->SetColor(WithAlpha(LerpColor(ICE_CYAN, ICE_PALE, 0.4f), Alpha));
		const IGraphics::CQuadItem Quad(Mote.m_Pos.x - Size, Mote.m_Pos.y - Size, Size * 2.0f, Size * 2.0f);
		Graphics()->QuadsDrawTL(&Quad, 1);
	}

	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
	Graphics()->BlendNormal();
}

void CBlackFrost::DrawAura(const vec2 &Origin, float Scale, float Glow)
{
	if(!m_GlowTexture.IsValid() || Glow <= 0.0f)
		return;

	const float R = 30.0f * Scale;
	Graphics()->BlendAdditive();
	Graphics()->TextureSet(m_GlowTexture);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(0.26f, 0.62f, 0.94f, 0.075f * Glow * m_Fade);
	const IGraphics::CQuadItem Quad(Origin.x - R, Origin.y - R, R * 2.0f, R * 2.0f);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
	Graphics()->BlendNormal();
}

void CBlackFrost::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const bool Enabled = g_Config.m_FcBlackFrost != 0;
	int aIds[2] = {GameClient()->m_aLocalIds[0], g_Config.m_FcBlackFrostDummy ? GameClient()->m_aLocalIds[1] : -1};

	bool AnyVisible = false;
	if(Enabled)
	{
		for(int i = 0; i < 2; i++)
		{
			const int Id = aIds[i];
			if(Id < 0 || Id >= MAX_CLIENTS || (i == 1 && Id == aIds[0]))
				continue;
			if(GameClient()->m_aClients[Id].m_Active && GameClient()->m_Snap.m_aCharacters[Id].m_Active)
			{
				AnyVisible = true;
				break;
			}
		}
	}
	else
	{
		aIds[0] = -1;
		aIds[1] = -1;
	}

	const float Dt = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	const float TargetFade = AnyVisible ? 1.0f : 0.0f;
	m_Fade = TargetFade > m_Fade ? std::min(TargetFade, m_Fade + Dt * 2.6f) : std::max(TargetFade, m_Fade - Dt * 4.0f);

	if(m_Fade <= 0.0f)
	{
		if(!AnyVisible)
			OnReset();
		return;
	}

	EnsureTextures();

	const float Anim = g_Config.m_FcBlackFrostAnim / 100.0f;
	const float Glow = g_Config.m_FcBlackFrostGlow / 100.0f;
	const float Scale = g_Config.m_FcBlackFrostScale / 100.0f;
	const float Density = g_Config.m_FcBlackFrostDensity / 100.0f;

	const bool bShards = g_Config.m_FcBlackFrostShards != 0;
	const bool bParticles = g_Config.m_FcBlackFrostParticles != 0 && Density > 0.0f;

	const int ShardCount = std::clamp(round_to_int((float)MAX_SHARDS * Density * 0.55f), 2, (int)MAX_SHARDS);

	for(int i = 0; i < 2; i++)
	{
		const int Id = aIds[i];
		if(Id < 0 || Id >= MAX_CLIENTS || (i == 1 && Id == aIds[0]))
			continue;
		if(!GameClient()->m_aClients[Id].m_Active || !GameClient()->m_Snap.m_aCharacters[Id].m_Active)
			continue;

		const vec2 Origin = GameClient()->m_aClients[Id].m_RenderPos;

		for(int s = 0; s < MAX_SHARDS; s++)
		{
			if(s >= ShardCount)
				m_aaShards[i][s].m_Active = false;
			else if(!m_aaShards[i][s].m_Active)
				SpawnShard(i, s, Origin, Scale);
		}

		if(bShards)
		{
			UpdateShards(i, Origin, Dt, Scale, Anim);
			RenderShards(i, Scale, Glow);
		}

		if(bParticles)
		{
			UpdateMotes(i, Origin, Dt, Scale, Anim);
			RenderMotes(i, Scale, Glow);
		}

		DrawAura(Origin, Scale, Glow);
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->TextureClear();
	Graphics()->BlendNormal();
}
