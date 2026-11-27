#include "celestial_aura.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/image.h>
#include <engine/shared/config.h>

#include <game/client/components/tclient/fx/fgf_glow.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float HALO_RADIUS = 21.0f;
constexpr float HALO_HEAD_GAP = 9.0f;
constexpr float TEE_HEAD_TOP = 32.0f;
constexpr float WING_SHOULDER_Y = -12.0f;
constexpr float RING_TEXTURE_FACTOR = 1.25f;

const ColorRGBA HALO_AMBER(1.0f, 0.36f, 0.05f, 1.0f);
const ColorRGBA HALO_GOLD(1.0f, 0.78f, 0.26f, 1.0f);
const ColorRGBA HALO_WHITE(1.0f, 0.97f, 0.88f, 1.0f);

ColorRGBA LerpColor(ColorRGBA From, ColorRGBA To, float Amount)
{
	Amount = std::clamp(Amount, 0.0f, 1.0f);
	return ColorRGBA(
		From.r + (To.r - From.r) * Amount,
		From.g + (To.g - From.g) * Amount,
		From.b + (To.b - From.b) * Amount,
		From.a + (To.a - From.a) * Amount);
}

ColorRGBA WithAlpha(ColorRGBA Color, float Alpha)
{
	return ColorRGBA(Color.r, Color.g, Color.b, std::clamp(Alpha, 0.0f, 1.0f));
}

ColorRGBA WarmHighlight(ColorRGBA Color, float Amount)
{
	if(Amount <= 0.0f)
		return Color;
	const ColorRGBA Warm(
		std::min(1.0f, Color.r * 0.92f + 0.28f),
		std::min(1.0f, Color.g * 0.80f + 0.18f),
		std::min(1.0f, Color.b * 0.66f + 0.08f),
		Color.a);
	return LerpColor(Color, Warm, std::clamp(Amount, 0.0f, 1.0f));
}

ColorRGBA Shade(ColorRGBA Color, float Factor)
{
	return ColorRGBA(Color.r * Factor, Color.g * Factor, Color.b * Factor * 1.06f, Color.a);
}

float Hash01(int Seed)
{
	uint32_t X = (uint32_t)Seed * 0x9E3779B1u + 0x7F4A7C15u;
	X ^= X >> 15;
	X *= 0x85EBCA77u;
	X ^= X >> 13;
	X *= 0xC2B2AE3Du;
	X ^= X >> 16;
	return (float)(X & 0xFFFFFFu) / (float)0x1000000u;
}

float HashSigned(int Seed)
{
	return Hash01(Seed) * 2.0f - 1.0f;
}

float SmoothStep(float Edge0, float Edge1, float X)
{
	const float T = std::clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
	return T * T * (3.0f - 2.0f * T);
}

IGraphics::CTextureHandle MakeTexture(IGraphics *pGraphics, int Width, int Height, float (*pAlpha)(float, float), const char *pName)
{
	CImageInfo Image;
	Image.m_Width = Width;
	Image.m_Height = Height;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.m_pData = (uint8_t *)malloc((size_t)Width * Height * 4);
	for(int y = 0; y < Height; y++)
	{
		for(int x = 0; x < Width; x++)
		{
			const float Dx = (x + 0.5f) / Width * 2.0f - 1.0f;
			const float Dy = (y + 0.5f) / Height * 2.0f - 1.0f;
			uint8_t *pPixel = &Image.m_pData[((size_t)y * Width + x) * 4];
			pPixel[0] = 255;
			pPixel[1] = 255;
			pPixel[2] = 255;
			pPixel[3] = (uint8_t)std::clamp(round_to_int(pAlpha(Dx, Dy) * 255.0f), 0, 255);
		}
	}
	return pGraphics->LoadTextureRawMove(Image, 0, pName);
}

float RingAlpha(float Dx, float Dy)
{
	const float Distance = std::sqrt(Dx * Dx + Dy * Dy);
	if(Distance >= 1.0f)
		return 0.0f;
	const float Band = std::exp(-std::pow((Distance - 0.820f) / 0.056f, 2.0f));
	const float Core = std::exp(-std::pow((Distance - 0.820f) / 0.020f, 2.0f)) * 0.85f;
	const float Bloom = std::exp(-std::pow((Distance - 0.820f) / 0.150f, 2.0f)) * 0.22f;
	const float A = std::clamp(std::max(Band, Core) + Bloom, 0.0f, 1.0f);
	return A * std::clamp((1.0f - Distance) / 0.05f, 0.0f, 1.0f);
}

