/* Copyright © 2026 BestProject Team */
#ifndef GAME_CLIENT_COMPONENTS_FGF_MOTIONBLUR_H
#define GAME_CLIENT_COMPONENTS_FGF_MOTIONBLUR_H
#include <base/color.h>
#include <base/vmath.h>
#include <engine/client/enums.h>
#include <engine/shared/protocol.h>
#include <game/client/component.h>
#include <functional>
#include <vector>
class CFgfMotionBlur : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;
	void RenderCrosshair(vec2 Center, vec2 TargetPos, float Alpha, float Scale, const std::function<void(vec2, float, float)> &Draw);
private:
	struct SSample
	{
		float m_Time;
		vec2 m_Pos;
	};

	std::vector<SSample> m_avTeePath[MAX_CLIENTS];
	std::vector<SSample> m_vCrosshairPath;

	float m_LastCrosshairTime = -1.0f;

	static vec2 PathAt(const std::vector<SSample> &vPath, float Time);
	static void Prune(std::vector<SSample> &vPath, float Time, float Keep);

	bool ShowFor(int ClientId, int Self, int Dummy, int Others);
	float PlayerAlpha(int ClientId);

	void RenderTeeBlur(int ClientId, float Time, float Alpha);
};
#endif
