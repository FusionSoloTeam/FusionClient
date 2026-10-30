#include "death_tracker.h"

#include <base/time.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>

#include <algorithm>

static constexpr float MARKER_SIZE = 5.0f;
static constexpr float MARKER_THICKNESS = 2.2f;
static constexpr float MARKER_OUTLINE = 0.5f;
static constexpr float MARKER_COUNT_FONT_SIZE = 9.0f;
static constexpr float MARKER_COUNT_PADDING_X = 2.0f;
static constexpr float MARKER_COUNT_PADDING_Y = 0.7f;
static constexpr float MARKER_COUNT_GAP = 2.5f;
static constexpr float MARKER_SCALE_MIN = 0.1f;
static constexpr float MARKER_SCALE_MAX = 8.0f;
static constexpr int MARKER_CAP_SEGMENTS = 8;
static constexpr size_t MARKER_DRAW_BATCH = 32;

static void DrawCrossShape(IGraphics *pGraphics, const vec2 Center, float TipDistance, float HalfWidth)
{
	vec2 aRing[4 * (MARKER_CAP_SEGMENTS + 2)];
	int Num = 0;
	for(int Arm = 0; Arm < 4; Arm++)
	{
		const float Angle = pi / 4.0f + Arm * (pi / 2.0f);
		const vec2 Axis(std::cos(Angle), std::sin(Angle));
		const vec2 Side(-Axis.y, Axis.x);
		const vec2 Tip = Center + Axis * TipDistance;
		for(int i = 0; i <= MARKER_CAP_SEGMENTS; i++)
		{
			const float Step = pi - pi * i / MARKER_CAP_SEGMENTS;
			aRing[Num++] = Tip + (Side * std::cos(Step) + Axis * std::sin(Step)) * HalfWidth;
		}
		aRing[Num++] = Center + (Axis + Side) * HalfWidth;
	}

	IGraphics::CFreeformItem aItems[4 * (MARKER_CAP_SEGMENTS + 2) / 2];
	int NumItems = 0;
	for(int i = 0; i < Num; i += 2)
		aItems[NumItems++] = IGraphics::CFreeformItem(Center, aRing[i], aRing[(i + 2) % Num], aRing[i + 1]);
	pGraphics->QuadsDrawFreeform(aItems, NumItems);
}

CDeathTracker::CDeathTracker()
{
	CDeathTracker::Clear();
}

void CDeathTracker::Clear()
{
	m_vMarkers.clear();
	for(int &Count : m_aDeathCount)
		Count = 0;
}

void CDeathTracker::OnReset()
{
	Clear();
}

void CDeathTracker::OnMapLoad()
{
	Clear();
}

void CDeathTracker::AddMarker(int ClientId, vec2 Pos)
{
	if((int)m_vMarkers.size() >= g_Config.m_FcDeathTrackerMax)
		m_vMarkers.erase(m_vMarkers.begin());

	SDeathMarker Marker;
	Marker.m_Pos = Pos;
	Marker.m_Count = ++m_aDeathCount[ClientId];
	Marker.m_Time = time();
	m_vMarkers.push_back(Marker);
}

void CDeathTracker::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType != NETMSGTYPE_SV_KILLMSG || !g_Config.m_FcDeathTracker)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const CNetMsg_Sv_KillMsg *pMsg = static_cast<CNetMsg_Sv_KillMsg *>(pRawMsg);
	const int Victim = pMsg->m_Victim;
	if(Victim < 0 || Victim >= MAX_CLIENTS || !GameClient()->m_Snap.m_aCharacters[Victim].m_Active)
		return;
	if(!g_Config.m_FcDeathTrackerOthers && Victim != GameClient()->m_aLocalIds[0] && Victim != GameClient()->m_aLocalIds[1])
		return;

	AddMarker(Victim, GameClient()->m_aClients[Victim].m_RenderPos);
}

