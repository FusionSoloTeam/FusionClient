#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_VIGNETTE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_VIGNETTE_H

#include <base/color.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CVignette : public CComponent
{
	void DrawBand(const CUIRect &Band, int Side, ColorRGBA Color, float Alpha) const;
	void DrawSide(int Side, float Width, ColorRGBA Color, float Alpha) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
