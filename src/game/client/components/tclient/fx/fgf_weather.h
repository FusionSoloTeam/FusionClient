/* Copyright © 2026 BestProject Team */
#ifndef GAME_CLIENT_COMPONENTS_FGF_WEATHER_H
#define GAME_CLIENT_COMPONENTS_FGF_WEATHER_H
#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>
#include "fgf_glow.h"
#include "own_tee.h"
#include "fx_screen_rect.h"

enum
{
	FGF_WEATHER_SNOW = 0,
	FGF_WEATHER_RAIN,
	FGF_WEATHER_FIREFLIES,
	FGF_WEATHER_DUST,
	FGF_WEATHER_LEAVES,
	FGF_WEATHER_SAKURA,
};

class CFgfWeather : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

private:
	static constexpr int MAX_DROPS = 1024;
	static constexpr int MAX_SPLASHES = 256;

	struct CDrop
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Phase;
		bool m_Settled;
	};

	struct CSplash
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
	};

	static constexpr int MAX_DUST = 192;

	struct CDustMote
	{
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f);
		float m_Life = 0.0f;
		float m_LifeSpan = 1.0f;
		float m_Size = 1.0f;
		float m_Phase = 0.0f;
		bool m_Sparkle = false;
		bool m_Active = false;
	};

	IGraphics::CTextureHandle m_GlowTexture;
	IGraphics::CTextureHandle m_LeafTexture;
	IGraphics::CTextureHandle m_PetalTexture;
	IGraphics::CTextureHandle m_SnowflakeTexture;

	int m_Kind = FGF_WEATHER_SNOW;
	CDrop m_aDrops[MAX_DROPS];
	int m_NumDrops = 0;
	CSplash m_aSplashes[MAX_SPLASHES];
	int m_NumSplashes = 0;
	CDustMote m_aDust[MAX_DUST];
	float m_DustSpawnAccumulator = 0.0f;
	float m_LastDustTime = 0.0f;
	float m_SpawnAccumulator = 0.0f;
	float m_LastTime = 0.0f;

	CDrop *NewDrop();
	void Splash(vec2 Pos);
	void Spawn(const CFxScreenRect &View, float Passed, float Time);
	void Update(const CFxScreenRect &View, float Passed, float Time);
	void Draw(float Time);
	void AmbientDust();
};
#endif
