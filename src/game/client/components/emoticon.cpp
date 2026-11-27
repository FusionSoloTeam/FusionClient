/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "emoticon.h"

#include "chat.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include "hud_layout.h"

namespace
{
	constexpr float EMOTE_SPACING = 48.0f;
	constexpr float EYE_SPACING = 56.0f;
	constexpr float ROW_OFFSET = 44.0f;
	constexpr float EMOTE_ROW_PAD = 48.0f;
	constexpr float EYE_ROW_PAD = 40.0f;
}

CEmoticon::CEmoticon()
{
	CEmoticon::OnReset();
}

void CEmoticon::ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData)
{
	CEmoticon *pSelf = (CEmoticon *)pUserData;

	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;

	if(!pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(pSelf->GameClient()->m_BindWheel.IsActive())
			pSelf->m_Active = false;
		else
			pSelf->m_Active = pResult->GetInteger(0) != 0;
	}
}

void CEmoticon::ConEmote(IConsole::IResult *pResult, void *pUserData)
{
	((CEmoticon *)pUserData)->Emote(pResult->GetInteger(0));
}

void CEmoticon::OnConsoleInit()
{
	Console()->Register("+emote", "", CFGFLAG_CLIENT, ConKeyEmoticon, this, "Open emote selector");
	Console()->Register("emote", "i[emote-id]", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
}

void CEmoticon::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	m_TouchPressedOutside = false;
}

void CEmoticon::OnRelease()
{
	m_Active = false;
}

bool CEmoticon::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CEmoticon::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	return false;
}

