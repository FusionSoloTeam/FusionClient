/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "hud.h"

#include <algorithm>
#include <queue>
#include <vector>

#include "binds.h"
#include "camera.h"
#include "controls.h"
#include "voting.h"

#include <base/color.h>
#include <base/time.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/hud_layout.h>
#include <game/client/components/tclient/gradient.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/layers.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>

void CHud::HudGradientText(float x, float y, float Size, const char *pText, float LineWidth) const
{
	CTextCursor Cursor;
	Cursor.SetPosition(vec2(x, y));
	Cursor.m_FontSize = Size;
	Cursor.m_LineWidth = LineWidth;
	if(BCGradient_ApplyEverythingHook)
		BCGradient_ApplyEverythingHook(&Cursor, pText, -1);
	TextRender()->TextEx(&Cursor, pText, -1);
}

CHud::CHud()
{
	m_FPSTextContainerIndex.Reset();
	m_DDRaceEffectsTextContainerIndex.Reset();
	m_PlayerAngleTextContainerIndex.Reset();
	m_PlayerPrevAngle = -INFINITY;

	for(int i = 0; i < 2; i++)
	{
		m_aPlayerSpeedTextContainers[i].Reset();
		m_aPlayerPrevSpeed[i] = -INFINITY;
		m_aPlayerPositionContainers[i].Reset();
		m_aPlayerPrevPosition[i] = -INFINITY;
	}
}

void CHud::ResetHudContainers()
{
	for(auto &ScoreInfo : m_aScoreInfo)
	{
		TextRender()->DeleteTextContainer(ScoreInfo.m_OptionalNameTextContainerIndex);
		TextRender()->DeleteTextContainer(ScoreInfo.m_TextRankContainerIndex);
		TextRender()->DeleteTextContainer(ScoreInfo.m_TextScoreContainerIndex);
		Graphics()->DeleteQuadContainer(ScoreInfo.m_RoundRectQuadContainerIndex);

		ScoreInfo.Reset();
	}

	TextRender()->DeleteTextContainer(m_FPSTextContainerIndex);
	TextRender()->DeleteTextContainer(m_DDRaceEffectsTextContainerIndex);
	TextRender()->DeleteTextContainer(m_PlayerAngleTextContainerIndex);
	m_PlayerPrevAngle = -INFINITY;
	for(int i = 0; i < 2; i++)
	{
		TextRender()->DeleteTextContainer(m_aPlayerSpeedTextContainers[i]);
		m_aPlayerPrevSpeed[i] = -INFINITY;
		TextRender()->DeleteTextContainer(m_aPlayerPositionContainers[i]);
		m_aPlayerPrevPosition[i] = -INFINITY;
	}
}

void CHud::OnWindowResize()
{
	ResetHudContainers();
}

void CHud::OnReset()
{
	m_FinishPredictionDataDirty = true;
	m_FinishPredictionWindowTime = -1.0f;
	m_FinishPredictionWindowDistance = -1.0f;
	m_FinishPredictionMeasuredSpeed = -1.0f;
	m_FinishPredictionLastRemaining = -1.0f;
	m_FinishPredictionLastRaceTime = -1.0f;
	m_TimeCpDiff = 0.0f;
	m_DDRaceTime = 0;
	m_FinishTimeLastReceivedTick = 0;
	m_TimeCpLastReceivedTick = 0;
	m_ShowFinishTime = false;
	m_aPlayerRecord[0] = -1.0f;
	m_aPlayerRecord[1] = -1.0f;
	m_aPlayerSpeed[0] = 0;
	m_aPlayerSpeed[1] = 0;
	m_aLastPlayerSpeedChange[0] = ESpeedChange::NONE;
	m_aLastPlayerSpeedChange[1] = ESpeedChange::NONE;
	m_LastSpectatorCountTick = 0;

	ResetHudContainers();
}

void CHud::OnInit()
{
	OnReset();

	Graphics()->SetColor(1.0, 1.0, 1.0, 1.0);

	m_HudQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	PrepareAmmoHealthAndArmorQuads();

	// all cursors for the different weapons
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteCursor, ScaleX, ScaleY);
		m_aCursorOffset[i] = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	}

	// the flags
	m_FlagOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 8.f, 16.f);

	PreparePlayerStateQuads();

	Graphics()->QuadContainerUpload(m_HudQuadContainerIndex);
}

void CHud::RenderGameTimer()
{
	float Half = m_Width / 2.0f;

	if(!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH))
	{
		char aBuf[32];
		int Time = 0;
		if(GameClient()->m_Snap.m_pGameInfoObj->m_TimeLimit && (GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
		{
			Time = GameClient()->m_Snap.m_pGameInfoObj->m_TimeLimit * 60 - ((Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed());

			if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
				Time = 0;
		}
		else if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
		{
			// The Warmup timer is negative in this case to make sure that incompatible clients will not see a warmup timer
			Time = (Client()->GameTick(g_Config.m_ClDummy) + GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer) / Client()->GameTickSpeed();
		}
		else
			Time = (Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed();

		str_time((int64_t)Time * 100, ETimeFormat::DAYS, aBuf, sizeof(aBuf));
		float FontSize = 10.0f;
		static float s_TextWidthM = TextRender()->TextWidth(FontSize, "00:00", -1, -1.0f);
		static float s_TextWidthH = TextRender()->TextWidth(FontSize, "00:00:00", -1, -1.0f);
		static float s_TextWidth0D = TextRender()->TextWidth(FontSize, "0d 00:00:00", -1, -1.0f);
		static float s_TextWidth00D = TextRender()->TextWidth(FontSize, "00d 00:00:00", -1, -1.0f);
		static float s_TextWidth000D = TextRender()->TextWidth(FontSize, "000d 00:00:00", -1, -1.0f);
		float w = Time >= 3600 * 24 * 100 ? s_TextWidth000D : (Time >= 3600 * 24 * 10 ? s_TextWidth00D : (Time >= 3600 * 24 ? s_TextWidth0D : (Time >= 3600 ? s_TextWidthH : s_TextWidthM)));
		// last 60 sec red, last 10 sec blink
		if(GameClient()->m_Snap.m_pGameInfoObj->m_TimeLimit && Time <= 60 && (GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
		{
			float Alpha = Time <= 10 && (2 * time() / time_freq()) % 2 ? 0.5f : 1.0f;
			TextRender()->TextColor(1.0f, 0.25f, 0.25f, Alpha);
		}
		const auto TimerLayout = HudLayout::Get(HudLayout::MODULE_GAME_TIMER, m_Width, m_Height);
		const float TimerScale = std::clamp(TimerLayout.m_Scale / 100.0f, 0.25f, 3.0f);
		Graphics()->MapScreen(Half - TimerLayout.m_X / TimerScale, 2.0f - TimerLayout.m_Y / TimerScale, Half - TimerLayout.m_X / TimerScale + m_Width / TimerScale, 2.0f - TimerLayout.m_Y / TimerScale + m_Height / TimerScale);
		HudGradientText(Half - w / 2, 2, FontSize, aBuf, -1.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
	}
}

void CHud::RenderPauseNotification()
{
	if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED &&
		!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
	{
		const char *pText = Localize("Game paused");
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(FontSize, pText, -1, -1.0f);
		TextRender()->Text(150.0f * Graphics()->ScreenAspect() + -w / 2.0f, 50.0f, FontSize, pText, -1.0f);
	}
}

void CHud::RenderSuddenDeath()
{
	if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH)
	{
		float Half = m_Width / 2.0f;
		const char *pText = Localize("Sudden Death");
		float FontSize = 12.0f;
		float w = TextRender()->TextWidth(FontSize, pText, -1, -1.0f);
		TextRender()->Text(Half - w / 2, 2, FontSize, pText, -1.0f);
	}
}

void CHud::RenderScoreHud()
{
	// render small score hud
	if(!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
	{
		const auto ScoreLayout = HudLayout::Get(HudLayout::MODULE_SCORE, m_Width, m_Height);
		const float ScoreScale = std::clamp(ScoreLayout.m_Scale / 100.0f, 0.25f, 3.0f);
		const float ScoreRight = ScoreLayout.m_X + 100.0f * ScoreScale;
		Graphics()->MapScreen(m_Width - ScoreRight / ScoreScale, 229.0f - ScoreLayout.m_Y / ScoreScale, m_Width - ScoreRight / ScoreScale + m_Width / ScoreScale, 229.0f - ScoreLayout.m_Y / ScoreScale + m_Height / ScoreScale);

		float StartY = 229.0f; // the height of this display is 56, so EndY is 285

		const float ScoreSingleBoxHeight = 18.0f;

		bool ForceScoreInfoInit = !m_aScoreInfo[0].m_Initialized || !m_aScoreInfo[1].m_Initialized;
		m_aScoreInfo[0].m_Initialized = m_aScoreInfo[1].m_Initialized = true;

		if(GameClient()->IsTeamPlay() && GameClient()->m_Snap.m_pGameDataObj)
		{
			char aScoreTeam[2][16];
			str_format(aScoreTeam[TEAM_RED], sizeof(aScoreTeam[TEAM_RED]), "%d", GameClient()->m_Snap.m_pGameDataObj->m_TeamscoreRed);
			str_format(aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam[TEAM_BLUE]), "%d", GameClient()->m_Snap.m_pGameDataObj->m_TeamscoreBlue);

			bool aRecreateTeamScore[2] = {str_comp(aScoreTeam[0], m_aScoreInfo[0].m_aScoreText) != 0, str_comp(aScoreTeam[1], m_aScoreInfo[1].m_aScoreText) != 0};

			const int aFlagCarrier[2] = {
				GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierRed,
				GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue};

			bool RecreateRect = ForceScoreInfoInit;
			for(int t = 0; t < 2; t++)
			{
				if(aRecreateTeamScore[t])
				{
					m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(14.0f, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE], -1, -1.0f);
					str_copy(m_aScoreInfo[t].m_aScoreText, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE]);
					RecreateRect = true;
				}
			}

			static float s_TextWidth100 = TextRender()->TextWidth(14.0f, "100", -1, -1.0f);
			float ScoreWidthMax = maximum(maximum(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_TextWidth100);
			float Split = 3.0f;
			float ImageSize = (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) ? 16.0f : Split;
			for(int t = 0; t < 2; t++)
			{
				const int ScoreCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, ScoreLayout.m_X + (100.0f - (ScoreWidthMax + ImageSize + 2 * Split)) * ScoreScale, ScoreLayout.m_Y + t * 20.0f * ScoreScale, (ScoreWidthMax + ImageSize + 2 * Split) * ScoreScale, ScoreSingleBoxHeight * ScoreScale, m_Width, m_Height);
				// draw box
				if(RecreateRect || m_aScoreInfo[t].m_LastCorners != ScoreCorners)
				{
					m_aScoreInfo[t].m_LastCorners = ScoreCorners;
					Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);

					if(t == 0)
						Graphics()->SetColor(0.975f, 0.17f, 0.17f, 0.3f);
					else
						Graphics()->SetColor(0.17f, 0.46f, 0.975f, 0.3f);
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWidthMax - ImageSize - 2 * Split, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split, ScoreSingleBoxHeight, 5.0f, ScoreCorners);
				}
				Graphics()->TextureClear();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
					Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

				// draw score
				if(aRecreateTeamScore[t])
				{
					CTextCursor Cursor;
					Cursor.SetPosition(vec2(m_Width - ScoreWidthMax + (ScoreWidthMax - m_aScoreInfo[t].m_ScoreTextWidth) / 2 - Split, StartY + t * 20 + (18.f - 14.f) / 2.f));
					Cursor.m_FontSize = 14.0f;
					TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &Cursor, aScoreTeam[t]);
				}
				if(m_aScoreInfo[t].m_TextScoreContainerIndex.Valid())
				{
					ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
					ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, TColor, TOutlineColor);
				}

				if(GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
				{
					int BlinkTimer = (GameClient()->m_aFlagDropTick[t] != 0 &&
								 (Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_aFlagDropTick[t]) / Client()->GameTickSpeed() >= 25) ?
								 10 :
								 20;
					if(aFlagCarrier[t] == FLAG_ATSTAND || (aFlagCarrier[t] == FLAG_TAKEN && ((Client()->GameTick(g_Config.m_ClDummy) / BlinkTimer) & 1)))
					{
						// draw flag
						Graphics()->TextureSet(t == 0 ? GameClient()->m_GameSkin.m_SpriteFlagRed : GameClient()->m_GameSkin.m_SpriteFlagBlue);
						Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
						Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_FlagOffset, m_Width - ScoreWidthMax - ImageSize, StartY + 1.0f + t * 20);
					}
					else if(aFlagCarrier[t] >= 0)
					{
						// draw name of the flag holder
						int Id = aFlagCarrier[t] % MAX_CLIENTS;
						const char *pName = GameClient()->m_aClients[Id].m_aName;
						if(str_comp(pName, m_aScoreInfo[t].m_aPlayerNameText) != 0 || RecreateRect)
						{
							str_copy(m_aScoreInfo[t].m_aPlayerNameText, pName);

							float w = TextRender()->TextWidth(8.0f, pName, -1, -1.0f);

							CTextCursor Cursor;
							Cursor.SetPosition(vec2(minimum(m_Width - w - 1.0f, m_Width - ScoreWidthMax - ImageSize - 2 * Split), StartY + (t + 1) * 20.0f - 2.0f));
							Cursor.m_FontSize = 8.0f;
							TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &Cursor, pName);
						}

						if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex.Valid())
						{
							ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
							ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
							TextRender()->RenderTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, TColor, TOutlineColor);
						}

						// draw tee of the flag holder
						CTeeRenderInfo TeeInfo = GameClient()->m_aClients[Id].m_RenderInfo;
						TeeInfo.m_Size = ScoreSingleBoxHeight;

						const CAnimState *pIdleState = CAnimState::GetIdle();
						vec2 OffsetToMid;
						CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
						vec2 TeeRenderPos(m_Width - ScoreWidthMax - TeeInfo.m_Size / 2 - Split, StartY + (t * 20) + ScoreSingleBoxHeight / 2.0f + OffsetToMid.y);

						RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
					}
				}
				StartY += 8.0f;
			}
		}
		else
		{
			int Local = -1;
			int aPos[2] = {1, 2};
			const CNetObj_PlayerInfo *apPlayerInfo[2] = {nullptr, nullptr};
			int i = 0;
			for(int t = 0; t < 2 && i < MAX_CLIENTS && GameClient()->m_Snap.m_apInfoByScore[i]; ++i)
			{
				if(GameClient()->m_Snap.m_apInfoByScore[i]->m_Team != TEAM_SPECTATORS)
				{
					apPlayerInfo[t] = GameClient()->m_Snap.m_apInfoByScore[i];
					if(apPlayerInfo[t]->m_ClientId == GameClient()->m_Snap.m_LocalClientId)
						Local = t;
					++t;
				}
			}
			// search local player info if not a spectator, nor within top2 scores
			if(Local == -1 && GameClient()->m_Snap.m_pLocalInfo && GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
			{
				for(; i < MAX_CLIENTS && GameClient()->m_Snap.m_apInfoByScore[i]; ++i)
				{
					if(GameClient()->m_Snap.m_apInfoByScore[i]->m_Team != TEAM_SPECTATORS)
						++aPos[1];
					if(GameClient()->m_Snap.m_apInfoByScore[i]->m_ClientId == GameClient()->m_Snap.m_LocalClientId)
					{
						apPlayerInfo[1] = GameClient()->m_Snap.m_apInfoByScore[i];
						Local = 1;
						break;
					}
				}
			}
			char aScore[2][16];
			for(int t = 0; t < 2; ++t)
			{
				if(apPlayerInfo[t])
				{
					if(Client()->IsSixup() && GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE)
						str_time((int64_t)absolute(apPlayerInfo[t]->m_Score) / 10, ETimeFormat::MINS_CENTISECS, aScore[t], sizeof(aScore[t]));
					else if(GameClient()->m_GameInfo.m_TimeScore)
					{
						CGameClient::CClientData &ClientData = GameClient()->m_aClients[apPlayerInfo[t]->m_ClientId];
						if(GameClient()->m_ReceivedDDNetPlayerFinishTimes && ClientData.m_FinishTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
						{
							int64_t TimeSeconds = static_cast<int64_t>(absolute(ClientData.m_FinishTimeSeconds));
							int64_t TimeMillis = TimeSeconds * 1000 + (absolute(ClientData.m_FinishTimeMillis) % 1000);

							str_time(TimeMillis / 10, ETimeFormat::HOURS, aScore[t], sizeof(aScore[t]));
						}
						else if(apPlayerInfo[t]->m_Score != FinishTime::NOT_FINISHED_TIMESCORE)
						{
							str_time((int64_t)absolute(apPlayerInfo[t]->m_Score) * 100, ETimeFormat::HOURS, aScore[t], sizeof(aScore[t]));
						}
						else
							aScore[t][0] = 0;
					}
					else
						str_format(aScore[t], sizeof(aScore[t]), "%d", apPlayerInfo[t]->m_Score);
				}
				else
					aScore[t][0] = 0;
			}

			bool RecreateScores = str_comp(aScore[0], m_aScoreInfo[0].m_aScoreText) != 0 || str_comp(aScore[1], m_aScoreInfo[1].m_aScoreText) != 0 || m_LastLocalClientId != GameClient()->m_Snap.m_LocalClientId;
			m_LastLocalClientId = GameClient()->m_Snap.m_LocalClientId;

			bool RecreateRect = ForceScoreInfoInit;
			for(int t = 0; t < 2; t++)
			{
				if(RecreateScores)
				{
					m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(14.0f, aScore[t], -1, -1.0f);
					str_copy(m_aScoreInfo[t].m_aScoreText, aScore[t]);
					RecreateRect = true;
				}

				if(apPlayerInfo[t])
				{
					int Id = apPlayerInfo[t]->m_ClientId;
					if(Id >= 0 && Id < MAX_CLIENTS)
					{
						const char *pName = GameClient()->m_aClients[Id].m_aName;
						if(str_comp(pName, m_aScoreInfo[t].m_aPlayerNameText) != 0)
							RecreateRect = true;
					}
				}
				else
				{
					if(m_aScoreInfo[t].m_aPlayerNameText[0] != 0)
						RecreateRect = true;
				}

				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				if(str_comp(aBuf, m_aScoreInfo[t].m_aRankText) != 0)
					RecreateRect = true;
			}

			static float s_TextWidth10 = TextRender()->TextWidth(14.0f, "10", -1, -1.0f);
			float ScoreWidthMax = maximum(maximum(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_TextWidth10);
			float Split = 3.0f, ImageSize = 16.0f, PosSize = 16.0f;

			for(int t = 0; t < 2; t++)
			{
				const int ScoreCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, ScoreLayout.m_X + (100.0f - (ScoreWidthMax + ImageSize + 2 * Split + PosSize)) * ScoreScale, ScoreLayout.m_Y + t * 20.0f * ScoreScale, (ScoreWidthMax + ImageSize + 2 * Split + PosSize) * ScoreScale, ScoreSingleBoxHeight * ScoreScale, m_Width, m_Height);
				// draw box
				if(RecreateRect || m_aScoreInfo[t].m_LastCorners != ScoreCorners)
				{
					m_aScoreInfo[t].m_LastCorners = ScoreCorners;
					Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);

					if(t == Local)
						Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
					else
						Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.25f);
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWidthMax - ImageSize - 2 * Split - PosSize, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split + PosSize, ScoreSingleBoxHeight, 5.0f, ScoreCorners);
				}
				Graphics()->TextureClear();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
					Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

				if(RecreateScores)
				{
					CTextCursor Cursor;
					Cursor.SetPosition(vec2(m_Width - ScoreWidthMax + (ScoreWidthMax - m_aScoreInfo[t].m_ScoreTextWidth) - Split, StartY + t * 20 + (18.f - 14.f) / 2.f));
					Cursor.m_FontSize = 14.0f;
					TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &Cursor, aScore[t]);
				}
				// draw score
				if(m_aScoreInfo[t].m_TextScoreContainerIndex.Valid())
				{
					ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
					ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, TColor, TOutlineColor);
				}

				if(apPlayerInfo[t])
				{
					// draw name
					int Id = apPlayerInfo[t]->m_ClientId;
					if(Id >= 0 && Id < MAX_CLIENTS)
					{
						const char *pName = GameClient()->m_aClients[Id].m_aName;
						if(RecreateRect)
						{
							str_copy(m_aScoreInfo[t].m_aPlayerNameText, pName);

							CTextCursor Cursor;
							Cursor.SetPosition(vec2(minimum(m_Width - TextRender()->TextWidth(8.0f, pName) - 1.0f, m_Width - ScoreWidthMax - ImageSize - 2 * Split - PosSize), StartY + (t + 1) * 20.0f - 2.0f));
							Cursor.m_FontSize = 8.0f;
							TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &Cursor, pName);
						}

						if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex.Valid())
						{
							ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
							ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
							TextRender()->RenderTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, TColor, TOutlineColor);
						}

						// draw tee
						CTeeRenderInfo TeeInfo = GameClient()->m_aClients[Id].m_RenderInfo;
						TeeInfo.m_Size = ScoreSingleBoxHeight;

						const CAnimState *pIdleState = CAnimState::GetIdle();
						vec2 OffsetToMid;
						CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
						vec2 TeeRenderPos(m_Width - ScoreWidthMax - TeeInfo.m_Size / 2 - Split, StartY + (t * 20) + ScoreSingleBoxHeight / 2.0f + OffsetToMid.y);

						RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
					}
				}
				else
				{
					m_aScoreInfo[t].m_aPlayerNameText[0] = 0;
				}

				// draw position
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				if(RecreateRect)
				{
					str_copy(m_aScoreInfo[t].m_aRankText, aBuf);

					CTextCursor Cursor;
					Cursor.SetPosition(vec2(m_Width - ScoreWidthMax - ImageSize - Split - PosSize, StartY + t * 20 + (18.f - 10.f) / 2.f));
					Cursor.m_FontSize = 10.0f;
					TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextRankContainerIndex, &Cursor, aBuf);
				}
				if(m_aScoreInfo[t].m_TextRankContainerIndex.Valid())
				{
					ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
					ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextRankContainerIndex, TColor, TOutlineColor);
				}

				StartY += 8.0f;
			}
		}
	}

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
}