IGraphics::CTextureHandle MakeFeatherTexture(IGraphics *pGraphics, bool Rounded, const char *pName)
{
	constexpr int FeatherW = 192;
	constexpr int FeatherH = 112;
	// feather spans the full texture; gutters and tip fade keep the quad edges clear
	CImageInfo Feather;
	Feather.m_Width = FeatherW;
	Feather.m_Height = FeatherH;
	Feather.m_Format = CImageInfo::FORMAT_RGBA;
	Feather.m_pData = (uint8_t *)malloc((size_t)FeatherW * FeatherH * 4);
	for(int y = 0; y < FeatherH; y++)
	{
		for(int x = 0; x < FeatherW; x++)
		{
			constexpr float U_GUTTER = 0.018f;
			const float RawU = (x + 0.5f) / FeatherW;
			const float U = (RawU - U_GUTTER) / (1.0f - 2.0f * U_GUTTER);
			const float V = (y + 0.5f) / FeatherH * 2.0f - 1.0f;
			const float Uc = std::clamp(U, 0.0f, 1.0f);

			float W;
			if(Rounded)
			{
				const float Taper = Uc < 0.55f ? 1.0f : std::pow(std::clamp((1.0f - Uc) / 0.45f, 0.0f, 1.0f), 0.55f);
				W = 0.58f * Taper * (1.0f - 0.22f * Uc * Uc);
			}
			else
			{
				const float Taper = Uc < 0.55f ? 1.0f : std::pow(std::clamp((1.0f - Uc) / 0.45f, 0.0f, 1.0f), 0.90f);
				W = 0.60f * Taper;
			}

			W *= 0.88f + 0.12f * std::sin(pi * std::min(1.0f, Uc * 2.4f));
			W *= 1.0f + 0.030f * std::sin(Uc * 34.0f);

			const float RootTaper = std::pow(std::clamp(Uc / 0.16f, 0.0f, 1.0f), 0.58f);
			W *= mix(0.11f, 1.0f, RootTaper);
			W = std::max(W, 0.0f);

			const float Inside = (W - std::abs(V)) / 0.050f;
			const float RootFade = SmoothStep(0.0f, 1.0f, std::clamp(U / 0.085f, 0.0f, 1.0f));
			const float TipFade = SmoothStep(0.0f, 1.0f, std::clamp((1.0f - U) / 0.045f, 0.0f, 1.0f));
			const float GutterMask = RootFade * TipFade;
			const float A = SmoothStep(0.0f, 1.0f, std::clamp(Inside, 0.0f, 1.0f)) * GutterMask;

			float ShadeVal = 1.0f;
			const float Rachis = std::exp(-std::pow(V / 0.050f, 2.0f));
			ShadeVal *= 1.0f - 0.26f * Rachis * (1.0f - Uc * 0.45f);

			const float EdgeSoft = std::clamp(W * 0.62f, 0.030f, 0.170f);
			const float Edge = std::clamp(1.0f - (W - std::abs(V)) / EdgeSoft, 0.0f, 1.0f);
			ShadeVal *= 1.0f - 0.30f * Edge;
			ShadeVal *= 1.0f + 0.13f * std::clamp(-V, 0.0f, 1.0f);
			ShadeVal *= 1.0f - 0.10f * std::clamp(V, 0.0f, 1.0f);
			ShadeVal *= 1.0f + 0.055f * std::sin(Uc * 26.0f + V * 18.0f);

			uint8_t *pPixel = &Feather.m_pData[((size_t)y * FeatherW + x) * 4];
			const uint8_t C = (uint8_t)std::clamp(round_to_int(ShadeVal * 255.0f), 0, 255);
			pPixel[0] = C;
			pPixel[1] = C;
			pPixel[2] = C;
			pPixel[3] = (uint8_t)std::clamp(round_to_int(A * 255.0f), 0, 255);
		}
	}
	return pGraphics->LoadTextureRawMove(Feather, 0, pName);
}
} // namespace

void CCelestialAura::OnInit()
{
	OnReset();
}

void CCelestialAura::OnReset()
{
	for(auto &Particle : m_aParticles)
		Particle.m_Active = false;
	m_NextParticle = 0;
	m_SpawnAccum = 0.0f;
	m_Fade = 0.0f;
	for(int i = 0; i < 2; i++)
	{
		m_aFlapPhase[i] = 0.0f;
		m_aMoveAmt[i] = 0.0f;
		m_aSpeed[i] = 0.0f;
		m_aSpin[i] = 0.0f;
		m_aLastPos[i] = vec2(0.0f, 0.0f);
		m_aLastPosValid[i] = false;
		m_aHaloPos[i] = vec2(0.0f, 0.0f);
		m_aHaloPosValid[i] = false;
	}
}

void CCelestialAura::OnShutdown()
{
	if(m_GlowTexture.IsValid())
		Graphics()->UnloadTexture(&m_GlowTexture);
	if(m_RingTexture.IsValid())
		Graphics()->UnloadTexture(&m_RingTexture);
	if(m_FeatherTexture.IsValid())
		Graphics()->UnloadTexture(&m_FeatherTexture);
	if(m_FeatherRoundTexture.IsValid())
		Graphics()->UnloadTexture(&m_FeatherRoundTexture);
	if(m_PetalTexture.IsValid())
		Graphics()->UnloadTexture(&m_PetalTexture);
	m_TexturesReady = false;
}