void CEmoticon::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	static const auto QuadEaseInOut = [](float t) -> float {
		if(t == 0.0f)
			return 0.0f;
		if(t == 1.0f)
			return 1.0f;
		return (t < 0.5f) ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2) / 2.0f);
	};
	static const auto PositiveMod = [](float x, float y) -> float {
		return std::fmod(x + y, y);
	};

	const float AnimationTime = (float)g_Config.m_TcAnimateWheelTime / 1000.0f;
	const float ItemAnimationTime = AnimationTime / 2.0f;

	if(AnimationTime != 0.0f)
	{
		for(float &Time : m_aAnimationTimeEmotes)
			Time = std::max(0.0f, Time - Client()->RenderFrameTime());
		for(float &Time : m_aAnimationTimeEyeEmotes)
			Time = std::max(0.0f, Time - Client()->RenderFrameTime());
	}

	if(!m_Active)
	{
		if(m_TouchPressedOutside)
		{
			m_SelectedEmote = -1;
			m_SelectedEyeEmote = -1;
			m_TouchPressedOutside = false;
		}

		if(m_WasActive && m_SelectedEmote != -1)
			Emote(m_SelectedEmote);
		if(m_WasActive && m_SelectedEyeEmote != -1)
			EyeEmote(m_SelectedEyeEmote);
		m_WasActive = false;

		if(AnimationTime == 0.0f)
			return;

		m_AnimationTime -= Client()->RenderFrameTime() * 3.0f; // Close animation 3x faster
		if(m_AnimationTime <= 0.0f)
		{
			m_AnimationTime = 0.0f;
			return;
		}
	}
	else
	{
		if(AnimationTime != 0.0f)
		{
			m_AnimationTime += Client()->RenderFrameTime();
			if(m_AnimationTime > AnimationTime)
				m_AnimationTime = AnimationTime;
		}
		m_WasActive = true;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_Active || !GameClient()->m_Snap.m_pLocalCharacter)
	{
		m_Active = false;
		m_WasActive = false;
	}

	const CUIRect Screen = *Ui()->Screen();
	const int LayoutMode = std::clamp(g_Config.m_FcEmoteWheelLayout, 0, 2);

	vec2 WheelCenter = Screen.Center();
	float WheelScale = 1.0f;
	if(LayoutMode != 0)
	{
		const float HudW = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
		const float HudH = HudLayout::CANVAS_HEIGHT;
		const HudLayout::SModuleLayout WheelLayout = HudLayout::Get(HudLayout::MODULE_EMOTE_WHEEL, HudW, HudH);
		WheelCenter = vec2(WheelLayout.m_X * (Screen.w / HudW), WheelLayout.m_Y * (Screen.h / HudH));
		WheelScale = std::clamp(WheelLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	}
	const bool EyeWheel = GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel && GameClient()->m_aLocalIds[g_Config.m_ClDummy] >= 0;

	int aWheelEmotes[NUM_EMOTICONS];
	for(int i = 0; i < NUM_EMOTICONS; i++)
		aWheelEmotes[i] = i;
	const int WheelEmoteCount = NUM_EMOTICONS;

	if(LayoutMode != 0)
	{
		const float LineHalf = (std::max((WheelEmoteCount - 1) * 0.5f * EMOTE_SPACING + EMOTE_ROW_PAD,
			(NUM_EMOTES - 1) * 0.5f * EYE_SPACING + EYE_ROW_PAD) + 4.0f) * WheelScale;
		const float LineThickness = 2.0f * (ROW_OFFSET + 48.0f) * WheelScale;
		const float FitW = LayoutMode == 1 ? LineHalf * 2.0f : LineThickness;
		const float FitH = LayoutMode == 1 ? LineThickness : LineHalf * 2.0f;
		WheelScale *= std::min({1.0f, Screen.w * 0.9f / FitW, Screen.h * 0.7f / FitH});
	}

	const float EmoteSpacing = EMOTE_SPACING * WheelScale;
	const float EyeSpacing = EYE_SPACING * WheelScale;
	const float RowOffset = ROW_OFFSET * WheelScale;
	const float EmoteRowBoundX = (WheelEmoteCount - 1) * 0.5f * EmoteSpacing + EMOTE_ROW_PAD * WheelScale;
	const float EyeRowBoundX = (NUM_EMOTES - 1) * 0.5f * EyeSpacing + EYE_ROW_PAD * WheelScale;
	float BoundX = 170.0f * WheelScale;
	float BoundY = 170.0f * WheelScale;
	if(LayoutMode == 1)
	{
		BoundX = std::max(EmoteRowBoundX, EyeRowBoundX) + 4.0f * WheelScale;
		BoundY = RowOffset + 48.0f * WheelScale;
	}
	else if(LayoutMode == 2)
	{
		BoundX = RowOffset + 48.0f * WheelScale;
		BoundY = std::max((WheelEmoteCount - 1) * 0.5f * EmoteSpacing + EMOTE_ROW_PAD * WheelScale,
			(NUM_EMOTES - 1) * 0.5f * EyeSpacing + EYE_ROW_PAD * WheelScale) + 4.0f * WheelScale;
	}

	const bool WasTouchPressed = m_TouchState.m_AnyPressed;
	Ui()->UpdateTouchState(m_TouchState);
	if(m_TouchState.m_AnyPressed)
	{
		const vec2 TouchPos = m_TouchState.m_PrimaryPosition * Screen.Size() - WheelCenter;
		if(LayoutMode == 0)
		{
			const float TouchCenterDistance = length(TouchPos);
			if(TouchCenterDistance <= 170.0f * WheelScale)
			{
				m_SelectorMouse = TouchPos;
			}
			else if(TouchCenterDistance > 190.0f * WheelScale)
			{
				m_TouchPressedOutside = true;
			}
		}
		else if(std::fabs(TouchPos.x) <= BoundX && std::fabs(TouchPos.y) <= BoundY)
		{
			m_SelectorMouse = TouchPos;
		}
		else
		{
			m_TouchPressedOutside = true;
		}
	}
	else if(WasTouchPressed)
	{
		m_Active = false;
	}

	std::array<float, 5> aAnimationPhase;
	if(AnimationTime == 0.0f)
	{
		aAnimationPhase.fill(1.0f);
	}
	else
	{
		aAnimationPhase[0] = QuadEaseInOut(m_AnimationTime / AnimationTime);
		aAnimationPhase[1] = aAnimationPhase[0] * aAnimationPhase[0];
		aAnimationPhase[2] = aAnimationPhase[1] * aAnimationPhase[1];
		aAnimationPhase[3] = aAnimationPhase[2] * aAnimationPhase[2];
		aAnimationPhase[4] = aAnimationPhase[3] * aAnimationPhase[3];
	}

	if(LayoutMode == 0)
	{
		if(length(m_SelectorMouse) > 170.0f * WheelScale)
			m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f * WheelScale;
	}
	else
	{
		m_SelectorMouse.x = std::clamp(m_SelectorMouse.x, -BoundX, BoundX);
		m_SelectorMouse.y = std::clamp(m_SelectorMouse.y, -BoundY, BoundY);
	}

	const float SelectorAngle = angle(m_SelectorMouse);

	auto SlotFromAxis = [](float Pos, float Spacing, int Count) -> int {
		return std::clamp((int)std::round(Pos / Spacing + (Count - 1) * 0.5f), 0, Count - 1);
	};

	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	if(LayoutMode == 0)
	{
		if(length(m_SelectorMouse) > 110.0f * WheelScale)
			m_SelectedEmote = aWheelEmotes[(int)PositiveMod(std::round(SelectorAngle / (2.0f * pi) * (float)WheelEmoteCount), (float)WheelEmoteCount)];
		else if(length(m_SelectorMouse) > 40.0f * WheelScale)
			m_SelectedEyeEmote = PositiveMod(std::round(SelectorAngle / (2.0f * pi) * (float)NUM_EMOTES), NUM_EMOTES);
	}
	else if(LayoutMode == 1)
	{
		if(std::fabs(m_SelectorMouse.y + RowOffset) <= 40.0f * WheelScale && std::fabs(m_SelectorMouse.x) <= EmoteRowBoundX)
			m_SelectedEmote = aWheelEmotes[SlotFromAxis(m_SelectorMouse.x, EmoteSpacing, WheelEmoteCount)];
		else if(EyeWheel && std::fabs(m_SelectorMouse.y - RowOffset) <= 36.0f * WheelScale && std::fabs(m_SelectorMouse.x) <= EyeRowBoundX)
			m_SelectedEyeEmote = SlotFromAxis(m_SelectorMouse.x, EyeSpacing, NUM_EMOTES);
	}
	else
	{
		if(std::fabs(m_SelectorMouse.x + RowOffset) <= 40.0f * WheelScale && std::fabs(m_SelectorMouse.y) <= (WheelEmoteCount - 1) * 0.5f * EmoteSpacing + 44.0f * WheelScale)
			m_SelectedEmote = aWheelEmotes[SlotFromAxis(m_SelectorMouse.y, EmoteSpacing, WheelEmoteCount)];
		else if(EyeWheel && std::fabs(m_SelectorMouse.x - RowOffset) <= 36.0f * WheelScale && std::fabs(m_SelectorMouse.y) <= (NUM_EMOTES - 1) * 0.5f * EyeSpacing + 40.0f * WheelScale)
			m_SelectedEyeEmote = SlotFromAxis(m_SelectorMouse.y, EyeSpacing, NUM_EMOTES);
	}
	if(!EyeWheel)
		m_SelectedEyeEmote = -1;

	if(m_SelectedEmote != -1)
	{
		m_aAnimationTimeEmotes[m_SelectedEmote] += Client()->RenderFrameTime() * 2.0f; // To counteract earlier decrement
		if(m_aAnimationTimeEmotes[m_SelectedEmote] >= ItemAnimationTime)
			m_aAnimationTimeEmotes[m_SelectedEmote] = ItemAnimationTime;
	}
	if(m_SelectedEyeEmote != -1)
	{
		m_aAnimationTimeEyeEmotes[m_SelectedEyeEmote] += Client()->RenderFrameTime() * 2.0f; // To counteract earlier decrement
		if(m_aAnimationTimeEyeEmotes[m_SelectedEyeEmote] >= ItemAnimationTime)
			m_aAnimationTimeEyeEmotes[m_SelectedEyeEmote] = ItemAnimationTime;
	}

	Ui()->MapScreen();

	auto EmoteOffset = [&](int i) -> vec2 {
		if(LayoutMode == 1)
			return vec2((i - (WheelEmoteCount - 1) * 0.5f) * EmoteSpacing, -RowOffset);
		if(LayoutMode == 2)
			return vec2(-RowOffset, (i - (WheelEmoteCount - 1) * 0.5f) * EmoteSpacing);
		float Angle = 2 * pi * i / (float)WheelEmoteCount;
		if(Angle > pi)
			Angle -= 2.0f * pi;
		return direction(Angle) * 150.0f * WheelScale;
	};
	auto EyeOffset = [&](int j) -> vec2 {
		if(LayoutMode == 1)
			return vec2((j - (NUM_EMOTES - 1) * 0.5f) * EyeSpacing, RowOffset);
		if(LayoutMode == 2)
			return vec2(RowOffset, (j - (NUM_EMOTES - 1) * 0.5f) * EyeSpacing);
		float Angle = 2 * pi * j / (float)NUM_EMOTES;
		if(Angle > pi)
			Angle -= 2.0f * pi;
		return direction(Angle) * 70.0f * WheelScale;
	};

	if(LayoutMode == 0)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f * aAnimationPhase[0]);
		Graphics()->DrawCircle(WheelCenter.x, WheelCenter.y, 190.0f * WheelScale * aAnimationPhase[0], 64);
		Graphics()->QuadsEnd();
	}
	else
	{
		Graphics()->DrawRect(WheelCenter.x - BoundX, WheelCenter.y - BoundY, BoundX * 2.0f, BoundY * 2.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f * aAnimationPhase[0]), IGraphics::CORNER_ALL, 12.0f * WheelScale);
	}

	Graphics()->WrapClamp();
	for(int i = 0; i < WheelEmoteCount; i++)
	{
		const int Emote = aWheelEmotes[i];
		const vec2 Offset = EmoteOffset(i) * aAnimationPhase[1];

		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadsBegin();
		const float Phase = ItemAnimationTime == 0.0f ? (Emote == m_SelectedEmote ? 1.0f : 0.0f) : QuadEaseInOut(m_aAnimationTimeEmotes[Emote] / ItemAnimationTime);
		const float Size = (50.0f + Phase * 30.0f) * WheelScale * aAnimationPhase[1];
		IGraphics::CQuadItem QuadItem(WheelCenter.x + Offset.x, WheelCenter.y + Offset.y, Size * aAnimationPhase[1], Size * aAnimationPhase[1]);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	Graphics()->WrapNormal();

	if(EyeWheel)
	{
		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_RenderInfo;

		if(LayoutMode == 0)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.3f * aAnimationPhase[2]);
			Graphics()->DrawCircle(WheelCenter.x, WheelCenter.y, 100.0f * WheelScale * aAnimationPhase[2], 64);
			Graphics()->QuadsEnd();
		}
		else
		{
			const float EyeBound = (NUM_EMOTES - 1) * 0.5f * EyeSpacing + 40.0f * WheelScale;
			if(LayoutMode == 1)
				Graphics()->DrawRect(WheelCenter.x - EyeRowBoundX, WheelCenter.y + RowOffset - 38.0f * WheelScale, EyeRowBoundX * 2.0f, 76.0f * WheelScale, ColorRGBA(1.0f, 1.0f, 1.0f, 0.3f * aAnimationPhase[2]), IGraphics::CORNER_ALL, 10.0f * WheelScale);
			else
				Graphics()->DrawRect(WheelCenter.x + RowOffset - 38.0f * WheelScale, WheelCenter.y - EyeBound, 76.0f * WheelScale, EyeBound * 2.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.3f * aAnimationPhase[2]), IGraphics::CORNER_ALL, 10.0f * WheelScale);
		}

		for(int Emote = 0; Emote < NUM_EMOTES; Emote++)
		{
			const vec2 Offset = EyeOffset(Emote) * aAnimationPhase[3];
			const float Phase = ItemAnimationTime == 0.0f ? (Emote == m_SelectedEyeEmote ? 1.0f : 0.0f) : QuadEaseInOut(m_aAnimationTimeEyeEmotes[Emote] / ItemAnimationTime);
			TeeInfo.m_Size = (48.0f + Phase * 18.0f) * WheelScale * aAnimationPhase[3];
			RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, vec2(-1.0f, 0.0f), WheelCenter + Offset, aAnimationPhase[3]);
		}

		if(LayoutMode == 0)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f * aAnimationPhase[4]);
			Graphics()->DrawCircle(WheelCenter.x, WheelCenter.y, 30.0f * WheelScale * aAnimationPhase[4], 64);
			Graphics()->QuadsEnd();
		}
	}
	else
		m_SelectedEyeEmote = -1;

	RenderTools()->RenderCursor(WheelCenter + m_SelectorMouse, 24.0f * WheelScale, aAnimationPhase[0]);
}

