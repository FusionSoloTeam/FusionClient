#include "skin_gradient.h"

#include <base/color.h>
#include <base/math.h>
#include <base/time.h>
#include <engine/shared/config.h>

#include <cmath>

namespace
{
	constexpr int STRIPS = 56;
	constexpr int RADIAL_GRID = 26;

	template<typename T>
	T VcClamp(T Value, T Min, T Max)
	{
		return Value < Min ? Min : (Value > Max ? Max : Value);
	}

	struct SGradientState
	{
		ColorRGBA m_Color1;
		ColorRGBA m_Color2;
		int m_Mode;
		float m_Phase;
		float m_Intensity;
		float m_Glow;
		float m_Bands;
	};

	vec2 VcRotateRadians(vec2 v, float Angle)
	{
		const float s = std::sin(Angle);
		const float c = std::cos(Angle);
		return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
	}

	struct SProjector
	{
		vec2 m_Center;
		vec2 m_Scale;
		vec2 m_Half;
		float m_Angle;
		bool m_VertexSpace;

		vec2 Project(vec2 Local, float Spread) const
		{
			if(m_VertexSpace)
			{
				vec2 Vertex = vec2(Local.x * 2.0f * m_Half.x, Local.y * 2.0f * m_Half.y) * Spread;
				Vertex = vec2(Vertex.x * m_Scale.x, Vertex.y * m_Scale.y);
				return m_Center + VcRotateRadians(Vertex, m_Angle);
			}
			return m_Center + VcRotateRadians(vec2(Local.x * m_Scale.x, Local.y * m_Scale.y) * Spread, m_Angle);
		}
	};

	float AxisValue(float u, float v, int Mode)
	{
		switch(Mode)
		{
		case 1:
			return v;
		case 2:
			return u;
		case 3:
		{
			const float dx = u - 0.5f;
			const float dy = v - 0.5f;
			return VcClamp(std::sqrt(dx * dx + dy * dy) * 2.0f, 0.0f, 1.0f);
		}
		default:
			return (u + v) * 0.5f;
		}
	}

	ColorRGBA MixColor(const ColorRGBA &From, const ColorRGBA &To, float t)
	{
		return ColorRGBA(
			From.r + (To.r - From.r) * t,
			From.g + (To.g - From.g) * t,
			From.b + (To.b - From.b) * t,
			1.0f);
	}

	float Wave(float t)
	{
		return 0.5f + 0.5f * std::cos(t * 2.0f * pi);
	}

	ColorRGBA BaseSample(const SGradientState &State, float u, float v)
	{
		const float t = AxisValue(u, v, State.m_Mode) * State.m_Bands + State.m_Phase;
		ColorRGBA result = MixColor(State.m_Color1, State.m_Color2, Wave(t));
		result.a = State.m_Intensity;
		return result;
	}