void CCelestialAura::EnsureTextures()
{
	if(m_TexturesReady)
		return;

	m_GlowTexture = FgfGlow::CreateGlowTexture(Graphics());
	m_PetalTexture = FgfGlow::CreatePetalTexture(Graphics());
	m_RingTexture = MakeTexture(Graphics(), 256, 256, RingAlpha, "celestial_ring");

	m_FeatherTexture = MakeFeatherTexture(Graphics(), false, "celestial_feather");
	m_FeatherRoundTexture = MakeFeatherTexture(Graphics(), true, "celestial_feather_round");

	m_TexturesReady = true;
}

CCelestialAura::SParticle *CCelestialAura::NewParticle()
{
	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		const int Index = (m_NextParticle + i) % MAX_PARTICLES;
		if(!m_aParticles[Index].m_Active)
		{
			m_NextParticle = (Index + 1) % MAX_PARTICLES;
			return &m_aParticles[Index];
		}
	}
	return nullptr;
}

void CCelestialAura::SpawnParticle(float Time, const vec2 &Origin, const vec2 &Shoulder, float WingScale, int Owner)
{
	SParticle *pSlot = NewParticle();
	if(pSlot == nullptr)
		return;

	const int Seed = (int)(Time * 4096.0f) * 31 + m_NextParticle * 977 + Owner * 131;
	const float KindRoll = Hash01(Seed + 3);

	SParticle &Particle = *pSlot;
	Particle = SParticle{};
	Particle.m_Active = true;
	Particle.m_Owner = Owner;
	Particle.m_Phase = Hash01(Seed + 5) * 2.0f * pi;
	Particle.m_Rot = Hash01(Seed + 6) * 2.0f * pi;

	// different lifecycle per kind: halo-orbiting embers/sparks, wing-falling petals
	if(KindRoll < 0.62f)
	{
		Particle.m_Kind = PARTICLE_EMBER;
		const float Angle = Hash01(Seed + 7) * 2.0f * pi;
		const float Radius = HALO_RADIUS * (0.85f + Hash01(Seed + 8) * 1.5f);
		Particle.m_Pos = direction(Angle) * Radius + vec2(0.0f, HALO_RADIUS * 0.35f);
		Particle.m_Vel = vec2(HashSigned(Seed + 9) * 9.0f, -14.0f - Hash01(Seed + 10) * 20.0f);
		Particle.m_LifeSpan = 1.6f + Hash01(Seed + 11) * 1.7f;
		Particle.m_Size = 2.0f + Hash01(Seed + 12) * 2.4f;
		Particle.m_Sway = 6.0f + Hash01(Seed + 13) * 12.0f;
		Particle.m_RotSpeed = 0.0f;
		const float Tone = Hash01(Seed + 14);
		Particle.m_Color = Tone < 0.5f ?
					   LerpColor(ColorRGBA(1.0f, 0.74f, 0.30f, 1.0f), ColorRGBA(1.0f, 0.50f, 0.18f, 1.0f), Tone * 2.0f) :
					   LerpColor(ColorRGBA(1.0f, 0.56f, 0.16f, 1.0f), ColorRGBA(1.0f, 0.84f, 0.42f, 1.0f), (Tone - 0.5f) * 2.0f);
	}
	else if(KindRoll < 0.86f)
	{
		Particle.m_Kind = PARTICLE_SPARK;
		const float Angle = Hash01(Seed + 15) * 2.0f * pi;
		const float Radius = HALO_RADIUS * (0.75f + Hash01(Seed + 16) * 0.9f);
		Particle.m_Pos = direction(Angle) * Radius;
		Particle.m_Vel = vec2(HashSigned(Seed + 17) * 12.0f, -28.0f - Hash01(Seed + 18) * 28.0f);
		Particle.m_LifeSpan = 0.9f + Hash01(Seed + 19) * 1.1f;
		Particle.m_Size = 1.2f + Hash01(Seed + 20) * 1.2f;
		Particle.m_Sway = 10.0f + Hash01(Seed + 21) * 16.0f;
		Particle.m_RotSpeed = 0.0f;
		Particle.m_Color = LerpColor(ColorRGBA(1.0f, 0.90f, 0.62f, 1.0f), ColorRGBA(1.0f, 0.72f, 0.34f, 1.0f), Hash01(Seed + 22));
	}
	else
	{
		Particle.m_Kind = PARTICLE_PETAL;
		Particle.m_Pos = (Shoulder - Origin) + vec2(HashSigned(Seed + 23) * 58.0f * WingScale, -34.0f * WingScale + HashSigned(Seed + 24) * 30.0f * WingScale);
		Particle.m_Vel = vec2(HashSigned(Seed + 25) * 7.0f, -9.0f - Hash01(Seed + 26) * 13.0f);
		Particle.m_LifeSpan = 2.2f + Hash01(Seed + 27) * 1.8f;
		Particle.m_Size = 3.0f + Hash01(Seed + 28) * 2.6f;
		Particle.m_Sway = 8.0f + Hash01(Seed + 29) * 14.0f;
		Particle.m_RotSpeed = HashSigned(Seed + 30) * 1.6f;
		Particle.m_Color = Hash01(Seed + 31) < 0.5f ? ColorRGBA(0.78f, 0.28f, 0.32f, 1.0f) : ColorRGBA(0.90f, 0.46f, 0.46f, 1.0f);
	}

	Particle.m_Life = Particle.m_LifeSpan;
}

