#include "tile_under_cursor.h"

#include <base/color.h>
#include <base/math.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <cmath>

void CTileUnderCursor::OnRender()
{
	if(!g_Config.m_FcOutlineUnderCursor)
		return;

	if(GameClient()->m_Snap.m_pLocalCharacter == nullptr)
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const float Zoom = GameClient()->m_Camera.m_RenderZoom;
	const float Aspect = Graphics()->WorldAspect();

	const vec2 CursorOffset = (GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy] - Center) * Zoom;
	const vec2 AimWorld = Center + rotate(CursorOffset, -GameClient()->m_Camera.TiltAngle());

	const CScreenRect MapRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Aspect, Zoom);
	Graphics()->MapScreen(MapRect);

	if(AimWorld.x < 0.0f || AimWorld.y < 0.0f ||
		AimWorld.x >= Collision()->GetWidth() * 32.0f || AimWorld.y >= Collision()->GetHeight() * 32.0f ||
		Collision()->GetTileIndex(Collision()->GetPureMapIndex(AimWorld.x, AimWorld.y)) == TILE_AIR)
	{
		return;
	}

	const float TileX = std::floor(AimWorld.x / 32.0f) * 32.0f;
	const float TileY = std::floor(AimWorld.y / 32.0f) * 32.0f;

	const ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_FcOutlineUnderCursorColor, true));
	const float Border = g_Config.m_FcOutlineUnderCursorWidth;

	Graphics()->DrawRect(TileX, TileY, 32.0f, Border, Color, IGraphics::CORNER_NONE, 0.0f);
	Graphics()->DrawRect(TileX, TileY + 32.0f - Border, 32.0f, Border, Color, IGraphics::CORNER_NONE, 0.0f);
	Graphics()->DrawRect(TileX, TileY, Border, 32.0f, Color, IGraphics::CORNER_NONE, 0.0f);
	Graphics()->DrawRect(TileX + 32.0f - Border, TileY, Border, 32.0f, Color, IGraphics::CORNER_NONE, 0.0f);
}
