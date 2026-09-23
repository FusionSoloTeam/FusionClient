/* Copyright © 2026 BestProject Team */
#include "lightning.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <generated/client_data.h>

#include <game/client/components/particles.h>
#include <game/client/gameclient.h>

#include "own_tee.h"

#include <algorithm>
#include <cmath>

using FgfGlow::Blend;
using FgfGlow::Whiten;

static constexpr int NUM_STROKES = 3;
static constexpr float STROKE_TIMES[NUM_STROKES] = {0.0f, 0.085f, 0.19f};
static constexpr float STROKE_POWERS[NUM_STROKES] = {1.0f, 0.7f, 0.5f};
static constexpr float STROKE_DECAY = 0.05f;
static constexpr float BOLT_LIFE = 0.8f;

void CLightning::OnInit()
{
	m_GlowTexture = FgfGlow::CreateGlowTexture(Graphics());
	m_RingTexture = FgfGlow::CreateRingTexture(Graphics());
	m_BeamTexture = FgfGlow::CreateBeamTexture(Graphics());
}

void CLightning::OnReset()
{
	m_NumBolts = 0;
	m_Glow.Clear();
	m_ShakeStrength = 0.0f;
}

void CLightning::AddShake(float Strength)
{
	m_ShakeStrength = std::max(m_ShakeStrength, Strength);
}

void CLightning::ApplyCameraShake(vec2 &Center)
{
	const float Now = LocalTime();
	const float ShakePassed = std::clamp(Now - m_LastShakeTime, 0.0f, 0.1f);
	m_LastShakeTime = Now;
	if(m_ShakeStrength > 0.01f)
	{
		Center += vec2(std::sin(Now * 82.0f), std::sin(Now * 63.0f + 1.7f)) * m_ShakeStrength;
		m_ShakeStrength -= m_ShakeStrength * std::min(1.0f, ShakePassed * 9.0f);
	}
	else
	{
		m_ShakeStrength = 0.0f;
	}
}

void CLightning::OnNewSnapshot()
{
	if(!g_Config.m_TcFreezeLightning || GameClient()->m_SuppressEvents || BcFxSuppressed(GameClient()))
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CGameClient::CSnapState::CCharacterInfo &Character = GameClient()->m_Snap.m_aCharacters[i];
		if(!Character.m_Active || !Character.m_HasExtendedData || !Character.m_pPrevExtendedData)
			continue;
		if(!BcIsOwnTee(GameClient(), i))
			continue;
		if(Character.m_pPrevExtendedData->m_FreezeEnd != 0 || Character.m_ExtendedData.m_FreezeEnd == 0)
			continue;

		const vec2 Pos = mix(vec2(Character.m_Prev.m_X, Character.m_Prev.m_Y),
			vec2(Character.m_Cur.m_X, Character.m_Cur.m_Y),
			Client()->IntraGameTick(g_Config.m_ClDummy));
		Strike(Pos, GameClient()->IsOtherTeam(i) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f, i == GameClient()->m_Snap.m_LocalClientId);
		if(i == GameClient()->m_Snap.m_LocalClientId && g_Config.m_TcFreezeLightningShake > 0)
			AddShake(g_Config.m_TcFreezeLightningShake / 100.0f * 14.0f);
	}
}

CLightning::CPalette CLightning::Palette()
{
	const ColorHSLA Hsl(g_Config.m_TcFreezeLightningColor);
	ColorHSLA DeepHsl = Hsl;
	DeepHsl.h = std::fmod(DeepHsl.h + 0.07f, 1.0f);
	DeepHsl.l *= 0.8f;

	CPalette Result;
	Result.m_Primary = color_cast<ColorRGBA>(Hsl);
	Result.m_Secondary = color_cast<ColorRGBA>(DeepHsl);
	Result.m_Core = Whiten(Result.m_Primary, 0.85f);
	return Result;
}