void CCelestialAura::UpdateParticles(float Dt, float Time, int Owner)
{
	for(auto &Particle : m_aParticles)
	{
		if(!Particle.m_Active || Particle.m_Owner != Owner)
			continue;

		Particle.m_Life -= Dt;
		if(Particle.m_Life <= 0.0f)
		{
			Particle.m_Active = false;
			continue;
		}

		Particle.m_Vel.x += std::sin(Time * 1.9f + Particle.m_Phase) * Particle.m_Sway * Dt;
		Particle.m_Vel.x *= 1.0f - std::clamp(Dt * 1.4f, 0.0f, 1.0f);
		Particle.m_Pos += Particle.m_Vel * Dt;
		Particle.m_Rot += Particle.m_RotSpeed * Dt;

		// kill drifted particles so they never accumulate forever
		if(Particle.m_Pos.y < -190.0f)
			Particle.m_Active = false;
	}
}

void CCelestialAura::RenderParticles(const SParams &Params, const vec2 &Origin, int Owner)
{
	const float Anim = Params.m_Anim;

	Graphics()->BlendAdditive();
	Graphics()->WrapClamp();

	if(m_GlowTexture.IsValid())
	{
		Graphics()->TextureSet(m_GlowTexture);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(0.0f);
		for(const auto &Particle : m_aParticles)
		{
			if(!Particle.m_Active || Particle.m_Owner != Owner || Particle.m_Kind == PARTICLE_PETAL)
				continue;

			const float T = 1.0f - Particle.m_Life / Particle.m_LifeSpan;
			const float Fade = std::min(1.0f, T / 0.18f) * std::min(1.0f, (1.0f - T) / 0.42f);
			const float Size = Particle.m_Size * (0.75f + 0.55f * T) * (1.0f + 0.12f * std::sin(Params.m_Time * 5.0f + Particle.m_Phase) * Anim);
			const float Alpha = Fade * Params.m_Alpha * (Particle.m_Kind == PARTICLE_SPARK ? 0.95f : 0.72f);

			Graphics()->SetColor(WithAlpha(Particle.m_Color, Alpha));
			const vec2 P = Origin + Particle.m_Pos;
			const IGraphics::CQuadItem Quad(P.x - Size, P.y - Size, Size * 2.0f, Size * 2.0f);
			Graphics()->QuadsDrawTL(&Quad, 1);
		}
		Graphics()->QuadsEnd();
	}

	if(m_PetalTexture.IsValid())
	{
		Graphics()->TextureSet(m_PetalTexture);
		Graphics()->QuadsBegin();
		for(const auto &Particle : m_aParticles)
		{
			if(!Particle.m_Active || Particle.m_Owner != Owner || Particle.m_Kind != PARTICLE_PETAL)
				continue;

			const float T = 1.0f - Particle.m_Life / Particle.m_LifeSpan;
			const float Fade = std::min(1.0f, T / 0.20f) * std::min(1.0f, (1.0f - T) / 0.45f);
			const float Size = Particle.m_Size * (0.8f + 0.4f * T);

			Graphics()->QuadsSetRotation(Particle.m_Rot);
			Graphics()->SetColor(WithAlpha(Particle.m_Color, Fade * Params.m_Alpha * 0.66f));
			const vec2 P = Origin + Particle.m_Pos;
			const IGraphics::CQuadItem Quad(P.x - Size, P.y - Size, Size * 2.0f, Size * 2.0f);
			Graphics()->QuadsDrawTL(&Quad, 1);
		}
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();
	}

	Graphics()->WrapNormal();
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
}