void CDeathTracker::OnRender()
{
	const int64_t Now = time();
	const int64_t Duration = (int64_t)g_Config.m_FcDeathTrackerTime * time_freq();
	for(auto It = m_vMarkers.begin(); It != m_vMarkers.end();)
	{
		if(Now - It->m_Time >= Duration)
			It = m_vMarkers.erase(It);
		else
			++It;
	}
	if(m_vMarkers.empty())
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	auto MarkerAlpha = [&](const SDeathMarker &Marker) {
		const float Elapsed = (float)(Now - Marker.m_Time) / (float)time_freq();
		const float Left = (float)Duration / (float)time_freq() - Elapsed;
		return std::clamp(std::min(Left, Elapsed * 4.0f), 0.0f, 1.0f);
	};

	const float Zoom = std::max(GameClient()->m_Camera.m_RenderZoom, 0.01f);
	const float Scale = std::clamp(Zoom, MARKER_SCALE_MIN, MARKER_SCALE_MAX);
	const float HalfLength = MARKER_SIZE * Scale;
	const float HalfThickness = MARKER_THICKNESS * 0.5f * Scale;
	const float Outline = MARKER_OUTLINE * Scale;
	const float CountFontSize = MARKER_COUNT_FONT_SIZE * Scale;
	const float CountPaddingX = MARKER_COUNT_PADDING_X * Scale;
	const float CountPaddingY = MARKER_COUNT_PADDING_Y * Scale;
	const float CountGap = MARKER_COUNT_GAP * Scale;

	const ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_FcDeathTrackerColor, true));
	const CScreenRect PrevScreen = Graphics()->GetScreen();
	Graphics()->SetScreenRotation(GameClient()->m_Camera.TiltAngle());
	Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, Zoom);

	Graphics()->TextureClear();

	auto DrawLayer = [&](const ColorRGBA &LayerColor, float HalfWidth) {
		for(size_t Start = 0; Start < m_vMarkers.size(); Start += MARKER_DRAW_BATCH)
		{
			const size_t End = std::min(m_vMarkers.size(), Start + MARKER_DRAW_BATCH);
			Graphics()->QuadsBegin();
			for(size_t i = Start; i < End; i++)
			{
				const SDeathMarker &Marker = m_vMarkers[i];
				Graphics()->SetColor(LayerColor.r, LayerColor.g, LayerColor.b, LayerColor.a * MarkerAlpha(Marker));
				DrawCrossShape(Graphics(), Marker.m_Pos, HalfLength, HalfWidth);
			}
			Graphics()->QuadsEnd();
		}
	};

	DrawLayer(ColorRGBA(0.0f, 0.0f, 0.0f, 0.55f), HalfThickness + Outline);
	DrawLayer(Color, HalfThickness);

	for(const SDeathMarker &Marker : m_vMarkers)
	{
		const float Alpha = MarkerAlpha(Marker);
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%d", Marker.m_Count);

		const STextBoundingBox TextBox = TextRender()->TextBoundingBox(CountFontSize, aBuf);
		const float BadgeWidth = TextBox.m_W + CountPaddingX * 2.0f;
		const float BadgeHeight = TextBox.m_H + CountPaddingY * 2.0f;
		const float BadgeX = Marker.m_Pos.x + HalfLength + CountGap;
		const float BadgeY = Marker.m_Pos.y - BadgeHeight * 0.5f;

		Graphics()->DrawRect(BadgeX - Outline * 0.5f, BadgeY - Outline * 0.5f, BadgeWidth + Outline, BadgeHeight + Outline, ColorRGBA(Color.r, Color.g, Color.b, 0.55f * Alpha), IGraphics::CORNER_ALL, (BadgeHeight + Outline) * 0.35f);
		Graphics()->DrawRect(BadgeX, BadgeY, BadgeWidth, BadgeHeight, ColorRGBA(0.04f, 0.07f, 0.09f, 0.8f * Alpha), IGraphics::CORNER_ALL, BadgeHeight * 0.35f);

		TextRender()->TextColor(Color.r + (1.0f - Color.r) * 0.35f, Color.g + (1.0f - Color.g) * 0.35f, Color.b + (1.0f - Color.b) * 0.35f, Alpha);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.4f * Alpha);
		TextRender()->Text(BadgeX + CountPaddingX - TextBox.m_X, BadgeY + CountPaddingY - TextBox.m_Y, CountFontSize, aBuf, -1.0f);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->MapScreen(PrevScreen);
	Graphics()->SetScreenRotation(0.0f);
}