void CLightning::BuildPath(CPath *pPath, vec2 From, vec2 To, int NumPoints, float Wildness)
{
	pPath->m_NumPoints = 0;
	if(distance(From, To) < 1.0f)
		return;

	pPath->m_NumPoints = NumPoints;
	pPath->m_aPoints[0] = From;
	pPath->m_aPoints[NumPoints - 1] = To;

	const vec2 Normal = normalize(vec2(From.y - To.y, To.x - From.x));
	float Displace = Wildness;
	for(int Step = NumPoints - 1; Step > 1; Step /= 2)
	{
		for(int i = Step / 2; i < NumPoints; i += Step)
			pPath->m_aPoints[i] = (pPath->m_aPoints[i - Step / 2] + pPath->m_aPoints[i + Step / 2]) * 0.5f + Normal * random_float(-Displace, Displace);
		Displace *= 0.5f;
	}
}

float CLightning::Intensity(const CBolt &Bolt)
{
	float Result = 0.0f;
	for(int i = 0; i < Bolt.m_Strokes; i++)
	{
		const float Since = Bolt.m_Age - STROKE_TIMES[i];
		if(Since >= 0.0f)
			Result = std::max(Result, STROKE_POWERS[i] * std::exp(-Since / STROKE_DECAY));
	}
	Result += 0.18f * std::exp(-Bolt.m_Age / 0.25f);
	return std::min(1.0f, Result);
}

void CLightning::Strike(vec2 Target, float Alpha, bool Local)
{
	const int Style = std::clamp(g_Config.m_TcFreezeLightningStyle, 0, 2);
	if(Style == 1)
	{
		StrikeNova(Target, Alpha, Local);
		return;
	}
	if(Style == 2)
	{
		StrikeShards(Target, Alpha, Local);
		return;
	}

	if(m_NumBolts >= MAX_BOLTS)
		return;

	const float Height = g_Config.m_TcFreezeLightningHeight;
	CBolt &Bolt = m_aBolts[m_NumBolts++];
	Bolt = CBolt{};
	Bolt.m_Origin = Target + vec2(random_float(-0.3f, 0.3f) * Height, -Height);
	Bolt.m_Target = Target;
	Bolt.m_Alpha = Alpha;
	Bolt.m_Local = Local;
	BuildPath(&Bolt.m_Channel, Bolt.m_Origin, Target, MAX_POINTS, Height * 0.16f);

	Stroke(Bolt, Palette());
}

void CLightning::StrikeNova(vec2 Target, float Alpha, bool Local)
{
	const float Size = g_Config.m_TcFreezeLightningSize / 100.0f;
	const CPalette Colors = Palette();
	m_StyleFlash = maximum(m_StyleFlash, 0.85f * Alpha * (Local ? 1.0f : 0.6f));

	m_Glow.AddRing(Target, 0.55f, 26.0f * Size, 320.0f * Size, 0.90f * Alpha, Colors.m_Primary);
	m_Glow.AddRing(Target, 0.85f, 12.0f * Size, 200.0f * Size, 0.70f * Alpha, Colors.m_Core);

	if(!g_Config.m_TcFreezeLightningImpact)
		return;

	const int Shards = 10 + g_Config.m_TcFreezeLightningBranches * 8;
	for(int i = 0; i < Shards; i++)
	{
		CGlowParticles::CSpark *pSpark = m_Glow.NewSpark();
		if(pSpark == nullptr)
			break;
		const float Angle = (float)i / (float)Shards * 2.0f * pi + random_float(-0.12f, 0.12f);
		pSpark->m_Pos = Target + direction(Angle) * random_float(4.0f, 18.0f) * Size;
		pSpark->m_Vel = direction(Angle) * random_float(80.0f, 240.0f) * Size;
		pSpark->m_Gravity = 140.0f;
		pSpark->m_Drag = 2.4f;
		pSpark->m_LifeSpan = random_float(0.35f, 0.75f);
		pSpark->m_StartSize = random_float(6.0f, 13.0f) * Size;
		pSpark->m_EndSize = 0.0f;
		pSpark->m_StartAlpha = Alpha;
		pSpark->m_EndAlpha = 0.0f;
		pSpark->m_Color = (i % 3 == 0) ? Colors.m_Core : Colors.m_Primary;
	}
}

