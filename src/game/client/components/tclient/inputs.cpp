/* Copyright © 2026 BestProject Team */
#include "inputs.h"

#include <base/math.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>

namespace
{
	int InputMode()
	{
		if(g_Config.m_TcInputs >= TcInputs::INPUT_MODE_FAST && g_Config.m_TcInputs <= TcInputs::INPUT_MODE_BEST)
			return g_Config.m_TcInputs;
		if(g_Config.m_TcInputs == TcInputs::INPUT_MODE_OFF && g_Config.m_TcFastInput)
			return TcInputs::INPUT_MODE_FAST;
		return TcInputs::INPUT_MODE_OFF;
	}
	float FastModeOffsetTicks()
	{
		if(InputMode() != TcInputs::INPUT_MODE_FAST || g_Config.m_TcFastInputAmount <= 0)
			return 0.0f;
		return g_Config.m_TcFastInputAmount / 20.0f;
	}

	float BestModeOffsetTicks()
	{
		if(InputMode() != TcInputs::INPUT_MODE_BEST || g_Config.m_TcBestInputAmount <= 0)
			return 0.0f;

		float Offset = g_Config.m_TcBestInputAmount / 100.0f;

		if(g_Config.m_TcBestInputSmoothing > 0)
			Offset *= 1.0f - (g_Config.m_TcBestInputSmoothing / 200.0f);

		if(g_Config.m_TcBestInputLatencyComp > 0)
			Offset *= 1.0f + (g_Config.m_TcBestInputLatencyComp / 100.0f);

		return Offset;
	}

	float SaikoModeOffsetTicks()
	{
		if(InputMode() != TcInputs::INPUT_MODE_SAIKO || g_Config.m_TcSaikoInputAmount <= 0)
			return 0.0f;
		return g_Config.m_TcSaikoInputAmount / 100.0f;
	}

	float DeltaModeOffsetTicks()
	{
		if(InputMode() != TcInputs::INPUT_MODE_DELTA || g_Config.m_TcDeltaInputAmount <= 0)
			return 0.0f;
		return g_Config.m_TcDeltaInputAmount / 100.0f;
	}

	float FModeOffsetTicks()
	{
		if(InputMode() != TcInputs::INPUT_MODE_F || g_Config.m_TcFInputAmount <= 0)
			return 0.0f;
		return g_Config.m_TcFInputAmount / 1000.0f;
	}
} // namespace

float TcInputs::EffectiveOffsetTicks()
{
	switch(InputMode())
	{
	case TcInputs::INPUT_MODE_FAST: return FastModeOffsetTicks();
	case TcInputs::INPUT_MODE_BEST: return BestModeOffsetTicks();
	case TcInputs::INPUT_MODE_SAIKO: return SaikoModeOffsetTicks();
	case TcInputs::INPUT_MODE_DELTA: return DeltaModeOffsetTicks();
	case TcInputs::INPUT_MODE_F: return FModeOffsetTicks();
	default: return 0.0f;
	}
}

int TcInputs::PredictionTicks(float OffsetTicks)
{
	if(OffsetTicks <= 0.0f)
		return 0;
	return (int)std::ceil(OffsetTicks);
}

int TcInputs::PredictionTicksOthers(float OffsetTicks)
{
	return PredictionTicks(OffsetTicks);
}

void TcInputs::ApplyOffset(float OffsetTicks, int &Tick, float &Intra)
{
	if(OffsetTicks <= 0.0f)
		return;

	const int WholeTicks = (int)OffsetTicks;
	const float OffsetIntra = OffsetTicks - WholeTicks;

	const float CombinedIntra = Intra + OffsetIntra;
	const int CarryOverTicks = (int)CombinedIntra;

	Tick += WholeTicks + CarryOverTicks;
	Intra = CombinedIntra - CarryOverTicks;
}

float TcInputs::BestInterpolationAmount(float Fraction, float DeltaLength, bool Enable)
{
	if(!Enable)
		return Fraction;

	const float T = std::clamp(Fraction, 0.0f, 1.0f);
	const float T2 = T * T;
	const float CubicT = 3.0f * T2 - 2.0f * T2 * T;
	switch(std::clamp(g_Config.m_TcBestInputInterpolation, 1, 3))
	{
	case 2:
		return CubicT;
	case 3:
		return mix(T, CubicT, std::clamp(DeltaLength / 1000.0f, 0.0f, 1.0f));
	default:
		return T;
	}
}

vec2 TcInputs::BestInterpolate(vec2 PrevPos, vec2 CurPos, float Fraction, bool Enable)
{
	return mix(PrevPos, CurPos, BestInterpolationAmount(Fraction, length(CurPos - PrevPos), Enable));
}

float TcInputs::BestInterpolate(float PrevPos, float CurPos, float Fraction, bool Enable)
{
	return mix(PrevPos, CurPos, BestInterpolationAmount(Fraction, absolute(CurPos - PrevPos), Enable));
}

bool TcInputs::FastOthers()
{
	return InputMode() == TcInputs::INPUT_MODE_FAST && (g_Config.m_TcFastInputOthers != 0 || g_Config.m_TcFastInput != 0);
}

bool TcInputs::BestOthers()
{
	return InputMode() == TcInputs::INPUT_MODE_BEST && g_Config.m_TcBestInputOthers != 0;
}

bool TcInputs::SaikoOthers()
{
	return InputMode() == TcInputs::INPUT_MODE_SAIKO && g_Config.m_TcSaikoInputOthers != 0;
}

bool TcInputs::DeltaOthers()
{
	return InputMode() == TcInputs::INPUT_MODE_DELTA && g_Config.m_TcDeltaInputOthers != 0;
}

bool TcInputs::FOthers()
{
	return InputMode() == TcInputs::INPUT_MODE_F && g_Config.m_TcFInputOthers != 0;
}

bool TcInputs::CloudOthers()
{
	return InputMode() == TcInputs::INPUT_MODE_CLOUD && g_Config.m_TcCloudInputOthers != 0;
}

bool TcInputs::AnyOthers()
{
	return FastOthers() || BestOthers() || SaikoOthers() || DeltaOthers() || FOthers() || CloudOthers();
}

bool TcInputs::ImmediateOthers()
{
	return BestOthers() || SaikoOthers() || DeltaOthers();
}

bool TcInputs::FastInputActive()
{
	return EffectiveOffsetTicks() > 0.0f;
}

bool TcInputs::BestMode()
{
	return InputMode() == TcInputs::INPUT_MODE_BEST;
}