	ColorRGBA GlintSample(const SGradientState &State, float u, float v)
	{
		const float t = AxisValue(u, v, State.m_Mode) * State.m_Bands + State.m_Phase;
		const float f = t - std::floor(t);
		float band = 1.0f - std::fabs(f * 2.0f - 1.0f);
		band = std::pow(VcClamp(band, 0.0f, 1.0f), 5.0f);
		ColorRGBA result = MixColor(State.m_Color2, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), 0.45f);
		result.a = band * 0.45f * State.m_Glow;
		return result;
	}

	void EmitQuad(IGraphics *pGraphics, const SGradientState &State, const SProjector &Projector, vec2 Anchor, float AnchorSize, float U0, float V0, float U1, float V1, vec2 Q0, vec2 Q1, vec2 Q2, vec2 Q3, bool Glint)
	{
		const auto world = [&](vec2 Local) {
			return Projector.Project(Local, 1.0f);
		};
		const auto sample = [&](vec2 Local) {
			const vec2 World = world(Local);
			const float u = (World.x - Anchor.x) / AnchorSize + 0.5f;
			const float v = (World.y - Anchor.y) / AnchorSize + 0.5f;
			return Glint ? GlintSample(State, u, v) : BaseSample(State, u, v);
		};

		const vec2 W0 = world(Q0);
		const vec2 W1 = world(Q1);
		const vec2 W2 = world(Q2);
		const vec2 W3 = world(Q3);

		const auto texel = [&](vec2 Local) {
			return vec2(U0 + (Local.x + 0.5f) * (U1 - U0), V0 + (Local.y + 0.5f) * (V1 - V0));
		};
		const vec2 T0 = texel(Q0);
		const vec2 T1 = texel(Q1);
		const vec2 T2 = texel(Q2);
		const vec2 T3 = texel(Q3);

		IGraphics::CFreeformItem item(W0.x, W0.y, W1.x, W1.y, W3.x, W3.y, W2.x, W2.y);
		pGraphics->QuadsSetSubsetFree(T0.x, T0.y, T1.x, T1.y, T3.x, T3.y, T2.x, T2.y);
		pGraphics->SetColor4(sample(Q0), sample(Q1), sample(Q3), sample(Q2));
		pGraphics->QuadsDrawFreeform(&item, 1);
	}

	void DrawStrips(IGraphics *pGraphics, const SGradientState &State, const SProjector &Projector, vec2 Anchor, float AnchorSize, IGraphics::CTextureHandle Texture, float U0, float V0, float U1, float V1, bool Glint)
	{
		pGraphics->TextureSet(Texture);
		pGraphics->QuadsBegin();

		if(State.m_Mode == 3)
		{
			for(int y = 0; y < RADIAL_GRID; y++)
			{
				const float b0 = (float)y / RADIAL_GRID - 0.5f;
				const float b1 = (float)(y + 1) / RADIAL_GRID - 0.5f;
				for(int x = 0; x < RADIAL_GRID; x++)
				{
					const float a0 = (float)x / RADIAL_GRID - 0.5f;
					const float a1 = (float)(x + 1) / RADIAL_GRID - 0.5f;
					EmitQuad(pGraphics, State, Projector, Anchor, AnchorSize, U0, V0, U1, V1,
						vec2(a0, b0), vec2(a1, b0), vec2(a1, b1), vec2(a0, b1), Glint);
				}
			}
			pGraphics->QuadsEnd();
			return;
		}

		if(State.m_Mode == 1 || State.m_Mode == 2)
		{
			for(int i = 0; i < STRIPS; i++)
			{
				const float a0 = (float)i / STRIPS - 0.5f;
				const float a1 = (float)(i + 1) / STRIPS - 0.5f;
				if(State.m_Mode == 1)
					EmitQuad(pGraphics, State, Projector, Anchor, AnchorSize, U0, V0, U1, V1,
						vec2(-0.5f, a0), vec2(0.5f, a0), vec2(0.5f, a1), vec2(-0.5f, a1), Glint);
				else
					EmitQuad(pGraphics, State, Projector, Anchor, AnchorSize, U0, V0, U1, V1,
						vec2(a0, -0.5f), vec2(a1, -0.5f), vec2(a1, 0.5f), vec2(a0, 0.5f), Glint);
			}
			pGraphics->QuadsEnd();
			return;
		}

		const auto chord_a = [&](float s) {
			const float u_lo = VcClamp(s - 0.5f, -0.5f, 0.5f);
			return vec2(u_lo, s - u_lo);
		};
		const auto chord_b = [&](float s) {
			const float u_hi = VcClamp(s + 0.5f, -0.5f, 0.5f);
			return vec2(u_hi, s - u_hi);
		};

		for(int i = 0; i < STRIPS; i++)
		{
			const float s0 = -1.0f + 2.0f * (float)i / STRIPS;
			const float s1 = -1.0f + 2.0f * (float)(i + 1) / STRIPS;
			EmitQuad(pGraphics, State, Projector, Anchor, AnchorSize, U0, V0, U1, V1, chord_a(s0), chord_a(s1), chord_b(s1), chord_b(s0), Glint);
		}

		pGraphics->QuadsEnd();
	}

	void DrawHaloRing(IGraphics *pGraphics, const SGradientState &State, const SProjector &Projector, IGraphics::CTextureHandle Texture, float U0, float V0, float U1, float V1, float Spread, float Alpha)
	{
		pGraphics->TextureSet(Texture);
		pGraphics->QuadsBegin();

		const auto corner = [&](float a, float b) {
			return Projector.Project(vec2(a - 0.5f, b - 0.5f), Spread);
		};

		const vec2 P0 = corner(0.0f, 0.0f);
		const vec2 P1 = corner(1.0f, 0.0f);
		const vec2 P2 = corner(1.0f, 1.0f);
		const vec2 P3 = corner(0.0f, 1.0f);

		IGraphics::CFreeformItem item(P0.x, P0.y, P1.x, P1.y, P3.x, P3.y, P2.x, P2.y);

		ColorRGBA color = MixColor(State.m_Color1, State.m_Color2, 0.5f);
		color.r *= 0.7f;
		color.g *= 0.7f;
		color.b *= 0.7f;
		color.a = Alpha;

		pGraphics->QuadsSetSubsetFree(U0, V0, U1, V0, U0, V1, U1, V1);
		pGraphics->SetColor4(color, color, color, color);
		pGraphics->QuadsDrawFreeform(&item, 1);

		pGraphics->QuadsEnd();
	}

	float CurrentPhase();

	void RenderOverlay(IGraphics *pGraphics, const SProjector &Projector, vec2 Anchor, float AnchorSize, IGraphics::CTextureHandle Texture, float Alpha, float U0, float V0, float U1, float V1, bool Halo)
	{
		if(!Texture.IsValid() || Alpha <= 0.0f)
			return;

		const float SafeAnchorSize = AnchorSize > 1.0f ? AnchorSize : 1.0f;

		SGradientState state;
		state.m_Color1 = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_FcSkinGradientColor1));
		state.m_Color2 = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_FcSkinGradientColor2));
		state.m_Mode = g_Config.m_FcSkinGradientMode;
		state.m_Phase = CurrentPhase();
		state.m_Intensity = (g_Config.m_FcSkinGradientIntensity / 100.0f) * Alpha;
		state.m_Glow = (g_Config.m_FcSkinGradientGlow / 100.0f) * Alpha;
		state.m_Bands = (float)g_Config.m_FcSkinGradientBands;

		if(state.m_Intensity <= 0.0f && state.m_Glow <= 0.0f)
			return;

		if(state.m_Glow > 0.0f && Halo)
		{
			pGraphics->BlendAdditive();
			DrawHaloRing(pGraphics, state, Projector, Texture, U0, V0, U1, V1, 1.02f, state.m_Glow * 0.16f);
			DrawHaloRing(pGraphics, state, Projector, Texture, U0, V0, U1, V1, 1.045f, state.m_Glow * 0.07f);
		}

		if(state.m_Intensity > 0.0f)
		{
			pGraphics->BlendNormal();
			DrawStrips(pGraphics, state, Projector, Anchor, SafeAnchorSize, Texture, U0, V0, U1, V1, false);
		}

		if(state.m_Glow > 0.0f)
		{
			pGraphics->BlendAdditive();
			DrawStrips(pGraphics, state, Projector, Anchor, SafeAnchorSize, Texture, U0, V0, U1, V1, true);
		}

		pGraphics->BlendNormal();
		pGraphics->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	}

	float CurrentPhase()
	{
		const float speed = g_Config.m_FcSkinGradientSpeed / 100.0f;
		const double time = (double)time_get() / (double)time_freq();
		return (float)std::fmod(time * 0.22 * speed, 1.0);
	}
}