void CLightning::StrikeShards(vec2 Target, float Alpha, bool Local)
{
	const float Size = g_Config.m_TcFreezeLightningSize / 100.0f;
	const CPalette Colors = Palette();
	m_StyleFlash = maximum(m_StyleFlash, 0.55f * Alpha * (Local ? 1.0f : 0.5f));

	const int Shards = 8 + g_Config.m_TcFreezeLightningBranches * 10;
	for(int i = 0; i < Shards; i++)
	{
		CGlowParticles::CSpark *pSpark = m_Glow.NewSpark();
		if(pSpark == nullptr)
			break;
		const float Angle = (float)i / (float)Shards * 2.0f * pi + random_float(-0.20f, 0.20f);
		pSpark->m_Pos = Target;
		pSpark->m_Vel = direction(Angle) * random_float(150.0f, 430.0f) * Size;
		pSpark->m_Gravity = 300.0f;
		pSpark->m_Drag = 1.1f;
		pSpark->m_Stretch = 6.0f;
		pSpark->m_LifeSpan = random_float(0.50f, 0.95f);
		pSpark->m_StartSize = random_float(3.0f, 6.0f) * Size;
		pSpark->m_EndSize = 0.0f;
		pSpark->m_StartAlpha = Alpha;
		pSpark->m_EndAlpha = 0.0f;
		pSpark->m_Color = Colors.m_Core;
		pSpark->m_ShiftColor = true;
		pSpark->m_EndColor = Colors.m_Primary;
	}

	if(g_Config.m_TcFreezeLightningImpact)
		m_Glow.AddRing(Target, 0.40f, 14.0f * Size, 130.0f * Size, 0.80f * Alpha, Colors.m_Secondary);
}

void CLightning::Stroke(CBolt &Bolt, const CPalette &Palette)
{
	const int Index = Bolt.m_Strokes++;
	const float Height = g_Config.m_TcFreezeLightningHeight;
	CPath &Channel = Bolt.m_Channel;

	if(Index > 0 && Channel.m_NumPoints > 2)
	{
		const vec2 Normal = normalize(vec2(Bolt.m_Origin.y - Bolt.m_Target.y, Bolt.m_Target.x - Bolt.m_Origin.x));
		for(int i = 1; i < Channel.m_NumPoints - 1; i++)
			Channel.m_aPoints[i] += Normal * random_float(-4.0f, 4.0f);
	}

	Bolt.m_NumBranches = 0;
	const int Branches = std::min<int>(MAX_BRANCHES, g_Config.m_TcFreezeLightningBranches);
	const float DownAngle = angle(Bolt.m_Target - Bolt.m_Origin);
	for(int i = 0; i < Branches && Channel.m_NumPoints > 12; i++)
	{
		const int ForkIndex = std::min(Channel.m_NumPoints - 9, 3 + (int)(random_float() * (Channel.m_NumPoints - 12)));
		const vec2 From = Channel.m_aPoints[ForkIndex];
		const float Side = (i % 2 == 0) ? 1.0f : -1.0f;
		const float Length = Height * random_float(0.15f, 0.32f);
		const vec2 To = From + direction(DownAngle + Side * random_float(0.45f, 1.1f)) * Length;
		BuildPath(&Bolt.m_aBranches[Bolt.m_NumBranches], From, To, BRANCH_POINTS, Length * 0.22f);
		if(Bolt.m_aBranches[Bolt.m_NumBranches].m_NumPoints == 0)
			continue;
		Bolt.m_aBranchWidths[Bolt.m_NumBranches] = random_float(0.4f, 0.65f);
		Bolt.m_NumBranches++;
	}

	if(g_Config.m_TcFreezeLightningImpact)
		Impact(Bolt, Palette, STROKE_POWERS[Index]);
}