CUIRect CEmoticon::GetHudEditorRect() const
{
	const float HudW = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
	const float HudH = HudLayout::CANVAS_HEIGHT;
	const HudLayout::SModuleLayout Layout = HudLayout::Get(HudLayout::MODULE_EMOTE_WHEEL, HudW, HudH);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const int Count = NUM_EMOTICONS;
	const int Mode = std::clamp(g_Config.m_FcEmoteWheelLayout, 0, 2);

	const float EmoteHalf = ((Count - 1) * 0.5f * EMOTE_SPACING + EMOTE_ROW_PAD) * Scale;
	const float EyeHalf = ((NUM_EMOTES - 1) * 0.5f * EYE_SPACING + EYE_ROW_PAD) * Scale;
	const float LineHalf = std::max(EmoteHalf, EyeHalf) + 4.0f * Scale;
	const float LineThickness = 2.0f * (ROW_OFFSET + 48.0f) * Scale;

	float W, H;
	if(Mode == 1)
	{
		W = LineHalf * 2.0f;
		H = LineThickness;
	}
	else if(Mode == 2)
	{
		W = LineThickness;
		H = LineHalf * 2.0f;
	}
	else
	{
		W = H = 380.0f * Scale;
	}

	const CUIRect &Screen = *Ui()->Screen();
	const float Fit = std::min({1.0f, Screen.w * 0.9f / W, Screen.h * 0.7f / H});
	W *= Fit;
	H *= Fit;

	const float UiToHud = HudLayout::CANVAS_HEIGHT / maximum(Screen.h, 1.0f);
	W *= UiToHud;
	H *= UiToHud;
	return {Layout.m_X - W * 0.5f, Layout.m_Y - H * 0.5f, W, H};
}

void CEmoticon::Emote(int Emoticon)
{
	CNetMsg_Cl_Emoticon Msg;
	Msg.m_Emoticon = Emoticon;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker MsgDummy(NETMSGTYPE_CL_EMOTICON, false);
		MsgDummy.AddInt(Emoticon);
		Client()->SendMsg(!g_Config.m_ClDummy, &MsgDummy, MSGFLAG_VITAL);
	}
}

void CEmoticon::EyeEmote(int Emote)
{
	char aBuf[32];
	switch(Emote)
	{
	case EMOTE_NORMAL:
		str_format(aBuf, sizeof(aBuf), "/emote normal %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_PAIN:
		str_format(aBuf, sizeof(aBuf), "/emote pain %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_HAPPY:
		str_format(aBuf, sizeof(aBuf), "/emote happy %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_SURPRISE:
		str_format(aBuf, sizeof(aBuf), "/emote surprise %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_ANGRY:
		str_format(aBuf, sizeof(aBuf), "/emote angry %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_BLINK:
		str_format(aBuf, sizeof(aBuf), "/emote blink %d", g_Config.m_ClEyeDuration);
		break;
	}
	GameClient()->m_Chat.SendChat(0, aBuf);
}
