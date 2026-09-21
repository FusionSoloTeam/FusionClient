#include "light.h"

#include "character.h"

#include <generated/protocol.h>
#include <game/client/laser_data.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>

static float FitStepSpeed(float Delta, int StepCount, const float *pSpeeds, int NumSpeeds, float MaxError, float Increment)
{
	if(StepCount <= 0)
		return 0.0f;
	const float Sign = Delta < 0.0f ? -1.0f : 1.0f;
	float BestSpeed = 0.0f;
	float BestError = -1.0f;
	float Fallback = 0.0f;
	for(int StepsOffset = -1; StepsOffset <= 1; StepsOffset++)
	{
		const int Steps = StepCount + StepsOffset;
		if(Steps <= 0)
			continue;
		if(StepsOffset == 0)
			Fallback = Delta / (Steps * Increment);
		for(int Candidate = 0; Candidate < NumSpeeds; Candidate++)
		{
			const float Expected = Sign * pSpeeds[Candidate] * Increment * Steps;
			const float Error = absolute(Delta - Expected);
			if(Error < MaxError && (BestError < 0.0f || Error < BestError))
			{
				BestError = Error;
				BestSpeed = Sign * pSpeeds[Candidate];
			}
		}
	}
	return BestError < 0.0f ? Fallback : BestSpeed;
}

CLight::CLight(CGameWorld *pGameWorld, int Id, const CLaserData *pData, int Layer) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHT)
{
	m_Id = Id;
	m_Layer = Layer;
	m_Number = pData->m_SwitchNumber;
	m_To = vec2(0.0f, 0.0f);
	m_Rotation = 0.0f;
	m_Length = 0;
	m_CurveLength = 0.0f;
	m_LengthL = 0.0f;
	m_Speed = 0.0f;
	m_AngularSpeed = 0.0f;
	m_Tick = (GameWorld()->GameTickSpeed() * 0.15f);
	m_EvalTick = GameWorld()->GameTick();
	m_SteppedTick = (GameWorld()->GameTick() / maximum(1, (int)m_Tick)) * maximum(1, (int)m_Tick);
	m_Initialized = false;
	m_LastReadTick = -1;
	m_LastReadRotation = 0.0f;
	m_LastReadCurveLength = 0.0f;
	m_LastReadUnclipped = false;
	Read(pData);
}

float CLight::MaxRayLength(const vec2 &Dir)
{
	if(!Collision())
		return 4096.0f;
	vec2 Out;
	Collision()->IntersectNoLaser(m_Pos, m_Pos + Dir * 4096.0f, &Out, nullptr);
	return distance(m_Pos, Out);
}

void CLight::Read(const CLaserData *pData)
{
	m_Pos = pData->m_To;

	const vec2 End = pData->m_From;
	const float Len = distance(m_Pos, End);
	const int Tick = GameWorld()->GameTick();
	const int StepTicks = maximum(1, (int)m_Tick);

	AdvanceTo(Tick);

	if(Len <= 1.0f)
	{
		RecomputeEnd();
		return;
	}

	const vec2 Dir = (End - m_Pos) / Len;
	const float Rotation = std::atan2(Dir.x, Dir.y);
	const bool Unclipped = Len < MaxRayLength(Dir) - 2.0f;

	bool Resync = !m_Initialized;

	if(m_LastReadTick >= 0)
	{
		const int StepCount = Tick / StepTicks - m_LastReadTick / StepTicks;
		if(StepCount > 0 && StepCount <= 2)
		{
			const float RotationDelta = std::remainder(Rotation - m_LastReadRotation, 2.0f * pi);
			const float aAngularSpeeds[] = {pi / 360, pi / 180, pi / 90};
			const float AngularSpeed = FitStepSpeed(RotationDelta, StepCount, aAngularSpeeds, 3, maximum(0.02f, absolute(RotationDelta) * 0.2f), m_Tick);
			if(absolute(AngularSpeed - m_AngularSpeed) > 0.000001f)
			{
				m_AngularSpeed = AngularSpeed;
				Resync = true;
			}
			if(Unclipped && m_LastReadUnclipped)
			{
				const float CurveDelta = Len - m_LastReadCurveLength;
				if(absolute(CurveDelta) > 0.01f)
				{
					const float aCurveSpeeds[] = {1.0f, 3.0f, 5.0f};
					m_Speed = FitStepSpeed(CurveDelta, StepCount, aCurveSpeeds, 3, maximum(0.5f, absolute(CurveDelta) * 0.2f), m_Tick);
				}
			}
		}
	}
	m_LastReadTick = Tick;
	m_LastReadRotation = Rotation;
	m_LastReadCurveLength = Len;
	m_LastReadUnclipped = Unclipped;

	if(!Resync)
	{
		const float RotationDiff = absolute(std::remainder(Rotation - m_Rotation, 2.0f * pi));
		if(RotationDiff > 2.0f * absolute(m_AngularSpeed) * m_Tick + 0.05f)
			Resync = true;
	}

	if(Resync)
	{
		m_Rotation = Rotation;
		if(Unclipped || m_Length <= 0)
			m_CurveLength = Len;
		m_Length = maximum(m_Length, round_to_int(Len));
		m_Initialized = true;
	}
	else if(Unclipped)
	{
		if(absolute(Len - m_CurveLength) > 2.0f * absolute(m_Speed) * m_Tick + 4.0f)
		{
			m_CurveLength = Len;
			m_Length = maximum(m_Length, round_to_int(Len));
		}
	}

	RecomputeEnd();
}

