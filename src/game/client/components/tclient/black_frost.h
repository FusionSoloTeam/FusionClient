#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_BLACK_FROST_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_BLACK_FROST_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>

class CBlackFrost : public CComponent
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
		MAX_SHARDS = 40,
		MAX_MOTES = 80,
	};

	struct SShard
	{
		bool m_Active = false;
		int m_Seed = 0;
		float m_Age = 0.0f;
		float m_LifeSpan = 1.0f;
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f);
		float m_Size = 0.0f;
		float m_Twist = 0.0f;
		float m_TwistRate = 0.0f;
		float m_Wander = 1.0f;
		float m_Alpha = 1.0f;
	};

	struct SMote
	{
		bool m_Active = false;
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f);
		float m_Life = 0.0f;
		float m_LifeSpan = 1.0f;
		float m_Size = 1.0f;
		float m_Phase = 0.0f;
	};

	SShard m_aaShards[2][MAX_SHARDS];
	SMote m_aaMotes[2][MAX_MOTES];
	float m_aMoteAccum[2] = {};

	float m_Fade = 0.0f;

	IGraphics::CTextureHandle m_GlowTexture;
	bool m_TexturesReady = false;

	void EnsureTextures();

	void ResetShards(int Tee);
	void SpawnShard(int Tee, int Index, const vec2 &Origin, float Scale);
	void SpawnMote(int Tee, const vec2 &Origin, float Scale);
	void UpdateShards(int Tee, const vec2 &Origin, float Dt, float Scale, float Anim);
	void RenderShards(int Tee, float Scale, float Glow);
	void UpdateMotes(int Tee, const vec2 &Origin, float Dt, float Scale, float Anim);
	void RenderMotes(int Tee, float Scale, float Glow);
	void DrawAura(const vec2 &Origin, float Scale, float Glow);

	void IceQuad(const vec2 &P0, const vec2 &P1, const vec2 &P2, const vec2 &P3,
		const ColorRGBA &C0, const ColorRGBA &C1, const ColorRGBA &C2, const ColorRGBA &C3);
};

#endif