void CHud::RenderWarmupTimer()
{
	// render warmup timer
	if(GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer > 0 && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME))
	{
		char aBuf[256];
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(FontSize, Localize("Warmup"), -1, -1.0f);
		TextRender()->Text(150 * Graphics()->ScreenAspect() + -w / 2, 50, FontSize, Localize("Warmup"), -1.0f);

		int Seconds = GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer / Client()->GameTickSpeed();
		if(Seconds < 5)
			str_format(aBuf, sizeof(aBuf), "%d.%d", Seconds, (GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer * 10 / Client()->GameTickSpeed()) % 10);
		else
			str_format(aBuf, sizeof(aBuf), "%d", Seconds);
		w = TextRender()->TextWidth(FontSize, aBuf, -1, -1.0f);
		TextRender()->Text(150 * Graphics()->ScreenAspect() + -w / 2, 75, FontSize, aBuf, -1.0f);
	}
}

void CHud::RenderTextInfo()
{
	int Showfps = g_Config.m_ClShowfps;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		Showfps = 0;
#endif
	const auto FpsPingLayout = HudLayout::Get(HudLayout::MODULE_FPS_PING, m_Width, m_Height);
	const float FpsPingScale = std::clamp(FpsPingLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	Graphics()->MapScreen((m_Width - 10.0f) - FpsPingLayout.m_X / FpsPingScale, 5.0f - FpsPingLayout.m_Y / FpsPingScale, (m_Width - 10.0f) - FpsPingLayout.m_X / FpsPingScale + m_Width / FpsPingScale, 5.0f - FpsPingLayout.m_Y / FpsPingScale + m_Height / FpsPingScale);

	if(Showfps)
	{
		char aBuf[16];
		const int FramesPerSecond = round_to_int(1.0f / Client()->FrameTimeAverage());
		str_format(aBuf, sizeof(aBuf), "%d", FramesPerSecond);

		static float s_TextWidth0 = TextRender()->TextWidth(12.f, "0", -1, -1.0f);
		static float s_TextWidth00 = TextRender()->TextWidth(12.f, "00", -1, -1.0f);
		static float s_TextWidth000 = TextRender()->TextWidth(12.f, "000", -1, -1.0f);
		static float s_TextWidth0000 = TextRender()->TextWidth(12.f, "0000", -1, -1.0f);
		static float s_TextWidth00000 = TextRender()->TextWidth(12.f, "00000", -1, -1.0f);
		static const float s_aTextWidth[5] = {s_TextWidth0, s_TextWidth00, s_TextWidth000, s_TextWidth0000, s_TextWidth00000};

		int DigitIndex = GetDigitsIndex(FramesPerSecond, 4);

		CTextCursor Cursor;
		Cursor.SetPosition(vec2(m_Width - 10 - s_aTextWidth[DigitIndex], 5));
		Cursor.m_FontSize = 12.0f;
		auto OldFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(OldFlags | TEXT_RENDER_FLAG_ONE_TIME_USE);
		if(m_FPSTextContainerIndex.Valid())
			TextRender()->RecreateTextContainerSoft(m_FPSTextContainerIndex, &Cursor, aBuf);
		else
			TextRender()->CreateTextContainer(m_FPSTextContainerIndex, &Cursor, "0");
		TextRender()->SetRenderFlags(OldFlags);
		if(m_FPSTextContainerIndex.Valid())
		{
			TextRender()->RenderTextContainer(m_FPSTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor());
		}
	}
	if(g_Config.m_ClShowpred && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		int pred = Client()->GetPredictionTime();
		int tick = Client()->GameTick(g_Config.m_ClDummy);
		int length = SERVER_TICK_SPEED * g_Config.m_TcShowpredLength / 10;
		if(m_LastPredTick == 0 || tick - m_LastPredTick >= length || m_LastPredTick > tick || length > m_LastPredLength)
		{
			for(int i = 0; i < length; i++)
				m_LastPreds[i] = pred;
			m_LastPredTick = tick;
			m_LastPredLength = length;
		}
		int avgPred = 0, maxPred = 0;
		int hasBad = pred < 0;
		for(int i = 0; i < length; i++)
		{
			avgPred += m_LastPreds[i];
			if(m_LastPreds[i] > maxPred)
				maxPred = m_LastPreds[i];
			if(m_LastPreds[i] < 0)
				hasBad = 1;
		}
		int extremePred = 2 * pred - m_LastPreds[m_LastPredTick % length];
		int predDiff = (g_Config.m_TcShowpredExtremePred ? extremePred : pred) * length - avgPred;
		while(m_LastPredTick < tick)
			m_LastPreds[(m_LastPredTick++) % length] = pred;
		char aBuf[64];
		if(g_Config.m_TcShowpredShowPeak)
		{
			if(hasBad)
				str_format(aBuf, sizeof(aBuf), "%d (DEAD)", pred);
			else
				str_format(aBuf, sizeof(aBuf), "%d (%d)", pred, maxPred);
		}
		else
			str_format(aBuf, sizeof(aBuf), "%d", pred);
		if(g_Config.m_TcShowpredColor)
		{
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(hasBad ? 0.0f : (1.0f - (float)std::clamp(predDiff < 0 ? -predDiff / 5 : predDiff, 0, length * g_Config.m_TcShowpredWorstDiff) / (float)(length * g_Config.m_TcShowpredWorstDiff)) * 0.3f, 1.0f, 0.5f, 1.0f)));
		}
		TextRender()->Text(m_Width - 10 - TextRender()->TextWidth(12, aBuf, -1, -1.0f), Showfps ? 20 : 5, 12, aBuf, -1.0f);
		if(g_Config.m_TcShowpredColor)
			TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);

	if(g_Config.m_TcMiniDebug)
	{
		float FontSize = 8.0f;
		float TextHeight = 11.0f;
		char aBuf[64];
		float OffsetY = 3.0f;

		int PlayerId = GameClient()->m_Snap.m_LocalClientId;
		if(GameClient()->m_Snap.m_SpecInfo.m_Active)
			PlayerId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;

		if(g_Config.m_ClShowhudDDRace && GameClient()->m_Snap.m_aCharacters[PlayerId].m_HasExtendedData && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
			OffsetY += 50.0f;
		else if(g_Config.m_ClShowhudHealthAmmo && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
			OffsetY += 27.0f;

		vec2 Pos;
		if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW)
			Pos = vec2(GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].x, GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].y);
		else
			Pos = GameClient()->m_aClients[PlayerId].m_RenderPos;

		str_format(aBuf, sizeof(aBuf), "X: %.2f", Pos.x / 32.0f);
		TextRender()->Text(4, OffsetY, FontSize, aBuf, -1.0f);

		OffsetY += TextHeight;
		str_format(aBuf, sizeof(aBuf), "Y: %.2f", Pos.y / 32.0f);
		TextRender()->Text(4, OffsetY, FontSize, aBuf, -1.0f);
		if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
		{
			OffsetY += TextHeight;
			str_format(aBuf, sizeof(aBuf), "Angle: %d", GameClient()->m_aClients[PlayerId].m_RenderCur.m_Angle);
			TextRender()->Text(4.0f, OffsetY, FontSize, aBuf, -1.0f);

			OffsetY += TextHeight;
			str_format(aBuf, sizeof(aBuf), "VelY: %.2f", GameClient()->m_Snap.m_aCharacters[PlayerId].m_Cur.m_VelY / 256.0f * 50.0f / 32.0f);
			TextRender()->Text(4.0f, OffsetY, FontSize, aBuf, -1.0f);

			OffsetY += TextHeight;

			str_format(aBuf, sizeof(aBuf), "VelX: %.2f", GameClient()->m_Snap.m_aCharacters[PlayerId].m_Cur.m_VelX / 256.0f * 50.0f / 32.0f);
			TextRender()->Text(4.0f, OffsetY, FontSize, aBuf, -1.0f);
		}
	}
	if(g_Config.m_TcRenderCursorSpec && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW)
	{
		int CurWeapon = 1;
		Graphics()->SetColor(1.f, 1.f, 1.f, g_Config.m_TcRenderCursorSpecAlpha / 100.0f);
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponCursors[CurWeapon]);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aCursorOffset[CurWeapon], m_Width / 2.0f, m_Height / 2.0f, 0.36f, 0.36f);
	}
	// render team in freeze text and last notify
	if((g_Config.m_TcShowFrozenText > 0 || g_Config.m_TcShowFrozenHud > 0 || g_Config.m_TcNotifyWhenLast) && GameClient()->m_GameInfo.m_EntitiesDDRace)
	{
		int NumInTeam = 0;
		int NumFrozen = 0;
		int LocalTeamID = 0;
		if(GameClient()->m_Snap.m_LocalClientId >= 0 && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId >= 0)
		{
			if(GameClient()->m_Snap.m_SpecInfo.m_Active == 1 && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != -1)
				LocalTeamID = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId);
			else
				LocalTeamID = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId);
		}
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameClient()->m_Snap.m_apPlayerInfos[i])
				continue;

			if(GameClient()->m_Teams.Team(i) == LocalTeamID)
			{
				NumInTeam++;
				if(GameClient()->m_aClients[i].m_FreezeEnd > 0 || GameClient()->m_aClients[i].m_DeepFrozen)
					NumFrozen++;
			}
		}

		// Notify when last
		if(g_Config.m_TcNotifyWhenLast)
		{
			if(NumInTeam > 1 && NumInTeam - NumFrozen == 1)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcNotifyWhenLastColor)));
				float FontSize = g_Config.m_TcNotifyWhenLastSize;
				float XPos = std::clamp((g_Config.m_TcNotifyWhenLastX / 100.0f) * m_Width, 1.0f, m_Width - FontSize);
				float YPos = std::clamp((g_Config.m_TcNotifyWhenLastY / 100.0f) * m_Height, 1.0f, m_Height - FontSize);

				TextRender()->Text(XPos, YPos, FontSize, g_Config.m_TcNotifyWhenLastText, -1.0f);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
		}
		// Show freeze text
		char aBuf[64];
		if(g_Config.m_TcShowFrozenText == 1)
			str_format(aBuf, sizeof(aBuf), "%d / %d", NumInTeam - NumFrozen, NumInTeam);
		else if(g_Config.m_TcShowFrozenText == 2)
			str_format(aBuf, sizeof(aBuf), "%d / %d", NumFrozen, NumInTeam);
		if(g_Config.m_TcShowFrozenText > 0)
		{
			const auto FrozenTextLayout = HudLayout::Get(HudLayout::MODULE_FROZEN_TEXT, m_Width, m_Height);
			const float FrozenTextScale = std::clamp(FrozenTextLayout.m_Scale / 100.0f, 0.25f, 3.0f);
			Graphics()->MapScreen(m_Width / 2.0f - FrozenTextLayout.m_X / FrozenTextScale, 12.0f - FrozenTextLayout.m_Y / FrozenTextScale, m_Width / 2.0f - FrozenTextLayout.m_X / FrozenTextScale + m_Width / FrozenTextScale, 12.0f - FrozenTextLayout.m_Y / FrozenTextScale + m_Height / FrozenTextScale);
			TextRender()->Text(m_Width / 2.0f - TextRender()->TextWidth(10.0f, aBuf) / 2.0f, 12.0f, 10.0f, aBuf);
			Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
		}

		// str_format(aBuf, sizeof(aBuf), "%d", GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_PrevPredicted.m_FreezeEnd);
		// str_format(aBuf, sizeof(aBuf), "%d", g_Config.m_ClWhatsMyPing);
		// TextRender()->Text(0, m_Width / 2 - TextRender()->TextWidth(0, 10, aBuf, -1, -1.0f) / 2, 20, 10, aBuf, -1.0f);

		if(g_Config.m_TcShowFrozenHud > 0 && !GameClient()->m_Scoreboard.IsActive() && !(LocalTeamID == 0 && g_Config.m_TcFrozenHudTeamOnly))
		{
			CTeeRenderInfo FreezeInfo;
			const CSkin *pSkin = GameClient()->m_Skins.Find("x_ninja");
			FreezeInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
			FreezeInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
			FreezeInfo.m_BloodColor = pSkin->m_BloodColor;
			FreezeInfo.m_SkinMetrics = pSkin->m_Metrics;
			FreezeInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
			FreezeInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
			FreezeInfo.m_CustomColoredSkin = false;

			float ProgressiveOffset = 0.0f;
			float TeeSize = g_Config.m_TcFrozenHudTeeSize;
			int MaxTees = (int)(8.3f * (m_Width / m_Height) * 13.0f / TeeSize);
			if(!g_Config.m_ClShowfps && !g_Config.m_ClShowpred)
				MaxTees = (int)(9.5f * (m_Width / m_Height) * 13.0f / TeeSize);
			int MaxRows = g_Config.m_TcFrozenMaxRows;
			float StartPos = m_Width / 2.0f + 38.0f * (m_Width / m_Height) / 1.78f;

			const auto FrozenHudLayout = HudLayout::Get(HudLayout::MODULE_FROZEN_HUD, m_Width, m_Height);
			const float FrozenHudScale = std::clamp(FrozenHudLayout.m_Scale / 100.0f, 0.25f, 3.0f);
			const float FrozenHudAnchorX = StartPos - TeeSize / 2.0f;
			Graphics()->MapScreen(FrozenHudAnchorX - FrozenHudLayout.m_X / FrozenHudScale, 0.0f - FrozenHudLayout.m_Y / FrozenHudScale, FrozenHudAnchorX - FrozenHudLayout.m_X / FrozenHudScale + m_Width / FrozenHudScale, 0.0f - FrozenHudLayout.m_Y / FrozenHudScale + m_Height / FrozenHudScale);

			int TotalRows = std::min(MaxRows, (NumInTeam + MaxTees - 1) / MaxTees);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
			const int FrozenHudCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, FrozenHudLayout.m_X, FrozenHudLayout.m_Y, TeeSize * std::min(NumInTeam, MaxTees) * FrozenHudScale, (TeeSize + 3.0f + (TotalRows - 1) * TeeSize) * FrozenHudScale, m_Width, m_Height);
			Graphics()->DrawRectExt(StartPos - TeeSize / 2.0f, 0.0f, TeeSize * std::min(NumInTeam, MaxTees), TeeSize + 3.0f + (TotalRows - 1) * TeeSize, 5.0f, FrozenHudCorners);
			Graphics()->QuadsEnd();

			bool Overflow = NumInTeam > MaxTees * MaxRows;

			int NumDisplayed = 0;
			int NumInRow = 0;
			int CurrentRow = 0;

			for(int OverflowIndex = 0; OverflowIndex < 1 + Overflow; OverflowIndex++)
			{
				for(int i = 0; i < MAX_CLIENTS && NumDisplayed < MaxTees * MaxRows; i++)
				{
					if(!GameClient()->m_Snap.m_apPlayerInfos[i])
						continue;
					if(GameClient()->m_Teams.Team(i) == LocalTeamID)
					{
						bool Frozen = false;
						CTeeRenderInfo TeeInfo = GameClient()->m_aClients[i].m_RenderInfo;
						if(GameClient()->m_aClients[i].m_FreezeEnd > 0 || GameClient()->m_aClients[i].m_DeepFrozen)
						{
							if(!g_Config.m_TcShowFrozenHudSkins)
								TeeInfo = FreezeInfo;
							Frozen = true;
						}

						if(Overflow && Frozen && OverflowIndex == 0)
							continue;
						if(Overflow && !Frozen && OverflowIndex == 1)
							continue;

						NumDisplayed++;
						NumInRow++;
						if(NumInRow > MaxTees)
						{
							NumInRow = 1;
							ProgressiveOffset = 0.0f;
							CurrentRow++;
						}

						TeeInfo.m_Size = TeeSize;
						const CAnimState *pIdleState = CAnimState::GetIdle();
						vec2 OffsetToMid;
						CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
						vec2 TeeRenderPos(StartPos + ProgressiveOffset, TeeSize * (0.7f) + CurrentRow * TeeSize);
						float Alpha = 1.0f;
						CNetObj_Character CurChar = GameClient()->m_aClients[i].m_RenderCur;
						if(g_Config.m_TcShowFrozenHudSkins && Frozen)
						{
							Alpha = 0.6f;
							TeeInfo.m_ColorBody.r *= 0.4f;
							TeeInfo.m_ColorBody.g *= 0.4f;
							TeeInfo.m_ColorBody.b *= 0.4f;
							TeeInfo.m_ColorFeet.r *= 0.4f;
							TeeInfo.m_ColorFeet.g *= 0.4f;
							TeeInfo.m_ColorFeet.b *= 0.4f;
						}
						if(Frozen)
							RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_PAIN, vec2(1.0f, 0.0f), TeeRenderPos, Alpha);
						else
							RenderTools()->RenderTee(pIdleState, &TeeInfo, CurChar.m_Emote, vec2(1.0f, 0.0f), TeeRenderPos);
						ProgressiveOffset += TeeSize;
					}
				}
			}

			Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
		}
	}
}