void CCelestialAura::DrawFeather(IGraphics::CTextureHandle Texture, const vec2 &Root, float Angle, float Length, float Width,
	ColorRGBA ColorRoot, ColorRGBA ColorTip, float Alpha, float Highlight)
{
	if(!Texture.IsValid() || Length <= 0.5f || Width <= 0.5f)
		return;

	const vec2 Dir = direction(Angle);
	const vec2 Center = Root + Dir * (Length * 0.5f);

	const ColorRGBA Base = WithAlpha(ColorRoot, Alpha);
	const ColorRGBA Tip = WithAlpha(ColorTip, Alpha);
	const ColorRGBA UpRoot = WarmHighlight(Base, Highlight);
	const ColorRGBA UpTip = WarmHighlight(Tip, Highlight * 0.55f);
	const ColorRGBA DnRoot = Shade(Base, 0.70f);
	const ColorRGBA DnTip = Shade(Tip, 0.62f);

	Graphics()->SetColor4(UpRoot, UpTip, DnRoot, DnTip);
	Graphics()->QuadsSetRotation(Angle);
	const IGraphics::CQuadItem Quad(Center.x - Length * 0.5f, Center.y - Width * 0.5f, Length, Width);
	Graphics()->QuadsDrawTL(&Quad, 1);
}

