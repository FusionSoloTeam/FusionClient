#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_CURSOR_TRAIL_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_CURSOR_TRAIL_H

#include <game/client/component.h>

class CCursorTrail : public CComponent
{
	bool m_Above;

	void SampleCurrentPos();
	void RenderTrail() const;

public:
	CCursorTrail(bool Above);
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;
};

#endif