void CHud::RenderConnectionWarning()
{
	if(Client()->ConnectionProblems())
	{
		const char *pText = Localize("Connection Problems…");
		float w = TextRender()->TextWidth(24, pText, -1, -1.0f);
		TextRender()->Text(150 * Graphics()->ScreenAspect() - w / 2, 50, 24, pText, -1.0f);
	}
}

void CHud::RenderTeambalanceWarning()
{
	// render prompt about team-balance
	bool Flash = time() / (time_freq() / 2) % 2 == 0;
	if(GameClient()->IsTeamPlay())
	{
		int TeamDiff = GameClient()->m_Snap.m_aTeamSize[TEAM_RED] - GameClient()->m_Snap.m_aTeamSize[TEAM_BLUE];
		if(g_Config.m_ClWarningTeambalance && (TeamDiff >= 2 || TeamDiff <= -2))
		{
			const char *pText = Localize("Please balance teams!");
			if(Flash)
				TextRender()->TextColor(1, 1, 0.5f, 1);
			else
				TextRender()->TextColor(0.7f, 0.7f, 0.2f, 1.0f);
			TextRender()->Text(5, 50, 6, pText, -1.0f);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}
}

void CHud::RenderCursor()
{
	const float Scale = (float)g_Config.m_TcCursorScale / 100.0f;
	if(Scale <= 0.0f)
		return;

	int CurWeapon = 0;
	vec2 TargetPos;
	float Alpha = 1.0f;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	float aPoints[4];
	Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), 1.0f, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);

	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_pLocalCharacter)
	{
		// Render local cursor
		CurWeapon = maximum(0, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Predicted.m_ActiveWeapon);
		TargetPos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	}
	else
	{
		// Render spec cursor
		if(!g_Config.m_ClSpecCursor || !GameClient()->m_CursorInfo.IsAvailable())
			return;

		bool RenderSpecCursor = (GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW) || Client()->State() == IClient::STATE_DEMOPLAYBACK;

		if(!RenderSpecCursor)
			return;

		// Calculate factor to keep cursor on screen
		const vec2 HalfSize = vec2(Center.x - aPoints[0], Center.y - aPoints[1]);
		const vec2 ScreenPos = (GameClient()->m_CursorInfo.WorldTarget() - Center) / GameClient()->m_Camera.m_RenderZoom;
		const float ClampFactor = maximum(
			1.0f,
			absolute(ScreenPos.x / HalfSize.x),
			absolute(ScreenPos.y / HalfSize.y));

		CurWeapon = maximum(0, GameClient()->m_CursorInfo.Weapon() % NUM_WEAPONS);
		TargetPos = ScreenPos / ClampFactor + Center;
		if(ClampFactor != 1.0f)
			Alpha /= 2.0f;
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponCursors[CurWeapon]);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aCursorOffset[CurWeapon], TargetPos.x, TargetPos.y, Scale, Scale);
}

void CHud::PrepareAmmoHealthAndArmorQuads()
{
	float x = 5;
	float y = 5;
	IGraphics::CQuadItem Array[10];

	// ammo of the different weapons
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		// 0.6
		for(int n = 0; n < 10; n++)
			Array[n] = IGraphics::CQuadItem(x + n * 12, y, 10, 10);

		m_aAmmoOffset[i] = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

		// 0.7
		if(i == WEAPON_GRENADE)
		{
			// special case for 0.7 grenade
			for(int n = 0; n < 10; n++)
				Array[n] = IGraphics::CQuadItem(1 + x + n * 12, y, 10, 10);
		}
		else
		{
			for(int n = 0; n < 10; n++)
				Array[n] = IGraphics::CQuadItem(x + n * 12, y, 12, 12);
		}

		Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	}

	// health
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 10, 10);
	m_HealthOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// empty health
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 10, 10);
	m_EmptyHealthOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// armor meter
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 10, 10);
	m_ArmorOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// empty armor meter
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 10, 10);
	m_EmptyArmorOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
}

void CHud::RenderAmmoHealthAndArmor(const CNetObj_Character *pCharacter)
{
	if(!pCharacter)
		return;

	const auto HealthAmmoLayout = HudLayout::Get(HudLayout::MODULE_HEALTH_ARMOR_AMMO, m_Width, m_Height);
	const float HealthAmmoScale = std::clamp(HealthAmmoLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	Graphics()->MapScreen(5.0f - HealthAmmoLayout.m_X / HealthAmmoScale, 5.0f - HealthAmmoLayout.m_Y / HealthAmmoScale, 5.0f - HealthAmmoLayout.m_X / HealthAmmoScale + m_Width / HealthAmmoScale, 5.0f - HealthAmmoLayout.m_Y / HealthAmmoScale + m_Height / HealthAmmoScale);

	bool IsSixupGameSkin = GameClient()->m_GameSkin.IsSixup();
	int QuadOffsetSixup = (IsSixupGameSkin ? 10 : 0);

	if(GameClient()->m_GameInfo.m_HudAmmo)
	{
		// ammo display
		float AmmoOffsetY = GameClient()->m_GameInfo.m_HudHealthArmor ? 24 : 0;
		int CurWeapon = pCharacter->m_Weapon % NUM_WEAPONS;
		// 0.7 only
		if(CurWeapon == WEAPON_NINJA)
		{
			if(!GameClient()->m_GameInfo.m_HudDDRace && Client()->IsSixup())
			{
				const int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
				float NinjaProgress = std::clamp(pCharacter->m_AmmoCount - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)Max;
				RenderNinjaBarPos(5 + 10 * 12, 5, 6.f, 24.f, NinjaProgress);
			}
		}
		else if(CurWeapon >= 0 && GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon].IsValid())
		{
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon]);
			if(AmmoOffsetY > 0)
			{
				Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_aAmmoOffset[CurWeapon] + QuadOffsetSixup, std::clamp(pCharacter->m_AmmoCount, 0, 10), 0, AmmoOffsetY);
			}
			else
			{
				Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_aAmmoOffset[CurWeapon] + QuadOffsetSixup, std::clamp(pCharacter->m_AmmoCount, 0, 10));
			}
		}
	}

	if(GameClient()->m_GameInfo.m_HudHealthArmor)
	{
		// health display
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHealthFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_HealthOffset + QuadOffsetSixup, minimum(pCharacter->m_Health, 10));
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHealthEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_EmptyHealthOffset + QuadOffsetSixup + minimum(pCharacter->m_Health, 10), 10 - minimum(pCharacter->m_Health, 10));

		// armor display
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteArmorFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + QuadOffsetSixup, minimum(pCharacter->m_Armor, 10));
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteArmorEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + QuadOffsetSixup + minimum(pCharacter->m_Armor, 10), 10 - minimum(pCharacter->m_Armor, 10));
	}

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
}

