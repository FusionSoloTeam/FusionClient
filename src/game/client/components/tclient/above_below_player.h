#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_ABOVE_BELOW_PLAYER_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_ABOVE_BELOW_PLAYER_H
#include "game/client/component.h"
#include "game/client/ui_rect.h"

class CAboveBelowPlayer : public CComponent
{
	const float m_HideAnim = -0.25f;
	float m_AboveAnim = 0.0f;
	float m_SameAnim = 0.0f;
	float m_BelowAnim = 0.0f;
	bool m_PlayerAbove = false;
	bool m_PlayerSame = false;
	bool m_PlayerBelow = false;
	int m_EditorLines = 0;
	float m_EditorHeight = 0.0f;

	static float EaseInOutQuad(float T);

public:
	CAboveBelowPlayer();
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;
	CUIRect GetHudEditorRect() const;
};

#endif
