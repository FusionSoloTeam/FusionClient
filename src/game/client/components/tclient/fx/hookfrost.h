/* Copyright © 2026 BestProject Team */
#ifndef GAME_CLIENT_COMPONENTS_HOOKFROST_H
#define GAME_CLIENT_COMPONENTS_HOOKFROST_H
#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include "fgf_glow.h"
#include "own_tee.h"
#include "hookgrip.h"

class CHookFrost : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

private:
	struct CFrostState
	{
		float m_Flash = 0.0f;
		float m_Seed = 0.0f;
		float m_Flicker = 1.0f;
		float m_SnowAccumulator = 0.0f;
	};

	IGraphics::CTextureHandle m_GlowTexture;
	IGraphics::CTextureHandle m_RingTexture;
	IGraphics::CTextureHandle m_BeamTexture;

	CHookGripTracker m_Tracker;
	CFrostState m_aStates[MAX_CLIENTS];
	CGlowParticles m_Snow;
	float m_LastTime = 0.0f;

	void Grow(int Owner, const CHookGripTracker::CGrip &Grip, float Scale);
	void Flash(int Owner, const CHookGripTracker::CGrip &Grip, float Scale);
	void Shatter(int Owner, const CHookGripTracker::CGrip &Grip, float Scale);
	void EmitSnow(int Owner, const CHookGripTracker::CGrip &Grip, float Passed, float Scale);
	void Crystals(IGraphics *pGraphics, float Scale, const CHookGripTracker::CGrip &Grip, int Owner);
	void AddFlake(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Alpha, ColorRGBA Color);
	void AddShard(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Alpha, ColorRGBA Color);
	void Draw(float Scale, float Time);
};
#endif