void CHud::PreparePlayerStateQuads()
{
	float x = 5;
	float y = 5 + 24;
	IGraphics::CQuadItem Array[10];

	// Quads for displaying the available and used jumps
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	m_AirjumpOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	m_AirjumpEmptyOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// Quads for displaying weapons
	for(int Weapon = 0; Weapon < NUM_WEAPONS; ++Weapon)
	{
		const CDataWeaponspec &WeaponSpec = g_pData->m_Weapons.m_aId[Weapon];
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(WeaponSpec.m_pSpriteBody, ScaleX, ScaleY);
		constexpr float HudWeaponScale = 0.25f;
		float Width = WeaponSpec.m_VisualSize * ScaleX * HudWeaponScale;
		float Height = WeaponSpec.m_VisualSize * ScaleY * HudWeaponScale;
		m_aWeaponOffset[Weapon] = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, Width, Height);
	}

	// Quads for displaying capabilities
	m_EndlessJumpOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_EndlessHookOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_JetpackOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_TeleportGrenadeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_TeleportGunOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_TeleportLaserOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying prohibited capabilities
	m_SoloOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_CollisionDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_HookHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_HammerHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_GunHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_ShotgunHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_GrenadeHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_LaserHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying freeze status
	m_DeepFrozenOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_LiveFrozenOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying dummy actions
	m_DummyHammerOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_DummyCopyOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying team modes
	m_PracticeModeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_LockModeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_Team0ModeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
}

void CHud::RenderPlayerState(const int ClientId)
{
	const auto PlayerStateLayout = HudLayout::Get(HudLayout::MODULE_PLAYER_STATE, m_Width, m_Height);
	const float PlayerStateScale = std::clamp(PlayerStateLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float PlayerStateJumpsOffsetY = (GameClient()->m_GameInfo.m_HudHealthArmor && g_Config.m_ClShowhudHealthAmmo ? 24.0f : 0.0f) + (GameClient()->m_GameInfo.m_HudAmmo && g_Config.m_ClShowhudHealthAmmo ? 12.0f : 0.0f);
	const float PlayerStateAnchorX = 5.0f;
	const float PlayerStateAnchorY = 5.0f + 12.0f + PlayerStateJumpsOffsetY;
	Graphics()->MapScreen(PlayerStateAnchorX - PlayerStateLayout.m_X / PlayerStateScale, PlayerStateAnchorY - PlayerStateLayout.m_Y / PlayerStateScale, PlayerStateAnchorX - PlayerStateLayout.m_X / PlayerStateScale + m_Width / PlayerStateScale, PlayerStateAnchorY - PlayerStateLayout.m_Y / PlayerStateScale + m_Height / PlayerStateScale);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	// pCharacter contains the predicted character for local players or the last snap for players who are spectated
	CCharacterCore *pCharacter = &GameClient()->m_aClients[ClientId].m_Predicted;
	CNetObj_Character *pPlayer = &GameClient()->m_aClients[ClientId].m_RenderCur;
	int TotalJumpsToDisplay = 0;
	if(g_Config.m_ClShowhudJumpsIndicator)
	{
		int AvailableJumpsToDisplay;
		if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
		{
			bool Grounded = false;
			if(Collision()->CheckPoint(pPlayer->m_X + CCharacterCore::PhysicalSize() / 2,
				   pPlayer->m_Y + CCharacterCore::PhysicalSize() / 2 + 5))
			{
				Grounded = true;
			}
			if(Collision()->CheckPoint(pPlayer->m_X - CCharacterCore::PhysicalSize() / 2,
				   pPlayer->m_Y + CCharacterCore::PhysicalSize() / 2 + 5))
			{
				Grounded = true;
			}

			int UsedJumps = pCharacter->m_JumpedTotal;
			if(pCharacter->m_Jumps > 1)
			{
				UsedJumps += !Grounded;
			}
			else if(pCharacter->m_Jumps == 1)
			{
				// If the player has only one jump, each jump is the last one
				UsedJumps = pPlayer->m_Jumped & 2;
			}
			else if(pCharacter->m_Jumps == -1)
			{
				// The player has only one ground jump
				UsedJumps = !Grounded;
			}

			if(pCharacter->m_EndlessJump && UsedJumps >= absolute(pCharacter->m_Jumps))
			{
				UsedJumps = absolute(pCharacter->m_Jumps) - 1;
			}

			int UnusedJumps = absolute(pCharacter->m_Jumps) - UsedJumps;
			if(!(pPlayer->m_Jumped & 2) && UnusedJumps <= 0)
			{
				// In some edge cases when the player just got another number of jumps, UnusedJumps is not correct
				UnusedJumps = 1;
			}
			TotalJumpsToDisplay = maximum(minimum(absolute(pCharacter->m_Jumps), 10), 0);
			AvailableJumpsToDisplay = maximum(minimum(UnusedJumps, TotalJumpsToDisplay), 0);
		}
		else
		{
			TotalJumpsToDisplay = AvailableJumpsToDisplay = absolute(GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Jumps);
		}

		// render available and used jumps
		int JumpsOffsetY = ((GameClient()->m_GameInfo.m_HudHealthArmor && g_Config.m_ClShowhudHealthAmmo ? 24 : 0) +
				    (GameClient()->m_GameInfo.m_HudAmmo && g_Config.m_ClShowhudHealthAmmo ? 12 : 0));
		if(JumpsOffsetY > 0)
		{
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjump);
			Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_AirjumpOffset, AvailableJumpsToDisplay, 0, JumpsOffsetY);
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjumpEmpty);
			Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_AirjumpEmptyOffset + AvailableJumpsToDisplay, TotalJumpsToDisplay - AvailableJumpsToDisplay, 0, JumpsOffsetY);
		}
		else
		{
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjump);
			Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpOffset, AvailableJumpsToDisplay);
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjumpEmpty);
			Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpEmptyOffset + AvailableJumpsToDisplay, TotalJumpsToDisplay - AvailableJumpsToDisplay);
		}
	}

	float x = 5 + 12;
	float y = (5 + 12 + (GameClient()->m_GameInfo.m_HudHealthArmor && g_Config.m_ClShowhudHealthAmmo ? 24 : 0) +
		   (GameClient()->m_GameInfo.m_HudAmmo && g_Config.m_ClShowhudHealthAmmo ? 12 : 0));

	// render weapons
	{
		constexpr float aWeaponWidth[NUM_WEAPONS] = {16, 12, 12, 12, 12, 12};
		constexpr float aWeaponInitialOffset[NUM_WEAPONS] = {-3, -4, -1, -1, -2, -4};
		bool InitialOffsetAdded = false;
		for(int Weapon = 0; Weapon < NUM_WEAPONS; ++Weapon)
		{
			if(!pCharacter->m_aWeapons[Weapon].m_Got)
				continue;
			if(!InitialOffsetAdded)
			{
				x += aWeaponInitialOffset[Weapon];
				InitialOffsetAdded = true;
			}
			if(pPlayer->m_Weapon != Weapon)
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
			Graphics()->QuadsSetRotation(pi * 7 / 4);
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpritePickupWeapons[Weapon]);
			Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aWeaponOffset[Weapon], x, y);
			Graphics()->QuadsSetRotation(0);
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			x += aWeaponWidth[Weapon];
		}
		if(pCharacter->m_aWeapons[WEAPON_NINJA].m_Got)
		{
			const int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
			float NinjaProgress = std::clamp(pCharacter->m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000 - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)Max;
			if(NinjaProgress > 0.0f && GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
			{
				RenderNinjaBarPos(x, y - 12, 6.f, 24.f, NinjaProgress);
			}
		}
	}

	// render capabilities
	x = 5;
	y += 12;
	if(TotalJumpsToDisplay > 0)
	{
		y += 12;
	}
	bool HasCapabilities = false;
	if(pCharacter->m_EndlessJump)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudEndlessJump);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_EndlessJumpOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_EndlessHook)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudEndlessHook);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_EndlessHookOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_Jetpack)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudJetpack);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_JetpackOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HasTelegunGun && pCharacter->m_aWeapons[WEAPON_GUN].m_Got)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeleportGun);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_TeleportGunOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HasTelegunGrenade && pCharacter->m_aWeapons[WEAPON_GRENADE].m_Got)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeleportGrenade);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_TeleportGrenadeOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HasTelegunLaser && pCharacter->m_aWeapons[WEAPON_LASER].m_Got)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeleportLaser);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_TeleportLaserOffset, x, y);
	}

	// render prohibited capabilities
	x = 5;
	if(HasCapabilities)
	{
		y += 12;
	}
	bool HasProhibitedCapabilities = false;
	if(pCharacter->m_Solo)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudSolo);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_SoloOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_CollisionDisabled)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudCollisionDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_CollisionDisabledOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HookHitDisabled)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudHookHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_HookHitDisabledOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HammerHitDisabled)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudHammerHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_HammerHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_GrenadeHitDisabled && pCharacter->m_HasTelegunGun && pCharacter->m_aWeapons[WEAPON_GUN].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudGunHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LaserHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_ShotgunHitDisabled && pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudShotgunHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_ShotgunHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_GrenadeHitDisabled && pCharacter->m_aWeapons[WEAPON_GRENADE].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudGrenadeHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_GrenadeHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_LaserHitDisabled && pCharacter->m_aWeapons[WEAPON_LASER].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudLaserHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LaserHitDisabledOffset, x, y);
	}

	// render dummy actions and freeze state
	x = 5;
	if(HasProhibitedCapabilities)
	{
		y += 12;
	}
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo && GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_LOCK_MODE)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudLockMode);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LockModeOffset, x, y);
		x += 12;
	}
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo && GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_PRACTICE_MODE)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudPracticeMode);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_PracticeModeOffset, x, y);
		x += 12;
	}
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo && GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_TEAM0_MODE)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeam0Mode);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_Team0ModeOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_DeepFrozen)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudDeepFrozen);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DeepFrozenOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_LiveFrozen)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudLiveFrozen);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LiveFrozenOffset, x, y);
	}

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
}

void CHud::RenderNinjaBarPos(const float x, float y, const float Width, const float Height, float Progress, const float Alpha)
{
	Progress = std::clamp(Progress, 0.0f, 1.0f);

	// what percentage of the end pieces is used for the progress indicator and how much is the rest
	// half of the ends are used for the progress display
	const float RestPct = 0.5f;
	const float ProgPct = 0.5f;

	const float EndHeight = Width; // to keep the correct scale - the width of the sprite is as long as the height
	const float BarWidth = Width;
	const float WholeBarHeight = Height;
	const float MiddleBarHeight = WholeBarHeight - (EndHeight * 2.0f);
	const float EndProgressHeight = EndHeight * ProgPct;
	const float EndRestHeight = EndHeight * RestPct;
	const float ProgressBarHeight = WholeBarHeight - (EndProgressHeight * 2.0f);
	const float EndProgressProportion = EndProgressHeight / ProgressBarHeight;
	const float MiddleProgressProportion = MiddleBarHeight / ProgressBarHeight;

	// beginning piece
	float BeginningPieceProgress = 1;
	if(Progress <= 1)
	{
		if(Progress <= (EndProgressProportion + MiddleProgressProportion))
		{
			BeginningPieceProgress = 0;
		}
		else
		{
			BeginningPieceProgress = (Progress - EndProgressProportion - MiddleProgressProportion) / EndProgressProportion;
		}
	}
	// empty
	Graphics()->WrapClamp();
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarEmptyRight);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// Subset: btm_r, top_r, top_m, btm_m | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
	Graphics()->QuadsSetSubsetFree(1, 1, 1, 0, ProgPct - ProgPct * (1.0f - BeginningPieceProgress), 0, ProgPct - ProgPct * (1.0f - BeginningPieceProgress), 1);
	IGraphics::CQuadItem QuadEmptyBeginning(x, y, BarWidth, EndRestHeight + EndProgressHeight * (1.0f - BeginningPieceProgress));
	Graphics()->QuadsDrawTL(&QuadEmptyBeginning, 1);
	Graphics()->QuadsEnd();
	// full
	if(BeginningPieceProgress > 0.0f)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarFullLeft);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		// Subset: btm_m, top_m, top_r, btm_r | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(RestPct + ProgPct * (1.0f - BeginningPieceProgress), 1, RestPct + ProgPct * (1.0f - BeginningPieceProgress), 0, 1, 0, 1, 1);
		IGraphics::CQuadItem QuadFullBeginning(x, y + (EndRestHeight + EndProgressHeight * (1.0f - BeginningPieceProgress)), BarWidth, EndProgressHeight * BeginningPieceProgress);
		Graphics()->QuadsDrawTL(&QuadFullBeginning, 1);
		Graphics()->QuadsEnd();
	}

	// middle piece
	y += EndHeight;

	float MiddlePieceProgress = 1;
	if(Progress <= EndProgressProportion + MiddleProgressProportion)
	{
		if(Progress <= EndProgressProportion)
		{
			MiddlePieceProgress = 0;
		}
		else
		{
			MiddlePieceProgress = (Progress - EndProgressProportion) / MiddleProgressProportion;
		}
	}

	const float FullMiddleBarHeight = MiddleBarHeight * MiddlePieceProgress;
	const float EmptyMiddleBarHeight = MiddleBarHeight - FullMiddleBarHeight;

	// empty ninja bar
	if(EmptyMiddleBarHeight > 0.0f)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarEmpty);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		// select the middle portion of the sprite so we don't get edge bleeding
		if(EmptyMiddleBarHeight <= EndHeight)
		{
			// prevent pixel puree, select only a small slice
			// Subset: btm_r, top_r, top_m, btm_m | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
			Graphics()->QuadsSetSubsetFree(1, 1, 1, 0, 1.0f - (EmptyMiddleBarHeight / EndHeight), 0, 1.0f - (EmptyMiddleBarHeight / EndHeight), 1);
		}
		else
		{
			// Subset: btm_r, top_r, top_l, btm_l | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
			Graphics()->QuadsSetSubsetFree(1, 1, 1, 0, 0, 0, 0, 1);
		}
		IGraphics::CQuadItem QuadEmpty(x, y, BarWidth, EmptyMiddleBarHeight);
		Graphics()->QuadsDrawTL(&QuadEmpty, 1);
		Graphics()->QuadsEnd();
	}

	// full ninja bar
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarFull);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// select the middle portion of the sprite so we don't get edge bleeding
	if(FullMiddleBarHeight <= EndHeight)
	{
		// prevent pixel puree, select only a small slice
		// Subset: btm_m, top_m, top_r, btm_r | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(1.0f - (FullMiddleBarHeight / EndHeight), 1, 1.0f - (FullMiddleBarHeight / EndHeight), 0, 1, 0, 1, 1);
	}
	else
	{
		// Subset: btm_l, top_l, top_r, btm_r | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(0, 1, 0, 0, 1, 0, 1, 1);
	}
	IGraphics::CQuadItem QuadFull(x, y + EmptyMiddleBarHeight, BarWidth, FullMiddleBarHeight);
	Graphics()->QuadsDrawTL(&QuadFull, 1);
	Graphics()->QuadsEnd();

	// ending piece
	y += MiddleBarHeight;
	float EndingPieceProgress = 1;
	if(Progress <= EndProgressProportion)
	{
		EndingPieceProgress = Progress / EndProgressProportion;
	}
	// empty
	if(EndingPieceProgress < 1.0f)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarEmptyRight);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		// Subset: btm_l, top_l, top_m, btm_m | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(0, 1, 0, 0, ProgPct - ProgPct * EndingPieceProgress, 0, ProgPct - ProgPct * EndingPieceProgress, 1);
		IGraphics::CQuadItem QuadEmptyEnding(x, y, BarWidth, EndProgressHeight * (1.0f - EndingPieceProgress));
		Graphics()->QuadsDrawTL(&QuadEmptyEnding, 1);
		Graphics()->QuadsEnd();
	}
	// full
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarFullLeft);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// Subset: btm_m, top_m, top_l, btm_l | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
	Graphics()->QuadsSetSubsetFree(RestPct + ProgPct * EndingPieceProgress, 1, RestPct + ProgPct * EndingPieceProgress, 0, 0, 0, 0, 1);
	IGraphics::CQuadItem QuadFullEnding(x, y + (EndProgressHeight * (1.0f - EndingPieceProgress)), BarWidth, EndRestHeight + EndProgressHeight * EndingPieceProgress);
	Graphics()->QuadsDrawTL(&QuadFullEnding, 1);
	Graphics()->QuadsEnd();

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->WrapNormal();
}


