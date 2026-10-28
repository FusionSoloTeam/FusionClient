#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_TILE_UNDER_CURSOR_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_TILE_UNDER_CURSOR_H

#include <game/client/component.h>

class CTileUnderCursor : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