void CLight::AdvanceTo(int Tick)
{
	const int StepTicks = maximum(1, (int)m_Tick);
	while(m_SteppedTick + StepTicks <= Tick)
	{
		m_SteppedTick += StepTicks;
		m_EvalTick = m_SteppedTick;
		vec2 MoverSpeedDir;
		Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &MoverSpeedDir);
		m_Pos += MoverSpeedDir;
		Step();
	}
}

void CLight::Move()
{
	if(m_Speed != 0)
	{
		if((m_CurveLength >= m_Length && m_Speed > 0) || (m_CurveLength <= 0 && m_Speed < 0))
			m_Speed = -m_Speed;
		m_CurveLength += m_Speed * m_Tick + m_LengthL;
		m_LengthL = 0;
		if(m_CurveLength > m_Length)
		{
			m_LengthL = m_CurveLength - m_Length;
			m_CurveLength = m_Length;
		}
		else if(m_CurveLength < 0)
		{
			m_LengthL = 0 + m_CurveLength;
			m_CurveLength = 0;
		}
	}

	m_Rotation += m_AngularSpeed * m_Tick;
	if(m_Rotation > pi * 2)
		m_Rotation -= pi * 2;
	else if(m_Rotation < 0)
		m_Rotation += pi * 2;
}

void CLight::RecomputeEnd()
{
	const vec2 Direction = vec2(std::sin(m_Rotation), std::cos(m_Rotation));
	const vec2 NextPosition = m_Pos + Direction * m_CurveLength;
	Collision()->IntersectNoLaser(m_Pos, NextPosition, &m_To, nullptr);
}

void CLight::Step()
{
	Move();
	RecomputeEnd();
}

void CLight::Tick()
{
	AdvanceTo(GameWorld()->GameTick());
	HitCharacter();
}

bool CLight::HitCharacter()
{
	std::vector<CCharacter *> vpHitCharacters = GameWorld()->IntersectedCharacters(m_Pos, m_To, 0.0f, nullptr);
	if(vpHitCharacters.empty())
		return false;
	for(auto *pChar : vpHitCharacters)
	{
		if(m_Layer == LAYER_SWITCH && m_Number > 0 && (m_Number >= (int)Switchers().size() || !Switchers()[m_Number].m_aStatus[pChar->Team()]))
			continue;
		pChar->Freeze();
	}
	return true;
}

bool CLight::Match(const CLight *pLight) const
{
	return pLight->m_Number == m_Number && distance(pLight->m_Pos, m_Pos) < 4.f;
}

CLaserData CLight::GetData() const
{
	CLaserData Result;
	Result.m_From = m_To;
	Result.m_To = m_Pos;
	Result.m_StartTick = m_EvalTick;
	Result.m_ExtraInfo = true;
	Result.m_Owner = -1;
	Result.m_Type = LASERTYPE_FREEZE;
	Result.m_SwitchNumber = m_Number;
	Result.m_Subtype = 0;
	Result.m_Predict = true;
	Result.m_TuneZone = 0;
	return Result;
}
