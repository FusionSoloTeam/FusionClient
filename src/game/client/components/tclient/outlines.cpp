#include "outlines.h"

#include <base/dbg.h>
#include <base/math.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/mapitems.h>

#include <cmath>

// The order of this is the order of priority for outlines
enum
{
	OUTLINE_NONE = 0,
	OUTLINE_UNFREEZE,
	OUTLINE_FREEZE,
	OUTLINE_TELE,
	OUTLINE_KILL,
	OUTLINE_SOLID,
};

enum class OutlineLayer
{
	GAME,
	FRONT,
	TELE
};

class COutLineLayer
{
private:
	CMapItemLayerTilemap *GetLayer(CGameClient *pThis) const
	{
		if(m_Type == OutlineLayer::GAME)
			return pThis->Layers()->GameLayer();
		if(m_Type == OutlineLayer::FRONT)
			return pThis->Layers()->FrontLayer();
		if(m_Type == OutlineLayer::TELE)
			return pThis->Layers()->TeleLayer();
		dbg_assert(false, "Invalid value for m_Type");
	}
	int GetLayerData(CGameClient *pThis) const
	{
		if(m_Type == OutlineLayer::GAME)
			return pThis->Layers()->GameLayer()->m_Data;
		if(m_Type == OutlineLayer::FRONT)
			return pThis->Layers()->FrontLayer()->m_Front;
		if(m_Type == OutlineLayer::TELE)
			return pThis->Layers()->TeleLayer()->m_Tele;
		dbg_assert(false, "Invalid value for m_Type");
	}

public:
	const OutlineLayer m_Type;
	void GetMeta(CGameClient *pThis, ivec2 &Size) const
	{
		Size = {0, 0};
		const auto *pLayer = GetLayer(pThis);
		if(!pLayer)
			return;
		const size_t TileSize = m_Type == OutlineLayer::TELE ? sizeof(CTeleTile) : sizeof(CTile);
		const int DataSize = pThis->Layers()->Map()->GetDataSize(GetLayerData(pThis));
		if(DataSize <= 0 || (size_t)DataSize < (size_t)pLayer->m_Width * (size_t)pLayer->m_Height * TileSize)
			return;
		Size = {pLayer->m_Width, pLayer->m_Height};
	}
	void SetData(CGameClient *pThis, int *pData, const ivec2 &Size) const
	{
		const auto *pLayer = GetLayer(pThis);
		const auto *pTiles = (CTile *)pThis->Layers()->Map()->GetData(GetLayerData(pThis));
		for(int y = 0; y < pLayer->m_Height; ++y)
		{
			for(int x = 0; x < pLayer->m_Width; ++x)
			{
				const int Index = y * pLayer->m_Width + x;
				const int IndexOut = y * Size.x + x;
				if(m_Type == OutlineLayer::TELE)
				{
					const auto &Tile = ((CTeleTile *)pTiles)[Index];
					if(Tile.m_Number != 0 && Tile.m_Type != 0)
						pData[IndexOut] = OUTLINE_TELE;
				}
				else
				{
					const auto Tile = pTiles[Index].m_Index;
					if(Tile == TILE_SOLID || Tile == TILE_NOHOOK)
						pData[IndexOut] = OUTLINE_SOLID;
					else if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE)
						pData[IndexOut] = OUTLINE_FREEZE;
					else if(Tile == TILE_UNFREEZE || Tile == TILE_DUNFREEZE || Tile == TILE_LUNFREEZE)
						pData[IndexOut] = OUTLINE_UNFREEZE;
					else if(Tile == TILE_DEATH)
						pData[IndexOut] = OUTLINE_KILL;
				}
			}
		}
	}
};

// The order of this determines order of priority into the one map (tele + freeze = tele)
static constexpr COutLineLayer OUTLINE_LAYERS[] = {{OutlineLayer::TELE}, {OutlineLayer::GAME}, {OutlineLayer::FRONT}};

