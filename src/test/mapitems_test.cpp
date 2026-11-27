#include <game/map/render_map.h>
#include <game/mapitems.h>

#include <gtest/gtest-printers.h>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

namespace testing::internal
{
	template<>
	class UniversalPrinter<CFixedTime>
	{
	public:
		static void Print(const CFixedTime &FixedTime, std::ostream *pOutputStream)
		{
			*pOutputStream << "CFixedTime with internal value " << FixedTime.GetInternal();
		}
	};

}

TEST(Mapitems, FixedTimeRoundtrip)
{
	for(CFixedTime Fixed = CFixedTime(0); Fixed < CFixedTime(1000000); Fixed += CFixedTime(1))
	{
		ASSERT_EQ(Fixed, CFixedTime::FromSeconds(Fixed.AsSeconds()));
	}
}

TEST(Mapitems, OnlyOpaqueLayerColorsCanHaveOpaqueTiles)
{
	const float Threshold = 254.0f / 255.0f;
	EXPECT_FALSE(TileLayerHasOpaquePass(0.0f));
	EXPECT_FALSE(TileLayerHasOpaquePass(Threshold));
	EXPECT_TRUE(TileLayerHasOpaquePass(std::nextafter(Threshold, 1.0f)));
	EXPECT_TRUE(TileLayerHasOpaquePass(1.0f));
}

TEST(Mapitems, EditorTileBatchesPreserveOrderAndStreamCapacity)
{
	CEditorTileQuadBatch Batch;
	std::vector<std::array<int, 2>> vKeys;
	std::vector<IGraphics::CQuadItem> vItems;
	int StreamQuads = 0;
	int StreamResets = 0;
	auto Flush = [&](unsigned char Index, unsigned char Flags, const IGraphics::CQuadItem *pItems, int Count, bool ResetStream) {
		EXPECT_GT(Count, 0);
		EXPECT_LE(Count, 1024);
		if(ResetStream)
		{
			StreamQuads = 0;
			++StreamResets;
		}
		StreamQuads += Count;
		EXPECT_LE(StreamQuads, 4096);
		for(int i = 0; i < Count; ++i)
		{
			vKeys.push_back({Index, Flags});
			vItems.push_back(pItems[i]);
		}
	};
	Batch.Flush(Flush);
	EXPECT_TRUE(vItems.empty());
	for(int i = 0; i < 8500; ++i)
	{
		const unsigned char Index = 1 + (i / 1300) % 3;
		const unsigned char Flags = (i / 1000) % 2 ? TILEFLAG_XFLIP : 0;
		Batch.Add(Index, Flags, IGraphics::CQuadItem(i, -i, 32, 32), Flush);
	}
	Batch.Flush(Flush);
	ASSERT_EQ(vItems.size(), 8500u);
	ASSERT_EQ(vKeys.size(), vItems.size());
	EXPECT_GE(StreamResets, 3);
	for(int i = 0; i < 8500; ++i)
	{
		EXPECT_EQ(vKeys[i][0], 1 + (i / 1300) % 3);
		EXPECT_EQ(vKeys[i][1], (i / 1000) % 2 ? TILEFLAG_XFLIP : 0);
		EXPECT_EQ(vItems[i].m_X, i);
		EXPECT_EQ(vItems[i].m_Y, -i);
		EXPECT_EQ(vItems[i].m_Width, 32);
		EXPECT_EQ(vItems[i].m_Height, 32);
	}
}

TEST(Mapitems, EmptyTileRunsDoNotHideTilesOrSkipMarkers)
{
	std::array<CTile, 12> aTiles{};
	aTiles[2].m_Flags = TILEFLAG_XFLIP;
	aTiles[3].m_MustBe0 = 255;
	aTiles[9].m_Index = 7;
	EXPECT_EQ(SkipEmptyTileRun(aTiles.data(), 12), 8);
	EXPECT_EQ(SkipEmptyTileRun(aTiles.data(), 7), 4);

	// a nonzero skip marker jumps over later cells, so it is not skipped
	aTiles[3].m_Skip = 2;
	EXPECT_EQ(SkipEmptyTileRun(aTiles.data(), 12), 0);
	aTiles[3].m_Skip = 0;
	aTiles[4].m_Index = 2;
	EXPECT_EQ(SkipEmptyTileRun(aTiles.data(), 12), 4);
}

