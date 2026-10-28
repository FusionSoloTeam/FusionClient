#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_SKIN_GRADIENT_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_SKIN_GRADIENT_H

#include <engine/graphics.h>
#include <base/vmath.h>

namespace VcSkinGradient
{
	bool Enabled(bool IsLocal);
	void RenderTeeOverlay(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, vec2 Center, vec2 Size, vec2 Anchor, float AnchorSize, float Angle, float Alpha, float U0, float V0, float U1, float V1, bool Halo = true);
	void RenderTeeOverlayQuad(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, vec2 Center, vec2 Half, vec2 Scale, vec2 Anchor, float AnchorSize, float Angle, float Alpha, float U0, float V0, float U1, float V1, bool Halo = true);
}

#endif
