/* Copyright © 2026 BestProject Team */
#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_INPUTS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_INPUTS_H

#include <base/vmath.h>

namespace TcInputs
{

enum
{
	INPUT_MODE_OFF = 0,
	INPUT_MODE_FAST,
	INPUT_MODE_BEST,
	INPUT_MODE_SAIKO,
	INPUT_MODE_DELTA,
	INPUT_MODE_F,
	INPUT_MODE_CLOUD,
};

	float EffectiveOffsetTicks();

	int PredictionTicks(float OffsetTicks);
	int PredictionTicksOthers(float OffsetTicks);

	void ApplyOffset(float OffsetTicks, int &Tick, float &Intra);

	float BestInterpolationAmount(float Fraction, float DeltaLength, bool Enable);
	vec2 BestInterpolate(vec2 PrevPos, vec2 CurPos, float Fraction, bool Enable);
	float BestInterpolate(float PrevPos, float CurPos, float Fraction, bool Enable);

	bool FastOthers();
	bool BestOthers();
	bool SaikoOthers();
	bool DeltaOthers();
	bool FOthers();
	bool CloudOthers();
	bool AnyOthers();
	bool ImmediateOthers();

	bool FastInputActive();
	bool BestMode();
}

#endif
