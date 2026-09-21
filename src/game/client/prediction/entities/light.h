#ifndef GAME_CLIENT_PREDICTION_ENTITIES_LIGHT_H
#define GAME_CLIENT_PREDICTION_ENTITIES_LIGHT_H

#include <game/client/prediction/entity.h>

class CLaserData;

class CLight : public CEntity
{
	vec2 m_To;
	float m_Rotation;
	int m_Length;
	float m_CurveLength;
	float m_LengthL;
	float m_Speed;
	float m_AngularSpeed;
	float m_Tick;
	int m_EvalTick;
	int m_SteppedTick;
	bool m_Initialized;
	int m_LastReadTick;
	float m_LastReadRotation;
	float m_LastReadCurveLength;
	bool m_LastReadUnclipped;

	float MaxRayLength(const vec2 &Dir);
	void AdvanceTo(int Tick);
	void Move();
	void Step();
	void RecomputeEnd();
	bool HitCharacter();

public:
	CLight(CGameWorld *pGameWorld, int Id, const CLaserData *pData, int Layer);
	void Read(const CLaserData *pData);
	bool Match(const CLight *pLight) const;
	void Tick() override;
	CLaserData GetData() const;
};

#endif
