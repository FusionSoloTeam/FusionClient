#include "weapon_idle.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>

static constexpr float ANIM_DURATION = 1.9f;
static constexpr float WINDUP_END = 0.10f;
static constexpr float TWIRL_END = 0.74f;
static constexpr float IDLE_SPEED_THRESHOLD = 100.0f;
static constexpr float AIM_ANGLE_EPSILON = 0.015f;

static float Smoothstep(float V)
{
	return V * V * (3.0f - 2.0f * V);
}

float CWeaponIdle::AimAngleOf(int ClientId) const
{
	const auto &CharInfo = GameClient()->m_Snap.m_aCharacters[ClientId];
	if(CharInfo.m_HasExtendedDisplayInfo)
	{
		const auto &Data = CharInfo.m_ExtendedData;
		if(Data.m_TargetX != 0 || Data.m_TargetY != 0)
			return angle(vec2(Data.m_TargetX, Data.m_TargetY));
	}
	return CharInfo.m_Cur.m_Angle / 256.0f;
}

bool CWeaponIdle::IsIdle(int ClientId)
{
	SIdleState &State = m_aState[ClientId];
	const auto &CharInfo = GameClient()->m_Snap.m_aCharacters[ClientId];

	const float Intra = Client()->IntraGameTick(g_Config.m_ClDummy);
	vec2 Vel = mix(
		vec2(CharInfo.m_Prev.m_VelX / 256.0f, CharInfo.m_Prev.m_VelY / 256.0f),
		vec2(CharInfo.m_Cur.m_VelX / 256.0f, CharInfo.m_Cur.m_VelY / 256.0f),
		Intra);
	const float Speed = length(Vel) * (float)Client()->GameTickSpeed();
	const float AimAngle = AimAngleOf(ClientId);

	const bool AttackChanged = CharInfo.m_Cur.m_AttackTick != State.m_LastAttackTick;
	const bool HookChanged = CharInfo.m_Cur.m_HookState != State.m_LastHookState;

	bool bActive = Speed > IDLE_SPEED_THRESHOLD;
	if(State.m_HasLast)
	{
		float AngleDelta = AimAngle - State.m_LastAimAngle;
		while(AngleDelta > pi)
			AngleDelta -= 2.0f * pi;
		while(AngleDelta < -pi)
			AngleDelta += 2.0f * pi;
		bActive = bActive || absolute(AngleDelta) > AIM_ANGLE_EPSILON;
	}
	bActive = bActive || AttackChanged || HookChanged;

	if(AttackChanged)
		State.m_AttackCooldown = 0.5f;
	if(HookChanged)
		State.m_HookCooldown = 0.4f;

	State.m_LastAimAngle = AimAngle;
	State.m_LastAttackTick = CharInfo.m_Cur.m_AttackTick;
	State.m_LastHookState = CharInfo.m_Cur.m_HookState;
	State.m_HasLast = true;

	return !bActive && State.m_AttackCooldown <= 0.0f && State.m_HookCooldown <= 0.0f;
}

void CWeaponIdle::ComputePose(const SIdleState &State, float T, float AimAngle, float &RotOffset, vec2 &PosOffset) const
{
	RotOffset = 0.0f;
	PosOffset = vec2(0.0f, 0.0f);

	vec2 AimDir = direction(AimAngle);

	if(T < WINDUP_END)
	{
		// pull back and tilt so the spin lands harder
		const float U = T / WINDUP_END;
		const float Env = std::sin(U * pi);
		PosOffset = -AimDir * (5.0f * Env) + vec2(0.0f, 2.0f * Env);
		RotOffset = -State.m_Dir * 0.6f * Env;
		return;
	}

	vec2 MainPos(0.0f, 0.0f);
	float MainBob = 0.0f;
	float SpinAngle = 0.0f;

	if(T < TWIRL_END)
	{
		const float V = (T - WINDUP_END) / (TWIRL_END - WINDUP_END);
		const float E = Smoothstep(V);
		const float Phi = State.m_Dir * 2.0f * pi * E;
		const float Env = std::sin(V * pi);

		switch(State.m_Variant)
		{
		case 0: // wide orbit twirl
			MainPos = vec2(std::cos(Phi) * 30.0f, std::sin(Phi) * 20.0f) * Env;
			MainBob = 12.0f * Env;
			SpinAngle = Phi * State.m_Spins;
			break;
		case 1: // toss: tight spin, weapon leaps upward
			MainPos = vec2(std::cos(Phi) * 12.0f, std::sin(Phi) * 12.0f) * Env;
			MainBob = 46.0f * Env;
			SpinAngle = Phi * State.m_Spins;
			break;
		case 2: // figure eight
			MainPos = vec2(std::sin(2.0f * Phi) * 24.0f, std::sin(Phi) * 15.0f) * Env;
			SpinAngle = Phi * State.m_Spins;
			break;
		case 3: // helicopter: frantic spin in place, slight hop
			MainPos = vec2(std::cos(Phi) * 8.0f, std::sin(Phi) * 8.0f) * Env;
			MainBob = 22.0f * Env;
			SpinAngle = Phi * (State.m_Spins + 2);
			break;
		case 4: // loop: weapon does a vertical loop above the tee
			MainPos = vec2(std::sin(Phi) * 16.0f, -(1.0f - std::cos(Phi)) * 16.0f) * Env;
			SpinAngle = Phi * State.m_Spins;
			break;
		default: // 5: slash: tall counter-spinning orbit
			MainPos = vec2(std::cos(Phi) * 12.0f, std::sin(Phi) * 26.0f) * Env;
			SpinAngle = Phi * (-State.m_Spins);
			break;
		}
		MainPos = rotate_radians(MainPos, AimAngle);
	}

	PosOffset += MainPos;
	PosOffset.y -= MainBob;

	// sprite spin ends on a multiple of 2pi so the weapon lands cleanly
	RotOffset += SpinAngle;

	if(T >= TWIRL_END)
	{
		// catch presents the weapon forward so the flourish lands cleanly
		const float W = (T - TWIRL_END) / (1.0f - TWIRL_END);
		const float Env = std::sin(W * pi);
		PosOffset += AimDir * (7.0f * Env);
		RotOffset += State.m_Dir * 0.5f * Env;
	}
}