static void GetKeystrokesContentSize(float &Width, float &Height)
{
	const float Key = 14.0f;
	const float Gap = 2.0f;
	Width = Key * 2.0f + Gap;
	Height = Key * 2.0f + Gap;
}

void CHud::RenderKeystrokes()
{
	if(!g_Config.m_TcKeystrokes)
		return;

	const auto Layout = HudLayout::Get(HudLayout::MODULE_KEYSTROKES, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	float ContentW, ContentH;
	GetKeystrokesContentSize(ContentW, ContentH);

	Graphics()->MapScreen(-Layout.m_X / Scale, -Layout.m_Y / Scale, -Layout.m_X / Scale + m_Width / Scale, -Layout.m_Y / Scale + m_Height / Scale);

	if(Layout.m_BackgroundEnabled)
	{
		const ColorRGBA Back = color_cast<ColorRGBA>(ColorHSLA(Layout.m_BackgroundColor, true));
		Graphics()->TextureClear();
		Graphics()->DrawRect(0.0f, 0.0f, ContentW, ContentH, Back, IGraphics::CORNER_NONE, 0.0f);
	}

	const CNetObj_PlayerInput &PlayerInput = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy];
	const bool PressedA = PlayerInput.m_Direction < 0 || Input()->KeyIsPressed(KEY_A);
	const bool PressedD = PlayerInput.m_Direction > 0 || Input()->KeyIsPressed(KEY_D);
	const bool PressedLmb = Input()->KeyIsPressed(KEY_MOUSE_1);
	const bool PressedRmb = Input()->KeyIsPressed(KEY_MOUSE_2);

	const float Key = 14.0f;
	const float Gap = 2.0f;
	const float FontSize = 7.0f;
	const float PressedOpacity = g_Config.m_TcKeystrokesOpacity / 100.0f;

	auto DrawKey = [&](float X, float Y, const char *pLabel, bool Pressed) {
		const ColorRGBA Fill = Pressed ? ColorRGBA(1.0f, 1.0f, 1.0f, PressedOpacity) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.82f);
		Graphics()->TextureClear();
		Graphics()->DrawRect(X, Y, Key, Key, Fill, IGraphics::CORNER_NONE, 0.0f);
		const float TextW = TextRender()->TextWidth(FontSize, pLabel);
		TextRender()->TextColor(Pressed ? ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f) : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		TextRender()->Text(X + (Key - TextW) / 2.0f, Y + (Key - FontSize) / 2.0f - 0.5f, FontSize, pLabel, -1.0f);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	const float Row2 = Key + Gap;
	DrawKey(0.0f, 0.0f, "A", PressedA);
	DrawKey(Key + Gap, 0.0f, "D", PressedD);
	DrawKey(0.0f, Row2, "LMB", PressedLmb);
	DrawKey(Key + Gap, Row2, "RMB", PressedRmb);
}

CUIRect CHud::GetKeystrokesHudEditorRect() const
{
	if(!g_Config.m_TcKeystrokes)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const auto Layout = HudLayout::Get(HudLayout::MODULE_KEYSTROKES, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	float ContentW, ContentH;
	GetKeystrokesContentSize(ContentW, ContentH);
	CUIRect Rect = {Layout.m_X, Layout.m_Y, ContentW * Scale, ContentH * Scale};
	return Rect;
}

void CHud::EnsureFinishPredictionData()
{
	if(!m_FinishPredictionDataDirty)
		return;
	const float Now = Client()->LocalTime();
	if(Now < m_FinishPredictionNextRetryTime)
		return;

	m_vFinishPredictionDistances.clear();
	m_FinishPredictionMapWidth = 0;
	m_FinishPredictionMapHeight = 0;
	m_FinishPredictionStartDistance = -1.0f;
	m_FinishPredictionRaceStartTime = -1.0f;
	m_FinishPredictionSmoothedTime = -1.0f;

	auto Fail = [&]() {
		m_FinishPredictionNextRetryTime = Now + 2.0f;
	};

	if(Collision() == nullptr || Collision()->GetWidth() <= 0 || Collision()->GetHeight() <= 0)
	{
		Fail();
		return;
	}

	const int Width = Collision()->GetWidth();
	const int Height = Collision()->GetHeight();
	const int MapSize = Width * Height;
	m_FinishPredictionMapWidth = Width;
	m_FinishPredictionMapHeight = Height;
	m_vFinishPredictionDistances.assign(MapSize, -1);
	std::vector<unsigned char> vPassable(MapSize, 0);

	using TNode = std::pair<int, int>;
	std::priority_queue<TNode, std::vector<TNode>, std::greater<>> Queue;
	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			const int Index = y * Width + x;
			const int GameTile = Collision()->GetTileIndex(Index);
			const int FrontTile = Collision()->GetFrontTileIndex(Index);
			const bool Blocked = GameTile == TILE_SOLID || GameTile == TILE_NOHOOK || FrontTile == TILE_SOLID || FrontTile == TILE_NOHOOK;
			vPassable[Index] = Blocked ? 0 : 1;
			if(!Blocked && (GameTile == TILE_FINISH || FrontTile == TILE_FINISH))
			{
				m_vFinishPredictionDistances[Index] = 0;
				Queue.emplace(0, Index);
			}
		}
	}
	if(Queue.empty())
	{
		Fail();
		return;
	}

	const ivec2 aDirs[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
	const int aCosts[8] = {10, 10, 10, 10, 14, 14, 14, 14};
	while(!Queue.empty())
	{
		const TNode Node = Queue.top();
		Queue.pop();
		const int CurDist = Node.first;
		const int Index = Node.second;
		if(m_vFinishPredictionDistances[Index] != CurDist)
			continue;
		const int TileX = Index % Width;
		const int TileY = Index / Width;
		for(int i = 0; i < 8; ++i)
		{
			const int NextX = TileX + aDirs[i].x;
			const int NextY = TileY + aDirs[i].y;
			if(NextX < 0 || NextX >= Width || NextY < 0 || NextY >= Height)
				continue;
			const int NextIndex = NextY * Width + NextX;
			if(vPassable[NextIndex] == 0)
				continue;
			if(aDirs[i].x != 0 && aDirs[i].y != 0)
			{
				if(vPassable[TileY * Width + NextX] == 0 || vPassable[NextY * Width + TileX] == 0)
					continue;
			}
			const int NextDist = CurDist + aCosts[i];
			if(m_vFinishPredictionDistances[NextIndex] >= 0 && m_vFinishPredictionDistances[NextIndex] <= NextDist)
				continue;
			m_vFinishPredictionDistances[NextIndex] = NextDist;
			Queue.emplace(NextDist, NextIndex);
		}
	}

	float StartDistance = -1.0f;
	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			const int Index = y * Width + x;
			const int GameTile = Collision()->GetTileIndex(Index);
			const int FrontTile = Collision()->GetFrontTileIndex(Index);
			if((GameTile != TILE_START && FrontTile != TILE_START) || m_vFinishPredictionDistances[Index] < 0)
				continue;
			const float Distance = m_vFinishPredictionDistances[Index] / 10.0f * 32.0f;
			if(StartDistance < 0.0f || Distance < StartDistance)
				StartDistance = Distance;
		}
	}
	if(StartDistance <= 0.0f)
	{
		Fail();
		return;
	}

	m_FinishPredictionStartDistance = StartDistance;
	m_FinishPredictionDataDirty = false;
}

void CHud::RenderFinishPrediction()
{
	if(!g_Config.m_TcFinishPrediction || !GameClient()->m_GameInfo.m_Race)
		return;

	EnsureFinishPredictionData();
	if(m_FinishPredictionMapWidth <= 0 || m_FinishPredictionStartDistance <= 0.0f)
		return;

	const bool Spectating = GameClient()->m_Snap.m_SpecInfo.m_Active;
	const int ClientId = Spectating ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
	if(ClientId < 0 || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		return;
	const vec2 Pos = Spectating ? GameClient()->m_Snap.m_SpecInfo.m_Position : GameClient()->m_LocalCharacterPos;
	const int CenterIndex = Collision()->GetPureMapIndex(Pos);
	if(CenterIndex < 0 || CenterIndex >= m_FinishPredictionMapWidth * m_FinishPredictionMapHeight)
		return;
	const int CenterX = CenterIndex % m_FinishPredictionMapWidth;
	const int CenterY = CenterIndex / m_FinishPredictionMapWidth;
	float RemainingDistance = -1.0f;
	for(int Radius = 0; Radius <= 3 && RemainingDistance < 0.0f; ++Radius)
	{
		for(int OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
		{
			for(int OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
			{
				if(maximum(absolute(OffsetX), absolute(OffsetY)) != Radius)
					continue;
				const int X = CenterX + OffsetX;
				const int Y = CenterY + OffsetY;
				if(X < 0 || X >= m_FinishPredictionMapWidth || Y < 0 || Y >= m_FinishPredictionMapHeight)
					continue;
				const int Distance = m_vFinishPredictionDistances[Y * m_FinishPredictionMapWidth + X];
				if(Distance < 0)
					continue;
				const float Candidate = Distance / 10.0f * 32.0f;
				if(RemainingDistance < 0.0f || Candidate < RemainingDistance)
					RemainingDistance = Candidate;
			}
		}
	}
	if(RemainingDistance < 0.0f)
		return;

	const float Total = m_FinishPredictionStartDistance;
	const float Remaining = RemainingDistance;
	if(Remaining <= 1.0f)
		return;
	const float Progress = std::clamp(1.0f - Remaining / Total, 0.0f, 1.0f);

	const float Now = Client()->LocalTime();
	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	const bool RaceTimeFlag = pGameInfoObj != nullptr && (pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME) != 0;
	float CurrentRaceTime = -1.0f;
	if(RaceTimeFlag)
		CurrentRaceTime = (float)(Client()->GameTick(g_Config.m_ClDummy) + pGameInfoObj->m_WarmupTimer) / (float)Client()->GameTickSpeed();
	else if(m_DDRaceTime > 0)
		CurrentRaceTime = m_DDRaceTime / 100.0f;
	const bool RunReset = CurrentRaceTime < 0.0f || (m_FinishPredictionLastRaceTime >= 0.0f && CurrentRaceTime + 0.5f < m_FinishPredictionLastRaceTime) || Remaining >= Total - 2.0f;
	const bool JumpedBackwards = m_FinishPredictionLastRemaining > 0.0f && Remaining > m_FinishPredictionLastRemaining + 128.0f;
	if(RunReset)
	{
		m_FinishPredictionRaceStartTime = -1.0f;
		m_FinishPredictionSmoothedTime = -1.0f;
		m_FinishPredictionWindowTime = -1.0f;
		m_FinishPredictionWindowDistance = -1.0f;
		m_FinishPredictionLastRemaining = -1.0f;
	}
	m_FinishPredictionLastRaceTime = CurrentRaceTime;
	m_FinishPredictionLastRemaining = Remaining;

	if(m_FinishPredictionRaceStartTime < 0.0f && Remaining < Total - 2.0f)
	{
		m_FinishPredictionRaceStartTime = Now;
	}
	const bool Started = m_FinishPredictionRaceStartTime >= 0.0f;
	if(!Started && !g_Config.m_TcFinishPredictionShowAlways)
		return;

	if(JumpedBackwards || m_FinishPredictionWindowTime < 0.0f)
	{
		m_FinishPredictionWindowTime = Now;
		m_FinishPredictionWindowDistance = Remaining;
	}
	else if(Now - m_FinishPredictionWindowTime >= 2.0f)
	{
		const float WindowCovered = m_FinishPredictionWindowDistance - Remaining;
		const float WindowRate = WindowCovered / (Now - m_FinishPredictionWindowTime);
		if(WindowCovered > 8.0f && WindowRate > 20.0f)
		{
			if(m_FinishPredictionMeasuredSpeed < 0.0f)
				m_FinishPredictionMeasuredSpeed = WindowRate;
			else
				m_FinishPredictionMeasuredSpeed += (WindowRate - m_FinishPredictionMeasuredSpeed) * 0.3f;
		}
		m_FinishPredictionWindowTime = Now;
		m_FinishPredictionWindowDistance = Remaining;
	}

	const CNetObj_Character &CurChar = GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
	const float CurrentSpeed = length(vec2(CurChar.m_VelX / 256.0f, CurChar.m_VelY / 256.0f)) * Client()->GameTickSpeed();
	float Speed;
	if(g_Config.m_TcFinishPredictionSpeedMode == 0)
		Speed = maximum(CurrentSpeed, 60.0f);
	else
		Speed = m_FinishPredictionMeasuredSpeed > 40.0f ? m_FinishPredictionMeasuredSpeed : maximum(CurrentSpeed, 700.0f);

	const float RemainingTime = Remaining / maximum(Speed, 40.0f);
	float Display;
	if(g_Config.m_TcFinishPredictionTimeMode == 0)
	{
		Display = RemainingTime;
	}
	else
	{
		const float Base = CurrentRaceTime >= 0.0f ? CurrentRaceTime : (Started ? Now - m_FinishPredictionRaceStartTime : 0.0f);
		Display = Base + RemainingTime;
	}
	const float Smooth = g_Config.m_TcFinishPredictionSmooth / 100.0f;
	if(m_FinishPredictionSmoothedTime < 0.0f)
		m_FinishPredictionSmoothedTime = Display;
	else
		m_FinishPredictionSmoothedTime += (Display - m_FinishPredictionSmoothedTime) * minimum(Client()->RenderFrameTime() * (1.0f - Smooth) * 12.0f + 0.05f, 1.0f);
	Display = m_FinishPredictionSmoothedTime;

	char aTime[32];
	if(g_Config.m_TcFinishPredictionShowMillis)
	{
		const int Ms = maximum((int)(Display * 1000.0f), 0);
		str_format(aTime, sizeof(aTime), "%d:%02d.%03d", Ms / 60000, (Ms / 1000) % 60, Ms % 1000);
	}
	else
	{
		const int Sec = maximum((int)Display, 0);
		str_format(aTime, sizeof(aTime), "%d:%02d", Sec / 60, Sec % 60);
	}

	char aBuf[96];
	if(g_Config.m_TcFinishPredictionShowTime && g_Config.m_TcFinishPredictionShowPercent)
		str_format(aBuf, sizeof(aBuf), "%s (%.0f%%)", aTime, Progress * 100.0f);
	else if(g_Config.m_TcFinishPredictionShowTime)
		str_format(aBuf, sizeof(aBuf), "%s", aTime);
	else
		str_format(aBuf, sizeof(aBuf), "%.0f%%", Progress * 100.0f);

	const auto Layout = HudLayout::Get(HudLayout::MODULE_FINISH_PREDICTION, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float FontSize = 10.0f;
	const float TextW = TextRender()->TextWidth(FontSize, aBuf);
	const float BoxW = TextW + 4.0f;
	const float BoxH = FontSize + 4.0f;

	Graphics()->MapScreen(-Layout.m_X / Scale, -Layout.m_Y / Scale, -Layout.m_X / Scale + m_Width / Scale, -Layout.m_Y / Scale + m_Height / Scale);
	if(Layout.m_BackgroundEnabled)
	{
		const ColorRGBA Back = color_cast<ColorRGBA>(ColorHSLA(Layout.m_BackgroundColor, true));
		Graphics()->TextureClear();
		Graphics()->DrawRect(0.0f, 0.0f, BoxW, BoxH, Back, HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, Layout.m_X, Layout.m_Y, BoxW * Scale, BoxH * Scale, m_Width, m_Height), 5.0f);
	}
	TextRender()->Text(2.0f, 2.0f, FontSize, aBuf, -1.0f);
}

CUIRect CHud::GetFinishPredictionHudEditorRect() const
{
	if(!g_Config.m_TcFinishPrediction)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}
	const auto Layout = HudLayout::Get(HudLayout::MODULE_FINISH_PREDICTION, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float FontSize = 10.0f;
	const float BoxW = TextRender()->TextWidth(FontSize, "0:00.000 (100%)") + 4.0f;
	const float BoxH = FontSize + 4.0f;
	CUIRect Rect = {Layout.m_X, Layout.m_Y, BoxW * Scale, BoxH * Scale};
	return Rect;
}

CUIRect CHud::GetVoteHudEditorRect() const
{
	const float HudWidth = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
	const auto Layout = HudLayout::Get(HudLayout::MODULE_VOTE, HudWidth, HudLayout::CANVAS_HEIGHT);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const bool Mini = g_Config.m_TcMiniVoteHud > 0;
	const float BoxW = Mini ? 35.0f : 60.0f;
	const float BoxH = Mini ? 17.5f : 19.0f;
	CUIRect Rect = {Layout.m_X, Layout.m_Y, BoxW * Scale, BoxH * Scale};
	return Rect;
}

void CHud::RenderSpectatorCount()
{
	if(!g_Config.m_ClShowhudSpectatorCount)
	{
		return;
	}

	int Count = 0;
	if(Client()->IsSixup())
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == GameClient()->m_aLocalIds[0] || (GameClient()->Client()->DummyConnected() && i == GameClient()->m_aLocalIds[1]))
				continue;

			if(Client()->m_TranslationContext.m_aClients[i].m_PlayerFlags7 & protocol7::PLAYERFLAG_WATCHING)
			{
				Count++;
			}
		}
	}
	else
	{
		const CNetObj_SpectatorCount *pSpectatorCount = GameClient()->m_Snap.m_pSpectatorCount;
		if(!pSpectatorCount)
		{
			m_LastSpectatorCountTick = Client()->GameTick(g_Config.m_ClDummy);
			return;
		}
		Count = pSpectatorCount->m_NumSpectators;
	}

	if(Count == 0)
	{
		m_LastSpectatorCountTick = Client()->GameTick(g_Config.m_ClDummy);
		return;
	}

	// 1 second delay
	if(Client()->GameTick(g_Config.m_ClDummy) < m_LastSpectatorCountTick + Client()->GameTickSpeed())
		return;

	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Count);

	const float Fontsize = 6.0f;
	const float BoxHeight = 14.f;
	const float BoxWidth = 13.f + TextRender()->TextWidth(Fontsize, aBuf);

	float StartX = m_Width - BoxWidth;
	float StartY = 285.0f - BoxHeight - 4; // 4 units distance to the next display;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle)
	{
		StartY -= 4;
	}
	StartY -= GetMovementInformationBoxHeight();

	if(g_Config.m_ClShowhudScore)
	{
		StartY -= 56;
	}

	if(g_Config.m_ClShowhudDummyActions && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) && Client()->DummyConnected())
	{
		StartY = StartY - 29.0f - 4; // dummy actions height and padding
	}

	const auto SpectatorCountLayout = HudLayout::Get(HudLayout::MODULE_SPECTATOR_COUNT, m_Width, m_Height);
	const float SpectatorCountScale = std::clamp(SpectatorCountLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	Graphics()->MapScreen(StartX - SpectatorCountLayout.m_X / SpectatorCountScale, StartY - SpectatorCountLayout.m_Y / SpectatorCountScale, StartX - SpectatorCountLayout.m_X / SpectatorCountScale + m_Width / SpectatorCountScale, StartY - SpectatorCountLayout.m_Y / SpectatorCountScale + m_Height / SpectatorCountScale);

	const int SpectatorCountCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, SpectatorCountLayout.m_X, SpectatorCountLayout.m_Y, BoxWidth * SpectatorCountScale, BoxHeight * SpectatorCountScale, m_Width, m_Height);
	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), SpectatorCountCorners, 5.0f);

	float y = StartY + BoxHeight / 3;
	float x = StartX + 2;

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->Text(x, y, Fontsize, FontIcon::EYE, -1.0f);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->Text(x + Fontsize + 3.f, y, Fontsize, aBuf, -1.0f);

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
}