void CLightning::Impact(const CBolt &Bolt, const CPalette &Palette, float Power)
{
	const float Thickness = g_Config.m_TcFreezeLightningThickness / 100.0f;
	const float Alpha = Bolt.m_Alpha;

	m_Glow.AddRing(Bolt.m_Target, 0.5f, 20.0f * Thickness, 220.0f * Thickness * Power, 0.8f * Alpha * Power, Palette.m_Primary);
	if(Power >= 1.0f)
		m_Glow.AddRing(Bolt.m_Target, 0.75f, 10.0f * Thickness, 150.0f * Thickness, 0.5f * Alpha, Palette.m_Secondary);

	const int Sparks = round_to_int(26.0f * Power);
	for(int i = 0; i < Sparks; i++)
	{
		CGlowParticles::CSpark *pSpark = m_Glow.NewSpark();
		if(!pSpark)
			break;
		pSpark->m_Pos = Bolt.m_Target;
		pSpark->m_Vel = direction(random_float(-pi + 0.25f, -0.25f)) * random_float(250.0f, 800.0f) * std::sqrt(Power);
		pSpark->m_Gravity = 1000.0f;
		pSpark->m_Drag = 0.08f;
		pSpark->m_Stretch = 0.028f;
		pSpark->m_LifeSpan = random_float(0.3f, 0.75f);
		pSpark->m_StartSize = random_float(5.0f, 9.0f);
		pSpark->m_StartAlpha = Alpha;
		pSpark->m_Color = Whiten(Blend(Palette.m_Primary, Palette.m_Secondary, random_float()), 0.45f);
	}

	const int Glitter = round_to_int(12.0f * Power);
	for(int i = 0; i < Glitter; i++)
	{
		CGlowParticles::CSpark *pSpark = m_Glow.NewSpark();
		if(!pSpark)
			break;
		pSpark->m_Pos = Bolt.m_Target + random_direction() * random_float(0.0f, 20.0f);
		pSpark->m_Vel = random_direction() * random_float(30.0f, 140.0f);
		pSpark->m_Gravity = -15.0f;
		pSpark->m_Drag = 0.3f;
		pSpark->m_LifeSpan = random_float(0.5f, 1.0f);
		pSpark->m_StartSize = random_float(2.5f, 4.5f);
		pSpark->m_StartAlpha = 0.9f * Alpha;
		pSpark->m_Color = Palette.m_Core;
	}

	if(Power < 1.0f)
		return;

	for(int i = 0; i < 10; i++)
	{
		CParticle p;
		p.SetDefault();
		p.m_Spr = SPRITE_PART_SNOWFLAKE;
		p.m_Pos = Bolt.m_Target;
		p.m_Vel = random_direction() * random_float(50.0f, 260.0f);
		p.m_LifeSpan = random_float(0.6f, 1.2f);
		p.m_StartSize = random_float(14.0f, 26.0f);
		p.m_EndSize = 0.0f;
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float(-1.0f, 1.0f) * pi;
		p.m_Gravity = 180.0f;
		p.m_Friction = 0.9f;
		p.m_UseAlphaFading = true;
		p.m_StartAlpha = Alpha;
		p.m_EndAlpha = 0.0f;
		p.m_FlowAffected = 0.0f;
		p.m_Collides = false;
		p.m_Color = Palette.m_Primary;
		GameClient()->m_Particles.Add(CParticles::GROUP_EXTRA, &p);
	}
}

void CLightning::DrawPath(const CPath &Path, float Width, bool TaperToTip, ColorRGBA Color)
{
	float aWidths[MAX_POINTS];
	for(int i = 0; i < Path.m_NumPoints; i++)
	{
		const float Progress = i / (float)(Path.m_NumPoints - 1);
		aWidths[i] = Width * (TaperToTip ? 1.0f - Progress : mix(0.55f, 1.0f, Progress));
	}
	FgfGlow::DrawStrip(Graphics(), Path.m_aPoints, aWidths, Path.m_NumPoints, Color);
}

