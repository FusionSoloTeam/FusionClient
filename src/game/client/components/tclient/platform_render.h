#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_PLATFORM_RENDER_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_PLATFORM_RENDER_H

#include <engine/graphics.h>

#include <game/client/component.h>

#include <vector>

class CPlatformRender : public CComponent
{
	std::vector<IGraphics::CQuadItem> m_vQuads;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