void CHud::RenderDummyActions()
{
	if(!g_Config.m_ClShowhudDummyActions || (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) || !Client()->DummyConnected())
	{
		return;
	}
	// render small dummy actions hud
	const float BoxHeight = 29.0f;
	const float BoxWidth = 16.0f;

	float StartX = m_Width - BoxWidth;
	float StartY = 285.0f - BoxHeight - 4; // 4 units distance to the next display;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle)
	{
		StartY -= 4;
	}
	StartY -= GetMovementInformationBoxHeight();

	if(g_Config.m_ClShowhudScore)
	{
		StartY -= 56;
	}

	const auto DummyActionsLayout = HudLayout::Get(HudLayout::MODULE_DUMMY_ACTIONS, m_Width, m_Height);
	const float DummyActionsScale = std::clamp(DummyActionsLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	Graphics()->MapScreen(StartX - DummyActionsLayout.m_X / DummyActionsScale, StartY - DummyActionsLayout.m_Y / DummyActionsScale, StartX - DummyActionsLayout.m_X / DummyActionsScale + m_Width / DummyActionsScale, StartY - DummyActionsLayout.m_Y / DummyActionsScale + m_Height / DummyActionsScale);

	const int DummyActionsCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, DummyActionsLayout.m_X, DummyActionsLayout.m_Y, BoxWidth * DummyActionsScale, BoxHeight * DummyActionsScale, m_Width, m_Height);
	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), DummyActionsCorners, 5.0f);

	float y = StartY + 2;
	float x = StartX + 2;
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
	if(g_Config.m_ClDummyHammer)
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudDummyHammer);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DummyHammerOffset, x, y);
	y += 13;
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
	if(g_Config.m_ClDummyCopyMoves)
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudDummyCopy);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DummyCopyOffset, x, y);

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
}

inline int CHud::GetDigitsIndex(int Value, int Max)
{
	if(Value < 0)
	{
		Value *= -1;
	}
	int DigitsIndex = std::log10((Value ? Value : 1));
	if(DigitsIndex > Max)
	{
		DigitsIndex = Max;
	}
	if(DigitsIndex < 0)
	{
		DigitsIndex = 0;
	}
	return DigitsIndex;
}

inline float CHud::GetMovementInformationBoxHeight() const
{
	if(GameClient()->m_Snap.m_SpecInfo.m_Active && (GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW || GameClient()->m_aClients[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId].m_SpecCharPresent))
		return g_Config.m_ClShowhudPlayerPosition ? 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT + 2.0f : 0.0f;
	float BoxHeight = 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT * (g_Config.m_ClShowhudPlayerPosition + g_Config.m_ClShowhudPlayerSpeed) + MOVEMENT_INFORMATION_LINE_HEIGHT * g_Config.m_ClShowhudPlayerAngle * (g_Config.m_TcShowhudSmallerHud ? 1.0f : 2.0f);
	if(g_Config.m_TcShowhudPlayerCheckpoint)
		BoxHeight += MOVEMENT_INFORMATION_LINE_HEIGHT * (g_Config.m_TcShowhudSmallerHud ? 1.0f : 2.0f);
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle)
	{
		BoxHeight += 2.0f;
	}
	return BoxHeight;
}

float CHud::GetMovementInformationBoxWidth() const
{
	return g_Config.m_TcShowhudSmallerHud ? 48.0f : 62.0f;
}

void CHud::UpdateMovementInformationTextContainer(STextContainerIndex &TextContainer, float FontSize, float Value, float &PrevValue)
{
	Value = std::round(Value * 100.0f) / 100.0f; // Round to 2dp
	if(TextContainer.Valid() && PrevValue == Value && !g_Config.m_TcNameplateGradientEverything)
		return;
	PrevValue = Value;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%.2f", Value);

	CTextCursor Cursor;
	Cursor.m_FontSize = FontSize;
	if(BCGradient_ApplyEverythingHook)
		BCGradient_ApplyEverythingHook(&Cursor, aBuf, -1);
	TextRender()->RecreateTextContainer(TextContainer, &Cursor, aBuf);
}

void CHud::RenderMovementInformationTextContainer(STextContainerIndex &TextContainer, const ColorRGBA &Color, float X, float Y)
{
	if(TextContainer.Valid())
	{
		TextRender()->RenderTextContainer(TextContainer, Color, TextRender()->DefaultTextOutlineColor(), X - TextRender()->GetBoundingBoxTextContainer(TextContainer).m_W, Y);
	}
}

CHud::CMovementInformation CHud::GetMovementInformation(int ClientId, int Conn) const
{
	CMovementInformation Out;
	if(ClientId == SPEC_FREEVIEW)
	{
		Out.m_Pos = GameClient()->m_Camera.m_Center / 32.0f;
	}
	else if(GameClient()->m_aClients[ClientId].m_SpecCharPresent)
	{
		Out.m_Pos = GameClient()->m_aClients[ClientId].m_SpecChar / 32.0f;
	}
	else
	{
		const CNetObj_Character *pPrevChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev;
		const CNetObj_Character *pCurChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
		const float IntraTick = Client()->IntraGameTick(Conn);

		// To make the player position relative to blocks we need to divide by the block size
		Out.m_Pos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;

		const vec2 Vel = mix(vec2(pPrevChar->m_VelX, pPrevChar->m_VelY), vec2(pCurChar->m_VelX, pCurChar->m_VelY), IntraTick);

		float VelspeedX = Vel.x / 256.0f * Client()->GameTickSpeed();
		if(Vel.x >= -1.0f && Vel.x <= 1.0f)
		{
			VelspeedX = 0.0f;
		}
		float VelspeedY = Vel.y / 256.0f * Client()->GameTickSpeed();
		if(Vel.y >= -128.0f && Vel.y <= 128.0f)
		{
			VelspeedY = 0.0f;
		}
		// We show the speed in Blocks per Second (Bps) and therefore have to divide by the block size
		Out.m_Speed.x = VelspeedX / 32.0f;
		float VelspeedLength = length(vec2(Vel.x, Vel.y) / 256.0f) * Client()->GameTickSpeed();
		// Todo: Use Velramp tuning of each individual player
		// Since these tuning parameters are almost never changed, the default values are sufficient in most cases
		float Ramp = VelocityRamp(VelspeedLength, GameClient()->m_aTuning[Conn].m_VelrampStart, GameClient()->m_aTuning[Conn].m_VelrampRange, GameClient()->m_aTuning[Conn].m_VelrampCurvature);
		Out.m_Speed.x *= Ramp;
		Out.m_Speed.y = VelspeedY / 32.0f;

		float Angle = GameClient()->m_Players.GetPlayerTargetAngle(pPrevChar, pCurChar, ClientId, IntraTick);
		if(Angle < 0.0f)
		{
			Angle += 2.0f * pi;
		}
		Out.m_Angle = Angle * 180.0f / pi;
	}
	return Out;
}