COutlines::~COutlines()
{
	delete[] m_pMapData;
	DestroyLodPyramid();
}

void COutlines::DestroyLodPyramid()
{
	for(size_t i = 1; i < m_apLodData.size(); i++)
		delete[] m_apLodData[i];
	m_apLodData.clear();
	m_aLodSizes.clear();
}

void COutlines::BuildLodPyramid()
{
	DestroyLodPyramid();
	if(!m_pMapData)
		return;
	m_apLodData.push_back(m_pMapData);
	m_aLodSizes.push_back(m_MapDataSize);
	int Level = 0;
	while(m_aLodSizes[Level].x > 1 && m_aLodSizes[Level].y > 1)
	{
		const ivec2 PrevSize = m_aLodSizes[Level];
		const int *pPrev = m_apLodData[Level];
		const ivec2 NewSize(PrevSize.x / 2, PrevSize.y / 2);
		int *pNew = new int[NewSize.x * NewSize.y](OUTLINE_NONE);
		for(int y = 0; y < NewSize.y; y++)
		{
			for(int x = 0; x < NewSize.x; x++)
			{
				int Best = OUTLINE_NONE;
				for(int dy = 0; dy < 2; dy++)
				{
					for(int dx = 0; dx < 2; dx++)
					{
						const int SrcX = x * 2 + dx;
						const int SrcY = y * 2 + dy;
						if(SrcX >= PrevSize.x || SrcY >= PrevSize.y)
							continue;
						Best = std::max(Best, pPrev[SrcY * PrevSize.x + SrcX]);
					}
				}
				pNew[y * NewSize.x + x] = Best;
			}
		}
		Level++;
		m_apLodData.push_back(pNew);
		m_aLodSizes.push_back(NewSize);
	}
}

void COutlines::OnMapLoad()
{
	if(m_pMapData)
	{
		delete[] m_pMapData;
		m_pMapData = nullptr;
	}
	DestroyLodPyramid();

	// Find valid layers and size
	std::vector<const COutLineLayer *> vValidOutlineLayers;
	m_MapDataSize = {0, 0};
	for(const auto &Layer : OUTLINE_LAYERS)
	{
		ivec2 LayerSize;
		Layer.GetMeta(GameClient(), LayerSize);
		if(LayerSize.x <= 0 || LayerSize.y <= 0)
			continue;
		m_MapDataSize.x = std::max(m_MapDataSize.x, LayerSize.x);
		m_MapDataSize.y = std::max(m_MapDataSize.y, LayerSize.y);
		vValidOutlineLayers.push_back(&Layer);
	}
	if(m_MapDataSize.x <= 0 || m_MapDataSize.y <= 0)
		return;
	m_pMapData = new int[m_MapDataSize.x * m_MapDataSize.y](OUTLINE_NONE);

	// Do it
	for(const auto *pLayer : vValidOutlineLayers)
	{
		pLayer->SetData(GameClient(), m_pMapData, m_MapDataSize);
	}

	BuildLodPyramid();
}

void COutlines::OnInit()
{
	if(g_Config.m_TcOutlineGradientColor1 == 16744703)
		g_Config.m_TcOutlineGradientColor1 = 4278255488;
	if(g_Config.m_TcOutlineGradientColor2 == 2583658751)
		g_Config.m_TcOutlineGradientColor2 = 4288282496;

	auto MigratePickedColor = [](unsigned int &Value) {
		if((Value >> 24) != 0)
			return;
		Value = color_cast<ColorHSLA>(ColorRGBA(((Value >> 16) & 0xFF) / 255.0f, ((Value >> 8) & 0xFF) / 255.0f, (Value & 0xFF) / 255.0f, 1.0f)).Pack(true);
	};
	MigratePickedColor(g_Config.m_TcOutlineGradientColor1);
	MigratePickedColor(g_Config.m_TcOutlineGradientColor2);

	if(g_Config.m_TcOutlineGradient)
	{
		g_Config.m_TcOutlineGradientSolid = g_Config.m_TcOutlineGradientFreeze = g_Config.m_TcOutlineGradientUnfreeze = g_Config.m_TcOutlineGradientKill = g_Config.m_TcOutlineGradientTele = 1;
		g_Config.m_TcOutlineGradientColor2Solid = g_Config.m_TcOutlineGradientColor2Freeze = g_Config.m_TcOutlineGradientColor2Unfreeze = g_Config.m_TcOutlineGradientColor2Kill = g_Config.m_TcOutlineGradientColor2Tele = g_Config.m_TcOutlineGradientColor2;
		g_Config.m_TcOutlineGradient = 0;
	}
}