bool VcSkinGradient::Enabled(bool IsLocal)
{
	if(!g_Config.m_FcSkinGradient)
		return false;
	return IsLocal || g_Config.m_FcSkinGradientOthers;
}

void VcSkinGradient::RenderTeeOverlay(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, vec2 Center, vec2 Size, vec2 Anchor, float AnchorSize, float Angle, float Alpha, float U0, float V0, float U1, float V1, bool Halo)
{
	SProjector projector;
	projector.m_Center = Center;
	projector.m_Scale = Size;
	projector.m_Half = vec2(0.0f, 0.0f);
	projector.m_Angle = Angle;
	projector.m_VertexSpace = false;
	RenderOverlay(pGraphics, projector, Anchor, AnchorSize, Texture, Alpha, U0, V0, U1, V1, Halo);
}

void VcSkinGradient::RenderTeeOverlayQuad(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, vec2 Center, vec2 Half, vec2 Scale, vec2 Anchor, float AnchorSize, float Angle, float Alpha, float U0, float V0, float U1, float V1, bool Halo)
{
	SProjector projector;
	projector.m_Center = Center;
	projector.m_Scale = Scale;
	projector.m_Half = Half;
	projector.m_Angle = Angle;
	projector.m_VertexSpace = true;
	RenderOverlay(pGraphics, projector, Anchor, AnchorSize, Texture, Alpha, U0, V0, U1, V1, Halo);
}
