#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_WEAPON_IDLE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_WEAPON_IDLE_H

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <vector>

class CWeaponIdle : public CComponent
{
public:
	static constexpr float TRAIL_KEEP = 0.1f;

	struct STrailSample
	{
		float m_Time;
		float m_Rot;
		vec2 m_Pos;
	};

private:
	struct SIdleState
	{
		float m_IdleTime = 0.0f;
		float m_NextDelay = 0.0f;
		float m_AnimTime = -1.0f;
		float m_AttackCooldown = 0.0f;
		float m_HookCooldown = 0.0f;
		float m_LastAimAngle = 0.0f;
		int m_LastAttackTick = -1;
		int m_LastHookState = -1;
		bool m_HasLast = false;
		bool m_AnimActive = false;
		float m_CurRotOffset = 0.0f;
		vec2 m_CurPosOffset = vec2(0.0f, 0.0f);
		int m_Dir = 1;
		int m_Spins = 2;
		int m_Variant = 0;
		bool m_Combo = false;
		int m_ComboIndex = 0;
		int m_ComboStart = 0;
		std::vector<STrailSample> m_vTrail;
	};

	SIdleState m_aState[MAX_CLIENTS];

	bool IsIdle(int ClientId);
	float AimAngleOf(int ClientId) const;
	void ComputePose(const SIdleState &State, float T, float AimAngle, float &RotOffset, vec2 &PosOffset) const;
	void TriggerAnim(int ClientId, int Variant);
	void TriggerCombo(int ClientId);
	void TriggerLocalTee(int TeeArg, int Variant, bool Combo);

	static void ConWeaponAnim(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponAnimTwirl(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponAnimToss(IConsole::IResult *pResult, void *pUserData);
	static void ConWeaponAnimCombo(IConsole::IResult *pResult, void *pUserData);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;
	void OnConsoleInit() override;

	bool GetAnim(int ClientId, float AimAngle, float &RotOffset, vec2 &PosOffset) const;
	const std::vector<STrailSample> *GetTrail(int ClientId) const;
};

#endif