float COutlines::GetTileAnimationProgress(int Type, int x, int y, int LodLevel) const
{
	const int Scale = 1 << LodLevel;
	const int TileX = x * Scale;
	const int TileY = y * Scale;
	const auto *pLayers = GameClient()->Layers();
	const CMapRenderer &Renderer = GameClient()->m_MapLayersForeground.m_MapRenderer;
	if(Type == OUTLINE_TELE)
		return Renderer.GetTileAnimationProgress(pLayers->TeleLayer(), TileX, TileY);

	return std::min(
		Renderer.GetTileAnimationProgress(pLayers->GameLayer(), TileX, TileY),
		Renderer.GetTileAnimationProgress(pLayers->FrontLayer(), TileX, TileY));
}

void COutlines::OnRender()
{
	m_SystemAudio.SetEnabled(g_Config.m_TcOutlinePulsate || g_Config.m_TcLyricsBeat);
	if(!m_pMapData)
		return;
	if(GameClient()->m_MapLayersBackground.m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_ClOverlayEntities && g_Config.m_TcOutlineEntities)
		return;
	if(!g_Config.m_TcOutline)
		return;

	const bool AnimateFade = g_Config.m_FcTileAnimations && g_Config.m_FcTileAnimationMode == 1;
	const bool AnimatePlacing = g_Config.m_FcTileAnimations && g_Config.m_FcTileAnimationMode == 2;

	const float PulsateBonus = g_Config.m_TcOutlinePulsate ? m_SystemAudio.Level() * (g_Config.m_TcOutlinePulsateStrength / 100.0f) * 64.0f : 0.0f;
	const float GradientOffset = g_Config.m_TcOutlineGradientSpeed > 0 ? (float)(time_get() / (double)time_freq()) * (g_Config.m_TcOutlineGradientSpeed / 100.0f) * 0.5f : 0.0f;

	const CScreenRect PrevScreen = Graphics()->GetScreen();

	const CScreenRect WorldRect = Graphics()->MapScreenToWorld(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, 100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Graphics()->WorldAspect(), GameClient()->m_Camera.m_RenderZoom);
	Graphics()->MapScreen(WorldRect);

	const float Scale = 32.0f;

	const CScreenRect ScreenRect = Graphics()->GetScreen();

	const float VisibleTilesX = (ScreenRect.m_BottomRight.x - ScreenRect.m_TopLeft.x) / Scale;
	const float VisibleTilesY = (ScreenRect.m_BottomRight.y - ScreenRect.m_TopLeft.y) / Scale;
	const float BudgetX = std::max(1.0f, (float)Graphics()->ScreenWidth() / 12.0f);
	const float BudgetY = std::max(1.0f, (float)Graphics()->ScreenHeight() / 12.0f);
	int LodLevel = 0;
	while(LodLevel + 1 < (int)m_apLodData.size() &&
		(VisibleTilesX / (float)(1 << (LodLevel + 1)) > BudgetX || VisibleTilesY / (float)(1 << (LodLevel + 1)) > BudgetY))
		LodLevel++;
	const float CellWorld = Scale * (1 << LodLevel);
	const ivec2 LodSize = m_aLodSizes[LodLevel];
	const int *pLodData = m_apLodData[LodLevel];

	int StartY = std::clamp((int)std::floor(ScreenRect.m_TopLeft.y / CellWorld) - 1, 0, LodSize.y);
	int StartX = std::clamp((int)std::floor(ScreenRect.m_TopLeft.x / CellWorld) - 1, 0, LodSize.x);
	int EndY = std::clamp((int)std::ceil(ScreenRect.m_BottomRight.y / CellWorld) + 1, 0, LodSize.y);
	int EndX = std::clamp((int)std::ceil(ScreenRect.m_BottomRight.x / CellWorld) + 1, 0, LodSize.x);

	auto GetTile = [&](int x, int y) {
		x = std::clamp(x, 0, LodSize.x - 1);
		y = std::clamp(y, 0, LodSize.y - 1);
		return pLodData[y * LodSize.x + x];
	};

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	for(int y = StartY; y < EndY; y++)
	{
		for(int x = StartX; x < EndX; x++)
		{
			const int Type = GetTile(x, y);
			if(Type == OUTLINE_NONE)
				continue;
			class COutlineConfig
			{
			public:
				const int &m_Enable;
				const int &m_Width;
				const unsigned int &m_Color;
				const int &m_Gradient;
				const unsigned int &m_GradientColor2;
			};
			const COutlineConfig Config = [&]() -> COutlineConfig {
				if(Type == OUTLINE_SOLID)
					return {g_Config.m_TcOutlineSolid, g_Config.m_TcOutlineWidthSolid, g_Config.m_TcOutlineColorSolid, g_Config.m_TcOutlineGradientSolid, g_Config.m_TcOutlineGradientColor2Solid};
				if(Type == OUTLINE_FREEZE)
					return {g_Config.m_TcOutlineFreeze, g_Config.m_TcOutlineWidthFreeze, g_Config.m_TcOutlineColorFreeze, g_Config.m_TcOutlineGradientFreeze, g_Config.m_TcOutlineGradientColor2Freeze};
				if(Type == OUTLINE_UNFREEZE)
					return {g_Config.m_TcOutlineUnfreeze, g_Config.m_TcOutlineWidthUnfreeze, g_Config.m_TcOutlineColorUnfreeze, g_Config.m_TcOutlineGradientUnfreeze, g_Config.m_TcOutlineGradientColor2Unfreeze};
				if(Type == OUTLINE_KILL)
					return {g_Config.m_TcOutlineKill, g_Config.m_TcOutlineWidthKill, g_Config.m_TcOutlineColorKill, g_Config.m_TcOutlineGradientKill, g_Config.m_TcOutlineGradientColor2Kill};
				if(Type == OUTLINE_TELE)
					return {g_Config.m_TcOutlineTele, g_Config.m_TcOutlineWidthTele, g_Config.m_TcOutlineColorTele, g_Config.m_TcOutlineGradientTele, g_Config.m_TcOutlineGradientColor2Tele};
				dbg_assert(false, "Invalid value for Type at %d, %d/%d, %d", x, y, m_MapDataSize.x, m_MapDataSize.y);
			}();
			if(!Config.m_Enable || Config.m_Width <= 0)
				continue;
			const float Width = Config.m_Width + PulsateBonus;
			const float Progress = (AnimateFade || AnimatePlacing) ? GetTileAnimationProgress(Type, x, y, LodLevel) : 1.0f;
			const float Eased = 1.0f - std::pow(1.0f - Progress, 3.0f);
			const float YOffset = AnimatePlacing ? -(1.0f - Eased) * g_Config.m_FcTileAnimationPlacingDistance * 32.0f : 0.0f;
			// Find neighbours
			const bool aNeighbors[8] = {
				GetTile(x - 1, y - 1) >= Type,
				GetTile(x - 0, y - 1) >= Type,
				GetTile(x + 1, y - 1) >= Type,
				GetTile(x - 1, y + 0) >= Type,
				GetTile(x + 1, y + 0) >= Type,
				GetTile(x - 1, y + 1) >= Type,
				GetTile(x + 0, y + 1) >= Type,
				GetTile(x + 1, y + 1) >= Type,
			};
			// Figure out edges
			IGraphics::CQuadItem aQuads[8];
			int NumQuads = 0;
			const float TileX = x * CellWorld;
			const float TileY = y * CellWorld + YOffset;
			auto AddQuad = [&](float QX, float QY, float QW, float QH) {
				aQuads[NumQuads++] = IGraphics::CQuadItem(QX, QY, QW, QH);
			};
			// Lone corners first
			if(!aNeighbors[0] && aNeighbors[1] && aNeighbors[3])
				AddQuad(TileX, TileY, Width, Width);
			if(!aNeighbors[2] && aNeighbors[1] && aNeighbors[4])
				AddQuad(TileX + CellWorld - Width, TileY, Width, Width);
			if(!aNeighbors[5] && aNeighbors[3] && aNeighbors[6])
				AddQuad(TileX, TileY + CellWorld - Width, Width, Width);
			if(!aNeighbors[7] && aNeighbors[6] && aNeighbors[4])
				AddQuad(TileX + CellWorld - Width, TileY + CellWorld - Width, Width, Width);
			// Top
			if(!aNeighbors[1])
				AddQuad(TileX, TileY, CellWorld, Width);
			// Bottom
			if(!aNeighbors[6])
				AddQuad(TileX, TileY + CellWorld - Width, CellWorld, Width);
			// Left
			if(!aNeighbors[3])
			{
				if(aNeighbors[1] && aNeighbors[6])
					AddQuad(TileX, TileY, Width, CellWorld);
				else if(aNeighbors[6])
					AddQuad(TileX, TileY + Width, Width, CellWorld - Width);
				else if(aNeighbors[1])
					AddQuad(TileX, TileY, Width, CellWorld - Width);
				else
					AddQuad(TileX, TileY + Width, Width, CellWorld - Width * 2.0f);
			}
			// Right
			if(!aNeighbors[4])
			{
				if(aNeighbors[1] && aNeighbors[6])
					AddQuad(TileX + CellWorld - Width, TileY, Width, CellWorld);
				else if(aNeighbors[6])
					AddQuad(TileX + CellWorld - Width, TileY + Width, Width, CellWorld - Width);
				else if(aNeighbors[1])
					AddQuad(TileX + CellWorld - Width, TileY, Width, CellWorld - Width);
				else
					AddQuad(TileX + CellWorld - Width, TileY + Width, Width, CellWorld - Width * 2.0f);
			}
			if(NumQuads <= 0)
				continue;
			ColorRGBA OutlineColor = color_cast<ColorRGBA>(ColorHSLA(Config.m_Color, true));
			if(Config.m_Gradient)
			{
				const ColorRGBA GradientColor1 = color_cast<ColorRGBA>(ColorHSLA(Config.m_Color, true));
				const ColorRGBA GradientColor2 = color_cast<ColorRGBA>(ColorHSLA(Config.m_GradientColor2, true));
				const float Phase = std::fmod((x + y) * 0.03125f + GradientOffset, 1.0f);
				const float Wave = Phase < 0.5f ? Phase * 2.0f : 2.0f - Phase * 2.0f;
				OutlineColor = ColorRGBA(mix(GradientColor1.r, GradientColor2.r, Wave), mix(GradientColor1.g, GradientColor2.g, Wave), mix(GradientColor1.b, GradientColor2.b, Wave), mix(GradientColor1.a, GradientColor2.a, Wave));
			}
			if(AnimateFade)
			{
				const float SmoothProgress = Progress * Progress * (3.0f - 2.0f * Progress);
				OutlineColor.a *= SmoothProgress;
				if(OutlineColor.a <= 0.0f)
					continue;
			}
			Graphics()->SetColor(OutlineColor);
			Graphics()->QuadsDrawTL(aQuads, NumQuads);
		}
	}

	Graphics()->QuadsEnd();

	Graphics()->MapScreen(PrevScreen);
}