TEST(Mapitems, EmptyTileRunRespectsVisibleBoundsAndAllTileBytes)
{
	std::array<CTile, 18> aTiles{};
	for(int Start = 0; Start < 4; ++Start)
	{
		for(int Count = 0; Count <= 14; ++Count)
		{
			EXPECT_EQ(SkipEmptyTileRun(aTiles.data() + Start, Count), (Count / 4) * 4);
			for(int Position = 0; Position < Count; ++Position)
			{
				aTiles[Start + Position].m_Index = 1;
				EXPECT_EQ(SkipEmptyTileRun(aTiles.data() + Start, Count), (Position / 4) * 4);
				aTiles[Start + Position].m_Index = 0;
				aTiles[Start + Position].m_Skip = 1;
				EXPECT_EQ(SkipEmptyTileRun(aTiles.data() + Start, Count), (Position / 4) * 4);
				aTiles[Start + Position].m_Skip = 0;
			}
		}
	}
}

TEST(Mapitems, EmptyTileRunTraversalMatchesOriginalAfterEdits)
{
	std::array<CTile, 65> aTiles{};
	unsigned Seed = 1234567;
	for(int Iteration = 0; Iteration < 2000; ++Iteration)
	{
		for(auto &Tile : aTiles)
		{
			Seed = Seed * 1664525u + 1013904223u;
			Tile.m_Index = Seed % 7 == 0 ? (Seed >> 16) % 255 + 1 : 0;
			Tile.m_Flags = Seed >> 24;
			Tile.m_Skip = Seed % 13 == 0 ? (Seed >> 16) % 6 : 0;
			Tile.m_MustBe0 = Seed >> 8;
		}
		const int Begin = Iteration % 4;
		const int End = aTiles.size() - Iteration % 5;
		std::vector<int> vOriginal;
		std::vector<int> vOptimized;
		for(int x = Begin; x < End; ++x)
		{
			if(aTiles[x].m_Index)
				vOriginal.push_back(x);
			x += aTiles[x].m_Skip;
		}
		for(int x = Begin; x < End; ++x)
		{
			const int Skip = !aTiles[x].m_Index && aTiles[x].m_Skip == 0 && End - x >= 4 ? SkipEmptyTileRun(aTiles.data() + x, End - x) : 0;
			if(Skip > 0)
			{
				x += Skip - 1;
				continue;
			}
			if(aTiles[x].m_Index)
				vOptimized.push_back(x);
			x += aTiles[x].m_Skip;
		}
		ASSERT_EQ(vOriginal, vOptimized) << "iteration " << Iteration;
	}
}

TEST(Mapitems, EmptyTileRunDoesNotCrossClippedRows)
{
	for(int Width = 4; Width <= 9; ++Width)
	{
		std::vector<CTile> vTiles(Width * 3);
		for(int y = 0; y < 3; ++y)
		{
			vTiles[y * Width].m_Index = y + 1;
			vTiles[y * Width + Width - 1].m_Index = y + 4;
		}
		for(int Begin = 0; Begin < Width; ++Begin)
		{
			for(int End = Begin + 1; End <= Width; ++End)
			{
				std::vector<int> vOriginal;
				std::vector<int> vOptimized;
				for(int y = 0; y < 3; ++y)
				{
					for(int x = Begin; x < End; ++x)
					{
						const int c = x + y * Width;
						if(vTiles[c].m_Index)
							vOriginal.push_back(c);
						x += vTiles[c].m_Skip;
					}
					for(int x = Begin; x < End; ++x)
					{
						const int c = x + y * Width;
						if(!vTiles[c].m_Index && vTiles[c].m_Skip == 0 && End - x >= 4)
						{
							const int Skipped = SkipEmptyTileRun(vTiles.data() + c, End - x);
							if(Skipped > 0)
							{
								x += Skipped - 1;
								continue;
							}
						}
						if(vTiles[c].m_Index)
							vOptimized.push_back(c);
						x += vTiles[c].m_Skip;
					}
				}
				EXPECT_EQ(vOriginal, vOptimized) << "width " << Width << ", begin " << Begin << ", end " << End;
			}
		}
	}
}
