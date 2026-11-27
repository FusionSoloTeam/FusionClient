#include <engine/client/frame_generation.h>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST(FrameGeneration, OffPreservesRenderLimit)
{
	const auto Timing = FrameGenerationTiming(0, 144, 200, 0);
	EXPECT_EQ(Timing.m_Multiplier, 1);
	EXPECT_EQ(Timing.m_BaseRate, 200);
	EXPECT_EQ(Timing.PresentInterval(), 0ns);
}

TEST(FrameGeneration, SpreadsPresentsAcrossEachBaseFrame)
{
	for(int Multiplier = 2; Multiplier <= 4; ++Multiplier)
	{
		const auto Timing = FrameGenerationTiming(Multiplier, 144, 72, 0);
		EXPECT_EQ(Timing.m_Multiplier, Multiplier);
		EXPECT_EQ(Timing.m_BaseRate, 72);
		EXPECT_EQ(Timing.PresentInterval(), 1'000'000'000ns / (72 * Multiplier));
		for(int Step = 1; Step < Multiplier; ++Step)
		{
			EXPECT_GT(Timing.Phase(Step), Timing.Phase(Step - 1));
			EXPECT_LT(Timing.Phase(Step), 1.0f);
		}
		EXPECT_FLOAT_EQ(Timing.Phase(Multiplier), 1.0f);
	}
}

TEST(FrameGeneration, RespectsLowerRenderAndUpdateLimits)
{
	EXPECT_EQ(FrameGenerationTiming(3, 144, 40, 45).m_BaseRate, 40);
	const auto Timing = FrameGenerationTiming(3, 144, 45, 30);
	EXPECT_EQ(Timing.m_BaseRate, 30);
	EXPECT_EQ(Timing.PresentInterval(), 1'000'000'000ns / 90);
}

TEST(FrameGeneration, HandlesUnknownDisplayRateAndInvalidModes)
{
	EXPECT_EQ(FrameGenerationTiming(4, 0, 0, 0).m_BaseRate, 0);
	EXPECT_EQ(FrameGenerationTiming(1, 144, 0, 0).m_Multiplier, 1);
	EXPECT_EQ(FrameGenerationTiming(5, 144, 0, 0).m_Multiplier, 1);
}

TEST(FrameGeneration, MultiplierDoesNotReduceSourceRenderRate)
{
	for(int Multiplier = 2; Multiplier <= 4; ++Multiplier)
	{
		const auto Uncapped = FrameGenerationTiming(Multiplier, 144, 0, 0);
		EXPECT_EQ(Uncapped.m_BaseRate, 0);
		EXPECT_EQ(Uncapped.PresentInterval(), 0ns);
		EXPECT_EQ(FrameGenerationTiming(Multiplier, 144, 100, 0).m_BaseRate, 100);
		EXPECT_EQ(FrameGenerationTiming(Multiplier, 144, 120, 80).m_BaseRate, 80);
	}
}

TEST(FrameGeneration, VSyncKeepsRequestedSourceRate)
{
	EXPECT_EQ(FrameGenerationTiming(4, 144, 0, 0, true).m_BaseRate, 0);
	EXPECT_EQ(FrameGenerationTiming(4, 60, 60, 0, true).m_BaseRate, 60);
	EXPECT_EQ(FrameGenerationTiming(4, 144, 60, 0, true).m_BaseRate, 60);
	EXPECT_EQ(FrameGenerationTiming(4, 144, 30, 0, true).m_BaseRate, 30);
}

TEST(FrameGeneration, BypassesGenerationWhenSourceFillsTheDisplay)
{
	CFrameGenerationRateGate Gate;
	// measure the source before enabling generation rather than trusting the fps cap
	EXPECT_FALSE(Gate.Allow(144, 1.0f / 60.0f, 0ms));
	EXPECT_FALSE(Gate.Allow(144, 1.0f / 60.0f, 400ms));
	EXPECT_TRUE(Gate.Allow(144, 1.0f / 60.0f, 600ms));
	EXPECT_TRUE(Gate.Allow(144, 1.0f / 200.0f, 700ms));
	EXPECT_FALSE(Gate.Allow(144, 1.0f / 200.0f, 1300ms));
}

TEST(FrameGeneration, UnknownDisplayRateDoesNotOversubscribe)
{
	CFrameGenerationRateGate Gate;
	EXPECT_FALSE(Gate.Allow(0, 1.0f / 60.0f, 0ms));
	CFrameGenerationCadence Cadence;
	EXPECT_EQ(Cadence.NextOutputCount(4, 0, 60), 1);
}

TEST(FrameGeneration, EnablesFractionalGenerationNearDisplayRate)
{
	CFrameGenerationRateGate Gate;
	EXPECT_FALSE(Gate.Allow(60, 1.0f / 55.0f, 0ms));
	EXPECT_TRUE(Gate.Allow(60, 1.0f / 55.0f, 600ms));
}

TEST(FrameGeneration, FillsUnusedDisplaySlotsWithoutLoweringSourceRate)
{
	CFrameGenerationCadence Cadence;
	int Shown = 0;
	int Generated = 0;
	for(int Source = 0; Source < 50; ++Source)
	{
		const int Count = Cadence.NextOutputCount(4, 60, 50);
		EXPECT_GE(Count, 1);
		EXPECT_LE(Count, 2);
		Shown += Count;
		Generated += Count - 1;
	}
	EXPECT_EQ(Shown, 60);
	EXPECT_EQ(Generated, 10);
}

TEST(FrameGeneration, VSyncKeepsFractionalGenerationAvailable)
{
	// outputs vary per frame since 60/50 is fractional
	const auto Timing = FrameGenerationTiming(4, 60, 50, 0, true);
	EXPECT_EQ(Timing.m_Multiplier, 4);
	EXPECT_EQ(Timing.m_BaseRate, 50);
}

TEST(FrameGeneration, SourceEstimateIgnoresGenerationOverheadButTracksRaisedLimits)
{
	CFrameGenerationSourceRate Rate;
	EXPECT_EQ(Rate.Update(389, 1.0f / 45.0f, false), 45);
	EXPECT_EQ(Rate.Update(389, 1.0f / 30.0f, true), 45);
	EXPECT_EQ(Rate.Update(30, 1.0f / 30.0f, true), 30);
	EXPECT_EQ(Rate.Update(50, 1.0f / 30.0f, true), 50);
}

TEST(FrameGeneration, AvoidsGenerationWhenSourceCannotReachItsLimit)
{
	CFrameGenerationSourceRate Rate;
	Rate.Update(389, 1.0f / 45.0f, false);
	EXPECT_FALSE(Rate.HasRenderHeadroom());
	Rate.Update(60, 1.0f / 60.0f, false);
	EXPECT_TRUE(Rate.HasRenderHeadroom());
}

TEST(FrameGeneration, UnlimitedSourceHasNoKnownRenderHeadroom)
{
	CFrameGenerationSourceRate Rate;
	Rate.Update(0, 1.0f / 45.0f, false);
	EXPECT_FALSE(Rate.HasRenderHeadroom());
}

TEST(FrameGeneration, PausesWhenGenerationDropsSourceThroughput)
{
	CFrameGenerationSourceRate Rate;
	Rate.Update(60, 1.0f / 60.0f, false, 0ms);
	EXPECT_TRUE(Rate.HasRenderHeadroom());
	Rate.Update(60, 1.0f / 40.0f, true, 100ms);
	EXPECT_TRUE(Rate.HasRenderHeadroom()); // a single slow frame must not disable generation
	Rate.Update(60, 1.0f / 40.0f, true, 700ms);
	EXPECT_FALSE(Rate.HasRenderHeadroom());
}

TEST(FrameGeneration, RetainsHistoryForLowRateSources)
{
	EXPECT_EQ(FrameGenerationSourceGapLimit(10), 200ms);
	EXPECT_EQ(FrameGenerationSourceGapLimit(30), 100ms);
	EXPECT_EQ(FrameGenerationSourceGapLimit(0), 100ms);
}

TEST(FrameGeneration, EditorWorksWithoutAnOnlineGameOrRenderHeadroom)
{
	EXPECT_TRUE(FrameGenerationAllowedInScene(true, false, false));
	EXPECT_TRUE(FrameGenerationAllowsSourceRate(true, false));
	EXPECT_FALSE(FrameGenerationAllowedInScene(false, false, false));
	EXPECT_FALSE(FrameGenerationAllowedInScene(false, false, true));
	EXPECT_FALSE(FrameGenerationAllowedInScene(false, true, false));
	EXPECT_FALSE(FrameGenerationAllowsSourceRate(false, false));
	EXPECT_TRUE(FrameGenerationAllowedInScene(false, true, true));
}

