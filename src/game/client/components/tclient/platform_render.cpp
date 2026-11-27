#include "platform_render.h"

#include <base/color.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>

void CPlatformRender::OnRender()
{
	if(!g_Config.m_FcPlatformRender)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	const CCollision *pCollision = GameClient()->Collision();
	if(!pCollision)
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const float Zoom = GameClient()->m_Camera.m_RenderZoom;
	const CScreenRect MapRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Graphics()->WorldAspect(), Zoom);
	Graphics()->MapScreen(MapRect);

	const int MapW = pCollision->GetWidth();
	const int MapH = pCollision->GetHeight();
	const int StartTx = std::clamp((int)std::floor(MapRect.m_TopLeft.x / 32.0f) - 1, 0, MapW - 1);
	const int EndTx = std::clamp((int)std::ceil(MapRect.m_BottomRight.x / 32.0f) + 1, 0, MapW);
	const int StartTy = std::clamp((int)std::floor(MapRect.m_TopLeft.y / 32.0f) - 1, 0, MapH - 1);
	const int EndTy = std::clamp((int)std::ceil(MapRect.m_BottomRight.y / 32.0f) + 1, 0, MapH);

	auto TileType = [&](int tx, int ty) -> int {
		if(tx < 0 || ty < 0 || tx >= MapW || ty >= MapH)
			return TILE_AIR;
		return pCollision->GetTileIndex(ty * MapW + tx);
	};

	// entity tiles live in separate layers, so a platform holding one is not safe
	auto HasGameplayTile = [&](int tx, int ty) -> bool {
		if(tx < 0 || ty < 0 || tx >= MapW || ty >= MapH)
			return false;
		const int Index = ty * MapW + tx;
		const CTeleTile *pTele = pCollision->TeleLayer();
		if(pTele && pTele[Index].m_Type != 0)
			return true;
		const CSpeedupTile *pSpeedup = pCollision->SpeedupLayer();
		if(pSpeedup)
		{
			const CSpeedupTile &Tile = pSpeedup[Index];
			if(Tile.m_Type != 0 || Tile.m_Force != 0 || Tile.m_MaxSpeed != 0)
				return true;
		}
		const CSwitchTile *pSwitch = pCollision->SwitchLayer();
		if(pSwitch && (pSwitch[Index].m_Type != 0 || pSwitch[Index].m_Number != 0))
			return true;
		const CTuneTile *pTune = pCollision->TuneLayer();
		if(pTune && (pTune[Index].m_Number != 0 || pTune[Index].m_Type != 0))
			return true;
		return false;
	};

	m_vQuads.clear();
	for(int ty = StartTy; ty < EndTy; ++ty)
	{
		for(int tx = StartTx; tx < EndTx; ++tx)
		{
			const int Tile = TileType(tx, ty);
			if(Tile != TILE_SOLID && Tile != TILE_NOHOOK)
				continue;

			const int Above = TileType(tx, ty - 1);
			if(Above != TILE_AIR && Above != TILE_UNFREEZE && Above != TILE_START && Above != TILE_FINISH)
				continue;

			if(HasGameplayTile(tx, ty - 1))
				continue;

			const int Above2 = TileType(tx, ty - 2);
			if(Above2 == TILE_FREEZE || Above2 == TILE_DEATH)
				continue;

			m_vQuads.emplace_back(tx * 32.0f, ty * 32.0f, 32.0f, 32.0f);
		}
	}
	if(m_vQuads.empty())
		return;

	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_FcPlatformRenderColor, true));
	ColorRGBA Top = Color;
	Top.a = 0.45f;
	ColorRGBA Bottom = Color;
	Bottom.a = 0.0f;

	Graphics()->TextureClear();
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->SetColor4(Top, Top, Bottom, Bottom);
	Graphics()->QuadsDrawTL(m_vQuads.data(), (int)m_vQuads.size());
	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