void CCelestialAura::DrawWing(int Side, const vec2 &Shoulder, const SParams &Params, float WingScale)
{
	const float S = (float)Side;
	const float Alpha = Params.m_Alpha;
	const float Anim = Params.m_Anim;
	const float Time = Params.m_Time;
	const float Highlight = 0.22f + 0.32f * Params.m_Pulse;

	const auto Local = [&](float X, float Y) {
		return Shoulder + vec2(X * S, Y) * WingScale;
	};

	const vec2 Spine0 = Local(1.0f, -1.0f);
	const vec2 Spine1 = Local(18.0f, -28.0f);
	const vec2 Spine2 = Local(42.0f, -33.0f);
	const vec2 Spine3 = Local(62.0f, -8.0f);

	const float Flap = Params.m_Flap;
	const float Move = Params.m_Move;

	const float Stroke = std::sin(Flap);
	const float LiftAngle = -Stroke * (0.040f + 0.170f * Move) * Anim * S;
	const float Fold = (0.5f - 0.5f * Stroke) * (0.050f + 0.150f * Move) * Anim;

	const auto SpinePoint = [&](float T) {
		const float U = 1.0f - T;
		vec2 P = Spine0 * (U * U * U) + Spine1 * (3.0f * U * U * T) + Spine2 * (3.0f * U * T * T) + Spine3 * (T * T * T);
		P.y += std::sin(Flap - T * 2.15f) * (0.9f + 6.2f * T) * Move * Anim * WingScale;
		const vec2 Rel = P - Spine0;
		const float C = std::cos(LiftAngle);
		const float Sn = std::sin(LiftAngle);
		const vec2 Rot(Rel.x * C - Rel.y * Sn, Rel.x * Sn + Rel.y * C);
		return Spine0 + vec2(Rot.x * (1.0f - Fold), Rot.y);
	};

	const auto Hang = [&](float T) {
		const float Base = mix(1.52f, 0.94f, std::pow(T, 0.85f));
		const float Swing = std::sin(Flap - T * 1.75f);
		return Base + Swing * (0.014f + 0.105f * Move) * Anim + Fold * 0.42f;
	};

	const auto SpanLength = [&](float T) {
		const float Splay = 1.0f + Stroke * Move * (0.05f + 0.09f * T) * Anim;
		return mix(30.0f, 56.0f, std::pow(T, 0.62f)) * WingScale * Splay;
	};

	struct SFeather
	{
		vec2 m_Root;
		float m_Angle;
		float m_Length;
		float m_Width;
	};

	const auto Place = [&](float T, int Seed, float Wobble, float LengthBias, float WidthBias, SFeather &Out) {
		const float Lag = HashSigned(Seed) * 0.30f;
		const float Ang = Hang(T) + HashSigned(Seed) * 0.045f +
			std::sin(Time * 0.62f + (float)Seed * 0.07f) * Wobble * Anim +
			std::sin(Flap - T * 1.75f + Lag) * (0.010f + 0.060f * Move) * Anim;
		const float Dir = S > 0.0f ? Ang : pi - Ang;
		const float Breathe = 1.0f + std::sin(Flap - T * 2.15f + Lag) * (0.008f + 0.040f * Move) * Anim;
		Out.m_Root = SpinePoint(T);
		Out.m_Angle = Dir;
		Out.m_Length = SpanLength(T) * LengthBias * Breathe * (1.0f + HashSigned(Seed + 1) * 0.085f);
		Out.m_Width = WidthBias * WingScale;
	};

	const ColorRGBA MembTop(0.44f, 0.08f, 0.22f, 1.0f);
	const ColorRGBA MembBottom(0.17f, 0.02f, 0.12f, 1.0f);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0.0f);
	// membrane as short segments so the hem follows the bezier spine
	constexpr int MEMBRANE_STEPS = 14;
	for(int i = 0; i < MEMBRANE_STEPS; i++)
	{
		const float T0 = (float)i / MEMBRANE_STEPS;
		const float T1 = (float)(i + 1) / MEMBRANE_STEPS;
		const vec2 R0 = SpinePoint(T0);
		const vec2 R1 = SpinePoint(T1);
		const vec2 B0 = R0 + direction(S > 0.0f ? Hang(T0) : pi - Hang(T0)) * SpanLength(T0) * 0.58f;
		const vec2 B1 = R1 + direction(S > 0.0f ? Hang(T1) : pi - Hang(T1)) * SpanLength(T1) * 0.58f;
		const ColorRGBA Top = WithAlpha(MembTop, Alpha * 0.90f);
		const ColorRGBA Bottom = WithAlpha(MembBottom, Alpha * 0.80f);
		Graphics()->SetColor4(Top, Top, Bottom, Bottom);
		const IGraphics::CFreeformItem Quad(R0, R1, B1, B0);
		Graphics()->QuadsDrawFreeform(&Quad, 1);
	}

	Graphics()->QuadsEnd();

	Graphics()->TextureSet(m_FeatherTexture);
	Graphics()->WrapClamp();
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0.0f);

	// primaries first so the shorter rows cover their roots
	constexpr int NUM_PRIMARIES = 17;
	for(int i = 0; i < NUM_PRIMARIES; i++)
	{
		const float Tt = 0.18f + 0.82f * ((float)i / (float)(NUM_PRIMARIES - 1));
		const int Seed = 1000 + i * 37;
		SFeather Feather;
		Place(Tt, Seed, 0.055f, 1.0f, mix(13.5f, 17.5f, Tt), Feather);
		const float Tone = Hash01(2000 + i * 53);
		const ColorRGBA ColorRoot = LerpColor(ColorRGBA(0.78f, 0.20f, 0.38f, 1.0f), ColorRGBA(0.64f, 0.13f, 0.31f, 1.0f), Tone);
		const ColorRGBA ColorTip = LerpColor(ColorRGBA(0.46f, 0.05f, 0.28f, 1.0f), ColorRGBA(0.34f, 0.03f, 0.24f, 1.0f), Tone);
		DrawFeather(m_FeatherTexture, Feather.m_Root, Feather.m_Angle, Feather.m_Length, Feather.m_Width, ColorRoot, ColorTip, Alpha * 0.99f, Highlight * 0.45f);
	}

	constexpr int NUM_SECONDARIES = 16;
	for(int i = 0; i < NUM_SECONDARIES; i++)
	{
		const float Tt = 0.10f + 0.86f * ((float)i / (float)(NUM_SECONDARIES - 1));
		const int Seed = 3000 + i * 61;
		SFeather Feather;
		Place(Tt, Seed, 0.045f, 0.72f, mix(14.0f, 18.0f, Tt), Feather);
		const float Tone = Hash01(4000 + i * 47);
		const ColorRGBA ColorRoot = LerpColor(ColorRGBA(0.92f, 0.44f, 0.54f, 1.0f), ColorRGBA(0.82f, 0.31f, 0.45f, 1.0f), Tone);
		const ColorRGBA ColorTip = LerpColor(ColorRGBA(0.70f, 0.15f, 0.38f, 1.0f), ColorRGBA(0.56f, 0.09f, 0.32f, 1.0f), Tone);
		DrawFeather(m_FeatherTexture, Feather.m_Root, Feather.m_Angle, Feather.m_Length, Feather.m_Width, ColorRoot, ColorTip, Alpha, Highlight * 0.85f);
	}

	Graphics()->QuadsEnd();

	Graphics()->TextureSet(m_FeatherRoundTexture);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetRotation(0.0f);

	constexpr int NUM_COVERTS = 15;
	for(int i = 0; i < NUM_COVERTS; i++)
	{
		const float Tt = 0.02f + 0.88f * ((float)i / (float)(NUM_COVERTS - 1));
		const int Seed = 5000 + i * 71;
		SFeather Feather;
		Place(Tt, Seed, 0.040f, 0.60f, mix(17.0f, 21.0f, Tt), Feather);
		Feather.m_Root.y -= 2.5f * WingScale;
		const float Tone = Hash01(6000 + i * 43);
		const ColorRGBA ColorRoot = LerpColor(ColorRGBA(0.99f, 0.90f, 0.90f, 1.0f), ColorRGBA(0.97f, 0.82f, 0.85f, 1.0f), Tone);
		const ColorRGBA ColorTip = LerpColor(ColorRGBA(0.95f, 0.66f, 0.74f, 1.0f), ColorRGBA(0.88f, 0.54f, 0.66f, 1.0f), Tone);
		DrawFeather(m_FeatherRoundTexture, Feather.m_Root, Feather.m_Angle, Feather.m_Length, Feather.m_Width, ColorRoot, ColorTip, Alpha, Highlight * 1.25f);
	}

	Graphics()->QuadsEnd();
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->WrapNormal();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CCelestialAura::DrawHalo(const vec2 &Center, const SParams &Params, float HaloScale)
{
	const float Time = Params.m_Time;
	const float Alpha = Params.m_Alpha;
	const float Anim = Params.m_Anim;
	const float Glow = Params.m_Glow;
	const float Pulse = Params.m_Pulse;
	const float Radius = HALO_RADIUS * HaloScale;

	Graphics()->WrapClamp();

	if(m_GlowTexture.IsValid() && Glow > 0.0f)
	{
		Graphics()->BlendAdditive();
		Graphics()->TextureSet(m_GlowTexture);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(0.0f);
		static const float s_aSizes[] = {4.2f, 2.8f, 1.95f};
		static const float s_aAlphas[] = {0.100f, 0.165f, 0.255f};
		for(int i = 0; i < 3; i++)
		{
			const float R = Radius * s_aSizes[i] * (1.0f + 0.05f * std::sin(Time * 0.85f + i * 1.9f) * Anim);
			Graphics()->SetColor(1.0f, 0.46f + 0.06f * i, 0.15f + 0.03f * i, s_aAlphas[i] * Glow * Alpha * Pulse);
			const IGraphics::CQuadItem Quad(Center.x - R, Center.y - R, R * 2.0f, R * 2.0f);
			Graphics()->QuadsDrawTL(&Quad, 1);
		}
		Graphics()->QuadsEnd();
		Graphics()->TextureClear();
	}

	if(m_GlowTexture.IsValid())
	{
		Graphics()->BlendAdditive();
		Graphics()->TextureSet(m_GlowTexture);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(0.0f);

		constexpr int SEGMENTS = 72;
		const float Pitch = 1.16f + 0.10f * std::sin(Time * 0.7f) * Anim;
		const float Yaw = Params.m_Spin;
		const float CP = std::cos(Pitch), SP = std::sin(Pitch);
		const float CY = std::cos(Yaw), SY = std::sin(Yaw);

		for(int i = 0; i < SEGMENTS; i++)
		{
			const float Theta = ((float)i + 0.5f) / (float)SEGMENTS * 2.0f * pi;
			const float Lx = std::cos(Theta);
			const float Ly = std::sin(Theta);

			const float Y1 = Ly * CP;
			const float Z1 = Ly * SP;

			const float X2 = Lx * CY + Z1 * SY;
			const float Z2 = -Lx * SY + Z1 * CY;

			const float Persp = 1.0f + 0.18f * Z2;
			const float Depth = 0.55f + 0.45f * (0.5f + 0.5f * Z2);

			const vec2 P = Center + vec2(X2, Y1) * Radius * Persp;
			const float Size = Radius * 0.175f * Persp;

			const ColorRGBA Color = ColorRGBA(1.0f, 0.84f + 0.10f * Z2, 0.42f + 0.16f * Z2, 1.0f);
			Graphics()->SetColor(WithAlpha(Color, 0.62f * Depth * Alpha * Pulse));

			const IGraphics::CQuadItem Quad(P.x - Size, P.y - Size, Size * 2.0f, Size * 2.0f);
			Graphics()->QuadsDrawTL(&Quad, 1);
		}

		Graphics()->QuadsEnd();
		Graphics()->TextureClear();
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->WrapNormal();
	Graphics()->BlendNormal();
}

void CCelestialAura::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const bool Enabled = g_Config.m_FcCelestial != 0;
	int aIds[2] = {GameClient()->m_aLocalIds[0], g_Config.m_FcCelestialDummy ? GameClient()->m_aLocalIds[1] : -1};

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

	const float Time = LocalTime();
	const float Anim = g_Config.m_FcCelestialAnim / 100.0f;
	const float Glow = g_Config.m_FcCelestialGlow / 100.0f;
	const float HaloScale = g_Config.m_FcCelestialHaloSize / 100.0f;
	const float WingScale = g_Config.m_FcCelestialWingScale / 100.0f;
	const float Density = g_Config.m_FcCelestialDensity / 100.0f;

	const bool bHalo = g_Config.m_FcCelestialHalo != 0;
	const bool bWings = g_Config.m_FcCelestialWings != 0;
	const bool bParticles = g_Config.m_FcCelestialParticles != 0 && Density > 0.0f;

	for(int i = 0; i < 2; i++)
	{
		const int Id = aIds[i];
		if(Id < 0 || Id >= MAX_CLIENTS || (i == 1 && Id == aIds[0]))
			continue;
		if(!GameClient()->m_aClients[Id].m_Active || !GameClient()->m_Snap.m_aCharacters[Id].m_Active)
			continue;

		const vec2 Pos = GameClient()->m_aClients[Id].m_RenderPos;

		if(m_aLastPosValid[i])
		{
			const float RawSpeed = length(Pos - m_aLastPos[i]) / std::max(Dt, 0.0001f);
			m_aSpeed[i] = mix(m_aSpeed[i], RawSpeed, std::clamp(Dt * 3.0f, 0.0f, 1.0f));
			const float Target = std::clamp(m_aSpeed[i] / 520.0f, 0.0f, 1.0f);
			m_aMoveAmt[i] = mix(m_aMoveAmt[i], Target, std::clamp(Dt * (Target > m_aMoveAmt[i] ? 2.0f : 1.1f), 0.0f, 1.0f));
		}
		m_aLastPos[i] = Pos;
		m_aLastPosValid[i] = true;

		const float FlapRate = mix(0.26f, 1.05f, m_aMoveAmt[i]) * Anim;
		m_aFlapPhase[i] += Dt * FlapRate * 2.0f * pi;
		if(m_aFlapPhase[i] > 2.0f * pi * 64.0f)
			m_aFlapPhase[i] -= 2.0f * pi * 64.0f;

		const float SpinRate = mix(0.30f, 2.60f, m_aMoveAmt[i]) * Anim;
		m_aSpin[i] += Dt * SpinRate;
		if(m_aSpin[i] > 2.0f * pi * 64.0f)
			m_aSpin[i] -= 2.0f * pi * 64.0f;

		const float Bob = std::sin(Time * 0.55f + i * 1.7f) * 1.3f * Anim;
		const vec2 HaloTarget = Pos + vec2(0.0f, -(TEE_HEAD_TOP + HALO_HEAD_GAP + HALO_RADIUS * HaloScale) + Bob);

		const float Follow = std::clamp((float)g_Config.m_FcCelestialHaloFollow / 100.0f, 0.0f, 1.0f);
		if(!m_aHaloPosValid[i])
		{
			m_aHaloPos[i] = HaloTarget;
			m_aHaloPosValid[i] = true;
		}
		else
		{
			const float FollowSpeed = mix(1.6f, 26.0f, Follow * Follow);
			m_aHaloPos[i] += (HaloTarget - m_aHaloPos[i]) * std::clamp(Dt * FollowSpeed, 0.0f, 1.0f);
		}

		const vec2 HaloCenter = m_aHaloPos[i];
		const vec2 Shoulder = Pos + vec2(0.0f, WING_SHOULDER_Y);

		SParams Params;
		Params.m_Alpha = m_Fade;
		Params.m_Time = Time;
		Params.m_Anim = Anim;
		Params.m_Glow = Glow;
		Params.m_Pulse = 0.80f + 0.20f * std::sin(Time * 0.75f) * Anim;
		Params.m_Flap = m_aFlapPhase[i];
		Params.m_Move = m_aMoveAmt[i];
		Params.m_Spin = m_aSpin[i];

		if(bParticles)
		{
			UpdateParticles(Dt, Time, i);
			m_SpawnAccum += Dt * 11.0f * Density;
			const int SpawnCount = (int)m_SpawnAccum;
			m_SpawnAccum -= (float)SpawnCount;
			for(int k = 0; k < SpawnCount; k++)
				SpawnParticle(Time, HaloCenter, Shoulder, WingScale, i);
		}

		if(m_GlowTexture.IsValid() && Glow > 0.0f)
		{
			Graphics()->BlendAdditive();
			Graphics()->TextureSet(m_GlowTexture);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetRotation(0.0f);
			for(int Side = -1; Side <= 1; Side += 2)
			{
				const vec2 HazeCenter = Shoulder + vec2((float)Side * 30.0f * WingScale, -30.0f * WingScale);
				const float HazeRadius = 54.0f * WingScale;
				Graphics()->SetColor(0.94f, 0.42f, 0.26f, 0.045f * Glow * Params.m_Alpha * Params.m_Pulse);
				const IGraphics::CQuadItem Haze(HazeCenter.x - HazeRadius, HazeCenter.y - HazeRadius, HazeRadius * 2.0f, HazeRadius * 2.0f);
				Graphics()->QuadsDrawTL(&Haze, 1);
			}
			Graphics()->QuadsEnd();
			Graphics()->TextureClear();
			Graphics()->BlendNormal();
		}

		if(bWings)
		{
			Graphics()->BlendNormal();
			DrawWing(-1, Shoulder, Params, WingScale);
			DrawWing(1, Shoulder, Params, WingScale);
			Graphics()->TextureClear();
		}

		if(bHalo)
			DrawHalo(HaloCenter, Params, HaloScale);

		if(bParticles)
			RenderParticles(Params, HaloCenter, i);
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->TextureClear();
	Graphics()->BlendNormal();
}