void CLightning::DrawBolts(const CPalette &Palette)
{
	if(m_NumBolts == 0 && !m_Glow.HasAny() && m_StyleFlash < 0.01f)
		return;

	const float Thickness = g_Config.m_TcFreezeLightningThickness / 100.0f;

	auto DrawBlob = [this](vec2 Pos, float Size, ColorRGBA Color) {
		Graphics()->SetColor(Color);
		IGraphics::CQuadItem Quad(Pos.x, Pos.y, Size, Size);
		Graphics()->QuadsDraw(&Quad, 1);
	};

	auto DrawBeams = [&](float Width, ColorRGBA Color, float AlphaScale) {
		FgfGlow::BeginStrips(Graphics(), m_BeamTexture);
		for(int i = 0; i < m_NumBolts; i++)
		{
			const CBolt &Bolt = m_aBolts[i];
			const float Alpha = AlphaScale * Intensity(Bolt) * Bolt.m_Alpha;
			DrawPath(Bolt.m_Channel, Width, false, Color.WithAlpha(Alpha));
			for(int b = 0; b < Bolt.m_NumBranches; b++)
				DrawPath(Bolt.m_aBranches[b], Width * Bolt.m_aBranchWidths[b], true, Color.WithAlpha(0.85f * Alpha));
		}
		Graphics()->QuadsEnd();
	};

	Graphics()->QuadsSetRotation(0.0f);

	Graphics()->BlendNormal();
	DrawBeams(12.0f * Thickness, Palette.m_Primary, 0.3f);
	Graphics()->TextureSet(m_GlowTexture);
	Graphics()->QuadsBegin();
	for(int i = 0; i < m_NumBolts; i++)
		DrawBlob(m_aBolts[i].m_Target, 200.0f * Thickness, Palette.m_Primary.WithAlpha(0.25f * Intensity(m_aBolts[i]) * m_aBolts[i].m_Alpha));
	Graphics()->QuadsEnd();

	Graphics()->BlendAdditive();
	m_Glow.Draw(Graphics(), m_GlowTexture, m_RingTexture);

	DrawBeams(26.0f * Thickness, Palette.m_Secondary, 0.55f);
	DrawBeams(9.0f * Thickness, Palette.m_Primary, 0.9f);
	DrawBeams(3.2f * Thickness, Palette.m_Core, 1.0f);

	Graphics()->TextureSet(m_GlowTexture);
	Graphics()->QuadsBegin();
	for(int i = 0; i < m_NumBolts; i++)
	{
		const CBolt &Bolt = m_aBolts[i];
		const float Alpha = Intensity(Bolt) * Bolt.m_Alpha;
		DrawBlob(Bolt.m_Origin, 170.0f * Thickness, Palette.m_Secondary.WithAlpha(0.35f * Alpha));
		DrawBlob(Bolt.m_Origin, 60.0f * Thickness, Palette.m_Primary.WithAlpha(0.45f * Alpha));
		DrawBlob(Bolt.m_Target, 260.0f * Thickness, Palette.m_Primary.WithAlpha(0.5f * Alpha));
		DrawBlob(Bolt.m_Target, 100.0f * Thickness, Palette.m_Core.WithAlpha(0.9f * Alpha));
	}
	Graphics()->QuadsEnd();

	float Flash = m_StyleFlash;
	for(int i = 0; i < m_NumBolts; i++)
		if(m_aBolts[i].m_Local)
			Flash = std::max(Flash, Intensity(m_aBolts[i]) * m_aBolts[i].m_Alpha);
	if(Flash > 0.01f)
	{
		const CFxScreenRect Screen = FxGetScreen(Graphics());
		const vec2 Center = (Screen.m_TopLeft + Screen.m_BottomRight) * 0.5f;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Palette.m_Primary.WithAlpha(0.08f * Flash));
		IGraphics::CQuadItem Quad(Center.x, Center.y, Screen.Width(), Screen.Height());
		Graphics()->QuadsDraw(&Quad, 1);
		Graphics()->QuadsEnd();
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->BlendNormal();
}

void CLightning::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(BcFxSuppressed(GameClient()))
		return;

	const float Time = LocalTime();
	const float Passed = std::clamp(Time - m_LastTime, 0.0f, 0.1f);
	m_LastTime = Time;

	const CPalette CurrentPalette = Palette();

	for(int i = 0; i < m_NumBolts;)
	{
		CBolt &Bolt = m_aBolts[i];
		Bolt.m_Age += Passed;
		while(Bolt.m_Strokes < NUM_STROKES && Bolt.m_Age >= STROKE_TIMES[Bolt.m_Strokes])
			Stroke(Bolt, CurrentPalette);
		if(Bolt.m_Age >= BOLT_LIFE)
		{
			Bolt = m_aBolts[--m_NumBolts];
			continue;
		}
		i++;
	}

	m_StyleFlash *= std::exp(-Passed * 5.0f);
	if(m_StyleFlash < 0.005f)
		m_StyleFlash = 0.0f;

	m_Glow.Update(Passed);
	DrawBolts(CurrentPalette);
}