void CWeaponIdle::OnRender()
{
	const bool Enabled = g_Config.m_FcWeaponIdleAnim != 0;
	const bool Paused = GameClient()->IsWorldPaused() || GameClient()->IsDemoPlaybackPaused();
	const float Dt = Paused ? 0.0f : Client()->RenderFrameTime();
	const float Delay = (float)g_Config.m_FcWeaponIdleDelay;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		SIdleState &State = m_aState[i];
		State.m_CurRotOffset = 0.0f;
		State.m_CurPosOffset = vec2(0.0f, 0.0f);
		State.m_AnimActive = false;

		const auto &CharInfo = GameClient()->m_Snap.m_aCharacters[i];
		if(!CharInfo.m_Active)
		{
			State = SIdleState();
			continue;
		}

		// keep only the recent trail window so ghosts fade out after the anim ends
		if(!State.m_vTrail.empty())
		{
			const float Cutoff = Client()->GlobalTime() - TRAIL_KEEP;
			size_t First = 0;
			while(First < State.m_vTrail.size() && State.m_vTrail[First].m_Time < Cutoff)
				First++;
			if(First > 0)
				State.m_vTrail.erase(State.m_vTrail.begin(), State.m_vTrail.begin() + First);
		}

		State.m_AttackCooldown = std::max(0.0f, State.m_AttackCooldown - Dt);
		State.m_HookCooldown = std::max(0.0f, State.m_HookCooldown - Dt);

		if(State.m_AnimTime >= 0.0f)
		{
			State.m_AnimTime += Dt;
			if(State.m_AnimTime >= ANIM_DURATION)
			{
				if(State.m_Combo && State.m_ComboIndex + 1 < 6 && IsIdle(i))
				{
					// chain flourishes so the combo plays back to back
					State.m_ComboIndex++;
					TriggerAnim(i, (State.m_ComboStart + State.m_ComboIndex) % 6);
					continue;
				}
				State.m_Combo = false;
				State.m_ComboIndex = 0;
				State.m_AnimTime = -1.0f;
				State.m_IdleTime = 0.0f;
				State.m_NextDelay = Delay * (0.75f + 0.5f * (rand() % 1000) / 1000.0f);
			}
			else
			{
				State.m_AnimActive = true;
				ComputePose(State, State.m_AnimTime / ANIM_DURATION, AimAngleOf(i), State.m_CurRotOffset, State.m_CurPosOffset);
				State.m_vTrail.push_back({Client()->GlobalTime(), State.m_CurRotOffset, State.m_CurPosOffset});
				if(State.m_vTrail.size() > 64)
					State.m_vTrail.erase(State.m_vTrail.begin());
				continue;
			}
		}

		if(Enabled && IsIdle(i))
		{
			State.m_IdleTime += Dt;
			const float RequiredDelay = State.m_NextDelay > 0.0f ? State.m_NextDelay : Delay;
			if(State.m_IdleTime >= RequiredDelay)
				TriggerAnim(i, -1);
		}
		else if(!State.m_AnimActive)
		{
			State.m_IdleTime = 0.0f;
		}
	}
}

void CWeaponIdle::TriggerAnim(int ClientId, int Variant)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	SIdleState &State = m_aState[ClientId];
	State.m_AnimTime = 0.0f;
	State.m_IdleTime = 0.0f;
	State.m_Dir = (rand() % 2) ? 1 : -1;
	State.m_Spins = 2 + (rand() % 2);
	State.m_Variant = (Variant >= 0 && Variant <= 5) ? Variant : rand() % 6;
	State.m_AnimActive = true;
	ComputePose(State, 0.0f, AimAngleOf(ClientId), State.m_CurRotOffset, State.m_CurPosOffset);
}

