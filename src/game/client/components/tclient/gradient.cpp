#include "gradient.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

void (*BCGradient_ApplyEverythingHook)(CTextCursor *pCursor, const char *pText, int Length) = nullptr;

namespace
{
static CBcGradient *s_pActiveGradient = nullptr;
static CGameClient *s_pGameClient = nullptr;

static std::vector<STextColorSplit> BuildMultiColorSplits(const char *pText, const std::function<ColorRGBA(float)> &SampleColor)
{
	std::vector<STextColorSplit> vSplits;
	size_t Size, Count;
	str_utf8_stats(pText, str_length(pText) + 1, SIZE_MAX, &Size, &Count);
	if(Count == 0)
		return vSplits;

	vSplits.reserve(Count);
	const char *pStr = pText;
	for(size_t i = 0; i < Count; i++)
	{
		int ByteOffset = (int)(pStr - pText);
		const char *pPrev = pStr;
		str_utf8_decode(&pStr);
		int ByteLen = (int)(pStr - pPrev);
		const float t = Count == 1 ? 0.5f : (float)i / (float)(Count - 1);
		vSplits.emplace_back(ByteOffset, ByteLen, SampleColor(t));
	}
	return vSplits;
}
} // namespace

float CBcGradient::AnimatePhase(double GlobalTime)
{
	return (float)std::fmod(GlobalTime * (g_Config.m_TcNameplateGradientAnimateSpeed / 100.0), 1.0);
}

void CBcGradient::GetSkinToneColors(int ClientId, const CGameClient *pGameClient, ColorRGBA &Body, ColorRGBA &Feet)
{
	Body = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Feet = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;

	const auto &RenderInfo = pGameClient->m_aClients[ClientId].m_RenderInfo;
	if(RenderInfo.m_CustomColoredSkin)
	{
		Body = RenderInfo.m_ColorBody;
		Feet = RenderInfo.m_ColorFeet;
	}
	else
	{
		Body = RenderInfo.m_BloodColor;
		Feet = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CBcGradient::GetAnimatedEndpointColors(int ClientId, const CGameClient *pGameClient, float Phase, ColorRGBA &Color1, ColorRGBA &Color2)
{
	ColorRGBA Body, Feet;
	GetSkinToneColors(ClientId, pGameClient, Body, Feet);
	const float Wave = 0.5f + 0.5f * std::cos(2.0f * pi * Phase);
	Color1 = ColorRGBA(
		Body.r + Wave * (Feet.r - Body.r),
		Body.g + Wave * (Feet.g - Body.g),
		Body.b + Wave * (Feet.b - Body.b),
		1.0f);
	Color2 = Feet;
}

std::vector<STextColorSplit> CBcGradient::BuildAnimatedColorSplits(const char *pText, const ColorRGBA &Color1, const ColorRGBA &Color2, float Phase)
{
	return BuildMultiColorSplits(pText, [&](float t) {
		const float Wave = 0.5f + 0.5f * std::cos(2.0f * pi * (t - Phase));
		return ColorRGBA(
			Color1.r + Wave * (Color2.r - Color1.r),
			Color1.g + Wave * (Color2.g - Color1.g),
			Color1.b + Wave * (Color2.b - Color1.b),
			1.0f);
	});
}

std::vector<STextColorSplit> CBcGradient::BuildAnimatedTextSplits(const char *pText, int ClientId, CGameClient *pGameClient, float Phase)
{
	ColorRGBA Body, Feet;
	GetSkinToneColors(ClientId, pGameClient, Body, Feet);
	return BuildAnimatedColorSplits(pText, Body, Feet, Phase);
}

void CBcGradient::ApplyEverythingGradient(CTextCursor *pCursor, const char *pText, int Length, CGameClient *pGameClient)
{
	if(!g_Config.m_TcNameplateGradientEverything || pCursor == nullptr || pText == nullptr || !pCursor->m_vColorSplits.empty())
		return;

	char aText[512];
	if(Length < 0)
		Length = str_length(pText);
	Length = minimum(Length, (int)sizeof(aText) - 1);
	str_copy(aText, pText, Length + 1);

	const int LocalId = pGameClient->m_Snap.m_LocalClientId;
	const float Phase = AnimatePhase(pGameClient->Client()->GlobalTime());
	const int CharOffset = pCursor->m_CharCount;
	const std::vector<STextColorSplit> vSplits = BuildAnimatedTextSplits(aText, LocalId, pGameClient, Phase);
	pCursor->m_vColorSplits.clear();
	pCursor->m_vColorSplits.reserve(vSplits.size());
	for(const STextColorSplit &Split : vSplits)
		pCursor->m_vColorSplits.emplace_back(CharOffset + Split.m_CharIndex, Split.m_Length, Split.m_Color);
}

static void GradientEverythingHook(CTextCursor *pCursor, const char *pText, int Length)
{
	if(s_pGameClient == nullptr)
		return;
	CBcGradient::ApplyEverythingGradient(pCursor, pText, Length, s_pGameClient);
}

void CBcGradient::OnInit()
{
	s_pActiveGradient = this;
	s_pGameClient = GameClient();
	BCGradient_ApplyEverythingHook = GradientEverythingHook;
}

void CBcGradient::OnShutdown()
{
	if(s_pActiveGradient == this)
	{
		s_pActiveGradient = nullptr;
		s_pGameClient = nullptr;
		BCGradient_ApplyEverythingHook = nullptr;
	}
}

void CBcGradient::RefreshCachedText()
{
	Ui()->OnElementsReset();
	GameClient()->m_Chat.RebuildChat();
	GameClient()->m_NamePlates.ResetNamePlates();
	GameClient()->m_Hud.ResetHudContainers();
	GameClient()->m_InfoMessages.OnWindowResize();
	GameClient()->m_Broadcast.OnWindowResize();
}

void CBcGradient::OnRender()
{
	const int Everything = g_Config.m_TcNameplateGradientEverything;
	const int Nick = g_Config.m_TcNameplateGradient;
	const int Clan = g_Config.m_TcNameplateGradientClan;

	bool NeedRefresh = false;
	if(m_LastEverything < 0)
	{
		// First frame: just seed tracked values, no rebuild.
	}
	else if(Everything != m_LastEverything || Nick != m_LastNick || Clan != m_LastClan)
	{
		NeedRefresh = true;
	}

	m_LastEverything = Everything;
	m_LastNick = Nick;
	m_LastClan = Clan;

	if(NeedRefresh)
		RefreshCachedText();
}
