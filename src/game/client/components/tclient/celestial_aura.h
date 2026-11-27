#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_CELESTIAL_AURA_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_CELESTIAL_AURA_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>

class CCelestialAura : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }

	void OnInit() override;
	void OnReset() override;
	void OnShutdown() override;
	void OnRender() override;

private:
	enum
	{
		MAX_PARTICLES = 192,
		PARTICLE_EMBER = 0,
		PARTICLE_SPARK,
		PARTICLE_PETAL,
	};

	struct SParams
	{
		float m_Alpha = 1.0f;
		float m_Time = 0.0f;
		float m_Anim = 1.0f;
		float m_Glow = 1.0f;
		float m_Pulse = 1.0f;
		float m_Flap = 0.0f;
		float m_Move = 0.0f;
		float m_Spin = 0.0f;
	};

	struct SParticle
	{
		bool m_Active;
		int m_Owner;
		int m_Kind;
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Rot;
		float m_RotSpeed;
		float m_Phase;
		float m_Sway;
		ColorRGBA m_Color;
	};

	SParticle m_aParticles[MAX_PARTICLES] = {};
	int m_NextParticle = 0;
	float m_SpawnAccum = 0.0f;
	float m_Fade = 0.0f;

	float m_aFlapPhase[2] = {};
	float m_aMoveAmt[2] = {};
	float m_aSpeed[2] = {};
	float m_aSpin[2] = {};
	vec2 m_aLastPos[2] = {};
	bool m_aLastPosValid[2] = {};
	vec2 m_aHaloPos[2] = {};
	bool m_aHaloPosValid[2] = {};

	IGraphics::CTextureHandle m_GlowTexture;
	IGraphics::CTextureHandle m_RingTexture;
	IGraphics::CTextureHandle m_FeatherTexture;
	IGraphics::CTextureHandle m_FeatherRoundTexture;
	IGraphics::CTextureHandle m_PetalTexture;
	bool m_TexturesReady = false;

	void EnsureTextures();

	SParticle *NewParticle();
	void SpawnParticle(float Time, const vec2 &Origin, const vec2 &Shoulder, float WingScale, int Owner);
	void UpdateParticles(float Dt, float Time, int Owner);
	void RenderParticles(const SParams &Params, const vec2 &Origin, int Owner);

	void DrawFeather(IGraphics::CTextureHandle Texture, const vec2 &Root, float Angle, float Length, float Width, ColorRGBA ColorRoot, ColorRGBA ColorTip, float Alpha, float Highlight);
	void DrawWing(int Side, const vec2 &Shoulder, const SParams &Params, float WingScale);
	void DrawHalo(const vec2 &Center, const SParams &Params, float HaloScale);
};

#endif