void CWeaponIdle::TriggerCombo(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	SIdleState &State = m_aState[ClientId];
	State.m_Combo = true;
	State.m_ComboIndex = 0;
	State.m_ComboStart = rand() % 6;
	TriggerAnim(ClientId, State.m_ComboStart);
}

void CWeaponIdle::TriggerLocalTee(int TeeArg, int Variant, bool Combo)
{
	if(TeeArg >= NUM_DUMMIES)
	{
		// 2 means fire on all connected local tees
		bool bAny = false;
		for(int i = 0; i < NUM_DUMMIES; i++)
		{
			const int LocalId = GameClient()->m_aLocalIds[i];
			if(LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
			{
				if(Combo)
					TriggerCombo(LocalId);
				else
				{
					m_aState[LocalId].m_Combo = false;
					m_aState[LocalId].m_ComboIndex = 0;
					TriggerAnim(LocalId, Variant);
				}
				bAny = true;
			}
		}
		if(!bAny)
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weaponanim", "no active tees found");
		return;
	}

	const int LocalId = TeeArg < 0 ? GameClient()->m_aLocalIds[g_Config.m_ClDummy] : GameClient()->m_aLocalIds[TeeArg];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS || !GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "weaponanim", "no active tee for that player slot");
		return;
	}
	if(Combo)
		TriggerCombo(LocalId);
	else
	{
		m_aState[LocalId].m_Combo = false;
		m_aState[LocalId].m_ComboIndex = 0;
		TriggerAnim(LocalId, Variant);
	}
}

void CWeaponIdle::ConWeaponAnim(IConsole::IResult *pResult, void *pUserData)
{
	CWeaponIdle *pSelf = static_cast<CWeaponIdle *>(pUserData);
	const int TeeArg = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : -1;
	const int Variant = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1;
	pSelf->TriggerLocalTee(TeeArg, Variant, false);
}

void CWeaponIdle::ConWeaponAnimTwirl(IConsole::IResult *pResult, void *pUserData)
{
	CWeaponIdle *pSelf = static_cast<CWeaponIdle *>(pUserData);
	const int TeeArg = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : -1;
	pSelf->TriggerLocalTee(TeeArg, 0, false);
}

void CWeaponIdle::ConWeaponAnimToss(IConsole::IResult *pResult, void *pUserData)
{
	CWeaponIdle *pSelf = static_cast<CWeaponIdle *>(pUserData);
	const int TeeArg = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : -1;
	pSelf->TriggerLocalTee(TeeArg, 1, false);
}

void CWeaponIdle::ConWeaponAnimCombo(IConsole::IResult *pResult, void *pUserData)
{
	CWeaponIdle *pSelf = static_cast<CWeaponIdle *>(pUserData);
	const int TeeArg = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : -1;
	pSelf->TriggerLocalTee(TeeArg, -1, true);
}

void CWeaponIdle::OnConsoleInit()
{
	Console()->Register("tc_weaponanim", "?i[tee] ?i[variant]", CFGFLAG_CLIENT, ConWeaponAnim, this, "Play a weapon idle flourish (tee: -1 = active, 0 = main, 1 = dummy, 2 = both; variant: 0 twirl, 1 toss, 2 figure8, 3 helicopter, 4 loop, 5 slash, -1 = random)");
	Console()->Register("tc_weaponanim_twirl", "?i[tee]", CFGFLAG_CLIENT, ConWeaponAnimTwirl, this, "Play the orbiting weapon idle flourish (tee: -1 = active, 0 = main, 1 = dummy, 2 = both)");
	Console()->Register("tc_weaponanim_toss", "?i[tee]", CFGFLAG_CLIENT, ConWeaponAnimToss, this, "Play the tossing weapon idle flourish (tee: -1 = active, 0 = main, 1 = dummy, 2 = both)");
	Console()->Register("tc_weaponanim_combo", "?i[tee]", CFGFLAG_CLIENT, ConWeaponAnimCombo, this, "Play all weapon idle flourishes in a row until you move (tee: -1 = active, 0 = main, 1 = dummy, 2 = both)");
}

void CWeaponIdle::OnReset()
{
	for(auto &State : m_aState)
		State = SIdleState();
}

bool CWeaponIdle::GetAnim(int ClientId, float AimAngle, float &RotOffset, vec2 &PosOffset) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	const SIdleState &State = m_aState[ClientId];
	if(!State.m_AnimActive)
		return false;
	// recompute with the live render angle so the orbit tracks the aim smoothly
	ComputePose(State, State.m_AnimTime / ANIM_DURATION, AimAngle, RotOffset, PosOffset);
	return true;
}

const std::vector<CWeaponIdle::STrailSample> *CWeaponIdle::GetTrail(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return nullptr;
	const std::vector<STrailSample> &vTrail = m_aState[ClientId].m_vTrail;
	return vTrail.empty() ? nullptr : &vTrail;
}