void CHud::RenderMovementInformation()
{
	const int ClientId = GameClient()->m_Snap.m_SpecInfo.m_Active ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
	const bool PosOnly = ClientId == SPEC_FREEVIEW || (GameClient()->m_aClients[ClientId].m_SpecCharPresent);
	// Draw the information depending on settings: Position, speed and target angle
	// This display is only to present the available information from the last snapshot, not to interpolate or predict
	if(!g_Config.m_ClShowhudPlayerPosition && (PosOnly || (!g_Config.m_ClShowhudPlayerSpeed && !g_Config.m_ClShowhudPlayerAngle)))
	{
		return;
	}
	const float LineSpacer = 1.0f; // above and below each entry
	const float Fontsize = 6.0f;

	float BoxHeight = GetMovementInformationBoxHeight();
	const SMovementInformationDummyState DummyState = GetMovementInformationDummyState(PosOnly);
	BoxHeight += DummyState.m_ExtraHeight;
	const bool ShowDummyPos = DummyState.m_ShowPos;
	const bool ShowDummySpeed = DummyState.m_ShowSpeed;
	const bool ShowDummyAngle = DummyState.m_ShowAngle;

	CMovementInformation DummyInfo{};
	if(DummyState.m_ClientId >= 0)
		DummyInfo = GetMovementInformation(DummyState.m_ClientId, DummyState.m_ClientId == GameClient()->m_aLocalIds[1]);

	const float BoxWidth = GetMovementInformationBoxWidth();

	float StartX = m_Width - BoxWidth;
	float StartY = 285.0f - BoxHeight - 4.0f; // 4 units distance to the next display;
	if(g_Config.m_ClShowhudScore)
	{
		StartY -= 56.0f;
	}

	const auto MovementLayout = HudLayout::Get(HudLayout::MODULE_MOVEMENT_INFO, m_Width, m_Height);
	const float MovementScale = std::clamp(MovementLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	Graphics()->MapScreen(StartX - MovementLayout.m_X / MovementScale, StartY - MovementLayout.m_Y / MovementScale, StartX - MovementLayout.m_X / MovementScale + m_Width / MovementScale, StartY - MovementLayout.m_Y / MovementScale + m_Height / MovementScale);

	const int MovementCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, MovementLayout.m_X, MovementLayout.m_Y, BoxWidth * MovementScale, BoxHeight * MovementScale, m_Width, m_Height);
	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), MovementCorners, 5.0f);

	const CMovementInformation Info = GetMovementInformation(ClientId, g_Config.m_ClDummy);

	float y = StartY + LineSpacer * 2.0f;
	const float LeftX = StartX + 2.0f;
	const float RightX = m_Width - 2.0f;

	if(g_Config.m_ClShowhudPlayerPosition)
	{
		HudGradientText(LeftX, y, Fontsize, Localize("Position:"), -1.0f);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		HudGradientText(LeftX, y, Fontsize, "X:", -1.0f);
		UpdateMovementInformationTextContainer(m_aPlayerPositionContainers[0], Fontsize, Info.m_Pos.x, m_aPlayerPrevPosition[0]);

		ColorRGBA TextColor = TextRender()->DefaultTextColor();
		if(ShowDummyPos && fabsf(Info.m_Pos.x - DummyInfo.m_Pos.x) < 0.01f)
			TextColor = ColorRGBA(0.2f, 1.0f, 0.2f, 1.0f);

		RenderMovementInformationTextContainer(m_aPlayerPositionContainers[0], TextColor, RightX, y);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		HudGradientText(LeftX, y, Fontsize, "Y:", -1.0f);
		UpdateMovementInformationTextContainer(m_aPlayerPositionContainers[1], Fontsize, Info.m_Pos.y, m_aPlayerPrevPosition[1]);
		RenderMovementInformationTextContainer(m_aPlayerPositionContainers[1], TextRender()->DefaultTextColor(), RightX, y);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		if(ShowDummyPos)
		{
			char aBuf[32];

			HudGradientText(LeftX, y, Fontsize, "DX:", -1.0f);
			str_format(aBuf, sizeof(aBuf), "%.2f", DummyInfo.m_Pos.x);

			ColorRGBA DummyTextColor = TextRender()->DefaultTextColor();
			if(fabsf(Info.m_Pos.x - DummyInfo.m_Pos.x) < 0.01f)
				DummyTextColor = ColorRGBA(0.2f, 1.0f, 0.2f, 1.0f);

			TextRender()->TextColor(DummyTextColor);
			HudGradientText(RightX - TextRender()->TextWidth(Fontsize, aBuf), y, Fontsize, aBuf, -1.0f);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;

			HudGradientText(LeftX, y, Fontsize, "DY:", -1.0f);
			str_format(aBuf, sizeof(aBuf), "%.2f", DummyInfo.m_Pos.y);
			HudGradientText(RightX - TextRender()->TextWidth(Fontsize, aBuf), y, Fontsize, aBuf, -1.0f);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		}
	}

	if(PosOnly)
	{
		Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
		return;
	}

	if(g_Config.m_ClShowhudPlayerSpeed)
	{
		HudGradientText(LeftX, y, Fontsize, Localize("Speed:"), -1.0f);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		const char aaCoordinates[][4] = {"X:", "Y:"};
		for(int i = 0; i < 2; i++)
		{
			ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
			if(m_aLastPlayerSpeedChange[i] == ESpeedChange::INCREASE)
				Color = ColorRGBA(0.0f, 1.0f, 0.0f, 1.0f);
			if(m_aLastPlayerSpeedChange[i] == ESpeedChange::DECREASE)
				Color = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
			HudGradientText(LeftX, y, Fontsize, aaCoordinates[i], -1.0f);
			UpdateMovementInformationTextContainer(m_aPlayerSpeedTextContainers[i], Fontsize, i == 0 ? Info.m_Speed.x : Info.m_Speed.y, m_aPlayerPrevSpeed[i]);
			RenderMovementInformationTextContainer(m_aPlayerSpeedTextContainers[i], Color, RightX, y);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		}

		if(ShowDummySpeed)
		{
			char aBuf[32];

			HudGradientText(LeftX, y, Fontsize, "DX:", -1.0f);
			str_format(aBuf, sizeof(aBuf), "%.2f", DummyInfo.m_Speed.x);
			HudGradientText(RightX - TextRender()->TextWidth(Fontsize, aBuf), y, Fontsize, aBuf, -1.0f);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;

			HudGradientText(LeftX, y, Fontsize, "DY:", -1.0f);
			str_format(aBuf, sizeof(aBuf), "%.2f", DummyInfo.m_Speed.y);
			HudGradientText(RightX - TextRender()->TextWidth(Fontsize, aBuf), y, Fontsize, aBuf, -1.0f);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		}

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if(g_Config.m_ClShowhudPlayerAngle)
	{
		HudGradientText(LeftX, y, Fontsize, Localize("Angle:"), -1.0f);
		if(!g_Config.m_TcShowhudSmallerHud)
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		UpdateMovementInformationTextContainer(m_PlayerAngleTextContainerIndex, Fontsize, Info.m_Angle, m_PlayerPrevAngle);
		RenderMovementInformationTextContainer(m_PlayerAngleTextContainerIndex, TextRender()->DefaultTextColor(), RightX, y);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		if(ShowDummyAngle)
		{
			char aBuf[32];

			HudGradientText(LeftX, y, Fontsize, "DA:", -1.0f);
			str_format(aBuf, sizeof(aBuf), "%.2f", DummyInfo.m_Angle);
			HudGradientText(RightX - TextRender()->TextWidth(Fontsize, aBuf), y, Fontsize, aBuf, -1.0f);
		}
	}

	if(g_Config.m_TcShowhudPlayerCheckpoint)
	{
		HudGradientText(LeftX, y, Fontsize, Localize("Checkpoint:"), -1.0f);
		if(!g_Config.m_TcShowhudSmallerHud)
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		int PlayerId = -1;
		if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW && GameClient()->m_Snap.m_SpecInfo.m_Active)
			PlayerId = GameClient()->m_aClients[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId].ClientId();
		else if(!GameClient()->m_Snap.m_SpecInfo.m_Active)
			PlayerId = GameClient()->m_Snap.m_LocalClientId;

		int CheckpointId = -1;
		if(PlayerId != -1)
		{
			const auto &Char = GameClient()->m_Snap.m_aCharacters[PlayerId];
			if(Char.m_Active && Char.m_HasExtendedData)
				CheckpointId = Char.m_ExtendedData.m_TeleCheckpoint;
		}

		if(CheckpointId == -1)
		{
			if(!g_Config.m_TcShowhudSmallerHud)
				HudGradientText(RightX - TextRender()->TextWidth(Fontsize, "No Info"), y, Fontsize, "No Info", -1.0f);
		}
		else
		{
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "%i", CheckpointId);
			HudGradientText(RightX - TextRender()->TextWidth(Fontsize, aBuf), y, Fontsize, aBuf, -1.0f);
		}
	}

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
}

CHud::SMovementInformationDummyState CHud::GetMovementInformationDummyState(bool PosOnly) const
{
	SMovementInformationDummyState State{};
	State.m_ClientId = -1;

	if(Client()->DummyConnected())
	{
		int DummyClientId = -1;

		if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			const int SpectId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;

			if(SpectId == GameClient()->m_aLocalIds[0])
			{
				DummyClientId = GameClient()->m_aLocalIds[1];
			}
			else if(SpectId == GameClient()->m_aLocalIds[1])
			{
				DummyClientId = GameClient()->m_aLocalIds[0];
			}
			else
			{
				DummyClientId = GameClient()->m_aLocalIds[1 - (g_Config.m_ClDummy ? 1 : 0)];
			}
		}
		else
		{
			DummyClientId = GameClient()->m_aLocalIds[1 - (g_Config.m_ClDummy ? 1 : 0)];
		}

		if(DummyClientId >= 0 && DummyClientId < MAX_CLIENTS &&
			GameClient()->m_aClients[DummyClientId].m_Active)
		{
			State.m_ClientId = DummyClientId;
		}
	}

	const bool HasDummyInfo = State.m_ClientId >= 0;
	State.m_ShowPos = HasDummyInfo && g_Config.m_ClShowhudPlayerPosition && g_Config.m_TcShowhudDummyPosition;
	State.m_ShowSpeed = HasDummyInfo && !PosOnly && g_Config.m_ClShowhudPlayerSpeed && g_Config.m_TcShowhudDummySpeed;
	State.m_ShowAngle = HasDummyInfo && !PosOnly && g_Config.m_ClShowhudPlayerAngle && g_Config.m_TcShowhudDummyAngle;

	if(State.m_ShowPos)
		State.m_ExtraHeight += 2.0f * MOVEMENT_INFORMATION_LINE_HEIGHT;
	if(State.m_ShowSpeed)
		State.m_ExtraHeight += 2.0f * MOVEMENT_INFORMATION_LINE_HEIGHT;
	if(State.m_ShowAngle)
		State.m_ExtraHeight += 1.0f * MOVEMENT_INFORMATION_LINE_HEIGHT;
	return State;
}

CUIRect CHud::GetMovementInformationHudEditorRect() const
{
	const int ClientId = GameClient()->m_Snap.m_SpecInfo.m_Active ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
	const bool PosOnly = ClientId == SPEC_FREEVIEW || (GameClient()->m_aClients[ClientId].m_SpecCharPresent);
	if(!g_Config.m_ClShowhudPlayerPosition && (PosOnly || (!g_Config.m_ClShowhudPlayerSpeed && !g_Config.m_ClShowhudPlayerAngle)))
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const SMovementInformationDummyState DummyState = GetMovementInformationDummyState(PosOnly);
	const float BoxHeight = GetMovementInformationBoxHeight() + DummyState.m_ExtraHeight;
	const auto Layout = HudLayout::Get(HudLayout::MODULE_MOVEMENT_INFO, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	CUIRect BoxRect = {Layout.m_X, Layout.m_Y, GetMovementInformationBoxWidth() * Scale, BoxHeight * Scale};
	return BoxRect;
}

CUIRect CHud::GetHealthAmmoHudEditorRect() const
{
	if(!g_Config.m_ClShowhudHealthAmmo)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const bool HealthArmor = GameClient()->m_GameInfo.m_HudHealthArmor;
	const bool Ammo = GameClient()->m_GameInfo.m_HudAmmo;
	float BoxHeight = 0.0f;
	if(HealthArmor && Ammo)
		BoxHeight = 36.0f;
	else if(HealthArmor)
		BoxHeight = 24.0f;
	else if(Ammo)
		BoxHeight = 12.0f;
	if(BoxHeight <= 0.0f)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const auto Layout = HudLayout::Get(HudLayout::MODULE_HEALTH_ARMOR_AMMO, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	CUIRect BoxRect = {Layout.m_X, Layout.m_Y, 125.0f * Scale, BoxHeight * Scale};
	return BoxRect;
}

CUIRect CHud::GetPlayerStateHudEditorRect() const
{
	if(!g_Config.m_ClShowhudDDRace || !GameClient()->m_GameInfo.m_HudDDRace)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const auto Layout = HudLayout::Get(HudLayout::MODULE_PLAYER_STATE, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	CUIRect BoxRect = {Layout.m_X, Layout.m_Y, 125.0f * Scale, 60.0f * Scale};
	return BoxRect;
}

CUIRect CHud::GetSpectatorCountHudEditorRect() const
{
	if(!g_Config.m_ClShowhudSpectatorCount)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const auto Layout = HudLayout::Get(HudLayout::MODULE_SPECTATOR_COUNT, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float BoxWidth = 13.0f + TextRender()->TextWidth(6.0f, "999");
	CUIRect BoxRect = {Layout.m_X, Layout.m_Y, BoxWidth * Scale, 14.0f * Scale};
	return BoxRect;
}

CUIRect CHud::GetDummyActionsHudEditorRect() const
{
	if(!g_Config.m_ClShowhudDummyActions || !Client()->DummyConnected() || (GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)))
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const auto Layout = HudLayout::Get(HudLayout::MODULE_DUMMY_ACTIONS, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	CUIRect BoxRect = {Layout.m_X, Layout.m_Y, 16.0f * Scale, 29.0f * Scale};
	return BoxRect;
}

CUIRect CHud::GetLocalTimeHudEditorRect() const
{
	if(!g_Config.m_ClShowLocalTimeAlways && !GameClient()->m_Scoreboard.IsActive())
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	char aTimeStr[16];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), g_Config.m_TcShowLocalTimeSeconds ? "%H:%M.%S" : "%H:%M");
	const float Width = std::round(TextRender()->TextBoundingBox(5.0f, aTimeStr).m_W);

	const auto Layout = HudLayout::Get(HudLayout::MODULE_LOCAL_TIME, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	CUIRect BoxRect = {Layout.m_X, Layout.m_Y, (Width + 10.0f) * Scale, 12.5f * Scale};
	return BoxRect;
}

CUIRect CHud::GetFpsPingHudEditorRect() const
{
	int Showfps = g_Config.m_ClShowfps;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		Showfps = 0;
#endif
	const bool ShowPred = g_Config.m_ClShowpred && Client()->State() != IClient::STATE_DEMOPLAYBACK;
	if(!Showfps && !ShowPred)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	float BoxWidth = 0.0f;
	if(Showfps)
		BoxWidth = maximum(BoxWidth, TextRender()->TextWidth(12.0f, "0000"));
	if(ShowPred)
		BoxWidth = maximum(BoxWidth, TextRender()->TextWidth(12.0f, "0000 (0000)"));
	const float BoxHeight = Showfps && ShowPred ? 37.0f : 17.0f;

	const auto Layout = HudLayout::Get(HudLayout::MODULE_FPS_PING, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	CUIRect BoxRect = {Layout.m_X - BoxWidth * Scale, Layout.m_Y, BoxWidth * Scale, BoxHeight * Scale};
	return BoxRect;
}

CUIRect CHud::GetFrozenTextHudEditorRect() const
{
	if(g_Config.m_TcShowFrozenText <= 0 || !GameClient()->m_GameInfo.m_EntitiesDDRace)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const float BoxWidth = TextRender()->TextWidth(10.0f, "00 / 00");
	const auto Layout = HudLayout::Get(HudLayout::MODULE_FROZEN_TEXT, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	CUIRect BoxRect = {Layout.m_X - BoxWidth * Scale * 0.5f, Layout.m_Y, BoxWidth * Scale, 17.0f * Scale};
	return BoxRect;
}

CUIRect CHud::GetFrozenHudEditorRect() const
{
	if(g_Config.m_TcShowFrozenHud <= 0 || !GameClient()->m_GameInfo.m_EntitiesDDRace || GameClient()->m_Scoreboard.IsActive())
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	int NumInTeam = 0;
	int LocalTeamID = 0;
	if(GameClient()->m_Snap.m_LocalClientId >= 0 && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId >= 0)
	{
		if(GameClient()->m_Snap.m_SpecInfo.m_Active == 1 && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != -1)
			LocalTeamID = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId);
		else
			LocalTeamID = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId);
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_Snap.m_apPlayerInfos[i])
			continue;
		if(GameClient()->m_Teams.Team(i) == LocalTeamID)
			NumInTeam++;
	}

	if((LocalTeamID == 0 && g_Config.m_TcFrozenHudTeamOnly) || NumInTeam <= 0)
	{
		CUIRect EmptyRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return EmptyRect;
	}

	const float TeeSize = g_Config.m_TcFrozenHudTeeSize;
	int MaxTees = (int)(8.3f * (m_Width / m_Height) * 13.0f / TeeSize);
	if(!g_Config.m_ClShowfps && !g_Config.m_ClShowpred)
		MaxTees = (int)(9.5f * (m_Width / m_Height) * 13.0f / TeeSize);
	const int MaxRows = g_Config.m_TcFrozenMaxRows;
	const int TotalRows = std::min(MaxRows, (NumInTeam + MaxTees - 1) / MaxTees);

	const auto Layout = HudLayout::Get(HudLayout::MODULE_FROZEN_HUD, m_Width, m_Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float BoxWidth = TeeSize * (float)std::min(NumInTeam, MaxTees);
	const float BoxHeight = TeeSize + 3.0f + (TotalRows - 1) * TeeSize;
	CUIRect BoxRect = {Layout.m_X, Layout.m_Y, BoxWidth * Scale, BoxHeight * Scale};
	return BoxRect;
}

void CHud::RenderSpectatorHud()
{
	if(!g_Config.m_ClShowhudSpectator)
		return;

	// TClient
	float AdjustedHeight = m_Height - (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f);
	// draw the box
	Graphics()->DrawRect(m_Width - 180.0f, AdjustedHeight - 15.0f, 180.0f, 15.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_TL, 5.0f);

	// draw the text
	char aBuf[128];
	if(GameClient()->m_MultiViewActivated)
	{
		str_copy(aBuf, Localize("Multi-View"));
	}
	else if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
	{
		const auto &Player = GameClient()->m_aClients[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId];
		if(g_Config.m_ClShowIds)
			str_format(aBuf, sizeof(aBuf), Localize("Following %d: %s", "Spectating"), Player.ClientId(), Player.m_aName);
		else
			str_format(aBuf, sizeof(aBuf), Localize("Following %s", "Spectating"), Player.m_aName);
	}
	else
	{
		str_copy(aBuf, Localize("Free-View"));
	}
	TextRender()->Text(m_Width - 174.0f, AdjustedHeight - 15.0f + (15.f - 8.f) / 2.f, 8.0f, aBuf, -1.0f);

	// draw the camera info
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Camera.SpectatingPlayer() && GameClient()->m_Camera.CanUseAutoSpecCamera() && g_Config.m_ClSpecAutoSync)
	{
		bool AutoSpecCameraEnabled = GameClient()->m_Camera.m_AutoSpecCamera;
		const char *pLabelText = Localize("AUTO", "Spectating Camera Mode Icon");
		const float TextWidth = TextRender()->TextWidth(6.0f, pLabelText);

		constexpr float RightMargin = 4.0f;
		constexpr float IconWidth = 6.0f;
		constexpr float Padding = 3.0f;
		const float TagWidth = IconWidth + TextWidth + Padding * 3.0f;
		const float TagX = m_Width - RightMargin - TagWidth;
		Graphics()->DrawRect(TagX, m_Height - 12.0f, TagWidth, 10.0f, ColorRGBA(1.0f, 1.0f, 1.0f, AutoSpecCameraEnabled ? 0.50f : 0.10f), IGraphics::CORNER_ALL, 2.5f);
		TextRender()->TextColor(1, 1, 1, AutoSpecCameraEnabled ? 1.0f : 0.65f);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->Text(TagX + Padding, m_Height - 10.0f, 6.0f, FontIcon::CAMERA, -1.0f);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		TextRender()->Text(TagX + Padding + IconWidth + Padding, m_Height - 10.0f, 6.0f, pLabelText, -1.0f);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CHud::RenderLocalTime(float x)
{
	if(!g_Config.m_ClShowLocalTimeAlways && !GameClient()->m_Scoreboard.IsActive())
		return;

	const bool Seconds = g_Config.m_TcShowLocalTimeSeconds; // TClient

	char aTimeStr[16];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), Seconds ? "%H:%M.%S" : "%H:%M");
	const float Width = std::round(TextRender()->TextBoundingBox(5.0f, aTimeStr).m_W);

	const auto LocalTimeLayout = HudLayout::Get(HudLayout::MODULE_LOCAL_TIME, m_Width, m_Height);
	const float LocalTimeScale = std::clamp(LocalTimeLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float LocalTimeBoxX = x - (Width + 15.0f);
	Graphics()->MapScreen(LocalTimeBoxX - LocalTimeLayout.m_X / LocalTimeScale, 0.0f - LocalTimeLayout.m_Y / LocalTimeScale, LocalTimeBoxX - LocalTimeLayout.m_X / LocalTimeScale + m_Width / LocalTimeScale, 0.0f - LocalTimeLayout.m_Y / LocalTimeScale + m_Height / LocalTimeScale);

	const int LocalTimeCorners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, LocalTimeLayout.m_X, LocalTimeLayout.m_Y, (Width + 10.0f) * LocalTimeScale, 12.5f * LocalTimeScale, m_Width, m_Height);
	Graphics()->DrawRect(x - (Width + 15.0f), 0.0f, Width + 10.0f, 12.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), LocalTimeCorners, 3.75f);
	HudGradientText(x - (Width + 10.0f), (12.5f - 5.f) / 2.f, 5.0f, aTimeStr, -1.0f);

	// Graphics()->DrawRect(x - 30.0f, 0.0f, 25.0f, 12.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_B, 3.75f);
	// TextRender()->Text(x - 25.0f, (12.5f - 5.f) / 2.f, 5.0f, aTimeStr, -1.0f);

	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);
}

