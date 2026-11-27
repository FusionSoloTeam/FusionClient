#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_VIGNETTE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_VIGNETTE_H

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CVignette : public CComponent
{
	IGraphics::CTextureHandle m_VignetteTexture;
	int m_VignetteTextureMode = -1;
	int m_VignetteTextureRadius = -1;
	int m_VignetteTextureSmoothness = -1;
	int m_VignetteTextureAspect = -1;

	void EnsureVignetteTexture();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnShutdown() override;
};

#endif