void CHud::OnNewSnapshot()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	int ClientId = -1;
	if(GameClient()->m_Snap.m_pLocalCharacter && !GameClient()->m_Snap.m_SpecInfo.m_Active && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		ClientId = GameClient()->m_Snap.m_LocalClientId;
	else if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		ClientId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;

	if(ClientId == -1)
		return;

	const CNetObj_Character *pPrevChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev;
	const CNetObj_Character *pCurChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
	const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
	ivec2 Vel = mix(ivec2(pPrevChar->m_VelX, pPrevChar->m_VelY), ivec2(pCurChar->m_VelX, pCurChar->m_VelY), IntraTick);

	CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
	if(pChar && pChar->IsGrounded())
		Vel.y = 0;

	int aVels[2] = {Vel.x, Vel.y};

	for(int i = 0; i < 2; i++)
	{
		int AbsVel = abs(aVels[i]);
		if(AbsVel > m_aPlayerSpeed[i])
		{
			m_aLastPlayerSpeedChange[i] = ESpeedChange::INCREASE;
		}
		if(AbsVel < m_aPlayerSpeed[i])
		{
			m_aLastPlayerSpeedChange[i] = ESpeedChange::DECREASE;
		}
		if(AbsVel < 2)
		{
			m_aLastPlayerSpeedChange[i] = ESpeedChange::NONE;
		}
		m_aPlayerSpeed[i] = AbsVel;
	}
}

void CHud::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	m_Width = 300.0f * Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);


#if defined(CONF_VIDEORECORDER)
	if((IVideo::Current() && g_Config.m_ClVideoShowhud) || (!IVideo::Current() && g_Config.m_ClShowhud))
#else
	if(g_Config.m_ClShowhud)
#endif
	{
		if(GameClient()->m_Snap.m_pLocalCharacter && !GameClient()->m_Snap.m_SpecInfo.m_Active && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		{
			if(g_Config.m_ClShowhudHealthAmmo)
			{
				RenderAmmoHealthAndArmor(GameClient()->m_Snap.m_pLocalCharacter);
			}
			if(GameClient()->m_Snap.m_aCharacters[GameClient()->m_Snap.m_LocalClientId].m_HasExtendedData && g_Config.m_ClShowhudDDRace && GameClient()->m_GameInfo.m_HudDDRace)
			{
				RenderPlayerState(GameClient()->m_Snap.m_LocalClientId);
			}
			RenderSpectatorCount();
			RenderMovementInformation();
			RenderDDRaceEffects();
		}
		else if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			int SpectatorId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;
			if(SpectatorId != SPEC_FREEVIEW && g_Config.m_ClShowhudHealthAmmo)
			{
				RenderAmmoHealthAndArmor(&GameClient()->m_Snap.m_aCharacters[SpectatorId].m_Cur);
			}
			if(SpectatorId != SPEC_FREEVIEW &&
				GameClient()->m_Snap.m_aCharacters[SpectatorId].m_HasExtendedData &&
				g_Config.m_ClShowhudDDRace &&
				(!GameClient()->m_MultiViewActivated || GameClient()->m_MultiViewShowHud) &&
				GameClient()->m_GameInfo.m_HudDDRace)
			{
				RenderPlayerState(SpectatorId);
			}
			RenderMovementInformation();
			RenderSpectatorHud();
		}

		if(g_Config.m_ClShowhudTimer)
			RenderGameTimer();
		RenderPauseNotification();
		RenderSuddenDeath();
		if(g_Config.m_ClShowhudScore)
			RenderScoreHud();
		RenderDummyActions();
		RenderWarmupTimer();
		RenderTextInfo();
		GameClient()->m_TClient.RenderCenterLines();
		RenderLocalTime((m_Width / 7) * 3);
		RenderKeystrokes();
		RenderFinishPrediction();
		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderConnectionWarning();
		RenderTeambalanceWarning();
		GameClient()->m_Voting.Render();
		if(g_Config.m_ClShowRecord)
			RenderRecord();
	}
	RenderCursor();
}

void CHud::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_DDRACETIME || MsgType == NETMSGTYPE_SV_DDRACETIMELEGACY)
	{
		CNetMsg_Sv_DDRaceTime *pMsg = (CNetMsg_Sv_DDRaceTime *)pRawMsg;

		m_DDRaceTime = pMsg->m_Time;

		m_ShowFinishTime = pMsg->m_Finish != 0;

		if(!m_ShowFinishTime)
		{
			m_TimeCpDiff = (float)pMsg->m_Check / 100;
			m_TimeCpLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);
		}
		else
		{
			m_FinishTimeDiff = (float)pMsg->m_Check / 100;
			m_FinishTimeLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_RECORD || MsgType == NETMSGTYPE_SV_RECORDLEGACY)
	{
		CNetMsg_Sv_Record *pMsg = (CNetMsg_Sv_Record *)pRawMsg;

		// NETMSGTYPE_SV_RACETIME on old race servers
		if(MsgType == NETMSGTYPE_SV_RECORDLEGACY && GameClient()->m_GameInfo.m_DDRaceRecordMessage)
		{
			m_DDRaceTime = pMsg->m_ServerTimeBest; // First value: m_Time

			m_FinishTimeLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);

			if(pMsg->m_PlayerTimeBest) // Second value: m_Check
			{
				m_TimeCpDiff = (float)pMsg->m_PlayerTimeBest / 100;
				m_TimeCpLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);
			}
		}
		else if(MsgType == NETMSGTYPE_SV_RECORD || GameClient()->m_GameInfo.m_RaceRecordMessage)
		{
			// ignore m_ServerTimeBest, it's handled by the game client
			m_aPlayerRecord[g_Config.m_ClDummy] = (float)pMsg->m_PlayerTimeBest / 100;
		}
	}
}

void CHud::RenderDDRaceEffects()
{
	if(m_DDRaceTime)
	{
		char aBuf[64];
		char aTime[32];
		if(m_ShowFinishTime && m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
		{
			str_time(m_DDRaceTime, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "Finish time: %s", aTime);

			// calculate alpha (4 sec 1 than get lower the next 2 sec)
			float Alpha = 1.0f;
			if(m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 4 < Client()->GameTick(g_Config.m_ClDummy) && m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
			{
				// lower the alpha slowly to blend text out
				Alpha = ((float)(m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 6) - (float)Client()->GameTick(g_Config.m_ClDummy)) / (float)(Client()->GameTickSpeed() * 2);
			}

			TextRender()->TextColor(1, 1, 1, Alpha);
			CTextCursor Cursor;
			Cursor.SetPosition(vec2(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(12, aBuf) / 2, 20));
			Cursor.m_FontSize = 12.0f;
			TextRender()->RecreateTextContainer(m_DDRaceEffectsTextContainerIndex, &Cursor, aBuf);
			if(m_FinishTimeDiff != 0.0f && m_DDRaceEffectsTextContainerIndex.Valid())
			{
				if(m_FinishTimeDiff < 0)
				{
					str_time_float(-m_FinishTimeDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
					str_format(aBuf, sizeof(aBuf), "-%s", aTime);
					TextRender()->TextColor(0.5f, 1.0f, 0.5f, Alpha); // green
				}
				else
				{
					str_time_float(m_FinishTimeDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
					str_format(aBuf, sizeof(aBuf), "+%s", aTime);
					TextRender()->TextColor(1.0f, 0.5f, 0.5f, Alpha); // red
				}
				CTextCursor DiffCursor;
				DiffCursor.SetPosition(vec2(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(10, aBuf) / 2, 34));
				DiffCursor.m_FontSize = 10.0f;
				TextRender()->AppendTextContainer(m_DDRaceEffectsTextContainerIndex, &DiffCursor, aBuf);
			}
			if(m_DDRaceEffectsTextContainerIndex.Valid())
			{
				auto OutlineColor = TextRender()->DefaultTextOutlineColor();
				OutlineColor.a *= Alpha;
				TextRender()->RenderTextContainer(m_DDRaceEffectsTextContainerIndex, TextRender()->DefaultTextColor(), OutlineColor);
			}
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		else if(g_Config.m_ClShowhudTimeCpDiff && !m_ShowFinishTime && m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
		{
			if(m_TimeCpDiff < 0)
			{
				str_time_float(-m_TimeCpDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
				str_format(aBuf, sizeof(aBuf), "-%s", aTime);
			}
			else
			{
				str_time_float(m_TimeCpDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
				str_format(aBuf, sizeof(aBuf), "+%s", aTime);
			}

			// calculate alpha (4 sec 1 than get lower the next 2 sec)
			float Alpha = 1.0f;
			if(m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 4 < Client()->GameTick(g_Config.m_ClDummy) && m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
			{
				// lower the alpha slowly to blend text out
				Alpha = ((float)(m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 6) - (float)Client()->GameTick(g_Config.m_ClDummy)) / (float)(Client()->GameTickSpeed() * 2);
			}

			if(m_TimeCpDiff > 0)
				TextRender()->TextColor(1.0f, 0.5f, 0.5f, Alpha); // red
			else if(m_TimeCpDiff < 0)
				TextRender()->TextColor(0.5f, 1.0f, 0.5f, Alpha); // green
			else if(!m_TimeCpDiff)
				TextRender()->TextColor(1, 1, 1, Alpha); // white

			CTextCursor Cursor;
			Cursor.SetPosition(vec2(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(10, aBuf) / 2, 20));
			Cursor.m_FontSize = 10.0f;
			TextRender()->RecreateTextContainer(m_DDRaceEffectsTextContainerIndex, &Cursor, aBuf);

			if(m_DDRaceEffectsTextContainerIndex.Valid())
			{
				auto OutlineColor = TextRender()->DefaultTextOutlineColor();
				OutlineColor.a *= Alpha;
				TextRender()->RenderTextContainer(m_DDRaceEffectsTextContainerIndex, TextRender()->DefaultTextColor(), OutlineColor);
			}
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}
}

void CHud::RenderRecord()
{
	if(GameClient()->m_MapBestTimeSeconds != FinishTime::UNSET && GameClient()->m_MapBestTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
	{
		char aBuf[64];
		TextRender()->Text(5, 75, 6, Localize("Server best:"), -1.0f);
		char aTime[32];
		int64_t TimeCentiseconds = static_cast<int64_t>(GameClient()->m_MapBestTimeSeconds) * 100 + static_cast<int64_t>(GameClient()->m_MapBestTimeMillis) / 10;
		str_time(TimeCentiseconds, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), "%s%s", GameClient()->m_MapBestTimeSeconds > 3600 ? "" : "   ", aTime);
		TextRender()->Text(53, 75, 6, aBuf, -1.0f);
	}

	if(GameClient()->m_ReceivedDDNetPlayerFinishTimes)
	{
		const int PlayerTimeSeconds = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_FinishTimeSeconds;
		if(PlayerTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
		{
			char aBuf[64];
			TextRender()->Text(5, 82, 6, Localize("Personal best:"), -1.0f);
			char aTime[32];
			const int PlayerTimeMillis = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_FinishTimeMillis;
			int64_t TimeCentiseconds = static_cast<int64_t>(PlayerTimeSeconds) * 100 + static_cast<int64_t>(PlayerTimeMillis) / 10;
			str_time(TimeCentiseconds, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PlayerTimeSeconds > 3600 ? "" : "   ", aTime);
			TextRender()->Text(53, 82, 6, aBuf, -1.0f);
		}
	}
	else
	{
		const float PlayerRecord = m_aPlayerRecord[g_Config.m_ClDummy];
		if(PlayerRecord > 0.0f)
		{
			char aBuf[64];
			TextRender()->Text(5, 82, 6, Localize("Personal best:"), -1.0f);
			char aTime[32];
			str_time_float(PlayerRecord, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PlayerRecord > 3600 ? "" : "   ", aTime);
			TextRender()->Text(53, 82, 6, aBuf, -1.0f);
		}
	}
}
