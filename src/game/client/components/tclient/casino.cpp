#include "casino.h"

#include <base/hash.h>
#include <base/math.h>
#include <base/secure.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/config.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>

namespace
{
	inline ColorRGBA Lerp(const ColorRGBA &A, const ColorRGBA &B, float T)
	{
		return ColorRGBA(
			A.r + (B.r - A.r) * T,
			A.g + (B.g - A.g) * T,
			A.b + (B.b - A.b) * T,
			A.a + (B.a - A.a) * T);
	}

	inline ColorRGBA Shade(const ColorRGBA &C, float Mul, float Alpha = -1.0f)
	{
		return ColorRGBA(
			std::clamp(C.r * Mul, 0.0f, 1.0f),
			std::clamp(C.g * Mul, 0.0f, 1.0f),
			std::clamp(C.b * Mul, 0.0f, 1.0f),
			Alpha < 0.0f ? C.a : Alpha);
	}

	const ColorRGBA CASINO_BG = ColorRGBA(0.043f, 0.047f, 0.066f, 1.0f);
	const ColorRGBA CASINO_BG_DEEP = ColorRGBA(0.024f, 0.027f, 0.040f, 1.0f);
	const ColorRGBA CASINO_PANEL = ColorRGBA(0.098f, 0.110f, 0.150f, 1.0f);
	const ColorRGBA CASINO_PANEL_LIGHT = ColorRGBA(0.150f, 0.168f, 0.222f, 1.0f);
	const ColorRGBA CASINO_EDGE = ColorRGBA(0.235f, 0.262f, 0.340f, 1.0f);
	const ColorRGBA CASINO_ACCENT = ColorRGBA(0.290f, 0.860f, 0.560f, 1.0f);
	const ColorRGBA CASINO_GOLD = ColorRGBA(1.000f, 0.788f, 0.310f, 1.0f);
	const ColorRGBA CASINO_GOLD_DEEP = ColorRGBA(0.640f, 0.440f, 0.110f, 1.0f);
	const ColorRGBA CASINO_RED = ColorRGBA(0.860f, 0.215f, 0.250f, 1.0f);
	const ColorRGBA CASINO_BLACK = ColorRGBA(0.105f, 0.108f, 0.135f, 1.0f);
	const ColorRGBA CASINO_DIM = ColorRGBA(0.545f, 0.588f, 0.680f, 1.0f);
	const ColorRGBA CASINO_ICE = ColorRGBA(0.520f, 0.780f, 1.000f, 1.0f);
	const ColorRGBA CASINO_FELT = ColorRGBA(0.062f, 0.196f, 0.132f, 1.0f);
	const ColorRGBA CASINO_FELT_LIGHT = ColorRGBA(0.086f, 0.268f, 0.180f, 1.0f);

	// flat by design: 1px rules and tone steps carry depth, so rounding stays tiny
	constexpr float CASINO_RADIUS = 2.0f;
	constexpr float CASINO_RADIUS_MAX = 3.0f;

	inline float Square(float Rounding)
	{
		return std::clamp(Rounding, 0.0f, CASINO_RADIUS_MAX);
	}

	const char *const s_apGameNames[CASINO_GAME_COUNT] = {
		"Slots",
		"Wheel of Fortune",
		"Blackjack",
		"Caribbean Stud",
		"Hi-Lo",
		"Dragon Tiger",
		"Craps",
		"Sic Bo",
		"Lottery",
		"Crash",
		"Mines",
		"Dice",
	};

	const char *const s_apGameTags[CASINO_GAME_COUNT] = {
		"3 reels",
		"Multiplier",
		"21",
		"Poker",
		"Streak",
		"Cards",
		"Dice",
		"3 dice",
		"Numbers",
		"Cashout",
		"Grid",
		"Over/Under",
	};

	const char *const s_apSlotNames[6] = {"SEVEN", "DIAMOND", "STAR", "CHERRY", "LEMON", "BAR"};
	const int s_aSlotWeight[6] = {2, 4, 8, 14, 20, 30};
	const int s_aSlotPay[6] = {170, 87, 43, 23, 14, 9};
	const ColorRGBA s_aSlotColor[6] = {
		ColorRGBA(1.00f, 0.84f, 0.25f, 1.0f),
		ColorRGBA(0.45f, 0.90f, 1.00f, 1.0f),
		ColorRGBA(0.78f, 0.55f, 1.00f, 1.0f),
		ColorRGBA(1.00f, 0.35f, 0.40f, 1.0f),
		ColorRGBA(0.95f, 0.92f, 0.40f, 1.0f),
		ColorRGBA(0.70f, 0.75f, 0.85f, 1.0f),
	};

	const int s_aWheelWeight[7] = {520, 190, 130, 80, 45, 25, 10};
	const float s_aWheelValue[7] = {0.0f, 1.2f, 1.5f, 2.0f, 3.0f, 5.0f, 10.0f};
	const ColorRGBA s_aWheelColor[7] = {
		ColorRGBA(0.22f, 0.24f, 0.30f, 1.0f),
		ColorRGBA(0.30f, 0.55f, 0.85f, 1.0f),
		ColorRGBA(0.30f, 0.80f, 0.55f, 1.0f),
		ColorRGBA(0.95f, 0.80f, 0.30f, 1.0f),
		ColorRGBA(0.95f, 0.55f, 0.25f, 1.0f),
		ColorRGBA(0.90f, 0.35f, 0.55f, 1.0f),
		ColorRGBA(0.70f, 0.40f, 0.95f, 1.0f),
	};

	const int s_aLotteryPay[6] = {0, 0, 2, 10, 300, 25000};


	const float SLOT_CELL = 54.0f;
	// first window holds the old symbols so the spin rolls from them
	constexpr int SLOT_STRIP_LEN = 24;

	int DiceSumPayout(int Sum)
	{
		static const int s_aCount[19] = {0, 0, 0, 1, 3, 6, 10, 15, 21, 25, 27, 27, 25, 21, 15, 10, 6, 3, 1};
		if(Sum < 3 || Sum > 18)
			return 0;
		const int Count = s_aCount[Sum];
		if(Count <= 0)
			return 0;
		const float Fair = (216.0f / (float)Count) - 1.0f;
		return std::max(1, (int)(Fair * 0.97f));
	}

	float MinesFairMultiplier(int Revealed, int Mines)
	{
		if(Revealed <= 0)
			return 1.0f;
		double Mult = 1.0;
		for(int i = 0; i < Revealed; i++)
			Mult *= (double)(25 - i) / (double)(25 - Mines - i);
		return (float)Mult;
	}

	// capped like the credit config, so a lucky round cannot wrap the balance
	int ClampWin(int64_t Amount)
	{
		return (int)std::clamp<int64_t>(Amount, 0, 1000000000);
	}

	// uniform [0, 100) roll, shared by payout and preview so odds cannot drift
	float DiceWinProbability(int Threshold, bool Over)
	{
		return Over ? (100.0f - (float)Threshold) / 100.0f : (float)Threshold / 100.0f;
	}

	// one payline table shared by the settle scoring and the render highlight
	const int s_aSlotLines[5][3][2] = {
		{{0, 0}, {1, 0}, {2, 0}},
		{{0, 1}, {1, 1}, {2, 1}},
		{{0, 2}, {1, 2}, {2, 2}},
		{{0, 0}, {1, 1}, {2, 2}},
		{{0, 2}, {1, 1}, {2, 0}},
	};

	// caribbean stud profit odds on the raise, by poker category
	const int s_aCaribbeanPays[10] = {0, 1, 2, 3, 4, 5, 7, 20, 50, 100};
}

void CCasino::OnInit()
{
	SyncFromConfig();
	ResetShoe();
	CheckDaily();
}

void CCasino::ConOpenCasino(IConsole::IResult *pResult, void *pUser)
{
	g_Config.m_UiSettingsPage = CMenus::SETTINGS_FUSION_GAMES;
	((CCasino *)pUser)->GameClient()->m_Menus.SetMenuPage(CMenus::PAGE_SETTINGS);
}

void CCasino::OnConsoleInit()
{
	Console()->Register("fc_casino", "", CFGFLAG_CLIENT, ConOpenCasino, this, "Open the casino");
}

void CCasino::OnReset()
{
	SyncFromConfig();
	ResetShoe();
}

void CCasino::SyncFromConfig()
{
	m_Credits = g_Config.m_FcCasinoCredits;
	m_TotalWagered = g_Config.m_FcCasinoTotalWagered;
	m_TotalWon = g_Config.m_FcCasinoTotalWon;
	m_BiggestWin = g_Config.m_FcCasinoBiggestWin;
	if(m_Credits < 0)
		m_Credits = 0;
}

void CCasino::SyncToConfig()
{
	g_Config.m_FcCasinoCredits = m_Credits;
	g_Config.m_FcCasinoTotalWagered = m_TotalWagered;
	g_Config.m_FcCasinoTotalWon = m_TotalWon;
	g_Config.m_FcCasinoBiggestWin = m_BiggestWin;
	m_SavePending = true;
}

int CCasino::DaysSinceDaily() const
{
	const int64_t Now = time_timestamp();
	const int64_t Last = g_Config.m_FcCasinoLastDaily;
	if(Last <= 0)
		return 9999;
	return (int)((Now - Last) / 86400);
}

// a zeroed daily timestamp would mint infinite claims; hashing it ties the stamp to the real config
void CCasino::DailyStampHash(int Timestamp, char *pOut, size_t OutSize)
{
	char aPayload[96];
	str_format(aPayload, sizeof(aPayload), "tclient-casino-daily-v1|%d", Timestamp);
	sha256_str(sha256(aPayload, str_length(aPayload)), pOut, OutSize);
}

bool CCasino::DailyStampUntouched() const
{
	if(g_Config.m_FcCasinoLastDaily <= 0)
	{
		// only a truly factory state may claim without a stamp; anything else is an edit
		return g_Config.m_FcCasinoDailyStamp[0] == '\0' &&
			g_Config.m_FcCasinoTotalWagered == 0 &&
			g_Config.m_FcCasinoTotalWon == 0 &&
			g_Config.m_FcCasinoCredits == 1000;
	}
	if(g_Config.m_FcCasinoDailyStamp[0] == '\0')
		return false;
	char aExpect[SHA256_MAXSTRSIZE];
	DailyStampHash(g_Config.m_FcCasinoLastDaily, aExpect, sizeof(aExpect));
	return str_comp(g_Config.m_FcCasinoDailyStamp, aExpect) == 0;
}

void CCasino::CheckDaily()
{
	// stamp hash depends only on the timestamp, recomputed when it changes
	if(m_DailyFrozenStamp != g_Config.m_FcCasinoLastDaily)
	{
		m_DailyFrozen = !DailyStampUntouched();
		m_DailyFrozenStamp = g_Config.m_FcCasinoLastDaily;
	}
	m_DailyReady = !m_DailyFrozen && DaysSinceDaily() >= 1;
}

// own stream, so unrelated rand() consumers cannot shift outcomes
unsigned int CCasino::NextRandom()
{
	if(m_RngState == 0)
	{
		secure_random_fill(&m_RngState, sizeof(m_RngState));
		if(m_RngState == 0)
			m_RngState = 0x9e3779b9u;
	}
	m_RngState ^= m_RngState << 13;
	m_RngState ^= m_RngState >> 17;
	m_RngState ^= m_RngState << 5;
	return m_RngState;
}

int CCasino::RandInt(int Max)
{
	if(Max <= 1)
		return 0;
	return (int)(NextRandom() % (unsigned int)Max);
}

float CCasino::RandFloat()
{
	return (float)(NextRandom() >> 8) / 16777216.0f; // [0, 1)
}

void CCasino::AddCredits(int Amount)
{
	if(Amount <= 0)
		return;
	m_Credits = (int)std::min<int64_t>((int64_t)m_Credits + Amount, 1000000000);
	SyncToConfig();
}

void CCasino::TakeCredits(int Amount)
{
	if(Amount <= 0)
		return;
	m_Credits -= Amount;
	if(m_Credits < 0)
		m_Credits = 0;
	SyncToConfig();
}

void CCasino::RecordBet(int Wagered)
{
	m_TotalWagered += Wagered;
	SyncToConfig();
}

void CCasino::RecordWin(int Won, int Staked)
{
	// net profit, so pushes and returns cannot inflate the lifetime totals
	const int Net = Won - Staked;
	if(Net > 0)
		m_TotalWon = (int)std::min<int64_t>((int64_t)m_TotalWon + Net, 1000000000);
	if(Net > m_BiggestWin)
		m_BiggestWin = Net;
	SyncToConfig();
}

void CCasino::Toast(const char *pMessage)
{
	str_copy(m_aToast, pMessage, sizeof(m_aToast));
	m_ToastTime = 3.0f;
}

void CCasino::Tick(float Dt)
{
	CheckDaily();

	m_TimeAccum += Dt;
	m_AnimTime = (int)m_TimeAccum;

	if(m_ToastTime > 0.0f)
		m_ToastTime -= Dt;

	if(m_SavePending)
	{
		m_SaveTimer += Dt;
		if(m_SaveTimer > 1.5f)
		{
			m_SaveTimer = 0.0f;
			m_SavePending = false;
			ConfigManager()->Save();
		}
	}

	if(m_SlotState == 1)
	{
		// the reels stop one after another, so each one runs on its own countdown
		bool AllStopped = true;
		for(int Reel = 0; Reel < 3; Reel++)
		{
			if(m_aSlotReelTime[Reel] > 0.0f)
			{
				m_aSlotReelTime[Reel] -= Dt;
				if(m_aSlotReelTime[Reel] <= 0.0f)
				{
					m_aSlotReelTime[Reel] = 0.0f;
					m_aSlotReelStopAnim[Reel] = 0.22f;
				}
				else
					AllStopped = false;
			}
			else if(m_aSlotReelStopAnim[Reel] > 0.0f)
				m_aSlotReelStopAnim[Reel] -= Dt;
		}
		if(AllStopped)
			SlotSettle();
	}
	if(m_WheelState == 1)
	{
		m_WheelSpinTimer += Dt;
		const float T = std::clamp(m_WheelSpinTimer / m_WheelSpinDuration, 0.0f, 1.0f);
		// ease-out cubic: fast launch, long settle
		const float Ease = 1.0f - (1.0f - T) * (1.0f - T) * (1.0f - T);
		m_WheelAngle = m_WheelSpinStart + (m_WheelSpinTarget - m_WheelSpinStart) * Ease;
		if(T >= 1.0f)
		{
			m_WheelAngle = m_WheelSpinTarget;
			m_WheelState = 2;
			// the pointer has landed, so the prize can move
			if(m_WheelPendingWin > 0)
			{
				AddCredits(m_WheelPendingWin);
				RecordWin(m_WheelPendingWin, m_WheelBet);
				m_WheelLastWin = m_WheelPendingWin;
				m_WheelPendingWin = 0;
			}
		}
	}
	if(m_CrapsState == 1)
	{
		m_CrapsTimer -= Dt;
		if(m_CrapsTimer <= 0.0f)
			m_CrapsState = 0;
	}
	if(m_SicBoState == 1)
	{
		m_SicBoTimer -= Dt;
		if(m_SicBoTimer <= 0.0f)
			m_SicBoState = 0;
	}
	if(m_LotteryState == 1)
	{
		m_LotteryTimer -= Dt;
		if(m_LotteryTimer <= 0.0f)
			m_LotteryState = 0;
	}
	if(m_DiceRolling)
	{
		m_DiceTimer -= Dt;
		m_DiceDisplay = RandFloat() * 100.0f;
		if(m_DiceTimer <= 0.0f)
		{
			m_DiceRolling = false;
			m_DiceDisplay = m_DiceRoll;
		}
	}
	if(m_HiLo.m_RevealTime > 0.0f)
	{
		m_HiLo.m_RevealTime -= Dt;
		if(m_HiLo.m_RevealTime <= 0.0f)
		{
			m_HiLo.m_RevealTime = 0.0f;
			// the card joins the strip only once the flip has landed on it
			if(m_HiLo.m_HistPending)
			{
				HiLoPushHistory(m_HiLo.m_HistPendingCard, m_HiLo.m_HistPendingDir);
				m_HiLo.m_HistPending = false;
			}
		}
	}
	if(m_HiLo.m_MultAnim > 0.0f)
		m_HiLo.m_MultAnim = std::max(0.0f, m_HiLo.m_MultAnim - Dt * 3.0f);
	if(m_HiLo.m_SettleAnim > 0.0f)
		m_HiLo.m_SettleAnim = std::max(0.0f, m_HiLo.m_SettleAnim - Dt * 1.6f);
	if(m_DragonTigerReveal > 0.0f)
	{
		m_DragonTigerReveal = std::max(0.0f, m_DragonTigerReveal - Dt);
		if(m_DragonTigerReveal <= 0.0f && m_DragonTigerHistPending >= 0)
		{
			// the reveal just finished: record the round and fire the settle burst
			for(int i = 7; i > 0; i--)
			{
				m_aDragonTigerHist[i] = m_aDragonTigerHist[i - 1];
				m_aDragonTigerHistWin[i] = m_aDragonTigerHistWin[i - 1];
			}
			m_aDragonTigerHist[0] = m_DragonTigerHistPending;
			m_aDragonTigerHistWin[0] = m_DragonTigerHistPendingWin ? 1 : 0;
			if(m_DragonTigerHistCount < 8)
				m_DragonTigerHistCount++;
			m_DragonTigerHistPending = -1;
			m_DragonTigerSettle = 1.0f;
			if(m_DragonTigerPendingWin > 0)
			{
				AddCredits(m_DragonTigerPendingWin);
				RecordWin(m_DragonTigerPendingWin, m_DragonTigerBet);
				m_DragonTigerLastWin = m_DragonTigerPendingWin;
				m_DragonTigerPendingWin = 0;
			}
		}
	}
	if(m_DragonTigerSettle > 0.0f)
		m_DragonTigerSettle = std::max(0.0f, m_DragonTigerSettle - Dt * 1.6f);
	if(m_CrashRunning && !m_CrashCashed)
	{
		m_CrashElapsed += Dt;
		m_CrashMultiplier = std::exp(0.155f * m_CrashElapsed);
		if(m_CrashMultiplier >= m_CrashPoint)
		{
			m_CrashMultiplier = m_CrashPoint;
			m_CrashRunning = false;
			m_CrashCashed = false;
			for(int i = (int)m_aCrashHistory.size() - 1; i > 0; i--)
				m_aCrashHistory[i] = m_aCrashHistory[i - 1];
			m_aCrashHistory[0] = m_CrashPoint;
			Toast("Crashed");
		}
	}
	if(m_Blackjack.m_State == 2)
	{
		m_Blackjack.m_Timer -= Dt;
		if(m_Blackjack.m_Timer <= 0.0f)
			BlackjackDealerPlay();
	}
	if(m_Blackjack.m_State == 5)
	{
		// deal animation: player, dealer, player, dealer hole, then play begins
		m_Blackjack.m_Timer -= Dt;
		if(m_Blackjack.m_Timer <= 0.0f)
		{
			const int Dealt = (int)(m_Blackjack.m_vHands[0].m_vCards.size() + m_Blackjack.m_vDealer.size());
			if(Dealt < 4)
			{
				if(Dealt % 2 == 0)
					m_Blackjack.m_vHands[0].m_vCards.push_back(DrawCardFromShoe());
				else
					m_Blackjack.m_vDealer.push_back(DrawCardFromShoe());
				m_Blackjack.m_Timer = 0.35f;
			}
			else
			{
				const int PlayerTotal = BlackjackTotal(m_Blackjack.m_vHands[0].m_vCards);
				const int DealerTotal = BlackjackTotal(m_Blackjack.m_vDealer);
				if(PlayerTotal == 21 || DealerTotal == 21)
					BlackjackDealerPlay();
				else
					m_Blackjack.m_State = 1;
			}
		}
	}
}

void CCasino::ResetShoe()
{
	m_vShoe.clear();
	for(int Deck = 0; Deck < 6; Deck++)
	{
		for(int Suit = 0; Suit < 4; Suit++)
		{
			for(int Rank = 0; Rank < 13; Rank++)
			{
				SCasinoCard Card;
				Card.m_Rank = Rank;
				Card.m_Suit = Suit;
				m_vShoe.push_back(Card);
			}
		}
	}
	for(int i = (int)m_vShoe.size() - 1; i > 0; i--)
	{
		const int j = RandInt(i + 1);
		std::swap(m_vShoe[i], m_vShoe[j]);
	}
	m_ShoePos = 0;
}

SCasinoCard CCasino::DrawCardFromShoe()
{
	if(m_ShoePos >= (int)m_vShoe.size() || m_ShoePos > (int)m_vShoe.size() * 3 / 4)
		ResetShoe();
	SCasinoCard Card = m_vShoe[m_ShoePos++];
	return Card;
}

int CCasino::CardValue(const SCasinoCard &Card)
{
	if(Card.m_Rank == 0)
		return 11;
	if(Card.m_Rank >= 10)
		return 10;
	return Card.m_Rank + 1;
}

int CCasino::BlackjackTotal(const std::vector<SCasinoCard> &vCards)
{
	int Total = 0;
	int Aces = 0;
	for(const auto &Card : vCards)
	{
		Total += CardValue(Card);
		if(Card.m_Rank == 0)
			Aces++;
	}
	while(Total > 21 && Aces > 0)
	{
		Total -= 10;
		Aces--;
	}
	return Total;
}

int CCasino::EvaluatePokerHand(const std::vector<SCasinoCard> &vCards)
{
	if(vCards.size() < 5)
		return 0;

	int aRankCount[13] = {};
	int aSuitCount[4] = {};
	for(const auto &Card : vCards)
	{
		aRankCount[Card.m_Rank]++;
		aSuitCount[Card.m_Suit]++;
	}

	bool Flush = false;
	for(int i = 0; i < 4; i++)
		if(aSuitCount[i] >= 5)
			Flush = true;

	int StraightHigh = -1;
	for(int Start = 12; Start >= -1; Start--)
	{
		bool All = true;
		for(int k = 0; k < 5; k++)
		{
			int Rank = Start + k;
			if(Rank < 0)
				Rank += 13;
			if(Rank > 12 || aRankCount[Rank] == 0)
			{
				All = false;
				break;
			}
		}
		if(All)
		{
			StraightHigh = Start + 4;
			break;
		}
	}
	if(StraightHigh < 0)
	{
		if(aRankCount[12] > 0 && aRankCount[0] > 0 && aRankCount[1] > 0 && aRankCount[2] > 0 && aRankCount[3] > 0)
			StraightHigh = 3;
	}

	int Pairs = 0, Trips = 0, Quads = 0;
	for(int i = 0; i < 13; i++)
	{
		if(aRankCount[i] == 4)
			Quads++;
		else if(aRankCount[i] == 3)
			Trips++;
		else if(aRankCount[i] == 2)
			Pairs++;
	}

	if(Flush && StraightHigh == 12)
		return 9;
	if(Flush && StraightHigh >= 0)
		return 8;
	if(Quads > 0)
		return 7;
	if(Trips > 0 && (Pairs > 0 || Trips > 1))
		return 6;
	if(Flush)
		return 5;
	if(StraightHigh >= 0)
		return 4;
	if(Trips > 0)
		return 3;
	if(Pairs >= 2)
		return 2;
	if(Pairs == 1)
		return 1;
	return 0;
}

const char *CCasino::PokerHandName(int Category)
{
	switch(Category)
	{
	case 9: return "Royal Flush";
	case 8: return "Straight Flush";
	case 7: return "Four of a Kind";
	case 6: return "Full House";
	case 5: return "Flush";
	case 4: return "Straight";
	case 3: return "Three of a Kind";
	case 2: return "Two Pair";
	case 1: return "One Pair";
	default: return "High Card";
	}
}

void CCasino::Shadow(const CUIRect *pRect, float Alpha)
{
	// no drop shadow: a 1px rule carries the separation instead
	CUIRect Rule = *pRect;
	Rule.x += 2.0f;
	Rule.w = std::max(0.0f, Rule.w - 4.0f);
	Rule.y = pRect->y + pRect->h;
	Rule.h = 1.0f;
	Rule.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, Alpha * 0.6f), IGraphics::CORNER_NONE, 0.0f);
}

void CCasino::Grad(const CUIRect *pRect, ColorRGBA Top, ColorRGBA Bottom, float Rounding)
{
	if(pRect->h <= 0.0f || pRect->w <= 0.0f)
		return;

	const float R = std::clamp(Square(Rounding), 0.0f, std::min(pRect->w, pRect->h) * 0.5f);
	pRect->Draw4(Top, Top, Bottom, Bottom, IGraphics::CORNER_ALL, R);
}

void CCasino::Frame(const CUIRect *pRect, ColorRGBA Fill, ColorRGBA Border, float Rounding)
{
	if(pRect->w <= 1.0f || pRect->h <= 1.0f)
		return;
	const float R = Square(Rounding);
	pRect->Draw(Border, IGraphics::CORNER_ALL, R);
	CUIRect Inner = *pRect;
	Inner.Margin(1.0f, &Inner);
	Grad(&Inner, Shade(Fill, 1.16f), Shade(Fill, 0.84f), std::max(0.0f, R - 1.0f));
}

void CCasino::Tile(const CUIRect *pRect, ColorRGBA Top, ColorRGBA Bottom, ColorRGBA Border, float Rounding)
{
	const float R = Square(Rounding);
	Shadow(pRect, 0.34f);
	Frame(pRect, Lerp(Top, Bottom, 0.5f), Border, R);
	Grad(pRect, Top, Bottom, R);
}

void CCasino::Pill(const CUIRect *pRect, const char *pText, ColorRGBA Fill, ColorRGBA TextColor, float FontSize)
{
	Frame(pRect, Fill, Shade(Fill, 1.55f), CASINO_RADIUS);
	Label(pRect, pText, FontSize, TEXTALIGN_MC, TextColor);
}

void CCasino::Coin(const vec2 &Pos, float Radius)
{
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Shade(CASINO_GOLD, 0.55f));
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius, 20);
	Graphics()->SetColor(CASINO_GOLD);
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius * 0.82f, 20);
	Graphics()->SetColor(Shade(CASINO_GOLD, 1.28f));
	Graphics()->DrawCircle(Pos.x - Radius * 0.18f, Pos.y - Radius * 0.18f, Radius * 0.34f, 14);
	Graphics()->QuadsEnd();
}

void CCasino::Label(const CUIRect *pRect, const char *pText, float Size, int Align, ColorRGBA Color)
{
	TextRender()->TextColor(Color);
	Ui()->DoLabel(pRect, pText, Size, Align);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

bool CCasino::Button(CUIRect *pRect, const char *pText, bool Active, bool Enabled, float FontSize)
{
	const bool Hovered = Enabled && Ui()->MouseHovered(pRect);
	const bool Pressed = Hovered && Ui()->MouseButton(0);
	const bool Clicked = Enabled && Ui()->MouseButtonClicked(0) && Hovered;

	ColorRGBA Top, Bottom, Border, TextColor;
	if(!Enabled)
	{
		Top = ColorRGBA(0.118f, 0.128f, 0.156f, 1.0f);
		Bottom = ColorRGBA(0.086f, 0.094f, 0.116f, 1.0f);
		Border = ColorRGBA(0.160f, 0.172f, 0.205f, 1.0f);
		TextColor = ColorRGBA(0.380f, 0.408f, 0.470f, 1.0f);
	}
	else if(Active)
	{
		Top = ColorRGBA(0.420f, 0.960f, 0.660f, 1.0f);
		Bottom = ColorRGBA(0.180f, 0.660f, 0.400f, 1.0f);
		Border = ColorRGBA(0.620f, 1.000f, 0.790f, 1.0f);
		TextColor = ColorRGBA(0.035f, 0.110f, 0.065f, 1.0f);
	}
	else if(Pressed)
	{
		Top = ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f);
		Bottom = ColorRGBA(0.212f, 0.238f, 0.300f, 1.0f);
		Border = ColorRGBA(0.400f, 0.360f, 0.200f, 1.0f);
		TextColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(Hovered)
	{
		Top = ColorRGBA(0.246f, 0.282f, 0.360f, 1.0f);
		Bottom = ColorRGBA(0.176f, 0.202f, 0.266f, 1.0f);
		Border = ColorRGBA(0.760f, 0.620f, 0.260f, 1.0f);
		TextColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		Top = ColorRGBA(0.204f, 0.232f, 0.300f, 1.0f);
		Bottom = ColorRGBA(0.140f, 0.158f, 0.208f, 1.0f);
		Border = CASINO_EDGE;
		TextColor = ColorRGBA(0.902f, 0.925f, 0.965f, 1.0f);
	}

	Shadow(pRect, Enabled ? 0.30f : 0.16f);
	Frame(pRect, Lerp(Top, Bottom, 0.5f), Border, 4.5f);
	Grad(pRect, Top, Bottom, 4.5f);

	if(Active || Hovered)
	{
		CUIRect Sheen;
		Sheen.x = pRect->x + 3.0f;
		Sheen.w = pRect->w - 6.0f;
		Sheen.y = pRect->y + 1.5f;
		Sheen.h = 1.0f;
		Sheen.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Active ? 0.45f : 0.18f), IGraphics::CORNER_NONE, 0.0f);
	}

	Label(pRect, pText, FontSize, TEXTALIGN_MC, TextColor);
	return Clicked;
}

void CCasino::BetRow(CUIRect *pRect, int *pBet, int MinBet, int MaxBet, bool Enabled)
{
	CUIRect Row = *pRect;
	Row.HSplitTop(24.0f, &Row, nullptr);

	CUIRect Minus, Value, Plus;
	Row.VSplitLeft(24.0f, &Minus, &Row);
	Row.VSplitRight(24.0f, &Row, &Plus);
	Row.VSplitLeft(4.0f, nullptr, &Value);
	Value.VSplitRight(4.0f, &Value, nullptr);

	if(Button(&Minus, "-", false, Enabled && *pBet > MinBet, 15.0f))
		*pBet = std::max(MinBet, *pBet - 10);

	CUIRect Inset = Value;
	Inset.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.42f), IGraphics::CORNER_ALL, CASINO_RADIUS);
	CUIRect Inner = Value;
	Inner.Margin(1.0f, &Inner);
	Grad(&Inner, CASINO_BG_DEEP, Shade(CASINO_BG_DEEP, 1.7f), 3.0f);

	Coin(vec2(Value.x + 13.0f, Value.y + Value.h * 0.5f), 4.5f);

	// typed amount is authoritative while focused; the input renders the text itself (caret, selection, no box)
	CUIRect Text = Value;
	Text.x += 22.0f;
	Text.w -= 26.0f;

	const bool FieldHovered = Enabled && Ui()->MouseHovered(&Text);
	if(FieldHovered && Ui()->MouseButtonClicked(0))
	{
		m_BetInput.SetInteger(*pBet);
		Ui()->SetActiveItem(&m_BetInput);
		m_BetInput.Activate(EInputPriority::UI);
		m_BetInput.SelectAll();
	}
	else if(!FieldHovered && Ui()->MouseButtonClicked(0))
	{
		m_BetInput.Deactivate();
		if(Ui()->ActiveItem() == &m_BetInput)
			Ui()->SetActiveItem(nullptr);
	}

	// the amount is locked while a round is in flight
	if(!Enabled && m_BetInput.IsActive())
	{
		m_BetInput.Deactivate();
		if(Ui()->ActiveItem() == &m_BetInput)
			Ui()->SetActiveItem(nullptr);
	}

	if(m_BetInput.IsActive() && !Ui()->Enabled())
		m_BetInput.Deactivate();

	if(!m_BetInput.IsActive())
		m_BetInput.SetInteger(*pBet);
	else
	{
		*pBet = std::clamp(m_BetInput.GetInteger(), MinBet, MaxBet);
		Inset.DrawOutline(ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.75f));
	}

	CUIRect Textbox = Text;
	Textbox.VMargin(2.0f, &Textbox);
	TextRender()->TextColor(m_BetInput.IsActive() ? ColorRGBA(1.0f, 0.960f, 0.840f, 1.0f) : CASINO_GOLD);
	m_BetInput.Render(&Textbox, 14.0f, TEXTALIGN_MR, false, Textbox.w, 0.0f);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	if(Button(&Plus, "+", false, Enabled && *pBet < MaxBet, 15.0f))
		*pBet = std::min(MaxBet, *pBet + 10);
}

void CCasino::DrawCard(const CUIRect *pRect, const SCasinoCard &Card, bool Hidden, bool Highlight)
{
	Shadow(pRect, 0.42f);

	const ColorRGBA Border = Highlight ? CASINO_GOLD : ColorRGBA(0.62f, 0.65f, 0.72f, 1.0f);
	pRect->Draw(Border, IGraphics::CORNER_ALL, CASINO_RADIUS);

	CUIRect Face = *pRect;
	Face.Margin(1.4f, &Face);
	if(Hidden)
	{
		Grad(&Face, ColorRGBA(0.145f, 0.220f, 0.430f, 1.0f), ColorRGBA(0.072f, 0.110f, 0.245f, 1.0f), 4.5f);

		CUIRect Emblem = Face;
		Emblem.Margin(Face.w * 0.26f, &Emblem);
		Emblem.Draw(ColorRGBA(0.780f, 0.640f, 0.280f, 0.85f), IGraphics::CORNER_ALL, 2.0f);
		CUIRect EmblemInner = Emblem;
		EmblemInner.Margin(2.0f, &EmblemInner);
		EmblemInner.Draw(ColorRGBA(0.115f, 0.180f, 0.380f, 1.0f), IGraphics::CORNER_ALL, 1.0f);

		if(Highlight)
		{
			CUIRect Ring = *pRect;
			Ring.DrawOutline(CASINO_GOLD);
		}
		return;
	}

	Grad(&Face, ColorRGBA(0.995f, 0.995f, 1.000f, 1.0f), ColorRGBA(0.878f, 0.892f, 0.925f, 1.0f), 4.5f);

	const bool Red = Card.m_Suit == 1 || Card.m_Suit == 2;
	const ColorRGBA Ink = Red ? ColorRGBA(0.780f, 0.130f, 0.185f, 1.0f) : ColorRGBA(0.098f, 0.105f, 0.145f, 1.0f);

	char aRank[8];
	if(Card.m_Rank == 0)
		str_copy(aRank, "A", sizeof(aRank));
	else if(Card.m_Rank == 10)
		str_copy(aRank, "J", sizeof(aRank));
	else if(Card.m_Rank == 11)
		str_copy(aRank, "Q", sizeof(aRank));
	else if(Card.m_Rank == 12)
		str_copy(aRank, "K", sizeof(aRank));
	else
		str_format(aRank, sizeof(aRank), "%d", Card.m_Rank + 1);

	const char *pSuit = "?";
	switch(Card.m_Suit)
	{
	case 0: pSuit = "S"; break;
	case 1: pSuit = "H"; break;
	case 2: pSuit = "D"; break;
	case 3: pSuit = "C"; break;
	}

	const float Scale = pRect->w / 62.0f;

	CUIRect Top = Face;
	Top.Margin(3.0f * Scale, &Top);
	Top.HSplitTop(13.0f * Scale, &Top, nullptr);
	Label(&Top, aRank, 13.0f * Scale, TEXTALIGN_ML, Ink);

	CUIRect TopSuit = Face;
	TopSuit.Margin(3.0f * Scale, &TopSuit);
	TopSuit.HSplitTop(13.0f * Scale, nullptr, &TopSuit);
	TopSuit.HSplitTop(11.0f * Scale, &TopSuit, nullptr);
	Label(&TopSuit, pSuit, 10.0f * Scale, TEXTALIGN_ML, Ink);

	CUIRect Mid = Face;
	Mid.Margin(4.0f * Scale, &Mid);
	Mid.HSplitTop(Mid.h * 0.28f, nullptr, &Mid);
	Mid.HSplitBottom(Mid.h * 0.28f, &Mid, nullptr);
	Label(&Mid, pSuit, std::min(Mid.h, 34.0f * Scale), TEXTALIGN_MC, Ink);

	CUIRect Bottom = Face;
	Bottom.Margin(3.0f * Scale, &Bottom);
	Bottom.HSplitBottom(13.0f * Scale, nullptr, &Bottom);
	Label(&Bottom, aRank, 13.0f * Scale, TEXTALIGN_MR, Ink);

	if(Highlight)
	{
		CUIRect Ring = *pRect;
		Ring.DrawOutline(CASINO_GOLD);
	}
}

void CCasino::DrawDie(const CUIRect *pRect, int Value, bool Highlight)
{
	Shadow(pRect, 0.40f);

	const ColorRGBA FaceTop = Highlight ? ColorRGBA(1.000f, 0.965f, 0.880f, 1.0f) : ColorRGBA(0.965f, 0.968f, 0.985f, 1.0f);
	const ColorRGBA FaceBottom = Highlight ? ColorRGBA(0.930f, 0.868f, 0.720f, 1.0f) : ColorRGBA(0.812f, 0.828f, 0.870f, 1.0f);
	const ColorRGBA Border = Highlight ? CASINO_GOLD : ColorRGBA(0.560f, 0.580f, 0.640f, 1.0f);

	Frame(pRect, Lerp(FaceTop, FaceBottom, 0.5f), Border, 7.0f);
	Grad(pRect, FaceTop, FaceBottom, 7.0f);

	const float P = pRect->w * 0.085f;
	const float Cx = pRect->x + pRect->w * 0.5f;
	const float Cy = pRect->y + pRect->h * 0.5f;
	const float Off = pRect->w * 0.235f;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.115f, 0.120f, 0.160f, 1.0f);

	auto Dot = [&](float x, float y) {
		Graphics()->DrawCircle(x, y, P * 1.35f, 12);
	};

	if(Value == 1 || Value == 3 || Value == 5)
		Dot(Cx, Cy);
	if(Value >= 2)
	{
		Dot(Cx - Off, Cy - Off);
		Dot(Cx + Off, Cy + Off);
	}
	if(Value >= 4)
	{
		Dot(Cx + Off, Cy - Off);
		Dot(Cx - Off, Cy + Off);
	}
	if(Value == 6)
	{
		Dot(Cx - Off, Cy);
		Dot(Cx + Off, Cy);
	}

	Graphics()->QuadsEnd();
}

void CCasino::ReleaseTextInput()
{
	m_BetInput.Deactivate();
}

void CCasino::OnUpdate()
{
	// timers run off-screen so a spin never freezes or stalls the save
	Tick(std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f));
}

void CCasino::Render(CUIRect View)
{
	// flat shell: one dark tone, a faint grid and one gold accent rule
	View.Draw(CASINO_BG, IGraphics::CORNER_ALL, CASINO_RADIUS);
	{
		Ui()->ClipEnable(&View);

		const float GridStep = 26.0f;
		const ColorRGBA GridColor(1.0f, 1.0f, 1.0f, 0.016f);
		for(float x = View.x + GridStep; x < View.x + View.w - 1.0f; x += GridStep)
		{
			CUIRect Line;
			Line.x = x;
			Line.y = View.y;
			Line.w = 1.0f;
			Line.h = View.h;
			Line.Draw(GridColor, IGraphics::CORNER_NONE, 0.0f);
		}
		for(float y = View.y + GridStep; y < View.y + View.h - 1.0f; y += GridStep)
		{
			CUIRect Line;
			Line.x = View.x;
			Line.y = y;
			Line.w = View.w;
			Line.h = 1.0f;
			Line.Draw(GridColor, IGraphics::CORNER_NONE, 0.0f);
		}

		Ui()->ClipDisable();
	}

	CUIRect Header, Body;
	View.HSplitTop(74.0f, &Header, &Body);
	RenderHeader(&Header);

	{
		// single accent rule under the header, anchored by a short brighter segment on the left
		CUIRect AccentLine;
		AccentLine.x = View.x;
		AccentLine.y = Body.y - 1.0f;
		AccentLine.w = View.w;
		AccentLine.h = 1.0f;
		AccentLine.Draw(ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.30f), IGraphics::CORNER_NONE, 0.0f);

		CUIRect Anchor = AccentLine;
		Anchor.w = std::min(46.0f, View.w * 0.25f);
		Anchor.h = 2.0f;
		Anchor.y = AccentLine.y - 1.0f;
		Anchor.Draw(CASINO_GOLD, IGraphics::CORNER_NONE, 0.0f);
	}

	CUIRect List, Game;
	Body.VSplitLeft(214.0f, &List, &Game);
	RenderGameList(&List);

	CUIRect VLine;
	VLine.x = Game.x - 1.0f;
	VLine.w = 1.0f;
	VLine.y = Body.y;
	VLine.h = Body.h;
	VLine.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_NONE, 0.0f);

	switch(m_SelectedGame)
	{
	case CASINO_GAME_SLOT: RenderSlot(&Game); break;
	case CASINO_GAME_WHEEL: RenderWheel(&Game); break;
	case CASINO_GAME_BLACKJACK: RenderBlackjack(&Game); break;
	case CASINO_GAME_CARIBBEAN: RenderCaribbean(&Game); break;
	case CASINO_GAME_HILO: RenderHiLo(&Game); break;
	case CASINO_GAME_DRAGON_TIGER: RenderDragonTiger(&Game); break;
	case CASINO_GAME_CRAPS: RenderCraps(&Game); break;
	case CASINO_GAME_SICBO: RenderSicBo(&Game); break;
	case CASINO_GAME_LOTTERY: RenderLottery(&Game); break;
	case CASINO_GAME_CRASH: RenderCrash(&Game); break;
	case CASINO_GAME_MINES: RenderMines(&Game); break;
	case CASINO_GAME_DICE: RenderDice(&Game); break;
	default: break;
	}

	if(m_ToastTime > 0.0f)
	{
		const float Alpha = std::min(1.0f, m_ToastTime);
		CUIRect ToastRect;
		ToastRect.w = 300.0f;
		ToastRect.h = 34.0f;
		ToastRect.x = View.x + (View.w - ToastRect.w) * 0.5f;
		ToastRect.y = View.y + View.h - 52.0f;

		ToastRect.Draw(ColorRGBA(0.086f, 0.098f, 0.128f, 0.97f * Alpha), IGraphics::CORNER_ALL, CASINO_RADIUS);
		ToastRect.DrawOutline(ColorRGBA(0.400f, 0.780f, 0.560f, 0.85f * Alpha));

		CUIRect Bar = ToastRect;
		Bar.w = 3.0f;
		Bar.Draw(ColorRGBA(CASINO_ACCENT.r, CASINO_ACCENT.g, CASINO_ACCENT.b, Alpha), IGraphics::CORNER_NONE, 0.0f);

		Label(&ToastRect, m_aToast, 13.0f, TEXTALIGN_MC, ColorRGBA(0.880f, 1.000f, 0.930f, Alpha));
	}
}

void CCasino::RenderHeader(CUIRect *pRect)
{
	CUIRect Row = *pRect;
	Row.Margin(14.0f, &Row);
	Row.VMargin(4.0f, &Row);

	CUIRect Brand, Rest;
	Row.VSplitLeft(168.0f, &Brand, &Rest);

	Coin(vec2(Brand.x + 14.0f, Brand.y + Brand.h * 0.5f), 11.0f);

	CUIRect BrandText = Brand;
	BrandText.x += 36.0f;
	BrandText.w -= 36.0f;
	CUIRect BrandTitle, BrandSub;
	BrandText.HSplitTop(BrandText.h * 0.58f, &BrandTitle, &BrandSub);
	Label(&BrandTitle, "CASINO", 21.0f, TEXTALIGN_ML, CASINO_GOLD);
	Label(&BrandSub, "FUSION GAMING", 8.5f, TEXTALIGN_ML, CASINO_DIM);

	char aBuf[160];

	const float ChipH = std::min(46.0f, pRect->h - 16.0f);
	const float ChipY = pRect->y + (pRect->h - ChipH) * 0.5f;

	CUIRect Daily;
	Row.VSplitRight(146.0f, &Row, &Daily);
	Daily.h = 30.0f;
	Daily.y = pRect->y + (pRect->h - Daily.h) * 0.5f;
	Daily.x += 10.0f;
	Daily.w -= 10.0f;

	CUIRect Credits;
	Row.VSplitRight(190.0f, &Row, &Credits);
	Credits.h = ChipH;
	Credits.y = ChipY;
	Credits.x += 12.0f;
	Credits.w -= 12.0f;

	CUIRect Stats;
	const bool ShowStats = Row.w > 320.0f;
	if(ShowStats)
	{
		Row.VSplitRight(250.0f, &Row, &Stats);
		Stats.h = ChipH;
		Stats.y = ChipY;
		Stats.x += 14.0f;
		Stats.w -= 14.0f;
	}

	if(m_DailyFrozen)
	{
		Pill(&Daily, "DAILY LOCKED", ColorRGBA(0.180f, 0.078f, 0.086f, 1.0f), ColorRGBA(0.960f, 0.640f, 0.660f, 1.0f), 10.5f);
	}
	else if(m_DailyReady)
	{
		if(Button(&Daily, "CLAIM 500", true, true, 11.5f))
		{
			// a bonus is not a win: it must not inflate won or set the best-round record
			AddCredits(500);
			g_Config.m_FcCasinoLastDaily = (int)time_timestamp();
			DailyStampHash(g_Config.m_FcCasinoLastDaily, g_Config.m_FcCasinoDailyStamp, sizeof(g_Config.m_FcCasinoDailyStamp));
			SyncToConfig();
			m_DailyFrozenStamp = -1; // the stamp changed, so the cached verdict is stale
			CheckDaily();
			Toast("Daily bonus collected: +500 credits");
		}
	}
	else
	{
		const int64_t Now = time_timestamp();
		const int Remaining = std::max(0, (int)(86400 - (Now - g_Config.m_FcCasinoLastDaily)));
		str_format(aBuf, sizeof(aBuf), "DAILY IN %02d:%02d:%02d", Remaining / 3600, (Remaining / 60) % 60, Remaining % 60);
		Pill(&Daily, aBuf, ColorRGBA(0.098f, 0.108f, 0.140f, 1.0f), CASINO_DIM, 10.5f);
	}

	// credits: flat plate, 1px gold rule, no gloss
	Credits.Draw(ColorRGBA(0.086f, 0.076f, 0.048f, 1.0f), IGraphics::CORNER_ALL, CASINO_RADIUS);
	Credits.DrawOutline(ColorRGBA(CASINO_GOLD_DEEP.r, CASINO_GOLD_DEEP.g, CASINO_GOLD_DEEP.b, 0.95f));

	Coin(vec2(Credits.x + 25.0f, Credits.y + Credits.h * 0.5f), 10.0f);

	CUIRect ChipText = Credits;
	ChipText.x += 46.0f;
	ChipText.w -= 54.0f;

	CUIRect ChipValue, ChipCaption;
	ChipText.HSplitTop(ChipText.h * 0.58f, &ChipValue, &ChipCaption);
	str_format(aBuf, sizeof(aBuf), "%d", m_Credits);
	Label(&ChipValue, aBuf, 19.0f, TEXTALIGN_ML, ColorRGBA(1.000f, 0.900f, 0.620f, 1.0f));
	Label(&ChipCaption, "CREDITS", 8.5f, TEXTALIGN_ML, ColorRGBA(0.745f, 0.625f, 0.330f, 1.0f));

	if(!ShowStats)
		return;

	// stats: three columns split by hairlines, no framed box
	CUIRect StatsLine;
	StatsLine.x = Stats.x - 1.0f;
	StatsLine.y = Stats.y;
	StatsLine.w = 1.0f;
	StatsLine.h = Stats.h;
	StatsLine.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f), IGraphics::CORNER_NONE, 0.0f);

	CUIRect Inner = Stats;
	Inner.Margin(12.0f, &Inner);

	const float ColW = (Inner.w - 24.0f) / 3.0f;
	CUIRect ColA = Inner;
	ColA.w = ColW;
	CUIRect ColB = Inner;
	ColB.x += ColW + 12.0f;
	ColB.w = ColW;
	CUIRect ColC = Inner;
	ColC.x += (ColW + 12.0f) * 2.0f;
	ColC.w = ColW;

	for(int i = 1; i <= 2; i++)
	{
		CUIRect Sep;
		Sep.x = Inner.x + (ColW + 12.0f) * i - 6.0f;
		Sep.y = Inner.y + Inner.h * 0.18f;
		Sep.w = 1.0f;
		Sep.h = Inner.h * 0.64f;
		Sep.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_NONE, 0.0f);
	}

	auto StatColumn = [&](CUIRect Col, const char *pCaption, int Value, ColorRGBA ValueColor) {
		CUIRect Cap, Val;
		Col.HSplitTop(Col.h * 0.40f, &Cap, &Val);
		Label(&Cap, pCaption, 7.5f, TEXTALIGN_MC, CASINO_DIM);
		char aVal[24];
		str_format(aVal, sizeof(aVal), "%d", Value);
		Label(&Val, aVal, 13.0f, TEXTALIGN_MC, ValueColor);
	};

	StatColumn(ColA, "WAGERED", m_TotalWagered, ColorRGBA(0.820f, 0.855f, 0.930f, 1.0f));
	StatColumn(ColB, "WON", m_TotalWon, CASINO_ACCENT);
	StatColumn(ColC, "BEST", m_BiggestWin, CASINO_GOLD);
}

void CCasino::RenderGameList(CUIRect *pRect)
{
	static const char *const s_apGlyphs[CASINO_GAME_COUNT] = {
		"7", "W", "21", "CS", "HL", "DT", "CR", "SB", "LT", "X", "MN", "DC"};

	const float Gap = 2.0f;
	const float BaseRowH = 38.0f;

	CUIRect PanelRect = *pRect;
	PanelRect.Margin(10.0f, &PanelRect);
	PanelRect.VMargin(2.0f, &PanelRect);

	PanelRect.Draw(ColorRGBA(0.052f, 0.058f, 0.076f, 1.0f), IGraphics::CORNER_ALL, CASINO_RADIUS);
	PanelRect.DrawOutline(ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f));

	CUIRect List = PanelRect;
	List.Margin(8.0f, &List);

	CUIRect Head, HeadCount;
	List.HSplitTop(22.0f, &Head, &List);
	Head.VMargin(4.0f, &Head);
	Head.VSplitRight(58.0f, &Head, &HeadCount);
	Label(&Head, "GAME SELECT", 9.0f, TEXTALIGN_ML, CASINO_DIM);
	char aCount[32];
	str_format(aCount, sizeof(aCount), "%d TABLES", (int)CASINO_GAME_COUNT);
	Label(&HeadCount, aCount, 8.0f, TEXTALIGN_MR, ColorRGBA(0.360f, 0.395f, 0.470f, 1.0f));

	CUIRect Rule;
	Rule.x = List.x + 2.0f;
	Rule.y = List.y;
	Rule.w = List.w - 4.0f;
	Rule.h = 1.0f;
	Rule.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f), IGraphics::CORNER_NONE, 0.0f);
	List.HSplitTop(8.0f, nullptr, &List);

	CUIRect Clip = List;
	Clip.VSplitRight(10.0f, &Clip, nullptr);

	const int RowsFit = std::max(1, std::min((int)CASINO_GAME_COUNT, (int)std::floor((Clip.h + Gap) / (BaseRowH + Gap))));
	const float Step = RowsFit >= (int)CASINO_GAME_COUNT ? (BaseRowH + Gap) : Clip.h / (float)RowsFit;
	const float RowH = Step - Gap;

	const int MaxScrollRows = std::max(0, CASINO_GAME_COUNT - RowsFit);
	const float MaxScroll = (float)MaxScrollRows * Step;

	if(MaxScroll > 0.0f && Ui()->MouseHovered(&Clip))
	{
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
			m_GameListScroll -= Step;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
			m_GameListScroll += Step;
	}

	m_GameListScroll = std::round(std::clamp(m_GameListScroll, 0.0f, MaxScroll) / Step) * Step;

	Ui()->ClipEnable(&Clip);
	for(int i = 0; i < CASINO_GAME_COUNT; i++)
	{
		CUIRect Row;
		Row.x = Clip.x;
		Row.w = Clip.w;
		Row.h = RowH;
		Row.y = Clip.y + (float)i * Step - m_GameListScroll;

		if(Row.y + Row.h > Clip.y + Clip.h + 0.5f || Row.y < Clip.y - 0.5f)
			continue;

		const bool Selected = m_SelectedGame == i;
		const bool Hovered = Ui()->MouseHovered(&Row);

		// rows are flat blocks separated by a hairline; selection is a tone step plus a rail
		if(Selected)
			Row.Draw(ColorRGBA(0.166f, 0.138f, 0.056f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
		else if(Hovered)
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.045f), IGraphics::CORNER_NONE, 0.0f);

		CUIRect RowRule;
		RowRule.x = Row.x;
		RowRule.y = Row.y + Row.h - 1.0f;
		RowRule.w = Row.w;
		RowRule.h = 1.0f;
		RowRule.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Selected ? 0.10f : 0.05f), IGraphics::CORNER_NONE, 0.0f);

		if(Selected)
		{
			CUIRect Accent;
			Accent.x = Row.x;
			Accent.y = Row.y;
			Accent.w = 2.0f;
			Accent.h = Row.h - 1.0f;
			Accent.Draw(CASINO_GOLD, IGraphics::CORNER_NONE, 0.0f);
		}

		CUIRect Badge;
		Badge.x = Row.x + 10.0f;
		Badge.y = Row.y + (Row.h - 24.0f) * 0.5f;
		Badge.w = 24.0f;
		Badge.h = 24.0f;

		if(Selected)
		{
			Badge.Draw(CASINO_GOLD, IGraphics::CORNER_ALL, CASINO_RADIUS);
			Label(&Badge, s_apGlyphs[i], 10.0f, TEXTALIGN_MC, ColorRGBA(0.120f, 0.090f, 0.020f, 1.0f));
		}
		else
		{
			Badge.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, Hovered ? 0.22f : 0.13f));
			Label(&Badge, s_apGlyphs[i], 10.0f, TEXTALIGN_MC, ColorRGBA(0.780f, 0.820f, 0.900f, 1.0f));
		}

		CUIRect Name = Row;
		Name.x += 44.0f;
		Name.w -= 56.0f;
		Name.HSplitTop(Row.h * 0.54f, &Name, nullptr);
		Label(&Name, s_apGameNames[i], 13.0f, TEXTALIGN_ML, Selected ? CASINO_GOLD : ColorRGBA(0.905f, 0.925f, 0.965f, 1.0f));

		CUIRect Tag = Row;
		Tag.x += 45.0f;
		Tag.w -= 56.0f;
		Tag.HSplitTop(Row.h * 0.50f, nullptr, &Tag);
		Label(&Tag, s_apGameTags[i], 9.0f, TEXTALIGN_ML, Selected ? ColorRGBA(0.820f, 0.700f, 0.360f, 1.0f) : CASINO_DIM);

		if(Ui()->MouseButtonClicked(0) && Hovered)
			m_SelectedGame = i;
	}
	Ui()->ClipDisable();

	if(MaxScroll > 0.0f)
	{
		CUIRect Rail;
		Rail.x = Clip.x + Clip.w + 4.0f;
		Rail.w = 2.0f;
		Rail.y = Clip.y;
		Rail.h = Clip.h;
		Rail.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f), IGraphics::CORNER_NONE, 0.0f);

		CUIRect Knob;
		Knob.x = Rail.x;
		Knob.w = Rail.w;
		Knob.h = std::max(28.0f, Rail.h * ((float)RowsFit / (float)CASINO_GAME_COUNT));
		Knob.y = Rail.y + (Rail.h - Knob.h) * (m_GameListScroll / MaxScroll);
		Knob.Draw(ColorRGBA(0.420f, 0.450f, 0.530f, 0.9f), IGraphics::CORNER_NONE, 0.0f);
	}
}

static int SlotPickSymbol(unsigned int Random)
{
	int Total = 0;
	for(int i = 0; i < 6; i++)
		Total += s_aSlotWeight[i];
	int Roll = (int)(Random % (unsigned int)Total);
	for(int i = 0; i < 6; i++)
	{
		if(Roll < s_aSlotWeight[i])
			return i;
		Roll -= s_aSlotWeight[i];
	}
	return 5;
}

void CCasino::SlotSpin()
{
	if(m_SlotState == 1)
		return;
	const int TotalBet = m_SlotBet * m_SlotLines;
	if(m_Credits < TotalBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(TotalBet);
	RecordBet(TotalBet);
	// the payout is scored against the bet that was actually taken
	m_SlotRoundBet = m_SlotBet;

	// the strip starts from the visible symbols so the spin does not snap to a fresh set
	for(int Reel = 0; Reel < 3; Reel++)
		for(int Row = 0; Row < 3; Row++)
			m_aSlotPrevReels[Reel][Row] = m_aSlotReels[Reel][Row];

	for(int Reel = 0; Reel < 3; Reel++)
		for(int Row = 0; Row < 3; Row++)
			m_aSlotReels[Reel][Row] = SlotPickSymbol(NextRandom());

	m_SlotState = 1;
	m_SlotSpinSeed = (unsigned int)RandInt(1 << 20) + 1u;
	for(int Reel = 0; Reel < 3; Reel++)
	{
		m_aSlotReelTime[Reel] = 0.85f + (float)Reel * 0.32f + RandFloat() * 0.10f;
		m_aSlotReelDuration[Reel] = m_aSlotReelTime[Reel];
		// whole number of windows, so the window travels from the visible symbols to the result
		m_aSlotReelTravel[Reel] = (float)(SLOT_STRIP_LEN - 3);
		m_aSlotReelStopAnim[Reel] = 0.0f;
	}
	m_SlotLastWin = 0;
	for(int i = 0; i < 5; i++)
		m_aSlotLineWin[i] = 0;
}

void CCasino::SlotSettle()
{
	m_SlotState = 2;

	int64_t Total = 0;
	for(int Line = 0; Line < m_SlotLines; Line++)
	{
		const int A = m_aSlotReels[s_aSlotLines[Line][0][0]][s_aSlotLines[Line][0][1]];
		const int B = m_aSlotReels[s_aSlotLines[Line][1][0]][s_aSlotLines[Line][1][1]];
		const int C = m_aSlotReels[s_aSlotLines[Line][2][0]][s_aSlotLines[Line][2][1]];
		if(A == B && B == C)
		{
			const int Win = ClampWin((int64_t)s_aSlotPay[A] * m_SlotRoundBet);
			m_aSlotLineWin[Line] = Win;
			Total += Win;
		}
	}

	const int Payout = ClampWin(Total);
	m_SlotLastWin = Payout;
	if(Payout > 0)
	{
		AddCredits(Payout);
		RecordWin(Payout, m_SlotRoundBet * m_SlotLines);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Win %d credits", Payout);
		Toast(aBuf);
	}
}

void CCasino::WheelSpin()
{
	if(m_WheelState == 1)
		return;
	if(m_Credits < m_WheelBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_WheelBet);
	RecordBet(m_WheelBet);

	int Total = 0;
	for(int i = 0; i < 7; i++)
		Total += s_aWheelWeight[i];
	int Roll = RandInt(Total);
	int Segment = 6;
	for(int i = 0; i < 7; i++)
	{
		if(Roll < s_aWheelWeight[i])
		{
			Segment = i;
			break;
		}
		Roll -= s_aWheelWeight[i];
	}

	m_WheelResult = Segment;
	m_WheelState = 1;

	// land the pointer (angle 3*pi/2, straight up) on the middle of the rolled segment
	float Offset = 0.0f;
	for(int i = 0; i < Segment; i++)
		Offset += (float)s_aWheelWeight[i];
	const float Mid = (Offset + 0.5f * (float)s_aWheelWeight[Segment]) / (float)Total * 2.0f * pi;
	float Target = 3.0f * pi * 0.5f - Mid;
	while(Target < m_WheelAngle + 4.0f * 2.0f * pi)
		Target += 2.0f * pi;

	m_WheelSpinStart = m_WheelAngle;
	m_WheelSpinTarget = Target;
	m_WheelSpinTimer = 0.0f;
	m_WheelSpinDuration = 3.0f + RandFloat() * 1.5f;

	// balance moves only when the pointer lands
	m_WheelPendingWin = ClampWin((int64_t)std::llround((double)m_WheelBet * s_aWheelValue[Segment]));
	m_WheelLastWin = 0;
}

void CCasino::BlackjackDeal()
{
	if(m_Blackjack.m_State != 0 && m_Blackjack.m_State != 4)
		return;
	if(m_Credits < m_Blackjack.m_Stake)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_Blackjack.m_Stake);
	RecordBet(m_Blackjack.m_Stake);

	m_Blackjack.m_vDealer.clear();
	m_Blackjack.m_vHands.clear();
	m_Blackjack.m_ActiveHand = 0;
	m_Blackjack.m_LastWin = 0;
	m_Blackjack.m_HoleHidden = true;

	SCasinoBlackjackHand Hand;
	Hand.m_Stake = m_Blackjack.m_Stake;
	m_Blackjack.m_vHands.push_back(Hand);

	str_copy(m_Blackjack.m_aMessage, "", sizeof(m_Blackjack.m_aMessage));

	// the four opening cards reveal one by one in Tick
	m_Blackjack.m_State = 5;
	m_Blackjack.m_Timer = 0.30f;
}

void CCasino::BlackjackHit()
{
	if(m_Blackjack.m_State != 1)
		return;
	auto &Hand = m_Blackjack.m_vHands[m_Blackjack.m_ActiveHand];
	if(Hand.m_Done)
		return;

	Hand.m_vCards.push_back(DrawCardFromShoe());
	const int Total = BlackjackTotal(Hand.m_vCards);
	if(Total > 21)
	{
		Hand.m_Done = true;
		str_copy(m_Blackjack.m_aMessage, "Bust", sizeof(m_Blackjack.m_aMessage));
		BlackjackStand();
	}
	else if(Total == 21)
	{
		Hand.m_Done = true;
		BlackjackStand();
	}
}

void CCasino::BlackjackStand()
{
	if(m_Blackjack.m_State != 1)
		return;

	m_Blackjack.m_vHands[m_Blackjack.m_ActiveHand].m_Done = true;

	bool AnyOpen = false;
	for(int i = 0; i < (int)m_Blackjack.m_vHands.size(); i++)
	{
		if(!m_Blackjack.m_vHands[i].m_Done)
		{
			m_Blackjack.m_ActiveHand = i;
			AnyOpen = true;
			break;
		}
	}

	if(AnyOpen)
		return;

	m_Blackjack.m_State = 2;
	m_Blackjack.m_Timer = 0.55f;
}

void CCasino::BlackjackDouble()
{
	if(m_Blackjack.m_State != 1)
		return;
	auto &Hand = m_Blackjack.m_vHands[m_Blackjack.m_ActiveHand];
	if(Hand.m_Done || Hand.m_vCards.size() != 2)
		return;
	if(m_Credits < Hand.m_Stake)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(Hand.m_Stake);
	RecordBet(Hand.m_Stake);
	Hand.m_Stake *= 2;
	Hand.m_Doubled = true;
	Hand.m_vCards.push_back(DrawCardFromShoe());
	Hand.m_Done = true;
	BlackjackStand();
}

void CCasino::BlackjackSplit()
{
	if(m_Blackjack.m_State != 1)
		return;
	if(m_Blackjack.m_vHands.size() >= 2)
		return;
	auto &Hand = m_Blackjack.m_vHands[m_Blackjack.m_ActiveHand];
	if(Hand.m_vCards.size() != 2)
		return;
	if(CardValue(Hand.m_vCards[0]) != CardValue(Hand.m_vCards[1]))
		return;
	if(m_Credits < Hand.m_Stake)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(Hand.m_Stake);
	RecordBet(Hand.m_Stake);

	SCasinoCard Second = Hand.m_vCards[1];
	Hand.m_vCards.pop_back();
	Hand.m_vCards.push_back(DrawCardFromShoe());

	SCasinoBlackjackHand Split;
	Split.m_Stake = Hand.m_Stake;
	Split.m_FromSplit = true;
	Split.m_vCards.push_back(Second);
	Split.m_vCards.push_back(DrawCardFromShoe());
	m_Blackjack.m_vHands.push_back(Split);
}

void CCasino::BlackjackSurrender()
{
	if(m_Blackjack.m_State != 1)
		return;
	if(m_Blackjack.m_vHands.size() != 1)
		return;
	auto &Hand = m_Blackjack.m_vHands[0];
	if(Hand.m_vCards.size() != 2 || Hand.m_Doubled)
		return;

	Hand.m_Surrendered = true;
	Hand.m_Done = true;
	const int Refund = Hand.m_Stake / 2;
	AddCredits(Refund);
	RecordWin(0, Hand.m_Stake - Refund);
	// half the stake is gone, so this is a loss for the banner even though credits came back
	m_Blackjack.m_LastWin = 0;
	str_format(m_Blackjack.m_aMessage, sizeof(m_Blackjack.m_aMessage), "Surrendered  -%d", Hand.m_Stake - Refund);
	m_Blackjack.m_State = 4;
	m_Blackjack.m_HoleHidden = false;
}

void CCasino::BlackjackDealerPlay()
{
	m_Blackjack.m_HoleHidden = false;
	m_Blackjack.m_State = 3;

	bool AnyLive = false;
	for(const auto &Hand : m_Blackjack.m_vHands)
		if(!Hand.m_Surrendered && BlackjackTotal(Hand.m_vCards) <= 21)
			AnyLive = true;

	if(AnyLive)
	{
		while(BlackjackTotal(m_Blackjack.m_vDealer) < 17)
			m_Blackjack.m_vDealer.push_back(DrawCardFromShoe());
	}

	BlackjackSettle();
}

void CCasino::BlackjackSettle()
{
	const int DealerTotal = BlackjackTotal(m_Blackjack.m_vDealer);
	const bool DealerBust = DealerTotal > 21;
	const bool DealerNatural = m_Blackjack.m_vDealer.size() == 2 && DealerTotal == 21;

	int64_t TotalWin = 0;
	int64_t Net = 0;
	int Staked = 0;
	for(const auto &Hand : m_Blackjack.m_vHands)
		Staked += Hand.m_Stake;

	for(auto &Hand : m_Blackjack.m_vHands)
	{
		if(Hand.m_Surrendered)
		{
			Net -= Hand.m_Stake - Hand.m_Stake / 2;
			continue;
		}

		const int PlayerTotal = BlackjackTotal(Hand.m_vCards);
		const bool PlayerNatural = !Hand.m_FromSplit && Hand.m_vCards.size() == 2 && PlayerTotal == 21;

		if(PlayerTotal > 21)
		{
			Net -= Hand.m_Stake;
			continue;
		}
		if(PlayerNatural && DealerNatural)
		{
			// both naturals: the stake comes back and no 3:2 is paid
			TotalWin += Hand.m_Stake;
			continue;
		}
		if(PlayerNatural)
		{
			const int Win = Hand.m_Stake + (Hand.m_Stake * 3) / 2;
			TotalWin += Win;
			Net += Win - Hand.m_Stake;
			continue;
		}
		if(DealerNatural)
		{
			Net -= Hand.m_Stake;
			continue;
		}
		if(DealerBust || PlayerTotal > DealerTotal)
		{
			const int Win = Hand.m_Stake * 2;
			TotalWin += Win;
			Net += Win - Hand.m_Stake;
		}
		else if(PlayerTotal == DealerTotal)
		{
			TotalWin += Hand.m_Stake;
			Net += 0;
		}
		else
		{
			Net -= Hand.m_Stake;
		}
	}

	const int Payout = ClampWin(TotalWin);
	m_Blackjack.m_LastWin = Payout;
	if(Payout > 0)
	{
		AddCredits(Payout);
		RecordWin(Payout, Staked);
	}

	const int NetDisplay = (int)std::clamp<int64_t>(Net, -1000000000, 1000000000);
	char aBuf[128];
	if(NetDisplay > 0)
		str_format(aBuf, sizeof(aBuf), "You win %d", NetDisplay);
	else if(NetDisplay < 0)
		str_format(aBuf, sizeof(aBuf), "You lose %d", -NetDisplay);
	else
		str_copy(aBuf, "Push", sizeof(aBuf));
	str_copy(m_Blackjack.m_aMessage, aBuf, sizeof(m_Blackjack.m_aMessage));

	m_Blackjack.m_State = 4;
}

void CCasino::CaribbeanDeal()
{
	if(m_Caribbean.m_State != 0 && m_Caribbean.m_State != 2)
		return;
	if(m_Credits < m_Caribbean.m_Stake)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_Caribbean.m_Stake);
	RecordBet(m_Caribbean.m_Stake);
	// the ante and the raise are both settled at the stake that was taken here
	m_Caribbean.m_RoundStake = m_Caribbean.m_Stake;

	m_Caribbean.m_vPlayer.clear();
	m_Caribbean.m_vDealer.clear();
	for(int i = 0; i < 5; i++)
	{
		m_Caribbean.m_vPlayer.push_back(DrawCardFromShoe());
		m_Caribbean.m_vDealer.push_back(DrawCardFromShoe());
	}
	m_Caribbean.m_State = 1;
	m_Caribbean.m_LastWin = 0;
	str_copy(m_Caribbean.m_aMessage, "Play or fold?", sizeof(m_Caribbean.m_aMessage));
}

void CCasino::CaribbeanPlay()
{
	if(m_Caribbean.m_State != 1)
		return;
	const int Stake = m_Caribbean.m_RoundStake > 0 ? m_Caribbean.m_RoundStake : m_Caribbean.m_Stake;
	if(m_Credits < Stake)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(Stake);
	RecordBet(Stake);

	const int PlayerCat = EvaluatePokerHand(m_Caribbean.m_vPlayer);
	const int DealerCat = EvaluatePokerHand(m_Caribbean.m_vDealer);
	const bool Qualified = DealerCat >= 1;

	int64_t Total = 0;
	if(!Qualified)
	{
		// ante paid even money, the raise is returned
		Total = (int64_t)Stake * 3;
		str_copy(m_Caribbean.m_aMessage, "Dealer does not qualify", sizeof(m_Caribbean.m_aMessage));
	}
	else if(PlayerCat > DealerCat)
	{
		Total = (int64_t)Stake * 2 + (int64_t)Stake * s_aCaribbeanPays[PlayerCat];
		str_format(m_Caribbean.m_aMessage, sizeof(m_Caribbean.m_aMessage), "%s wins", PokerHandName(PlayerCat));
	}
	else if(PlayerCat == DealerCat)
	{
		Total = (int64_t)Stake * 2;
		str_copy(m_Caribbean.m_aMessage, "Tie - push", sizeof(m_Caribbean.m_aMessage));
	}
	else
	{
		str_format(m_Caribbean.m_aMessage, sizeof(m_Caribbean.m_aMessage), "Dealer %s wins", PokerHandName(DealerCat));
	}

	const int Payout = ClampWin(Total);
	m_Caribbean.m_LastWin = Payout;
	if(Payout > 0)
	{
		AddCredits(Payout);
		RecordWin(Payout, Stake * 2);
	}
	m_Caribbean.m_State = 2;
}

void CCasino::CaribbeanFold()
{
	if(m_Caribbean.m_State != 1)
		return;
	m_Caribbean.m_State = 2;
	m_Caribbean.m_LastWin = 0;
	str_copy(m_Caribbean.m_aMessage, "Folded", sizeof(m_Caribbean.m_aMessage));
}

void CCasino::HiLoStart()
{
	if(m_HiLo.m_State != 0 && m_HiLo.m_State != 2)
		return;
	if(m_Credits < m_HiLo.m_Stake)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_HiLo.m_Stake);
	RecordBet(m_HiLo.m_Stake);
	m_HiLo.m_RoundStake = m_HiLo.m_Stake;

	m_HiLo.m_Current = DrawCardFromShoe();
	m_HiLo.m_Prev = m_HiLo.m_Current;
	m_HiLo.m_Multiplier = 1.0f;
	m_HiLo.m_PrevMult = 1.0f;
	m_HiLo.m_LastMult = 0.0f;
	m_HiLo.m_MultAnim = 0.0f;
	m_HiLo.m_SettleAnim = 0.0f;
	m_HiLo.m_HistCount = 0;
	m_HiLo.m_Streak = 0;
	m_HiLo.m_State = 1;
	m_HiLo.m_LastWin = 0;
	m_HiLo.m_RevealTime = 0.0f;
	m_HiLo.m_HistPending = false;
	str_copy(m_HiLo.m_aMessage, "Higher or lower?", sizeof(m_HiLo.m_aMessage));
}

void CCasino::HiLoGuess(bool Higher)
{
	if(m_HiLo.m_State != 1)
		return;
	// a click during the flip would draw again and cut the animation short
	if(m_HiLo.m_RevealTime > 0.0f)
		return;

	int HigherCount = 0;
	int LowerCount = 0;
	int EqualCount = 0;
	for(int Rank = 0; Rank < 13; Rank++)
	{
		if(Rank > m_HiLo.m_Current.m_Rank)
			HigherCount += 4;
		else if(Rank < m_HiLo.m_Current.m_Rank)
			LowerCount += 4;
		else
			EqualCount += 4;
	}

	const int Relevant = 52 - EqualCount;
	const int Winning = Higher ? HigherCount : LowerCount;
	if(Relevant <= 0 || Winning <= 0)
	{
		Toast("No safe bet");
		return;
	}

	const SCasinoCard Next = DrawCardFromShoe();
	const bool Tie = Next.m_Rank == m_HiLo.m_Current.m_Rank;

	if(Tie)
	{
		str_copy(m_HiLo.m_aMessage, "Tie - push, chain continues", sizeof(m_HiLo.m_aMessage));
		m_HiLo.m_HistPendingCard = Next;
		m_HiLo.m_HistPendingDir = 0;
		m_HiLo.m_HistPending = true;
		m_HiLo.m_Prev = m_HiLo.m_Current;
		m_HiLo.m_Current = Next;
		m_HiLo.m_RevealTime = 0.5f;
		return;
	}

	const bool Correct = Higher ? (Next.m_Rank > m_HiLo.m_Current.m_Rank) : (Next.m_Rank < m_HiLo.m_Current.m_Rank);
	// the strip is updated when the flip lands, not while the card is still turning
	m_HiLo.m_HistPendingCard = Next;
	m_HiLo.m_HistPendingDir = Next.m_Rank > m_HiLo.m_Current.m_Rank ? 1 : -1;
	m_HiLo.m_HistPending = true;
	m_HiLo.m_Prev = m_HiLo.m_Current;
	m_HiLo.m_Current = Next;
	m_HiLo.m_RevealTime = 0.5f;

	if(!Correct)
	{
		m_HiLo.m_State = 2;
		m_HiLo.m_LastMult = m_HiLo.m_Multiplier;
		m_HiLo.m_SettleAnim = 1.0f;
		m_HiLo.m_Multiplier = 0.0f;
		m_HiLo.m_LastWin = 0;
		str_copy(m_HiLo.m_aMessage, "Wrong - lost", sizeof(m_HiLo.m_aMessage));
		return;
	}

	const float Probability = (float)Winning / (float)Relevant;
	m_HiLo.m_PrevMult = m_HiLo.m_Multiplier;
	m_HiLo.m_Multiplier *= 0.97f / Probability;
	if(m_HiLo.m_Multiplier > 5000.0f)
		m_HiLo.m_Multiplier = 5000.0f;
	m_HiLo.m_MultAnim = 1.0f;
	m_HiLo.m_Streak++;
	str_format(m_HiLo.m_aMessage, sizeof(m_HiLo.m_aMessage), "Correct! streak %d", m_HiLo.m_Streak);
}

void CCasino::HiLoCashOut()
{
	if(m_HiLo.m_State != 1)
		return;
	if(m_HiLo.m_Streak <= 0)
		return;
	const int Stake = m_HiLo.m_RoundStake > 0 ? m_HiLo.m_RoundStake : m_HiLo.m_Stake;
	const int Win = ClampWin((int64_t)std::llround((double)Stake * m_HiLo.m_Multiplier));
	m_HiLo.m_LastWin = Win;
	m_HiLo.m_LastMult = m_HiLo.m_Multiplier;
	m_HiLo.m_SettleAnim = 1.0f;
	m_HiLo.m_State = 2;
	m_HiLo.m_HistPending = false;
	AddCredits(Win);
	RecordWin(Win, Stake);
	str_format(m_HiLo.m_aMessage, sizeof(m_HiLo.m_aMessage), "Cashed out %d", Win);
}

void CCasino::HiLoPushHistory(const SCasinoCard &Card, int Dir)
{
	for(int i = 7; i > 0; i--)
	{
		m_HiLo.m_aHistory[i] = m_HiLo.m_aHistory[i - 1];
		m_HiLo.m_aHistDir[i] = m_HiLo.m_aHistDir[i - 1];
	}
	m_HiLo.m_aHistory[0] = Card;
	m_HiLo.m_aHistDir[0] = (signed char)Dir;
	if(m_HiLo.m_HistCount < 8)
		m_HiLo.m_HistCount++;
}

void CCasino::DragonTigerDeal()
{
	if(m_DragonTigerReveal > 0.0f)
		return;
	if(m_Credits < m_DragonTigerBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_DragonTigerBet);
	RecordBet(m_DragonTigerBet);

	// only a second round has real faces; the first deal holds the {0,0} sentinel
	const bool HadRound = m_DragonTigerState == 1;
	m_DragonTigerPrevDragon = m_DragonCard;
	m_DragonTigerPrevTiger = m_TigerCard;
	m_DragonTigerPrevValid = HadRound;
	m_DragonCard = DrawCardFromShoe();
	m_TigerCard = DrawCardFromShoe();
	m_DragonTigerState = 1;
	m_DragonTigerReveal = 0.9f;
	m_DragonTigerSettle = 0.0f;
	m_DragonTigerLastWin = 0;

	int Win = 0;
	if(m_DragonCard.m_Rank == m_TigerCard.m_Rank)
	{
		if(m_DragonTigerChoice == 2)
			Win = ClampWin((int64_t)m_DragonTigerBet * 12);
		m_DragonTigerHistPending = 2;
		str_format(m_aDragonTigerMsg, sizeof(m_aDragonTigerMsg), Win > 0 ? "TIE  +%d" : "TIE - LOST %d", Win > 0 ? Win : m_DragonTigerBet);
	}
	else if(m_DragonTigerChoice == 0 && m_DragonCard.m_Rank > m_TigerCard.m_Rank)
	{
		Win = ClampWin((int64_t)m_DragonTigerBet * 2);
		m_DragonTigerHistPending = 0;
		str_format(m_aDragonTigerMsg, sizeof(m_aDragonTigerMsg), "DRAGON WINS  +%d", Win);
	}
	else if(m_DragonTigerChoice == 1 && m_TigerCard.m_Rank > m_DragonCard.m_Rank)
	{
		Win = ClampWin((int64_t)m_DragonTigerBet * 2);
		m_DragonTigerHistPending = 1;
		str_format(m_aDragonTigerMsg, sizeof(m_aDragonTigerMsg), "TIGER WINS  +%d", Win);
	}
	else
	{
		m_DragonTigerHistPending = m_DragonCard.m_Rank > m_TigerCard.m_Rank ? 0 : 1;
		str_format(m_aDragonTigerMsg, sizeof(m_aDragonTigerMsg), "LOST  %d", m_DragonTigerBet);
	}

	m_DragonTigerHistPendingWin = Win > 0;
	// the reveal is still running, so the prize is committed by Tick at the end
	m_DragonTigerPendingWin = Win;
	m_DragonTigerLastWin = 0;
}

void CCasino::CrapsRoll()
{
	if(m_CrapsState == 1)
		return;

	if(m_CrapsPoint == 0)
	{
		if(m_Credits < m_CrapsBet)
		{
			Toast("Not enough credits");
			return;
		}
		TakeCredits(m_CrapsBet);
		RecordBet(m_CrapsBet);
		// the line keeps the come-out wager for every roll until it resolves
		m_CrapsRoundBet = m_CrapsBet;
	}
	const int RoundBet = m_CrapsRoundBet > 0 ? m_CrapsRoundBet : m_CrapsBet;

	m_aCrapsDice[0] = RandInt(6) + 1;
	m_aCrapsDice[1] = RandInt(6) + 1;
	const int Sum = m_aCrapsDice[0] + m_aCrapsDice[1];
	m_CrapsState = 1;
	m_CrapsTimer = 0.8f;

	int Win = 0;
	const bool PassBet = m_CrapsChoice == 0;

	if(m_CrapsPoint == 0)
	{
		if(Sum == 7 || Sum == 11)
		{
			Win = ClampWin((int64_t)RoundBet * 2);
			str_copy(m_aCrapsMsg, PassBet ? "Natural! Pass wins" : "Pass wins - you lose", sizeof(m_aCrapsMsg));
			if(!PassBet)
				Win = 0;
			if(PassBet)
				m_CrapsPoint = 0;
		}
		else if(Sum == 2 || Sum == 3 || Sum == 12)
		{
			Win = PassBet ? 0 : ClampWin((int64_t)RoundBet * 2);
			str_copy(m_aCrapsMsg, PassBet ? "Craps - pass loses" : "Craps - don't pass wins", sizeof(m_aCrapsMsg));
		}
		else
		{
			m_CrapsPoint = Sum;
			str_format(m_aCrapsMsg, sizeof(m_aCrapsMsg), "Point is %d", Sum);
			m_CrapsLastWin = 0;
			return;
		}
	}
	else
	{
		if(Sum == m_CrapsPoint)
		{
			Win = PassBet ? ClampWin((int64_t)RoundBet * 2) : 0;
			str_format(m_aCrapsMsg, sizeof(m_aCrapsMsg), "Point %d made", Sum);
			m_CrapsPoint = 0;
		}
		else if(Sum == 7)
		{
			Win = PassBet ? 0 : ClampWin((int64_t)RoundBet * 2);
			str_copy(m_aCrapsMsg, "Seven out", sizeof(m_aCrapsMsg));
			m_CrapsPoint = 0;
		}
		else
		{
			str_format(m_aCrapsMsg, sizeof(m_aCrapsMsg), "Rolled %d, point %d", Sum, m_CrapsPoint);
			m_CrapsLastWin = 0;
			return;
		}
	}

	m_CrapsLastWin = Win;
	if(Win > 0)
	{
		AddCredits(Win);
		RecordWin(Win, RoundBet);
	}
}

void CCasino::SicBoRoll()
{
	if(m_SicBoState == 1)
		return;
	if(m_Credits < m_SicBoBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_SicBoBet);
	RecordBet(m_SicBoBet);

	for(int i = 0; i < 3; i++)
		m_aSicBoDice[i] = RandInt(6) + 1;

	const int Sum = m_aSicBoDice[0] + m_aSicBoDice[1] + m_aSicBoDice[2];
	const bool Triple = m_aSicBoDice[0] == m_aSicBoDice[1] && m_aSicBoDice[1] == m_aSicBoDice[2];
	// m_SicBoSum holds the pick, not the roll, or the sum bet wins every time
	m_SicBoState = 1;
	m_SicBoTimer = 0.8f;

	int Win = 0;
	switch(m_SicBoChoice)
	{
	case 0:
		if(!Triple && Sum >= 4 && Sum <= 10)
			Win = ClampWin((int64_t)m_SicBoBet * 2);
		str_copy(m_aSicBoMsg, "Small", sizeof(m_aSicBoMsg));
		break;
	case 1:
		if(!Triple && Sum >= 11 && Sum <= 17)
			Win = ClampWin((int64_t)m_SicBoBet * 2);
		str_copy(m_aSicBoMsg, "Big", sizeof(m_aSicBoMsg));
		break;
	case 2:
		if(Triple)
			Win = ClampWin((int64_t)m_SicBoBet * 34);
		str_copy(m_aSicBoMsg, "Any triple", sizeof(m_aSicBoMsg));
		break;
	case 3:
		if(Triple && m_aSicBoDice[0] == m_SicBoTriple)
			Win = ClampWin((int64_t)m_SicBoBet * 201);
		str_copy(m_aSicBoMsg, "Specific triple", sizeof(m_aSicBoMsg));
		break;
	default:
		if(Sum == m_SicBoSum)
		{
			const int Pay = DiceSumPayout(Sum);
			Win = ClampWin((int64_t)m_SicBoBet * (Pay + 1));
		}
		str_copy(m_aSicBoMsg, "Sum", sizeof(m_aSicBoMsg));
		break;
	}

	m_SicBoLastWin = Win;
	if(Win > 0)
	{
		AddCredits(Win);
		RecordWin(Win, m_SicBoBet);
	}
}

void CCasino::LotteryDraw()
{
	if(m_LotteryState == 1)
		return;
	if(m_Credits < m_LotteryBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_LotteryBet);
	RecordBet(m_LotteryBet);

	std::array<int, 30> aPool;
	for(int i = 0; i < 30; i++)
		aPool[i] = i + 1;
	for(int i = 29; i > 0; i--)
		std::swap(aPool[i], aPool[RandInt(i + 1)]);

	int Matches = 0;
	for(int i = 0; i < 5; i++)
	{
		m_aLotteryDraw[i] = aPool[i];
		for(int k = 0; k < 5; k++)
			if(m_aLotteryPick[k] == aPool[i])
				Matches++;
	}

	m_LotteryMatches = Matches;
	m_LotteryState = 1;
	m_LotteryTimer = 1.2f;

	const int Win = ClampWin((int64_t)m_LotteryBet * s_aLotteryPay[std::clamp(Matches, 0, 5)]);
	m_LotteryLastWin = Win;
	if(Win > 0)
	{
		AddCredits(Win);
		RecordWin(Win, m_LotteryBet);
	}
}

void CCasino::CrashStart()
{
	if(m_CrashRunning)
		return;
	if(m_Credits < m_CrashBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_CrashBet);
	RecordBet(m_CrashBet);
	m_CrashRoundBet = m_CrashBet;
	m_CrashHasRound = true;

	const float Roll = RandFloat();
	m_CrashPoint = 0.97f / (1.0f - Roll * 0.999f);
	if(m_CrashPoint < 1.0f)
		m_CrashPoint = 1.0f;
	if(m_CrashPoint > 500.0f)
		m_CrashPoint = 500.0f;

	m_CrashMultiplier = 1.0f;
	m_CrashElapsed = 0.0f;
	m_CrashRunning = true;
	m_CrashCashed = false;
	m_CrashLastWin = 0;
}

void CCasino::CrashCashOut()
{
	if(!m_CrashRunning || m_CrashCashed)
		return;
	const int Stake = m_CrashRoundBet > 0 ? m_CrashRoundBet : m_CrashBet;
	const int Win = ClampWin((int64_t)std::llround((double)Stake * m_CrashMultiplier));
	m_CrashCashed = true;
	m_CrashRunning = false;
	m_CrashLastWin = Win;
	AddCredits(Win);
	RecordWin(Win, Stake);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Cashed out at %.2fx", m_CrashMultiplier);
	Toast(aBuf);

	for(int i = (int)m_aCrashHistory.size() - 1; i > 0; i--)
		m_aCrashHistory[i] = m_aCrashHistory[i - 1];
	m_aCrashHistory[0] = m_CrashMultiplier;
}

void CCasino::MinesStart()
{
	if(m_MinesActive)
		return;
	if(m_Credits < m_MinesBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_MinesBet);
	RecordBet(m_MinesBet);
	m_MinesRoundBet = m_MinesBet;

	m_aMinesField.fill(false);
	m_aMinesRevealed.fill(false);
	m_MinesRevealed = 0;
	m_MinesBusted = false;
	m_MinesActive = true;
	m_MinesMultiplier = 1.0f;

	int Placed = 0;
	while(Placed < m_MinesCount)
	{
		const int Index = RandInt(25);
		if(!m_aMinesField[Index])
		{
			m_aMinesField[Index] = true;
			Placed++;
		}
	}
}

void CCasino::MinesReveal(int Index)
{
	if(!m_MinesActive || m_MinesBusted)
		return;
	if(Index < 0 || Index >= 25)
		return;
	if(m_aMinesRevealed[Index])
		return;

	m_aMinesRevealed[Index] = true;
	if(m_aMinesField[Index])
	{
		m_MinesBusted = true;
		m_MinesActive = false;
		Toast("Busted");
		return;
	}

	m_MinesRevealed++;
	m_MinesMultiplier = MinesFairMultiplier(m_MinesRevealed, m_MinesCount) * 0.97f;
	if(m_MinesRevealed >= 25 - m_MinesCount)
		MinesCashOut();
}

void CCasino::MinesCashOut()
{
	if(!m_MinesActive || m_MinesRevealed <= 0)
		return;
	const int Stake = m_MinesRoundBet > 0 ? m_MinesRoundBet : m_MinesBet;
	const int Win = ClampWin((int64_t)std::llround((double)Stake * m_MinesMultiplier));
	m_MinesActive = false;
	AddCredits(Win);
	RecordWin(Win, Stake);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Cashed out %d", Win);
	Toast(aBuf);
}

void CCasino::DiceRoll()
{
	if(m_DiceRolling)
		return;
	if(m_Credits < m_DiceBet)
	{
		Toast("Not enough credits");
		return;
	}
	TakeCredits(m_DiceBet);
	RecordBet(m_DiceBet);

	m_DiceRoll = RandFloat() * 100.0f;
	const bool Won = m_DiceOver ? (m_DiceRoll > (float)m_DiceThreshold) : (m_DiceRoll < (float)m_DiceThreshold);

	m_DiceRolling = true;
	m_DiceTimer = 0.55f;

	int Win = 0;
	if(Won)
	{
		const float Probability = DiceWinProbability(m_DiceThreshold, m_DiceOver);
		const float Multiplier = 0.97f / std::max(Probability, 0.01f);
		Win = ClampWin((int64_t)std::llround((double)m_DiceBet * Multiplier));
		str_format(m_aDiceMsg, sizeof(m_aDiceMsg), "%.2f - won %d", m_DiceRoll, Win);
	}
	else
	{
		str_format(m_aDiceMsg, sizeof(m_aDiceMsg), "%.2f - lost", m_DiceRoll);
	}

	m_DiceLastWin = Win;
	if(Win > 0)
	{
		AddCredits(Win);
		RecordWin(Win, m_DiceBet);
	}
}

CUIRect CCasino::ScreenHeader(CUIRect *pRect, const char *pTitle, const char *pSub, const char *pGlyph, ColorRGBA Accent)
{
	CUIRect Head, Rest;
	pRect->HSplitTop(48.0f, &Head, &Rest);

	CUIRect Badge;
	Badge.w = 28.0f;
	Badge.h = 28.0f;
	Badge.x = Head.x;
	Badge.y = Head.y + (Head.h - Badge.h) * 0.5f;
	Badge.Draw(ColorRGBA(0.086f, 0.096f, 0.124f, 1.0f), IGraphics::CORNER_ALL, CASINO_RADIUS);
	Badge.DrawOutline(ColorRGBA(Accent.r, Accent.g, Accent.b, 0.65f));
	Label(&Badge, pGlyph, 11.0f, TEXTALIGN_MC, Accent);

	CUIRect Title = Head;
	Title.x += 40.0f;
	Title.w -= 40.0f;
	Title.HSplitTop(Head.h * 0.56f, &Title, nullptr);
	Label(&Title, pTitle, 15.0f, TEXTALIGN_ML, ColorRGBA(0.960f, 0.968f, 0.992f, 1.0f));

	CUIRect Sub = Head;
	Sub.x += 41.0f;
	Sub.w -= 40.0f;
	Sub.HSplitTop(Head.h * 0.54f, nullptr, &Sub);
	Label(&Sub, pSub, 9.0f, TEXTALIGN_ML, ColorRGBA(0.480f, 0.520f, 0.610f, 1.0f));

	Rest.HSplitTop(4.0f, nullptr, &Rest);
	return Rest;
}

void CCasino::Stage(const CUIRect *pRect, ColorRGBA Top, ColorRGBA Bottom, float Rounding)
{
	if(pRect->w <= 2.0f || pRect->h <= 2.0f)
		return;

	const float R = Square(Rounding);
	Frame(pRect, Lerp(Top, Bottom, 0.5f), CASINO_EDGE, R);

	CUIRect Inner = *pRect;
	Inner.Margin(1.0f, &Inner);
	Inner.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 0.045f));
}

void CCasino::Felt(const CUIRect *pRect, float Rounding)
{
	Stage(pRect, CASINO_FELT_LIGHT, CASINO_FELT, Rounding);

	CUIRect Inner = *pRect;
	Inner.Margin(6.0f, &Inner);
	if(Inner.w <= 4.0f || Inner.h <= 4.0f)
		return;

	Inner.DrawOutline(ColorRGBA(0.520f, 0.860f, 0.660f, 0.10f));

	CUIRect Glow = Inner;
	Glow.h = Inner.h * 0.45f;
	Grad(&Glow, ColorRGBA(0.130f, 0.360f, 0.240f, 0.55f), ColorRGBA(0.060f, 0.190f, 0.130f, 0.0f), Rounding);
}

bool CCasino::ActionButton(CUIRect *pRect, const char *pText, ColorRGBA Fill, bool Enabled, float FontSize)
{
	const bool Hovered = Enabled && Ui()->MouseHovered(pRect);
	const bool Pressed = Hovered && Ui()->MouseButton(0);
	const bool Clicked = Enabled && Ui()->MouseButtonClicked(0) && Hovered;

	ColorRGBA Top = Shade(Fill, 1.14f);
	ColorRGBA Bottom = Shade(Fill, 0.86f);
	ColorRGBA Border = Shade(Fill, 1.45f);
	ColorRGBA TextColor = ColorRGBA(0.045f, 0.055f, 0.045f, 1.0f);

	if(!Enabled)
	{
		Top = ColorRGBA(0.118f, 0.128f, 0.156f, 1.0f);
		Bottom = ColorRGBA(0.086f, 0.094f, 0.116f, 1.0f);
		Border = ColorRGBA(0.150f, 0.162f, 0.196f, 1.0f);
		TextColor = ColorRGBA(0.360f, 0.388f, 0.450f, 1.0f);
	}
	else if(Pressed)
	{
		Top = Shade(Fill, 0.86f);
		Bottom = Shade(Fill, 1.02f);
		Border = Shade(Fill, 1.30f);
	}
	else if(Hovered)
	{
		Top = Shade(Fill, 1.28f);
		Bottom = Shade(Fill, 0.94f);
		Border = Shade(Fill, 1.75f);
	}

	Frame(pRect, Lerp(Top, Bottom, 0.5f), Border, CASINO_RADIUS);
	Grad(pRect, Top, Bottom, CASINO_RADIUS);

	// hover marker: a 2px accent rule along the bottom edge instead of a glow
	if(Enabled && (Hovered || Pressed))
	{
		CUIRect Marker = *pRect;
		Marker.HSplitBottom(2.0f, &Marker, nullptr);
		Marker.Draw(Shade(Fill, Hovered ? 1.35f : 1.10f), IGraphics::CORNER_NONE, 0.0f);
	}

	Label(pRect, pText, FontSize, TEXTALIGN_MC, TextColor);
	return Clicked;
}

void CCasino::ResultBanner(CUIRect *pRect, const char *pText, bool Win, bool Neutral)
{
	ColorRGBA Fill = ColorRGBA(0.098f, 0.112f, 0.150f, 1.0f);
	ColorRGBA Border = ColorRGBA(0.190f, 0.210f, 0.265f, 1.0f);
	ColorRGBA TextColor = CASINO_DIM;

	if(Win)
	{
		Fill = ColorRGBA(0.070f, 0.208f, 0.132f, 1.0f);
		Border = ColorRGBA(0.300f, 0.780f, 0.520f, 1.0f);
		TextColor = ColorRGBA(0.720f, 1.000f, 0.830f, 1.0f);
	}
	else if(!Neutral)
	{
		Fill = ColorRGBA(0.180f, 0.078f, 0.086f, 1.0f);
		Border = ColorRGBA(0.560f, 0.220f, 0.240f, 1.0f);
		TextColor = ColorRGBA(0.960f, 0.640f, 0.660f, 1.0f);
	}

	Frame(pRect, Fill, Border, 6.0f);
	Label(pRect, pText, 13.0f, TEXTALIGN_MC, TextColor);
}

void CCasino::StatRow(CUIRect *pRect, const char *pLabel, const char *pValue, ColorRGBA ValueColor)
{
	CUIRect Row;
	pRect->HSplitTop(20.0f, &Row, pRect);

	CUIRect Left = Row, Right = Row;
	Right.w = Row.w * 0.52f;
	Right.x = Row.x + Row.w - Right.w;

	Label(&Left, pLabel, 10.0f, TEXTALIGN_ML, ColorRGBA(0.470f, 0.505f, 0.590f, 1.0f));
	Label(&Right, pValue, 12.0f, TEXTALIGN_MR, ValueColor);
}

void CCasino::RailCard(CUIRect *pRect, const char *pTitle)
{
	Frame(pRect, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.160f, 0.178f, 0.228f, 1.0f), 8.0f);

	if(pTitle == nullptr || pTitle[0] == '\0')
		return;

	CUIRect Title = *pRect;
	Title.Margin(10.0f, &Title);
	Title.HSplitTop(14.0f, &Title, nullptr);
	Label(&Title, pTitle, 8.5f, TEXTALIGN_ML, ColorRGBA(0.520f, 0.560f, 0.650f, 1.0f));

	CUIRect Line;
	Line.x = pRect->x + 10.0f;
	Line.y = pRect->y + 26.0f;
	Line.w = pRect->w - 20.0f;
	Line.h = 1.0f;
	Line.Draw(ColorRGBA(0.180f, 0.198f, 0.250f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
}

void CCasino::Ring(const vec2 &Center, float Radius, float Thickness, ColorRGBA Color, int Segments)
{
	if(Radius <= 0.5f)
		return;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	const float Inner = std::max(0.0f, Radius - Thickness * 0.5f);
	const float Outer = Radius + Thickness * 0.5f;
	for(int i = 0; i < Segments; i++)
	{
		const float A0 = 2.0f * pi * (float)i / (float)Segments;
		const float A1 = 2.0f * pi * (float)(i + 1) / (float)Segments;
		const vec2 D0(std::cos(A0), std::sin(A0));
		const vec2 D1(std::cos(A1), std::sin(A1));
		IGraphics::CFreeformItem Item(
			Center + D0 * Inner,
			Center + D0 * Outer,
			Center + D1 * Inner,
			Center + D1 * Outer);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}
	Graphics()->QuadsEnd();
}

void CCasino::GlowDot(const vec2 &Pos, float Radius, ColorRGBA Color)
{
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a * 0.18f);
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius * 2.0f, 18);
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a * 0.34f);
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius * 1.4f, 18);
	Graphics()->SetColor(Color);
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius, 18);
	Graphics()->QuadsEnd();
}

// pictogram per symbol so the reel reads as cherries, a lemon, a star without the caption
void CCasino::SlotSymbol(int Symbol, const CUIRect &Rect)
{
	const int Index = std::clamp(Symbol, 0, 5);
	const ColorRGBA Col = s_aSlotColor[Index];
	const ColorRGBA Hi = Shade(Col, 1.45f);
	const ColorRGBA Lo = Shade(Col, 0.58f);

	const float R = std::min(Rect.w, Rect.h) * 0.34f;
	const vec2 C(Rect.x + Rect.w * 0.5f, Rect.y + Rect.h * 0.5f);

	// the seven and the bar are typographic, everything else is drawn from polygons
	if(Index == 0)
	{
		const float FontSize = std::min(R * 2.4f, Rect.h * 0.72f);
		CUIRect Box(Rect.x, C.y - FontSize * 0.60f, Rect.w, FontSize * 1.2f);
		Label(&Box, "7", FontSize, TEXTALIGN_MC, Col);
		return;
	}
	if(Index == 5)
	{
		CUIRect Plate;
		Plate.w = std::min(Rect.w * 0.82f, R * 2.4f);
		Plate.h = std::min(Rect.h * 0.46f, R * 1.15f);
		Plate.x = C.x - Plate.w * 0.5f;
		Plate.y = C.y - Plate.h * 0.5f;
		Plate.Draw(Lo, IGraphics::CORNER_ALL, 1.0f);
		Plate.DrawOutline(Hi);
		Label(&Plate, "BAR", std::min(Plate.h * 0.66f, 14.0f), TEXTALIGN_MC, Hi);
		return;
	}

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	auto Tri = [&](const vec2 &A, const vec2 &B, const vec2 &D, const ColorRGBA &Fill) {
		Graphics()->SetColor(Fill.r, Fill.g, Fill.b, Fill.a);
		const IGraphics::CFreeformItem Item(A, B, D, D);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	auto Fan = [&](const vec2 &Center, const vec2 *pVerts, int Num, const ColorRGBA &Fill) {
		for(int i = 0; i < Num; i++)
			Tri(Center, pVerts[i], pVerts[(i + 1) % Num], Fill);
	};
	auto Bar = [&](const vec2 &A, const vec2 &B, float Width, const ColorRGBA &Fill) {
		const vec2 Delta = B - A;
		const float Len = length(Delta);
		if(Len < 0.001f)
			return;
		const vec2 Dir = Delta / Len;
		const vec2 Side(-Dir.y, Dir.x);
		const vec2 Half = Side * (Width * 0.5f);
		Graphics()->SetColor(Fill.r, Fill.g, Fill.b, Fill.a);
		const IGraphics::CFreeformItem Item(A + Half, B + Half, A - Half, B - Half);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	auto Ellipse = [&](const vec2 &Center, float Rx, float Ry, const ColorRGBA &Fill) {
		vec2 aVerts[28];
		for(int i = 0; i < 28; i++)
		{
			const float A = (float)i / 28.0f * 2.0f * pi;
			aVerts[i] = Center + vec2(std::cos(A) * Rx, std::sin(A) * Ry);
		}
		Fan(Center, aVerts, 28, Fill);
	};

	const ColorRGBA LeafColor(0.360f, 0.760f, 0.420f, 1.0f);

	switch(Index)
	{
	case 1: // diamond
	{
		const vec2 Top(C.x, C.y - R * 1.30f);
		const vec2 Right(C.x + R * 0.85f, C.y);
		const vec2 Bottom(C.x, C.y + R * 1.30f);
		const vec2 Left(C.x - R * 0.85f, C.y);
		Tri(Top, Right, Bottom, Col);
		Tri(Top, Bottom, Left, Col);
		// upper facet catches the light, lower one falls into shade
		Tri(Top, vec2(C.x + R * 0.40f, C.y - R * 0.10f), vec2(C.x, C.y + R * 0.05f), Hi);
		Tri(Top, vec2(C.x, C.y + R * 0.05f), vec2(C.x - R * 0.40f, C.y - R * 0.10f), Hi);
		Tri(vec2(C.x, C.y + R * 0.05f), vec2(C.x + R * 0.40f, C.y + R * 0.55f), Bottom, Lo);
		Tri(vec2(C.x, C.y + R * 0.05f), Bottom, vec2(C.x - R * 0.40f, C.y + R * 0.55f), Lo);
		break;
	}
	case 2: // star
	{
		vec2 aVerts[10];
		for(int i = 0; i < 10; i++)
		{
			const float A = -pi * 0.5f + (float)i / 10.0f * 2.0f * pi;
			const float Radius = (i % 2 == 0) ? R * 1.32f : R * 0.54f;
			aVerts[i] = C + direction(A) * Radius;
		}
		Fan(C, aVerts, 10, Col);
		vec2 aInner[5];
		for(int i = 0; i < 5; i++)
			aInner[i] = C + direction(-pi * 0.5f + (float)i / 5.0f * 2.0f * pi) * (R * 0.42f);
		Fan(C + vec2(0.0f, -R * 0.10f), aInner, 5, Hi);
		break;
	}
	case 3: // cherry: two berries, two stems and a leaf
	{
		const vec2 A(C.x - R * 0.52f, C.y + R * 0.55f);
		const vec2 B(C.x + R * 0.55f, C.y + R * 0.70f);
		const vec2 Stem(C.x + R * 0.30f, C.y - R * 1.15f);
		Bar(Stem, A, R * 0.17f, Lo);
		Bar(Stem, B, R * 0.17f, Lo);
		Tri(Stem, Stem + vec2(R * 0.95f, -R * 0.30f), Stem + vec2(R * 0.30f, R * 0.22f), LeafColor);
		Ellipse(A, R * 0.62f, R * 0.60f, Col);
		Ellipse(B, R * 0.62f, R * 0.60f, Col);
		Ellipse(A + vec2(-R * 0.18f, -R * 0.18f), R * 0.20f, R * 0.16f, Hi);
		Ellipse(B + vec2(-R * 0.18f, -R * 0.18f), R * 0.20f, R * 0.16f, Hi);
		break;
	}
	case 4: // lemon: pointed ellipse with a leaf
	{
		Ellipse(C, R * 1.28f, R * 0.86f, Col);
		Tri(vec2(C.x + R * 1.18f, C.y - R * 0.34f), vec2(C.x + R * 1.55f, C.y), vec2(C.x + R * 1.18f, C.y + R * 0.34f), Col);
		Tri(vec2(C.x - R * 1.18f, C.y - R * 0.34f), vec2(C.x - R * 1.55f, C.y), vec2(C.x - R * 1.18f, C.y + R * 0.34f), Col);
		Ellipse(C + vec2(-R * 0.22f, -R * 0.26f), R * 0.62f, R * 0.34f, Hi);
		Tri(vec2(C.x + R * 0.10f, C.y - R * 0.80f), vec2(C.x + R * 0.95f, C.y - R * 1.25f), vec2(C.x + R * 0.80f, C.y - R * 0.52f), LeafColor);
		break;
	}
	default:
		break;
	}

	Graphics()->QuadsEnd();
}

void CCasino::RenderSlot(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "CLASSIC 3-REEL SLOT", "3 REELS  /  1-5 PAYLINES  /  LEFT-TO-RIGHT", "7", CASINO_GOLD);

	CUIRect CabinetArea, Rail;
	Body.VSplitRight(268.0f, &CabinetArea, &Rail);
	CabinetArea.VSplitRight(14.0f, &CabinetArea, nullptr);

	const float CabinetH = std::min(CabinetArea.h - 84.0f, 396.0f);
	const float CabinetW = std::min(CabinetArea.w, 520.0f);

	CUIRect Cabinet;
	Cabinet.w = CabinetW;
	Cabinet.h = CabinetH;
	Cabinet.x = CabinetArea.x + (CabinetArea.w - CabinetW) * 0.5f;
	Cabinet.y = CabinetArea.y + std::max(0.0f, (CabinetArea.h - 84.0f - CabinetH) * 0.34f);

	Frame(&Cabinet, ColorRGBA(0.115f, 0.086f, 0.042f, 1.0f), CASINO_GOLD_DEEP, CASINO_RADIUS);
	Grad(&Cabinet, ColorRGBA(0.212f, 0.164f, 0.078f, 1.0f), ColorRGBA(0.108f, 0.082f, 0.038f, 1.0f), CASINO_RADIUS);

	CUIRect CabinetInner = Cabinet;
	CabinetInner.Margin(6.0f, &CabinetInner);
	Frame(&CabinetInner, ColorRGBA(0.060f, 0.062f, 0.078f, 1.0f), ColorRGBA(0.180f, 0.146f, 0.070f, 1.0f), CASINO_RADIUS);

	const float Pad = 12.0f;
	const float ReelW = (CabinetInner.w - Pad * 2.0f - 10.0f) / 3.0f;
	const float ReelH = CabinetInner.h - Pad * 2.0f;

	CUIRect aCellRects[3][3];

	// (1 - Left^2) eases into a stop; the travel spans the strip so the last frame lands on the result
	auto ReelOffset = [&](int Reel) {
		const float Duration = m_aSlotReelDuration[Reel];
		if(Duration <= 0.0f)
			return 0.0f;
		const float Left = std::clamp(m_aSlotReelTime[Reel] / Duration, 0.0f, 1.0f);
		return m_aSlotReelTravel[Reel] * (1.0f - Left * Left);
	};

	// strip runs from the pre-spin symbols to the result, so the reel visibly rolls between them
	auto StripSymbol = [&](int Reel, int Index) {
		constexpr int FinalStart = SLOT_STRIP_LEN - 3;
		if(Index <= 2)
			return m_aSlotPrevReels[Reel][std::clamp(Index, 0, 2)];
		if(Index >= FinalStart)
			return m_aSlotReels[Reel][std::min(Index - FinalStart, 2)];
		unsigned int Hash = m_SlotSpinSeed * 2654435761u + (unsigned int)(Reel + 1) * 40503u + (unsigned int)(Index + 1) * 2246822519u;
		Hash ^= Hash >> 15;
		Hash *= 2654435761u;
		Hash ^= Hash >> 13;
		return (int)(Hash % 6u);
	};

	for(int Reel = 0; Reel < 3; Reel++)
	{
		CUIRect Column;
		Column.x = CabinetInner.x + Pad + Reel * (ReelW + 5.0f);
		Column.y = CabinetInner.y + Pad;
		Column.w = ReelW;
		Column.h = ReelH;

		Frame(&Column, ColorRGBA(0.098f, 0.104f, 0.132f, 1.0f), ColorRGBA(0.255f, 0.220f, 0.120f, 1.0f), CASINO_RADIUS);

		CUIRect ReelInner = Column;
		ReelInner.Margin(3.0f, &ReelInner);
		ReelInner.Draw(ColorRGBA(0.036f, 0.040f, 0.052f, 1.0f), IGraphics::CORNER_ALL, 0.0f);

		const float Offset = ReelOffset(Reel);
		const bool Spinning = m_aSlotReelTime[Reel] > 0.0f;
		if(Spinning)
		{
			// scroll the strip through the reel window instead of swapping symbols in place
			const float CellH = ReelInner.h / 3.0f;
			Ui()->ClipEnable(&ReelInner);
			for(int i = 0; i < SLOT_STRIP_LEN; i++)
			{
				const float Y = ReelInner.y + ((float)i - Offset) * CellH;
				if(Y > ReelInner.y + ReelInner.h || Y + CellH < ReelInner.y)
					continue;

				CUIRect Face(ReelInner.x, Y, ReelInner.w, CellH);
				Face.Margin(3.0f, &Face);
				Face.Draw(ColorRGBA(0.094f, 0.101f, 0.128f, 1.0f), IGraphics::CORNER_ALL, CASINO_RADIUS);
				Face.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 0.055f));
				SlotSymbol(StripSymbol(Reel, i), Face);
			}
			Ui()->ClipDisable();

			// fades at both edges, so the strip reads as motion rather than as a jump
			CUIRect FadeTop = ReelInner;
			FadeTop.h = ReelInner.h * 0.26f;
			Grad(&FadeTop, ColorRGBA(0.024f, 0.027f, 0.036f, 0.82f), ColorRGBA(0.024f, 0.027f, 0.036f, 0.0f), 0.0f);

			CUIRect FadeBottom = ReelInner;
			FadeBottom.h = ReelInner.h * 0.26f;
			FadeBottom.y = ReelInner.y + ReelInner.h - FadeBottom.h;
			Grad(&FadeBottom, ColorRGBA(0.024f, 0.027f, 0.036f, 0.0f), ColorRGBA(0.024f, 0.027f, 0.036f, 0.82f), 0.0f);
		}
		else
		{
			for(int Row = 0; Row < 3; Row++)
			{
				CUIRect Cell;
				Cell.x = ReelInner.x;
				Cell.y = ReelInner.y + Row * (ReelInner.h / 3.0f);
				Cell.w = ReelInner.w;
				Cell.h = ReelInner.h / 3.0f;
				aCellRects[Reel][Row] = Cell;

				const int Symbol = m_aSlotReels[Reel][Row];

				bool OnWinningLine = false;
				if(m_SlotState == 2)
				{
					for(int Line = 0; Line < m_SlotLines; Line++)
					{
						if(m_aSlotLineWin[Line] > 0 && s_aSlotLines[Line][Reel][1] == Row)
							OnWinningLine = true;
					}
				}

				CUIRect Face = Cell;
				Face.Margin(3.0f, &Face);

				if(OnWinningLine)
				{
					const float Pulse = 0.55f + 0.45f * std::sin((float)m_AnimTime * 0.12f);
					Face.Draw(ColorRGBA(0.180f, 0.148f, 0.052f, 1.0f), IGraphics::CORNER_ALL, CASINO_RADIUS);
					CUIRect GlowRect = Face;
					GlowRect.x -= 1.0f;
					GlowRect.y -= 1.0f;
					GlowRect.w += 2.0f;
					GlowRect.h += 2.0f;
					GlowRect.Draw(ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.16f + 0.22f * Pulse), IGraphics::CORNER_ALL, CASINO_RADIUS_MAX);
				}
				else
				{
					// flat face plus a hairline, so neighbouring cells stay separated without shadows
					Face.Draw(ColorRGBA(0.094f, 0.101f, 0.128f, 1.0f), IGraphics::CORNER_ALL, CASINO_RADIUS);
					Face.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 0.055f));
				}

				SlotSymbol(Symbol, Face);

				if(OnWinningLine)
					Face.DrawOutline(CASINO_GOLD);
			}
		}

		// brief flash on the column frame the moment this reel locks in
		if(m_aSlotReelStopAnim[Reel] > 0.0f)
		{
			const float Flash = std::clamp(m_aSlotReelStopAnim[Reel] / 0.22f, 0.0f, 1.0f);
			Column.DrawOutline(ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.85f * Flash));
		}

		CUIRect Separator;
		Separator.x = Column.x;
		Separator.y = Column.y + Column.h - 3.0f;
		Separator.w = Column.w;
		Separator.h = 3.0f;
		Separator.Draw(ColorRGBA(0.255f, 0.220f, 0.120f, 0.75f), IGraphics::CORNER_NONE, 0.0f);
	}

	if(m_SlotState == 2)
	{
		for(int Line = 0; Line < m_SlotLines; Line++)
		{
			if(m_aSlotLineWin[Line] <= 0)
				continue;

			const vec2 A(aCellRects[0][s_aSlotLines[Line][0][1]].x + 4.0f, aCellRects[0][s_aSlotLines[Line][0][1]].y + aCellRects[0][s_aSlotLines[Line][0][1]].h * 0.5f);
			const vec2 B(aCellRects[2][s_aSlotLines[Line][2][1]].x + aCellRects[2][s_aSlotLines[Line][2][1]].w - 4.0f, aCellRects[2][s_aSlotLines[Line][2][1]].y + aCellRects[2][s_aSlotLines[Line][2][1]].h * 0.5f);

			const vec2 Dir = normalize(B - A);
			const vec2 Normal(-Dir.y, Dir.x);
			const float Thickness = 2.0f;

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.85f);
			IGraphics::CFreeformItem Item(
				A + Normal * Thickness,
				B + Normal * Thickness,
				A - Normal * Thickness,
				B - Normal * Thickness);
			Graphics()->QuadsDrawFreeform(&Item, 1);
			Graphics()->QuadsEnd();

			for(int Reel = 0; Reel < 3; Reel++)
			{
				const CUIRect &Cell = aCellRects[Reel][s_aSlotLines[Line][Reel][1]];
				GlowDot(vec2(Cell.x + Cell.w * 0.5f, Cell.y + Cell.h * 0.5f), 3.0f, CASINO_GOLD);
			}
		}
	}

	CUIRect PayTable;
	PayTable.x = Cabinet.x;
	PayTable.y = Cabinet.y + Cabinet.h + 12.0f;
	PayTable.w = Cabinet.w;
	PayTable.h = std::min(56.0f, CabinetArea.y + CabinetArea.h - PayTable.y);

	if(PayTable.h >= 34.0f)
	{
		Frame(&PayTable, ColorRGBA(0.076f, 0.084f, 0.110f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 7.0f);

		const float ChipW = (PayTable.w - 16.0f - 5.0f * 5.0f) / 6.0f;
		for(int i = 0; i < 6; i++)
		{
			CUIRect Chip;
			Chip.x = PayTable.x + 8.0f + i * (ChipW + 5.0f);
			Chip.y = PayTable.y + 6.0f;
			Chip.w = ChipW;
			Chip.h = PayTable.h - 12.0f;

			Chip.Draw(ColorRGBA(0.108f, 0.118f, 0.150f, 1.0f), IGraphics::CORNER_ALL, CASINO_RADIUS);
			Chip.DrawOutline(ColorRGBA(s_aSlotColor[i].r, s_aSlotColor[i].g, s_aSlotColor[i].b, 0.30f));

			CUIRect NameRect = Chip;
			NameRect.HSplitTop(Chip.h * 0.52f, &NameRect, nullptr);
			NameRect.HMargin(4.0f, &NameRect);
			Label(&NameRect, s_apSlotNames[i], std::min(Chip.h * 0.22f, 10.0f), TEXTALIGN_MC, ColorRGBA(0.700f, 0.730f, 0.800f, 1.0f));

			CUIRect PayRect = Chip;
			PayRect.HSplitTop(Chip.h * 0.50f, nullptr, &PayRect);
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%dx", s_aSlotPay[i]);
			Label(&PayRect, aBuf, std::min(Chip.h * 0.32f, 14.0f), TEXTALIGN_MC, s_aSlotColor[i]);
		}
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, LinesCard, SpinCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(88.0f, &LinesCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &SpinCard, &Ctrl);

	RailCard(&BetCard, "BET PER LINE");
	CUIRect BetInner = BetCard;
	BetInner.Margin(10.0f, &BetInner);
	BetInner.HSplitTop(16.0f, nullptr, &BetInner);
	BetInner.HSplitTop(8.0f, nullptr, &BetInner); // the stepper sits lower than the card title
	BetInner.HSplitTop(34.0f, &BetInner, nullptr);
	BetRow(&BetInner, &m_SlotBet, 10, std::max(10, m_Credits), m_SlotState != 1);

	RailCard(&LinesCard, "ACTIVE PAYLINES");
	CUIRect LinesInner = LinesCard;
	LinesInner.Margin(10.0f, &LinesInner);
	LinesInner.HSplitTop(16.0f, nullptr, &LinesInner);
	LinesInner.HSplitTop(34.0f, &LinesInner, &LinesInner);

	const float LineBtnW = (LinesInner.w - 4.0f * 5.0f) / 5.0f;
	for(int i = 0; i < 5; i++)
	{
		CUIRect Btn;
		Btn.x = LinesInner.x + i * (LineBtnW + 5.0f);
		Btn.y = LinesInner.y;
		Btn.w = LineBtnW;
		Btn.h = LinesInner.h;

		char aBuf[8];
		str_format(aBuf, sizeof(aBuf), "%d", i + 1);
		if(Button(&Btn, aBuf, m_SlotLines == i + 1, m_SlotState != 1, 13.0f))
			m_SlotLines = i + 1;
	}

	CUIRect TotalCard = SpinCard;
	RailCard(&TotalCard, "SPIN");

	CUIRect TotalInner = TotalCard;
	TotalInner.Margin(10.0f, &TotalInner);
	TotalInner.HSplitTop(18.0f, nullptr, &TotalInner);

	CUIRect TotalRow;
	TotalInner.HSplitTop(24.0f, &TotalRow, &TotalInner);
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%d", m_SlotBet * m_SlotLines);
		CUIRect LabelRect = TotalRow;
		LabelRect.w *= 0.5f;
		Label(&LabelRect, "TOTAL BET", 10.0f, TEXTALIGN_ML, ColorRGBA(0.480f, 0.515f, 0.600f, 1.0f));
		CUIRect ValueRect = TotalRow;
		ValueRect.x += TotalRow.w * 0.5f;
		ValueRect.w *= 0.5f;
		Label(&ValueRect, aBuf, 16.0f, TEXTALIGN_MR, CASINO_GOLD);
	}

	TotalInner.HSplitTop(10.0f, nullptr, &TotalInner);

	const bool Spinning = m_SlotState == 1;
	CUIRect SpinBtn;
	TotalInner.HSplitTop(58.0f, &SpinBtn, &TotalInner);
	if(ActionButton(&SpinBtn, Spinning ? "SPINNING" : "SPIN", CASINO_GOLD, !Spinning && m_Credits >= m_SlotBet * m_SlotLines, 20.0f))
		SlotSpin();

	TotalInner.HSplitTop(10.0f, nullptr, &TotalInner);

	CUIRect ResultBox;
	TotalInner.HSplitTop(std::max(28.0f, std::min(40.0f, TotalInner.h)), &ResultBox, &TotalInner);
	if(m_SlotLastWin > 0)
	{
		char aBuf[48];
		str_format(aBuf, sizeof(aBuf), "WIN  +%d", m_SlotLastWin);
		ResultBanner(&ResultBox, aBuf, true);
	}
	else if(m_SlotState == 2)
	{
		ResultBanner(&ResultBox, "NO WIN", false, true);
	}
	else
	{
		Frame(&ResultBox, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
		Label(&ResultBox, "READY", 11.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
	}
}

void CCasino::RenderWheel(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "WHEEL OF FORTUNE", "SEVEN SEGMENTS  /  WEIGHTED ODDS  /  ONE SPIN", "W", ColorRGBA(0.760f, 0.480f, 0.960f, 1.0f));

	CUIRect WheelArea, Rail;
	Body.VSplitRight(268.0f, &WheelArea, &Rail);
	WheelArea.VSplitRight(14.0f, &WheelArea, nullptr);

	Felt(&WheelArea, 10.0f);

	const vec2 Center(WheelArea.x + WheelArea.w * 0.5f, WheelArea.y + WheelArea.h * 0.48f);
	const float Radius = std::min(WheelArea.w, WheelArea.h) * 0.40f;

	int TotalWeight = 0;
	for(int i = 0; i < 7; i++)
		TotalWeight += s_aWheelWeight[i];

	const float Time = Client()->LocalTime();
	const bool Spinning = m_WheelState == 1;

	// dim backdrop disc so the wheel separates from the felt
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.150f, 0.135f, 0.180f, 1.0f);
	Graphics()->DrawCircle(Center.x, Center.y, Radius + 14.0f, 64);
	Graphics()->QuadsEnd();

	// slices: darker inner field, full-colour outer band, bright lip at the rim
	float Angle = m_WheelAngle;
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(int i = 0; i < 7; i++)
	{
		const float Slice = (float)s_aWheelWeight[i] / (float)TotalWeight * 2.0f * pi;
		const ColorRGBA Base = s_aWheelColor[i];
		const ColorRGBA InnerCol = Shade(Base, 0.62f);
		const ColorRGBA Lip = Shade(Base, 1.30f);
		const int Steps = std::max(4, (int)(Slice * 20.0f));
		for(int s = 0; s < Steps; s++)
		{
			const float A0 = Angle + Slice * (float)s / (float)Steps;
			const float A1 = Angle + Slice * (float)(s + 1) / (float)Steps;
			const vec2 D0(std::cos(A0), std::sin(A0));
			const vec2 D1(std::cos(A1), std::sin(A1));

			Graphics()->SetColor(InnerCol);
			IGraphics::CFreeformItem InnerPart(
				Center + D0 * (Radius * 0.16f),
				Center + D0 * (Radius * 0.62f),
				Center + D1 * (Radius * 0.16f),
				Center + D1 * (Radius * 0.62f));
			Graphics()->QuadsDrawFreeform(&InnerPart, 1);

			Graphics()->SetColor(Base);
			IGraphics::CFreeformItem OuterPart(
				Center + D0 * (Radius * 0.62f),
				Center + D0 * (Radius * 0.94f),
				Center + D1 * (Radius * 0.62f),
				Center + D1 * (Radius * 0.94f));
			Graphics()->QuadsDrawFreeform(&OuterPart, 1);

			Graphics()->SetColor(Lip);
			IGraphics::CFreeformItem LipPart(
				Center + D0 * (Radius * 0.94f),
				Center + D0 * Radius,
				Center + D1 * (Radius * 0.94f),
				Center + D1 * Radius);
			Graphics()->QuadsDrawFreeform(&LipPart, 1);
		}
		Angle += Slice;
	}
	Graphics()->QuadsEnd();

	// flash the winning segment once the wheel settles
	if(m_WheelState == 2 && m_WheelResult >= 0)
	{
		float WinAngle = m_WheelAngle;
		for(int i = 0; i < m_WheelResult; i++)
			WinAngle += (float)s_aWheelWeight[i] / (float)TotalWeight * 2.0f * pi;
		const float Slice = (float)s_aWheelWeight[m_WheelResult] / (float)TotalWeight * 2.0f * pi;
		const float Pulse = 0.20f + 0.12f * std::sin(Time * 6.0f);
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Pulse);
		const int Steps = std::max(4, (int)(Slice * 20.0f));
		for(int s = 0; s < Steps; s++)
		{
			const float A0 = WinAngle + Slice * (float)s / (float)Steps;
			const float A1 = WinAngle + Slice * (float)(s + 1) / (float)Steps;
			const vec2 D0(std::cos(A0), std::sin(A0));
			const vec2 D1(std::cos(A1), std::sin(A1));
			IGraphics::CFreeformItem Flash(
				Center + D0 * (Radius * 0.16f),
				Center + D0 * Radius,
				Center + D1 * (Radius * 0.16f),
				Center + D1 * Radius);
			Graphics()->QuadsDrawFreeform(&Flash, 1);
		}
		Graphics()->QuadsEnd();
	}

	// gold radial dividers between segments
	Angle = m_WheelAngle;
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Shade(CASINO_GOLD, 0.85f, 0.9f));
	for(int i = 0; i < 7; i++)
	{
		const vec2 D(std::cos(Angle), std::sin(Angle));
		const vec2 Perp(-D.y, D.x);
		const vec2 In = Center + D * (Radius * 0.16f);
		const vec2 Out = Center + D * Radius;
		IGraphics::CFreeformItem Divider(
			In - Perp * 1.2f,
			Out - Perp * 1.2f,
			In + Perp * 1.2f,
			Out + Perp * 1.2f);
		Graphics()->QuadsDrawFreeform(&Divider, 1);
		Angle += (float)s_aWheelWeight[i] / (float)TotalWeight * 2.0f * pi;
	}
	Graphics()->QuadsEnd();

	// bezel: deep gold band, bright edges, chase-light rivets
	Ring(Center, Radius + 7.0f, 12.0f, CASINO_GOLD_DEEP, 72);
	Ring(Center, Radius + 12.0f, 2.5f, CASINO_GOLD, 72);
	Ring(Center, Radius + 1.5f, 3.0f, Shade(CASINO_GOLD, 0.75f, 0.9f), 72);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	const int Rivets = 16;
	for(int r = 0; r < Rivets; r++)
	{
		const float A = 2.0f * pi * (float)r / (float)Rivets;
		const float Chase = Spinning ? 8.0f : 1.5f;
		const float Glow = 0.5f + 0.5f * std::sin(Time * Chase + (float)r * (2.0f * pi / (float)Rivets) * 2.0f);
		Graphics()->SetColor(Lerp(Shade(CASINO_GOLD, 0.55f), CASINO_GOLD, Glow));
		Graphics()->DrawCircle(Center.x + std::cos(A) * (Radius + 7.0f), Center.y + std::sin(A) * (Radius + 7.0f), 3.2f, 10);
	}
	Graphics()->QuadsEnd();

	// multiplier tags: dark pill behind the text so it reads on any segment colour
	Angle = m_WheelAngle;
	for(int i = 0; i < 7; i++)
	{
		const float Slice = (float)s_aWheelWeight[i] / (float)TotalWeight * 2.0f * pi;
		const float Mid = Angle + Slice * 0.5f;
		const vec2 Pos = Center + vec2(std::cos(Mid), std::sin(Mid)) * (Radius * 0.78f);

		char aBuf[16];
		if(s_aWheelValue[i] <= 0.0f)
			str_copy(aBuf, "LOSE", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "%.1fx", s_aWheelValue[i]);

		CUIRect Tag;
		Tag.w = 56.0f;
		Tag.h = 20.0f;
		Tag.x = Pos.x - Tag.w * 0.5f;
		Tag.y = Pos.y - Tag.h * 0.5f;
		Tag.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.42f), IGraphics::CORNER_ALL, Tag.h * 0.5f);
		const bool BigWin = s_aWheelValue[i] >= 5.0f;
		Label(&Tag, aBuf, 11.0f, TEXTALIGN_MC, BigWin ? CASINO_GOLD : ColorRGBA(1.0f, 1.0f, 1.0f, 0.95f));
		Angle += Slice;
	}

	// layered hub
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Shade(CASINO_GOLD_DEEP, 0.8f));
	Graphics()->DrawCircle(Center.x, Center.y, Radius * 0.16f, 32);
	Graphics()->SetColor(CASINO_GOLD);
	Graphics()->DrawCircle(Center.x, Center.y, Radius * 0.115f, 32);
	Graphics()->SetColor(ColorRGBA(1.0f, 0.940f, 0.720f, 1.0f));
	Graphics()->DrawCircle(Center.x, Center.y, Radius * 0.06f, 24);
	Graphics()->QuadsEnd();

	// pointer: gold outline, red body, pivot pin
	{
		const float TipY = Center.y - Radius + 4.0f;
		const float BaseY = Center.y - Radius - 18.0f;
		const float HalfW = 11.0f;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(CASINO_GOLD);
		IGraphics::CFreeformItem Outer(
			vec2(Center.x, TipY),
			vec2(Center.x - HalfW - 2.5f, BaseY - 2.5f),
			vec2(Center.x + HalfW + 2.5f, BaseY - 2.5f),
			vec2(Center.x + HalfW + 2.5f, BaseY - 2.5f));
		Graphics()->QuadsDrawFreeform(&Outer, 1);
		Graphics()->SetColor(CASINO_RED);
		IGraphics::CFreeformItem InnerT(
			vec2(Center.x, TipY - 3.0f),
			vec2(Center.x - HalfW, BaseY),
			vec2(Center.x + HalfW, BaseY),
			vec2(Center.x + HalfW, BaseY));
		Graphics()->QuadsDrawFreeform(&InnerT, 1);
		Graphics()->SetColor(CASINO_GOLD);
		Graphics()->DrawCircle(Center.x, BaseY, 5.0f, 16);
		Graphics()->QuadsEnd();
	}

	if(m_WheelState == 2 && m_WheelResult >= 0)
	{
		CUIRect Badge;
		Badge.w = 168.0f;
		Badge.h = 40.0f;
		Badge.x = WheelArea.x + WheelArea.w * 0.5f - Badge.w * 0.5f;
		Badge.y = WheelArea.y + WheelArea.h - Badge.h - 14.0f;

		char aBuf[64];
		if(s_aWheelValue[m_WheelResult] <= 0.0f)
			str_copy(aBuf, "NO PRIZE", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "%.1fx  +%d", s_aWheelValue[m_WheelResult], m_WheelLastWin);

		ResultBanner(&Badge, aBuf, m_WheelLastWin > 0);
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, SpinCard, OddsCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(112.0f, &SpinCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &OddsCard, &Ctrl);

	RailCard(&BetCard, "BET");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_WheelBet, 10, std::max(10, m_Credits), m_WheelState != 1);
	}

	RailCard(&SpinCard, "SPIN");
	{
		CUIRect Inner = SpinCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect SpinBtn;
		Inner.HSplitTop(56.0f, &SpinBtn, &Inner);
		if(ActionButton(&SpinBtn, m_WheelState == 1 ? "SPINNING" : "SPIN", ColorRGBA(0.720f, 0.440f, 0.940f, 1.0f), m_WheelState != 1, 18.0f))
			WheelSpin();

		Inner.HSplitTop(8.0f, nullptr, &Inner);
		CUIRect ResultBox;
		Inner.HSplitTop(std::max(24.0f, std::min(34.0f, Inner.h)), &ResultBox, &Inner);
		ResultBanner(&ResultBox, m_WheelState == 1 ? "SPINNING" : "READY", false, true);
	}

	RailCard(&OddsCard, "SEGMENTS");
	{
		CUIRect Inner = OddsCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(20.0f, nullptr, &Inner);

		// all seven segments must fit, so the row height follows the space left over
		const float RowH = std::clamp(Inner.h / 7.0f, 12.0f, 22.0f);

		for(int i = 0; i < 7; i++)
		{
			CUIRect Row;
			Inner.HSplitTop(RowH, &Row, &Inner);
			if(Row.h < 8.0f)
				break;

			CUIRect Swatch;
			Swatch.w = 10.0f;
			Swatch.h = 10.0f;
			Swatch.x = Row.x;
			Swatch.y = Row.y + (Row.h - Swatch.h) * 0.5f;
			Swatch.Draw(s_aWheelColor[i], IGraphics::CORNER_ALL, 0.0f);

			const float Pct = (float)s_aWheelWeight[i] / (float)TotalWeight;
			CUIRect BarTrack;
			BarTrack.x = Row.x + 18.0f;
			BarTrack.y = Row.y + Row.h * 0.5f - 2.0f;
			BarTrack.w = Row.w - 88.0f;
			BarTrack.h = 4.0f;
			BarTrack.Draw(ColorRGBA(0.130f, 0.145f, 0.180f, 1.0f), IGraphics::CORNER_NONE, 0.0f);

			CUIRect BarFill = BarTrack;
			BarFill.w = std::max(2.0f, BarTrack.w * Pct);
			BarFill.Draw(s_aWheelColor[i], IGraphics::CORNER_NONE, 0.0f);

			char aBuf[32];
			if(s_aWheelValue[i] <= 0.0f)
				str_format(aBuf, sizeof(aBuf), "LOSE %.0f%%", Pct * 100.0f);
			else
				str_format(aBuf, sizeof(aBuf), "%.1fx  %.0f%%", s_aWheelValue[i], Pct * 100.0f);

			CUIRect Text = Row;
			Text.x = Row.x + Row.w - 84.0f;
			Text.w = 84.0f;
			Label(&Text, aBuf, 9.0f, TEXTALIGN_MR, s_aWheelValue[i] <= 0.0f ? ColorRGBA(0.560f, 0.590f, 0.670f, 1.0f) : CASINO_GOLD);
		}
	}
}

void CCasino::RenderBlackjack(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "BLACKJACK", "DEALER STANDS ON 17  /  BLACKJACK PAYS 3:2  /  SPLIT ONCE", "BJ", ColorRGBA(0.320f, 0.780f, 0.520f, 1.0f));

	CUIRect Table, Rail;
	Body.VSplitRight(262.0f, &Table, &Rail);
	Table.VSplitRight(14.0f, &Table, nullptr);

	Felt(&Table, 12.0f);

	CUIRect DealerRow, PlayerRow;
	Table.HSplitTop(Table.h * 0.42f, &DealerRow, &PlayerRow);
	DealerRow.HMargin(12.0f, &DealerRow);
	PlayerRow.HMargin(10.0f, &PlayerRow);

	char aBuf[128];

	{
		CUIRect Tag;
		Tag.w = 148.0f;
		Tag.h = 24.0f;
		Tag.x = DealerRow.x + DealerRow.w * 0.5f - Tag.w * 0.5f;
		Tag.y = DealerRow.y;

		if(m_Blackjack.m_HoleHidden)
			str_copy(aBuf, "DEALER", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "DEALER   %d", BlackjackTotal(m_Blackjack.m_vDealer));

		Pill(&Tag, aBuf, ColorRGBA(0.070f, 0.130f, 0.098f, 1.0f), ColorRGBA(0.760f, 0.960f, 0.830f, 1.0f), 10.0f);
	}

	const float CardW = 74.0f;
	const float CardH = 104.0f;
	const float DealerStep = 30.0f;
	{
		const int Count = (int)m_Blackjack.m_vDealer.size();
		const float TotalW = CardW + std::max(0, Count - 1) * DealerStep;
		const float BaseX = DealerRow.x + (DealerRow.w - TotalW) * 0.5f;
		for(int i = 0; i < Count; i++)
		{
			CUIRect Card;
			Card.w = CardW;
			Card.h = CardH;
			Card.x = BaseX + i * DealerStep;
			Card.y = DealerRow.y + 34.0f;
			DrawCard(&Card, m_Blackjack.m_vDealer[i], m_Blackjack.m_HoleHidden && i == 1);
		}
	}

	{
		CUIRect Divider;
		Divider.x = Table.x + 20.0f;
		Divider.y = DealerRow.y + DealerRow.h - 2.0f;
		Divider.w = Table.w - 40.0f;
		Divider.h = 1.0f;
		Divider.Draw(ColorRGBA(0.520f, 0.860f, 0.660f, 0.16f), IGraphics::CORNER_NONE, 0.0f);

		CUIRect Mid;
		Mid.w = 120.0f;
		Mid.h = 16.0f;
		Mid.x = Table.x + Table.w * 0.5f - 60.0f;
		Mid.y = Divider.y - 8.0f;
		if(m_Blackjack.m_State == 0 || m_Blackjack.m_State == 4)
			Label(&Mid, "BLACKJACK PAYS 3:2", 8.0f, TEXTALIGN_MC, ColorRGBA(0.560f, 0.900f, 0.700f, 0.55f));
	}

	const int HandCount = (int)m_Blackjack.m_vHands.size();
	const float HandWidth = HandCount > 1 ? PlayerRow.w * 0.5f : PlayerRow.w;

	for(int h = 0; h < HandCount; h++)
	{
		const auto &Hand = m_Blackjack.m_vHands[h];
		const bool IsActive = h == m_Blackjack.m_ActiveHand && m_Blackjack.m_State == 1;

		CUIRect HandArea;
		HandArea.x = PlayerRow.x + h * HandWidth;
		HandArea.y = PlayerRow.y;
		HandArea.w = HandWidth;
		HandArea.h = PlayerRow.h;

		const int Count = (int)Hand.m_vCards.size();
		const float Step = 32.0f;
		const float TotalW = CardW + std::max(0, Count - 1) * Step;
		const float BaseX = HandArea.x + (HandArea.w - TotalW) * 0.5f;

		if(IsActive)
		{
			CUIRect Halo;
			Halo.x = BaseX - 8.0f;
			Halo.y = HandArea.y + 26.0f;
			Halo.w = TotalW + 16.0f;
			Halo.h = CardH + 16.0f;
			Halo.Draw(ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.10f), IGraphics::CORNER_ALL, CASINO_RADIUS_MAX);
		}

		for(int i = 0; i < Count; i++)
		{
			CUIRect Card;
			Card.w = CardW;
			Card.h = CardH;
			Card.x = BaseX + i * Step;
			Card.y = HandArea.y + 32.0f;
			DrawCard(&Card, Hand.m_vCards[i], false, IsActive);
		}

		const int Total = BlackjackTotal(Hand.m_vCards);
		if(Count > 0)
		{
			CUIRect TotalTag;
			TotalTag.w = 74.0f;
			TotalTag.h = 26.0f;
			TotalTag.x = BaseX + TotalW * 0.5f - TotalTag.w * 0.5f;
			TotalTag.y = HandArea.y + 32.0f + CardH + 12.0f;

			str_format(aBuf, sizeof(aBuf), "%d", Total);
			const ColorRGBA TotalFill = Total > 21 ? ColorRGBA(0.240f, 0.090f, 0.100f, 1.0f) : (Total == 21 ? ColorRGBA(0.220f, 0.180f, 0.060f, 1.0f) : ColorRGBA(0.060f, 0.090f, 0.075f, 1.0f));
			const ColorRGBA TotalInk = Total > 21 ? ColorRGBA(1.000f, 0.660f, 0.680f, 1.0f) : (Total == 21 ? CASINO_GOLD : ColorRGBA(0.860f, 0.920f, 0.890f, 1.0f));
			Pill(&TotalTag, aBuf, TotalFill, TotalInk, 15.0f);

			if(Hand.m_Surrendered)
			{
				CUIRect Tag = TotalTag;
				Tag.y += 30.0f;
				Tag.w = 92.0f;
				Tag.x = BaseX + TotalW * 0.5f - Tag.w * 0.5f;
				Pill(&Tag, "SURRENDERED", ColorRGBA(0.140f, 0.130f, 0.100f, 1.0f), ColorRGBA(0.720f, 0.700f, 0.620f, 1.0f), 8.0f);
			}
		}
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, ActionCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h - 56.0f, &ActionCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "STAKE");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_Blackjack.m_Stake, 10, std::max(10, m_Credits), m_Blackjack.m_State == 0 || m_Blackjack.m_State == 4);
	}

	RailCard(&ActionCard, "ACTIONS");
	{
		CUIRect Inner = ActionCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		if(m_Blackjack.m_State == 0 || m_Blackjack.m_State == 4)
		{
			CUIRect DealBtn;
			Inner.HSplitTop(54.0f, &DealBtn, &Inner);
			if(ActionButton(&DealBtn, m_Blackjack.m_State == 4 ? "DEAL AGAIN" : "DEAL", CASINO_GOLD, m_Credits >= m_Blackjack.m_Stake, 18.0f))
				BlackjackDeal();
		}
		else if(m_Blackjack.m_State == 1)
		{
			const char *apNames[4] = {"HIT", "STAND", "DOUBLE", "SPLIT"};
			const float BtnW = (Inner.w - 8.0f) * 0.5f;
			for(int i = 0; i < 4; i++)
			{
				CUIRect Btn;
				Btn.w = BtnW;
				Btn.h = 36.0f;
				Btn.x = Inner.x + (i % 2) * (BtnW + 8.0f);
				Btn.y = Inner.y + (i / 2) * 44.0f;

				bool Enabled = true;
				if(i == 2)
					Enabled = m_Credits >= m_Blackjack.m_vHands[m_Blackjack.m_ActiveHand].m_Stake && m_Blackjack.m_vHands[m_Blackjack.m_ActiveHand].m_vCards.size() == 2;
				if(i == 3)
				{
					auto &Hand = m_Blackjack.m_vHands[m_Blackjack.m_ActiveHand];
					Enabled = m_Blackjack.m_vHands.size() < 2 && Hand.m_vCards.size() == 2 &&
						CardValue(Hand.m_vCards[0]) == CardValue(Hand.m_vCards[1]) && m_Credits >= Hand.m_Stake;
				}

				if(Button(&Btn, apNames[i], false, Enabled, 13.0f))
				{
					if(i == 0) BlackjackHit();
					else if(i == 1) BlackjackStand();
					else if(i == 2) BlackjackDouble();
					else BlackjackSplit();
				}
			}

			CUIRect Surr;
			Surr.x = Inner.x;
			Surr.y = Inner.y + 92.0f;
			Surr.w = Inner.w;
			Surr.h = 26.0f;
			const bool CanSurrender = m_Blackjack.m_vHands.size() == 1 && m_Blackjack.m_vHands[0].m_vCards.size() == 2;
			if(Button(&Surr, "SURRENDER  1/2 BACK", false, CanSurrender, 9.0f))
				BlackjackSurrender();
		}
		else
		{
			const bool Dealing = m_Blackjack.m_State == 5;
			CUIRect Wait;
			Inner.HSplitTop(54.0f, &Wait, &Inner);
			Frame(&Wait, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.160f, 0.178f, 0.228f, 1.0f), 6.0f);

			// pulsing dots so the wait reads as a timer, not a dead panel
			char aDots[8] = "";
			const int Dots = Dealing ? (int)(Client()->LocalTime() * 3.0f) % 4 : 0;
			for(int d = 0; d < Dots; d++)
				str_append(aDots, ".", sizeof(aDots));
			char aWait[32];
			str_format(aWait, sizeof(aWait), Dealing ? "DEALING%s" : "DEALER PLAYS", aDots);
			Label(&Wait, aWait, 12.0f, TEXTALIGN_MC, ColorRGBA(0.520f, 0.560f, 0.650f, 1.0f));

			if(Dealing)
			{
				const int Dealt = (int)(m_Blackjack.m_vHands[0].m_vCards.size() + m_Blackjack.m_vDealer.size());
				CUIRect Track;
				Track.x = Wait.x + 20.0f;
				Track.y = Wait.y + Wait.h - 10.0f;
				Track.w = Wait.w - 40.0f;
				Track.h = 3.0f;
				Track.Draw(ColorRGBA(0.130f, 0.145f, 0.180f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
				CUIRect Fill = Track;
				Fill.w = Track.w * (float)Dealt / 4.0f;
				Fill.Draw(CASINO_GOLD, IGraphics::CORNER_NONE, 0.0f);
			}
		}
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect Box;
		Inner.HSplitTop(34.0f, &Box, &Inner);
		if(m_Blackjack.m_aMessage[0] != '\0')
			ResultBanner(&Box, m_Blackjack.m_aMessage, m_Blackjack.m_LastWin > 0);
		else
		{
			Frame(&Box, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
			Label(&Box, "PLACE YOUR STAKE", 10.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
		}
	}
}

void CCasino::RenderCaribbean(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "CARIBBEAN STUD", "DEALER QUALIFIES ON PAIR OR BETTER  /  PLAY DOUBLES YOUR STAKE", "CS", ColorRGBA(0.980f, 0.700f, 0.320f, 1.0f));

	CUIRect Table, Rail;
	Body.VSplitRight(262.0f, &Table, &Rail);
	Table.VSplitRight(14.0f, &Table, nullptr);

	Felt(&Table, 12.0f);

	CUIRect DealerRow, PlayerRow;
	Table.HSplitTop(Table.h * 0.48f, &DealerRow, &PlayerRow);
	DealerRow.HMargin(12.0f, &DealerRow);
	PlayerRow.HMargin(10.0f, &PlayerRow);

	char aBuf[160];

	{
		CUIRect Tag;
		Tag.w = 210.0f;
		Tag.h = 24.0f;
		Tag.x = DealerRow.x + DealerRow.w * 0.5f - Tag.w * 0.5f;
		Tag.y = DealerRow.y;

		if(m_Caribbean.m_State == 0)
			str_copy(aBuf, "DEALER", sizeof(aBuf));
		else if(m_Caribbean.m_State == 1)
			str_copy(aBuf, "DEALER  -  1 CARD SHOWN", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "DEALER  -  %s", PokerHandName(EvaluatePokerHand(m_Caribbean.m_vDealer)));

		Pill(&Tag, aBuf, ColorRGBA(0.070f, 0.130f, 0.098f, 1.0f), ColorRGBA(0.760f, 0.960f, 0.830f, 1.0f), 9.0f);
	}

	const float CardW = 72.0f;
	const float CardH = 102.0f;

	{
		const int Count = (int)m_Caribbean.m_vDealer.size();
		const float Step = 46.0f;
		const float TotalW = CardW + std::max(0, Count - 1) * Step;
		const float BaseX = DealerRow.x + (DealerRow.w - TotalW) * 0.5f;
		for(int i = 0; i < Count; i++)
		{
			CUIRect Card;
			Card.w = CardW;
			Card.h = CardH;
			Card.x = BaseX + i * Step;
			Card.y = DealerRow.y + 34.0f;
			DrawCard(&Card, m_Caribbean.m_vDealer[i], m_Caribbean.m_State == 1 && i > 0);
		}
	}

	{
		CUIRect Tag;
		Tag.w = 260.0f;
		Tag.h = 24.0f;
		Tag.x = PlayerRow.x + PlayerRow.w * 0.5f - Tag.w * 0.5f;
		Tag.y = PlayerRow.y;

		if(m_Caribbean.m_vPlayer.empty())
			str_copy(aBuf, "YOUR HAND", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "YOUR HAND  -  %s", PokerHandName(EvaluatePokerHand(m_Caribbean.m_vPlayer)));

		Pill(&Tag, aBuf, ColorRGBA(0.120f, 0.100f, 0.055f, 1.0f), CASINO_GOLD, 9.0f);
	}

	{
		const int Count = (int)m_Caribbean.m_vPlayer.size();
		const float Step = 46.0f;
		const float TotalW = CardW + std::max(0, Count - 1) * Step;
		const float BaseX = PlayerRow.x + (PlayerRow.w - TotalW) * 0.5f;
		for(int i = 0; i < Count; i++)
		{
			CUIRect Card;
			Card.w = CardW;
			Card.h = CardH;
			Card.x = BaseX + i * Step;
			Card.y = PlayerRow.y + 34.0f;
			DrawCard(&Card, m_Caribbean.m_vPlayer[i], false, m_Caribbean.m_State == 1);
		}
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, ActionCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h - 56.0f, &ActionCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "ANTE");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_Caribbean.m_Stake, 10, std::max(10, m_Credits), m_Caribbean.m_State == 0 || m_Caribbean.m_State == 2);
	}

	RailCard(&ActionCard, "ACTIONS");
	{
		CUIRect Inner = ActionCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		if(m_Caribbean.m_State == 0 || m_Caribbean.m_State == 2)
		{
			CUIRect DealBtn;
			Inner.HSplitTop(54.0f, &DealBtn, &Inner);
			if(ActionButton(&DealBtn, "DEAL", CASINO_GOLD, m_Credits >= m_Caribbean.m_Stake, 18.0f))
				CaribbeanDeal();
		}
		else if(m_Caribbean.m_State == 1)
		{
			CUIRect PlayBtn, FoldBtn;
			Inner.HSplitTop(46.0f, &PlayBtn, &Inner);
			Inner.HSplitTop(8.0f, nullptr, &Inner);
			Inner.HSplitTop(38.0f, &FoldBtn, &Inner);

			if(ActionButton(&PlayBtn, "PLAY", ColorRGBA(0.320f, 0.800f, 0.540f, 1.0f), m_Credits >= m_Caribbean.m_Stake, 16.0f))
				CaribbeanPlay();
			if(Button(&FoldBtn, "FOLD", false, true, 12.0f))
				CaribbeanFold();
		}

		CUIRect Info;
		Info.x = Inner.x;
		Info.w = Inner.w;
		Info.h = 16.0f;
		Info.y = ActionCard.y + ActionCard.h - 24.0f;
		Label(&Info, "RAISE MATCHES THE ANTE  /  PAYS THE TABLE", 8.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect Box;
		Inner.HSplitTop(34.0f, &Box, &Inner);
		if(m_Caribbean.m_aMessage[0] != '\0')
			ResultBanner(&Box, m_Caribbean.m_aMessage, m_Caribbean.m_LastWin > 0);
		else
		{
			Frame(&Box, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
			Label(&Box, "DEAL TO BEGIN", 10.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
		}
	}
}

void CCasino::RenderHiLo(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "HI-LO", "GUESS THE NEXT CARD  /  MULTIPLIER GROWS EACH CORRECT CALL", "HL", CASINO_ICE);

	CUIRect StageArea, Rail;
	Body.VSplitRight(262.0f, &StageArea, &Rail);
	StageArea.VSplitRight(14.0f, &StageArea, nullptr);

	Felt(&StageArea, 12.0f);

	CUIRect CardSlot, StreakArea;
	StageArea.HSplitTop(StageArea.h * 0.62f, &CardSlot, &StreakArea);
	CardSlot.HMargin(14.0f, &CardSlot);

	const float CardW = 118.0f;
	const float CardH = 166.0f;
	CUIRect Card;
	Card.w = CardW;
	Card.h = CardH;
	Card.x = CardSlot.x + CardSlot.w * 0.5f - CardW * 0.5f;
	Card.y = CardSlot.y + (CardSlot.h - CardH) * 0.5f - 10.0f;

	// plinth the card sits on, plus a gold halo while a guess is pending
	{
		CUIRect Plinth = Card;
		Plinth.x -= 10.0f;
		Plinth.y -= 10.0f;
		Plinth.w += 20.0f;
		Plinth.h += 20.0f;
		Plinth.Draw(ColorRGBA(CASINO_ICE.r, CASINO_ICE.g, CASINO_ICE.b, 0.07f), IGraphics::CORNER_ALL, 3.0f);
		Plinth.DrawOutline(ColorRGBA(CASINO_ICE.r, CASINO_ICE.g, CASINO_ICE.b, 0.22f));
		if(m_HiLo.m_State == 1)
		{
			CUIRect Halo = Card;
			Halo.x -= 5.0f;
			Halo.y -= 5.0f;
			Halo.w += 10.0f;
			Halo.h += 10.0f;
			Halo.DrawOutline(ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.45f));
		}
	}

	// settle burst: expanding ring on a cash out, red pulse on a loss
	if(m_HiLo.m_SettleAnim > 0.0f && m_HiLo.m_State == 2)
	{
		if(m_HiLo.m_LastWin > 0)
			Ring(vec2(Card.x + Card.w * 0.5f, Card.y + Card.h * 0.5f), 60.0f + 70.0f * (1.0f - m_HiLo.m_SettleAnim), 3.0f, ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, m_HiLo.m_SettleAnim * 0.5f));
		else
		{
			CUIRect Pulse = Card;
			Pulse.x -= 6.0f;
			Pulse.y -= 6.0f;
			Pulse.w += 12.0f;
			Pulse.h += 12.0f;
			Pulse.DrawOutline(ColorRGBA(CASINO_RED.r, CASINO_RED.g, CASINO_RED.b, m_HiLo.m_SettleAnim * 0.7f));
		}
	}

	// the reveal is a horizontal flip: the old card squashes to a sliver, the new one grows back
	const float FlipT = m_HiLo.m_RevealTime > 0.0f ? 1.0f - std::clamp(m_HiLo.m_RevealTime / 0.5f, 0.0f, 1.0f) : 1.0f;
	if(FlipT < 1.0f)
	{
		Ui()->ClipEnable(&CardSlot);
		const bool FirstHalf = FlipT < 0.5f;
		const float W = std::max(4.0f, CardW * (FirstHalf ? 1.0f - FlipT * 2.0f : (FlipT - 0.5f) * 2.0f));
		CUIRect Flip = Card;
		Flip.w = W;
		Flip.x = Card.x + (CardW - W) * 0.5f;
		DrawCard(&Flip, FirstHalf ? m_HiLo.m_Prev : m_HiLo.m_Current, false, !FirstHalf);
		Ui()->ClipDisable();
	}
	else
	{
		DrawCard(&Card, m_HiLo.m_Current, m_HiLo.m_State == 0, m_HiLo.m_State == 1);
	}

	// multiplier + cash pills under the card; the multiplier pops while it ticks up
	char aBuf[64];
	const float DispMult = m_HiLo.m_State == 2 ? m_HiLo.m_LastMult : m_HiLo.m_PrevMult + (m_HiLo.m_Multiplier - m_HiLo.m_PrevMult) * (1.0f - m_HiLo.m_MultAnim);
	CUIRect MultTag;
	MultTag.w = 116.0f;
	MultTag.h = 26.0f;
	MultTag.x = Card.x + CardW * 0.5f - MultTag.w - 4.0f;
	MultTag.y = Card.y + CardH + 8.0f;
	str_format(aBuf, sizeof(aBuf), "%.2fx", DispMult);
	if(m_HiLo.m_MultAnim > 0.0f)
	{
		MultTag.x -= 2.0f * m_HiLo.m_MultAnim;
		MultTag.y -= 2.0f * m_HiLo.m_MultAnim;
		MultTag.w += 4.0f * m_HiLo.m_MultAnim;
		MultTag.h += 4.0f * m_HiLo.m_MultAnim;
	}
	Pill(&MultTag, aBuf, ColorRGBA(0.120f, 0.100f, 0.055f, 1.0f), CASINO_GOLD, 14.0f);
	if(m_HiLo.m_MultAnim > 0.0f)
		MultTag.DrawOutline(ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.6f * m_HiLo.m_MultAnim));

	CUIRect CashTag;
	CashTag.w = 116.0f;
	CashTag.h = 26.0f;
	CashTag.x = Card.x + CardW * 0.5f + 4.0f;
	CashTag.y = Card.y + CardH + 8.0f;
	str_format(aBuf, sizeof(aBuf), "CASH  %d", m_HiLo.m_State == 2 ? m_HiLo.m_LastWin : (int)((m_HiLo.m_RoundStake > 0 ? m_HiLo.m_RoundStake : m_HiLo.m_Stake) * DispMult));
	Pill(&CashTag, aBuf, ColorRGBA(0.060f, 0.100f, 0.085f, 1.0f), ColorRGBA(0.700f, 0.960f, 0.800f, 1.0f), 10.0f);

	// odds meter: how the remaining ranks split into lower / tie / higher
	if(m_HiLo.m_State != 0)
	{
		int HigherCount = 0, LowerCount = 0, EqualCount = 0;
		for(int Rank = 0; Rank < 13; Rank++)
		{
			if(Rank > m_HiLo.m_Current.m_Rank)
				HigherCount += 4;
			else if(Rank < m_HiLo.m_Current.m_Rank)
				LowerCount += 4;
			else
				EqualCount += 4;
		}

		CUIRect Bar;
		Bar.x = StreakArea.x + 14.0f;
		Bar.w = StreakArea.w - 28.0f;
		Bar.y = StreakArea.y + 2.0f;
		Bar.h = 14.0f;
		Bar.Draw(ColorRGBA(0.043f, 0.047f, 0.066f, 0.85f), IGraphics::CORNER_ALL, 2.0f);

		const float LowerW = Bar.w * (float)LowerCount / 52.0f;
		const float TieW = Bar.w * (float)EqualCount / 52.0f;
		CUIRect Seg = Bar;
		Seg.w = LowerW;
		if(Seg.w > 0.5f)
			Seg.Draw(ColorRGBA(0.820f, 0.320f, 0.380f, 0.85f), IGraphics::CORNER_NONE, 0.0f);
		Seg.x += Seg.w;
		Seg.w = TieW;
		if(Seg.w > 0.5f)
			Seg.Draw(ColorRGBA(0.545f, 0.588f, 0.680f, 0.45f), IGraphics::CORNER_NONE, 0.0f);
		Seg.x += Seg.w;
		Seg.w = Bar.x + Bar.w - Seg.x;
		if(Seg.w > 0.5f)
			Seg.Draw(ColorRGBA(0.300f, 0.760f, 0.520f, 0.85f), IGraphics::CORNER_NONE, 0.0f);
		Bar.DrawOutline(ColorRGBA(0.235f, 0.262f, 0.340f, 0.8f));

		// white marker sits on the current rank inside the tie zone
		CUIRect Marker;
		Marker.w = 2.0f;
		Marker.h = Bar.h + 4.0f;
		Marker.x = Bar.x + LowerW + TieW * 0.5f - 1.0f;
		Marker.y = Bar.y - 2.0f;
		Marker.Draw(ColorRGBA(0.950f, 0.960f, 1.000f, 0.9f), IGraphics::CORNER_NONE, 0.0f);

		CUIRect Cap;
		Cap.x = Bar.x;
		Cap.w = Bar.w;
		Cap.y = Bar.y + Bar.h + 3.0f;
		Cap.h = 10.0f;
		str_format(aBuf, sizeof(aBuf), "LOWER %d%%   TIE %d%%   HIGHER %d%%", LowerCount * 100 / 52, EqualCount * 100 / 52, HigherCount * 100 / 52);
		Label(&Cap, aBuf, 8.0f, TEXTALIGN_MC, ColorRGBA(0.560f, 0.600f, 0.680f, 0.9f));
	}

	{
		CUIRect StreakLabel;
		StreakLabel.x = StreakArea.x;
		StreakLabel.y = StreakArea.y + 30.0f;
		StreakLabel.w = StreakArea.w;
		StreakLabel.h = 10.0f;
		Label(&StreakLabel, "STREAK", 8.0f, TEXTALIGN_MC, ColorRGBA(0.560f, 0.900f, 0.700f, 0.60f));
	}

	const float DotY = StreakArea.y + 50.0f;
	const float DotStep = 24.0f;
	const float DotStart = StreakArea.x + StreakArea.w * 0.5f - (9.0f * DotStep) * 0.5f;
	for(int i = 0; i < 10; i++)
	{
		const bool Lit = i < m_HiLo.m_Streak;
		const vec2 Pos(DotStart + i * DotStep + 8.0f, DotY);
		GlowDot(Pos, Lit ? 6.0f : 4.0f, Lit ? CASINO_GOLD : ColorRGBA(0.150f, 0.190f, 0.170f, 1.0f));
	}

	// guess history: newest card on the left, the bottom bar shows which way it went
	{
		const float ChipW = 30.0f;
		const float ChipH = 40.0f;
		const float ChipGap = 4.0f;
		const float StripW = 8.0f * ChipW + 7.0f * ChipGap;
		const float StripX = StreakArea.x + (StreakArea.w - StripW) * 0.5f;
		const float StripY = StreakArea.y + 64.0f;
		for(int i = 0; i < 8; i++)
		{
			CUIRect Chip;
			Chip.w = ChipW;
			Chip.h = ChipH;
			Chip.x = StripX + i * (ChipW + ChipGap);
			Chip.y = StripY;
			if(i >= m_HiLo.m_HistCount)
			{
				Frame(&Chip, ColorRGBA(0.055f, 0.100f, 0.075f, 0.6f), ColorRGBA(0.150f, 0.190f, 0.170f, 0.5f), 3.0f);
				continue;
			}
			const SCasinoCard &Hist = m_HiLo.m_aHistory[i];
			const bool Red = Hist.m_Suit == 1 || Hist.m_Suit == 2;
			const ColorRGBA Ink = Red ? ColorRGBA(0.830f, 0.430f, 0.470f, 1.0f) : ColorRGBA(0.760f, 0.780f, 0.830f, 1.0f);
			Frame(&Chip, ColorRGBA(0.075f, 0.120f, 0.090f, 1.0f), ColorRGBA(0.200f, 0.240f, 0.220f, 1.0f), 3.0f);

			char aRank[8];
			if(Hist.m_Rank == 0)
				str_copy(aRank, "A", sizeof(aRank));
			else if(Hist.m_Rank == 10)
				str_copy(aRank, "J", sizeof(aRank));
			else if(Hist.m_Rank == 11)
				str_copy(aRank, "Q", sizeof(aRank));
			else if(Hist.m_Rank == 12)
				str_copy(aRank, "K", sizeof(aRank));
			else
				str_format(aRank, sizeof(aRank), "%d", Hist.m_Rank + 1);
			const char *pSuit = "?";
			switch(Hist.m_Suit)
			{
			case 0: pSuit = "S"; break;
			case 1: pSuit = "H"; break;
			case 2: pSuit = "D"; break;
			case 3: pSuit = "C"; break;
			}

			CUIRect RankRect = Chip;
			RankRect.HSplitTop(18.0f, &RankRect, nullptr);
			Label(&RankRect, aRank, 12.0f, TEXTALIGN_MC, Ink);
			CUIRect SuitRect = Chip;
			SuitRect.HSplitTop(16.0f, nullptr, &SuitRect);
			SuitRect.HSplitTop(14.0f, &SuitRect, nullptr);
			Label(&SuitRect, pSuit, 9.0f, TEXTALIGN_MC, Ink);

			const int Dir = m_HiLo.m_aHistDir[i];
			CUIRect DirBar;
			DirBar.x = Chip.x + 3.0f;
			DirBar.w = Chip.w - 6.0f;
			DirBar.h = 3.0f;
			DirBar.y = Chip.y + Chip.h - 6.0f;
			DirBar.Draw(Dir > 0 ? ColorRGBA(0.300f, 0.760f, 0.520f, 1.0f) : (Dir < 0 ? ColorRGBA(0.820f, 0.320f, 0.380f, 1.0f) : ColorRGBA(0.545f, 0.588f, 0.680f, 0.6f)), IGraphics::CORNER_NONE, 0.0f);
		}
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, ActionCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h - 70.0f, &ActionCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "STAKE");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_HiLo.m_Stake, 10, std::max(10, m_Credits), m_HiLo.m_State == 0 || m_HiLo.m_State == 2);
	}

	RailCard(&ActionCard, "CALL");
	{
		CUIRect Inner = ActionCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		if(m_HiLo.m_State == 0 || m_HiLo.m_State == 2)
		{
			CUIRect StartBtn;
			Inner.HSplitTop(54.0f, &StartBtn, &Inner);
			if(ActionButton(&StartBtn, m_HiLo.m_State == 2 ? "PLAY AGAIN" : "START", CASINO_GOLD, m_Credits >= m_HiLo.m_Stake, 17.0f))
				HiLoStart();
		}
		else
		{
			CUIRect HigherBtn, LowerBtn;
			Inner.HSplitTop(46.0f, &HigherBtn, &Inner);
			Inner.HSplitTop(8.0f, nullptr, &Inner);
			Inner.HSplitTop(46.0f, &LowerBtn, &Inner);
			Inner.HSplitTop(8.0f, nullptr, &Inner);

			const bool CanCall = m_HiLo.m_RevealTime <= 0.0f;
			if(ActionButton(&HigherBtn, "HIGHER", ColorRGBA(0.300f, 0.760f, 0.520f, 1.0f), CanCall, 16.0f))
				HiLoGuess(true);
			if(ActionButton(&LowerBtn, "LOWER", ColorRGBA(0.820f, 0.320f, 0.380f, 1.0f), CanCall, 16.0f))
				HiLoGuess(false);

			CUIRect CashBtn;
			Inner.HSplitTop(36.0f, &CashBtn, &Inner);
			const int RoundStake = m_HiLo.m_RoundStake > 0 ? m_HiLo.m_RoundStake : m_HiLo.m_Stake;
			str_format(aBuf, sizeof(aBuf), "CASH OUT  %d", (int)(RoundStake * m_HiLo.m_Multiplier));
			if(ActionButton(&CashBtn, aBuf, CASINO_GOLD, m_HiLo.m_Streak > 0 && m_HiLo.m_RevealTime <= 0.0f, 13.0f))
				HiLoCashOut();

			CUIRect RiskCap;
			Inner.HSplitTop(4.0f, nullptr, &Inner);
			Inner.HSplitTop(10.0f, &RiskCap, &Inner);
			str_format(aBuf, sizeof(aBuf), "AT RISK %d", RoundStake);
			Label(&RiskCap, aBuf, 8.0f, TEXTALIGN_MC, ColorRGBA(0.470f, 0.505f, 0.590f, 0.9f));
		}
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect Box;
		Inner.HSplitTop(24.0f, &Box, &Inner);
		if(m_HiLo.m_aMessage[0] != '\0')
			ResultBanner(&Box, m_HiLo.m_aMessage, m_HiLo.m_State == 2 && m_HiLo.m_LastWin > 0, m_HiLo.m_State == 1);
		else
		{
			Frame(&Box, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
			Label(&Box, "AWAITING CALL", 10.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
		}
	}
}

void CCasino::RenderDragonTiger(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "DRAGON TIGER", "HIGHEST CARD WINS  /  DRAGON & TIGER PAY 1:1  /  TIE PAYS 11:1", "DT", CASINO_RED);

	CUIRect Table, Rail;
	Body.VSplitRight(262.0f, &Table, &Rail);
	Table.VSplitRight(14.0f, &Table, nullptr);

	Felt(&Table, 12.0f);
	const CUIRect FeltRect = Table; // the splits below turn Table into the 92px gutter

	CUIRect DragonSide, TigerSide;
	Table.VSplitLeft(Table.w * 0.5f - 46.0f, &DragonSide, &Table);
	Table.VSplitLeft(92.0f, nullptr, &TigerSide);

	const float CardW = 96.0f;
	const float CardH = 136.0f;

	const bool Revealing = m_DragonTigerReveal > 0.0f;
	const bool Betting = !Revealing;
	// reveal progress per side: dragon flips first, tiger follows half a beat later
	const float DragonT = std::clamp((0.9f - m_DragonTigerReveal) / 0.45f, 0.0f, 1.0f);
	const float TigerT = std::clamp((0.45f - m_DragonTigerReveal) / 0.45f, 0.0f, 1.0f);
	const bool TieRound = m_DragonTigerState == 1 && m_DragonCard.m_Rank == m_TigerCard.m_Rank;
	const bool DragonWon = m_DragonTigerState == 1 && !TieRound && m_DragonCard.m_Rank > m_TigerCard.m_Rank;
	const float SettleT = m_DragonTigerSettle;

	// the felt side panels are shortcuts for the rail pick buttons
	if(Betting)
	{
		if(Ui()->MouseHovered(&DragonSide) && Ui()->MouseButtonClicked(0))
			m_DragonTigerChoice = 0;
		if(Ui()->MouseHovered(&TigerSide) && Ui()->MouseButtonClicked(0))
			m_DragonTigerChoice = 1;
	}

	auto DrawSide = [&](CUIRect &Side, const char *pName, ColorRGBA Accent, const SCasinoCard &Card, const SCasinoCard &Prev, float FlipT, bool Selected, bool FaceDown, bool Winner) {
		CUIRect Tag;
		Tag.w = 128.0f;
		Tag.h = 26.0f;
		Tag.x = Side.x + Side.w * 0.5f - Tag.w * 0.5f;
		Tag.y = Side.y + 18.0f;
		Pill(&Tag, pName, Selected ? Accent : ColorRGBA(0.075f, 0.110f, 0.095f, 1.0f),
			Selected ? ColorRGBA(0.060f, 0.060f, 0.050f, 1.0f) : Shade(Accent, 1.05f), 12.0f);

		CUIRect Slot;
		Slot.w = CardW + 24.0f;
		Slot.h = CardH + 24.0f;
		Slot.x = Side.x + Side.w * 0.5f - Slot.w * 0.5f;
		Slot.y = Side.y + 56.0f;
		Slot.Draw(ColorRGBA(Accent.r * 0.20f, Accent.g * 0.20f, Accent.b * 0.20f, 0.55f), IGraphics::CORNER_ALL, CASINO_RADIUS_MAX);
		Slot.DrawOutline(ColorRGBA(Accent.r, Accent.g, Accent.b, 0.30f));

		CUIRect CardRect;
		CardRect.w = CardW;
		CardRect.h = CardH;
		CardRect.x = Slot.x + 12.0f;
		CardRect.y = Slot.y + 12.0f;

		// the reveal is a horizontal flip, same shape as the hi-lo one
		if(FlipT < 1.0f)
		{
			Ui()->ClipEnable(&Slot);
			const bool FirstHalf = FlipT < 0.5f;
			const float W = std::max(4.0f, CardW * (FirstHalf ? 1.0f - FlipT * 2.0f : (FlipT - 0.5f) * 2.0f));
			CUIRect Flip = CardRect;
			Flip.w = W;
			Flip.x = CardRect.x + (CardW - W) * 0.5f;
			DrawCard(&Flip, FirstHalf ? Prev : Card, FirstHalf && !m_DragonTigerPrevValid, !FirstHalf);
			Ui()->ClipDisable();
		}
		else
		{
			DrawCard(&CardRect, Card, FaceDown, Selected);
		}

		if(Betting)
		{
			// the chosen side glows, the other one recedes
			if(Selected)
			{
				CUIRect Halo = Slot;
				Halo.x -= 5.0f;
				Halo.y -= 5.0f;
				Halo.w += 10.0f;
				Halo.h += 10.0f;
				Halo.DrawOutline(ColorRGBA(Accent.r, Accent.g, Accent.b, 0.50f));
			}
			else
			{
				Slot.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.22f), IGraphics::CORNER_ALL, CASINO_RADIUS_MAX);
			}
		}
		else if(SettleT > 0.0f && m_DragonTigerState == 1)
		{
			// settle burst: winner ring, loser fade; a tie lights both sides
			if(Winner || TieRound)
				Ring(vec2(CardRect.x + CardRect.w * 0.5f, CardRect.y + CardRect.h * 0.5f), 56.0f + 60.0f * (1.0f - SettleT), 3.0f, ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, SettleT * 0.5f));
			else
				Slot.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.35f * SettleT), IGraphics::CORNER_ALL, CASINO_RADIUS_MAX);
		}

		// payout odds under the slot
		CUIRect Odds;
		Odds.w = 44.0f;
		Odds.h = 20.0f;
		Odds.x = Slot.x + Slot.w * 0.5f - Odds.w * 0.5f;
		Odds.y = Slot.y + Slot.h + 8.0f;
		Pill(&Odds, "1:1", ColorRGBA(Accent.r * 0.16f, Accent.g * 0.16f, Accent.b * 0.16f, 1.0f), Shade(Accent, 1.10f), 10.0f);
	};

	DrawSide(DragonSide, "DRAGON", CASINO_RED, m_DragonCard, m_DragonTigerPrevDragon, DragonT,
		m_DragonTigerChoice == 0, m_DragonTigerState == 0, DragonWon);

	{
		CUIRect VS;
		VS.w = 92.0f;
		VS.h = 92.0f;
		VS.x = FeltRect.x + FeltRect.w * 0.5f - VS.w * 0.5f;
		VS.y = FeltRect.y + FeltRect.h * 0.5f - VS.h * 0.5f;

		// solid medallion: GlowDot would splash two extra translucent discs over the felt
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA(0.086f, 0.096f, 0.124f, 1.0f));
		Graphics()->DrawCircle(VS.x + VS.w * 0.5f, VS.y + VS.h * 0.5f, 34.0f, 48);
		Graphics()->QuadsEnd();
		Ring(vec2(VS.x + VS.w * 0.5f, VS.y + VS.h * 0.5f), 34.0f, 1.5f, ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.30f), 48);
		Label(&VS, "VS", 24.0f, TEXTALIGN_MC, ColorRGBA(0.860f, 0.880f, 0.930f, 0.90f));
	}

	DrawSide(TigerSide, "TIGER", CASINO_ICE, m_TigerCard, m_DragonTigerPrevTiger, TigerT,
		m_DragonTigerChoice == 1, m_DragonTigerState == 0, !DragonWon);

	// tie zone on the felt, mirrors the third rail button
	{
		CUIRect TieBar;
		TieBar.w = 116.0f;
		TieBar.h = 30.0f;
		TieBar.x = FeltRect.x + FeltRect.w * 0.5f - TieBar.w * 0.5f;
		TieBar.y = FeltRect.y + FeltRect.h - 114.0f;
		const bool TieSel = m_DragonTigerChoice == 2;
		Pill(&TieBar, "TIE   11:1", TieSel ? CASINO_GOLD : ColorRGBA(0.120f, 0.100f, 0.055f, 1.0f),
			TieSel ? ColorRGBA(0.060f, 0.060f, 0.050f, 1.0f) : CASINO_GOLD, 12.0f);
		TieBar.DrawOutline(TieSel ? CASINO_GOLD : ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.25f));
		if(Betting && Ui()->MouseHovered(&TieBar) && Ui()->MouseButtonClicked(0))
			m_DragonTigerChoice = 2;
	}

	// last rounds: newest on the left, the underline marks how the pick fared
	{
		CUIRect Cap;
		Cap.x = FeltRect.x;
		Cap.w = FeltRect.w;
		Cap.y = FeltRect.y + FeltRect.h - 82.0f;
		Cap.h = 10.0f;
		Label(&Cap, "LAST ROUNDS", 8.0f, TEXTALIGN_MC, ColorRGBA(0.560f, 0.600f, 0.680f, 0.55f));

		const float ChipW = std::min(30.0f, (FeltRect.w - 40.0f - 7.0f * 4.0f) / 8.0f);
		const float ChipH = 22.0f;
		const float ChipGap = 4.0f;
		const float StripW = 8.0f * ChipW + 7.0f * ChipGap;
		const float StripX = FeltRect.x + (FeltRect.w - StripW) * 0.5f;
		const float StripY = FeltRect.y + FeltRect.h - 69.0f;
		for(int i = 0; i < 8; i++)
		{
			CUIRect Chip;
			Chip.w = ChipW;
			Chip.h = ChipH;
			Chip.x = StripX + i * (ChipW + ChipGap);
			Chip.y = StripY;
			if(i >= m_DragonTigerHistCount)
			{
				Frame(&Chip, ColorRGBA(0.055f, 0.100f, 0.075f, 0.6f), ColorRGBA(0.150f, 0.190f, 0.170f, 0.5f), 3.0f);
				continue;
			}
			const int Outcome = m_aDragonTigerHist[i];
			const ColorRGBA Accent = Outcome == 0 ? CASINO_RED : (Outcome == 1 ? CASINO_ICE : CASINO_GOLD);
			Frame(&Chip, ColorRGBA(Accent.r * 0.16f, Accent.g * 0.16f, Accent.b * 0.16f, 1.0f), ColorRGBA(Accent.r, Accent.g, Accent.b, 0.45f), 3.0f);
			Label(&Chip, Outcome == 0 ? "D" : (Outcome == 1 ? "T" : "="), 10.0f, TEXTALIGN_MC, Shade(Accent, 1.10f));

			CUIRect DirBar;
			DirBar.x = Chip.x + 3.0f;
			DirBar.w = Chip.w - 6.0f;
			DirBar.h = 3.0f;
			DirBar.y = Chip.y + Chip.h - 5.0f;
			DirBar.Draw(m_aDragonTigerHistWin[i] ? ColorRGBA(0.300f, 0.760f, 0.520f, 1.0f) : ColorRGBA(0.820f, 0.320f, 0.380f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
		}
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, PickCard, DealCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(130.0f, &PickCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(56.0f, &DealCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "BET");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_DragonTigerBet, 10, std::max(10, m_Credits), Betting);
	}

	RailCard(&PickCard, "SIDE");
	{
		CUIRect Inner = PickCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		const char *apNames[3] = {"DRAGON", "TIGER", "TIE"};
		const char *apOdds[3] = {"1:1", "1:1", "11:1"};
		for(int i = 0; i < 3; i++)
		{
			CUIRect Btn;
			Inner.HSplitTop(28.0f, &Btn, &Inner);
			if(i < 2)
				Inner.HSplitTop(4.0f, nullptr, &Inner);

			CUIRect Odds = Btn;
			Odds.w = 44.0f;
			Odds.h = 18.0f;
			Odds.x = Btn.x + Btn.w - Odds.w;
			Odds.y = Btn.y + (Btn.h - Odds.h) * 0.5f;
			Btn.w -= 50.0f;

			const ColorRGBA Fill = i == 0 ? CASINO_RED : (i == 1 ? CASINO_ICE : CASINO_GOLD);
			if(Button(&Btn, apNames[i], m_DragonTigerChoice == i, Betting, 12.0f))
				m_DragonTigerChoice = i;
			Pill(&Odds, apOdds[i], ColorRGBA(Fill.r * 0.16f, Fill.g * 0.16f, Fill.b * 0.16f, 1.0f), Shade(Fill, 1.10f), 10.0f);
		}
	}

	RailCard(&DealCard, "");
	{
		CUIRect Inner = DealCard;
		Inner.Margin(10.0f, &Inner);
		if(ActionButton(&Inner, m_DragonTigerState == 1 ? "DEAL AGAIN" : "DEAL", CASINO_GOLD, Betting && m_Credits >= m_DragonTigerBet, 17.0f))
			DragonTigerDeal();
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect Box;
		Inner.HSplitTop(24.0f, &Box, &Inner);
		if(Revealing)
			ResultBanner(&Box, "DEALING", false, true);
		else if(m_aDragonTigerMsg[0] != '\0')
			ResultBanner(&Box, m_aDragonTigerMsg, m_DragonTigerLastWin > 0);
		else
			ResultBanner(&Box, "PICK A SIDE", false, true);
	}
}

void CCasino::RenderCraps(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "CRAPS", "COME-OUT 7 OR 11 WINS  /  THEN MAKE YOUR POINT BEFORE A 7", "CR", ColorRGBA(0.520f, 0.840f, 0.620f, 1.0f));

	CUIRect Table, Rail;
	Body.VSplitRight(262.0f, &Table, &Rail);
	Table.VSplitRight(14.0f, &Table, nullptr);

	Felt(&Table, 12.0f);

	CUIRect DiceArea, PointArea;
	Table.HSplitTop(Table.h * 0.60f, &DiceArea, &PointArea);

	const float DieSize = std::min(108.0f, DiceArea.h * 0.52f);
	CUIRect Die1, Die2;
	Die1.w = DieSize;
	Die1.h = DieSize;
	Die1.x = DiceArea.x + DiceArea.w * 0.5f - DieSize - 14.0f;
	Die1.y = DiceArea.y + (DiceArea.h - DieSize) * 0.5f - 10.0f;
	Die2 = Die1;
	Die2.x += DieSize + 28.0f;

	DrawDie(&Die1, m_aCrapsDice[0], m_CrapsState == 1);
	DrawDie(&Die2, m_aCrapsDice[1], m_CrapsState == 1);

	char aBuf[128];
	{
		CUIRect SumTag;
		SumTag.w = 148.0f;
		SumTag.h = 30.0f;
		SumTag.x = DiceArea.x + DiceArea.w * 0.5f - SumTag.w * 0.5f;
		SumTag.y = Die1.y + DieSize + 14.0f;
		str_format(aBuf, sizeof(aBuf), "TOTAL  %d", m_aCrapsDice[0] + m_aCrapsDice[1]);
		Pill(&SumTag, aBuf, ColorRGBA(0.070f, 0.120f, 0.090f, 1.0f), ColorRGBA(0.780f, 0.960f, 0.850f, 1.0f), 13.0f);
	}

	{
		CUIRect Puck;
		Puck.w = 132.0f;
		Puck.h = 132.0f;
		Puck.x = PointArea.x + PointArea.w * 0.5f - Puck.w * 0.5f;
		Puck.y = PointArea.y + (PointArea.h - Puck.h) * 0.5f - 6.0f;

		const bool On = m_CrapsPoint > 0;
		// solid puck backdrop: GlowDot would splash two extra translucent rings around it
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA(0.090f, 0.150f, 0.110f, 1.0f));
		Graphics()->DrawCircle(Puck.x + Puck.w * 0.5f, Puck.y + Puck.h * 0.5f, Puck.w * 0.5f, 48);
		Graphics()->QuadsEnd();
		Ring(vec2(Puck.x + Puck.w * 0.5f, Puck.y + Puck.h * 0.5f), Puck.w * 0.5f - 4.0f, 3.0f,
			On ? ColorRGBA(0.980f, 0.800f, 0.280f, 1.0f) : ColorRGBA(0.400f, 0.480f, 0.440f, 1.0f), 48);

		if(On)
			str_format(aBuf, sizeof(aBuf), "%d", m_CrapsPoint);
		else
			str_copy(aBuf, "OFF", sizeof(aBuf));

		Label(&Puck, aBuf, On ? 48.0f : 20.0f, TEXTALIGN_MC, On ? CASINO_GOLD : ColorRGBA(0.480f, 0.560f, 0.520f, 1.0f));

		CUIRect Cap;
		Cap.w = 120.0f;
		Cap.h = 14.0f;
		Cap.x = Puck.x + Puck.w * 0.5f - 60.0f;
		Cap.y = Puck.y + Puck.h + 2.0f;
		Label(&Cap, On ? "POINT" : "COME-OUT", 9.0f, TEXTALIGN_MC, ColorRGBA(0.560f, 0.900f, 0.700f, 0.65f));
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, PickCard, RollCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(112.0f, &PickCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(56.0f, &RollCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "BET");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_CrapsBet, 10, std::max(10, m_Credits), m_CrapsPoint == 0 && m_CrapsState == 0);
	}

	RailCard(&PickCard, "LINE");
	{
		CUIRect Inner = PickCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect PassBtn, DontBtn;
		Inner.HSplitTop(34.0f, &PassBtn, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner);
		Inner.HSplitTop(34.0f, &DontBtn, &Inner);

		if(Button(&PassBtn, "PASS LINE", m_CrapsChoice == 0, m_CrapsPoint == 0, 12.0f))
			m_CrapsChoice = 0;
		if(Button(&DontBtn, "DON'T PASS", m_CrapsChoice == 1, m_CrapsPoint == 0, 12.0f))
			m_CrapsChoice = 1;

		CUIRect Note;
		Note.x = Inner.x;
		Note.w = Inner.w;
		Note.h = 14.0f;
		Note.y = PickCard.y + PickCard.h - 22.0f;
		Label(&Note, m_CrapsPoint == 0 ? "LINE LOCKED ON POINT" : "LINE OPEN", 8.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
	}

	RailCard(&RollCard, "");
	{
		CUIRect Inner = RollCard;
		Inner.Margin(10.0f, &Inner);
		if(ActionButton(&Inner, m_CrapsPoint == 0 ? "ROLL" : "ROLL FOR POINT", ColorRGBA(0.360f, 0.800f, 0.560f, 1.0f), m_CrapsState == 0, 15.0f))
			CrapsRoll();
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect Box;
		Inner.HSplitTop(34.0f, &Box, &Inner);
		if(m_aCrapsMsg[0] != '\0')
			ResultBanner(&Box, m_aCrapsMsg, m_CrapsLastWin > 0);
		else
		{
			Frame(&Box, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
			Label(&Box, "AWAITING ROLL", 10.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
		}
	}
}

void CCasino::RenderSicBo(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "SIC BO", "THREE DICE  /  SMALL 4-10  /  BIG 11-17  /  TRIPLES LOSE EVEN MONEY", "SB", ColorRGBA(0.520f, 0.760f, 0.980f, 1.0f));

	CUIRect Table, Rail;
	Body.VSplitRight(262.0f, &Table, &Rail);
	Table.VSplitRight(14.0f, &Table, nullptr);

	Felt(&Table, 12.0f);

	CUIRect DiceArea, Cloth;
	Table.HSplitTop(Table.h * 0.44f, &DiceArea, &Cloth);

	const float DieSize = std::min(88.0f, DiceArea.h * 0.62f);
	for(int i = 0; i < 3; i++)
	{
		CUIRect Die;
		Die.w = DieSize;
		Die.h = DieSize;
		Die.x = DiceArea.x + DiceArea.w * 0.5f - DieSize * 1.5f - 20.0f + i * (DieSize + 20.0f);
		Die.y = DiceArea.y + (DiceArea.h - DieSize) * 0.5f - 8.0f;
		DrawDie(&Die, m_aSicBoDice[i], m_SicBoState == 1);
	}

	char aBuf[128];
	{
		CUIRect SumTag;
		SumTag.w = 148.0f;
		SumTag.h = 28.0f;
		SumTag.x = DiceArea.x + DiceArea.w * 0.5f - SumTag.w * 0.5f;
		SumTag.y = DiceArea.y + DiceArea.h - 34.0f;
		str_format(aBuf, sizeof(aBuf), "TOTAL  %d", m_aSicBoDice[0] + m_aSicBoDice[1] + m_aSicBoDice[2]);
		Pill(&SumTag, aBuf, ColorRGBA(0.070f, 0.120f, 0.090f, 1.0f), ColorRGBA(0.780f, 0.960f, 0.850f, 1.0f), 12.0f);
	}

	CUIRect ClothInner = Cloth;
	ClothInner.Margin(10.0f, &ClothInner);

	CUIRect ClothTitle, MainRow, ExtraRow;
	ClothInner.HSplitTop(18.0f, &ClothTitle, &ClothInner);
	Label(&ClothTitle, "BETTING CLOTH", 9.0f, TEXTALIGN_ML, ColorRGBA(0.560f, 0.900f, 0.700f, 0.60f));

	const char *apNames[5] = {"SMALL  1:1", "BIG  1:1", "ANY TRIPLE  33:1", "TRIPLE  200:1", "SUM"};
	const char *apOddsNote[5] = {"4 - 10", "11 - 17", "any 3 of a kind", "exact triple", "3 - 18"};
	ClothInner.HSplitTop(ClothInner.h * 0.52f, &MainRow, &ExtraRow);
	ExtraRow.HSplitTop(8.0f, nullptr, &ExtraRow);

	const float MainW = (MainRow.w - 3.0f * 8.0f) / 4.0f;
	for(int i = 0; i < 4; i++)
	{
		CUIRect Btn;
		Btn.x = MainRow.x + i * (MainW + 8.0f);
		Btn.y = MainRow.y;
		Btn.w = MainW;
		Btn.h = MainRow.h;

		const bool Sel = m_SicBoChoice == i;
		ColorRGBA Fill = Sel ? ColorRGBA(0.980f, 0.800f, 0.280f, 1.0f) : ColorRGBA(0.085f, 0.140f, 0.115f, 1.0f);
		const bool Hovered = Ui()->MouseHovered(&Btn);
		if(!Sel && Hovered)
			Fill = Shade(Fill, 1.60f);

		Frame(&Btn, Fill, Shade(Fill, Sel ? 1.30f : 1.55f), 6.0f);
		if(Sel)
			Btn.DrawOutline(ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f));

		CUIRect Name = Btn;
		Name.HSplitTop(Btn.h * 0.54f, &Name, nullptr);
		Label(&Name, apNames[i], 10.0f, TEXTALIGN_MC, Sel ? ColorRGBA(0.080f, 0.070f, 0.020f, 1.0f) : ColorRGBA(0.820f, 0.940f, 0.870f, 1.0f));

		CUIRect Note = Btn;
		Note.HSplitTop(Btn.h * 0.50f, nullptr, &Note);
		Label(&Note, apOddsNote[i], 8.0f, TEXTALIGN_MC, Sel ? ColorRGBA(0.180f, 0.150f, 0.040f, 1.0f) : ColorRGBA(0.480f, 0.620f, 0.540f, 1.0f));

		if(Ui()->MouseButtonClicked(0) && Hovered)
			m_SicBoChoice = i;
	}

	CUIRect SumBtn;
	SumBtn.x = ExtraRow.x;
	SumBtn.y = ExtraRow.y;
	SumBtn.w = 96.0f;
	SumBtn.h = std::min(30.0f, ExtraRow.h);
	if(Button(&SumBtn, "SUM  3-18", m_SicBoChoice == 4, true, 10.0f))
		m_SicBoChoice = 4;

	if(m_SicBoChoice == 3)
	{
		CUIRect Row = ExtraRow;
		Row.x = SumBtn.x + SumBtn.w + 10.0f;
		Row.w = ExtraRow.w - SumBtn.w - 10.0f;
		for(int i = 0; i < 6; i++)
		{
			CUIRect Btn;
			Btn.w = (Row.w - 5.0f * 5.0f) / 6.0f;
			Btn.h = std::min(30.0f, Row.h);
			Btn.x = Row.x + i * (Btn.w + 5.0f);
			Btn.y = Row.y;
			str_format(aBuf, sizeof(aBuf), "%d", i + 1);
			if(Button(&Btn, aBuf, m_SicBoTriple == i + 1, true, 11.0f))
				m_SicBoTriple = i + 1;
		}
	}
	else if(m_SicBoChoice == 4)
	{
		CUIRect Row = ExtraRow;
		Row.x = SumBtn.x + SumBtn.w + 10.0f;
		Row.w = ExtraRow.w - SumBtn.w - 10.0f;
		for(int i = 0; i < 16; i++)
		{
			CUIRect Btn;
			Btn.w = (Row.w - 15.0f * 2.0f) / 16.0f;
			Btn.h = std::min(30.0f, Row.h);
			Btn.x = Row.x + i * (Btn.w + 2.0f);
			Btn.y = Row.y;
			str_format(aBuf, sizeof(aBuf), "%d", i + 3);
			if(Button(&Btn, aBuf, m_SicBoSum == i + 3, true, 9.0f))
				m_SicBoSum = i + 3;
		}
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, RollCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(56.0f, &RollCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "BET");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_SicBoBet, 10, std::max(10, m_Credits), m_SicBoState == 0);
	}

	RailCard(&RollCard, "");
	{
		CUIRect Inner = RollCard;
		Inner.Margin(10.0f, &Inner);
		if(ActionButton(&Inner, "ROLL", CASINO_GOLD, m_SicBoState == 0, 17.0f))
			SicBoRoll();
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect Box;
		Inner.HSplitTop(34.0f, &Box, &Inner);
		if(m_aSicBoMsg[0] != '\0')
			ResultBanner(&Box, m_aSicBoMsg, m_SicBoLastWin > 0);
		else
		{
			Frame(&Box, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
			Label(&Box, "PLACE A BET", 10.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
		}
	}
}

void CCasino::RenderLottery(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "LOTTERY  5 OF 30", "PICK FIVE  /  FIVE MATCHES PAYS 25000x", "LT", ColorRGBA(0.880f, 0.560f, 0.960f, 1.0f));

	CUIRect StageArea, Rail;
	Body.VSplitRight(262.0f, &StageArea, &Rail);
	StageArea.VSplitRight(14.0f, &StageArea, nullptr);

	Frame(&StageArea, ColorRGBA(0.070f, 0.074f, 0.098f, 1.0f), ColorRGBA(0.170f, 0.180f, 0.230f, 1.0f), 12.0f);

	CUIRect Inner = StageArea;
	Inner.Margin(14.0f, &Inner);

	CUIRect PickTitle, DrawTitle, DrawRow;
	Inner.HSplitTop(16.0f, &PickTitle, &Inner);
	Label(&PickTitle, "YOUR NUMBERS", 9.0f, TEXTALIGN_ML, ColorRGBA(0.520f, 0.560f, 0.650f, 1.0f));
	Inner.HSplitTop(6.0f, nullptr, &Inner);

	const float CellW = (Inner.w - 9.0f * 6.0f) / 10.0f;
	const float CellH = std::min(44.0f, (Inner.h * 0.52f - 12.0f) / 3.0f);

	char aBuf[96];
	for(int i = 0; i < 30; i++)
	{
		CUIRect Cell;
		Cell.w = CellW;
		Cell.h = CellH;
		Cell.x = Inner.x + (i % 10) * (CellW + 6.0f);
		Cell.y = Inner.y + (i / 10) * (CellH + 6.0f);

		bool Picked = false;
		for(int k = 0; k < 5; k++)
			if(m_aLotteryPick[k] == i + 1)
				Picked = true;

		const bool Hovered = Ui()->MouseHovered(&Cell);
		ColorRGBA Fill = Picked ? ColorRGBA(0.980f, 0.800f, 0.280f, 1.0f) : ColorRGBA(0.108f, 0.118f, 0.152f, 1.0f);
		if(!Picked && Hovered)
			Fill = Shade(Fill, 1.55f);

		Frame(&Cell, Fill, Shade(Fill, Picked ? 1.28f : 1.60f), 6.0f);
		str_format(aBuf, sizeof(aBuf), "%d", i + 1);
		Label(&Cell, aBuf, std::min(CellH * 0.44f, 16.0f), TEXTALIGN_MC, Picked ? ColorRGBA(0.080f, 0.070f, 0.020f, 1.0f) : ColorRGBA(0.760f, 0.800f, 0.880f, 1.0f));

		if(Ui()->MouseButtonClicked(0) && Hovered)
		{
			if(Picked)
			{
				for(int k = 0; k < 5; k++)
					if(m_aLotteryPick[k] == i + 1)
						m_aLotteryPick[k] = 0;
			}
			else
			{
				for(int k = 0; k < 5; k++)
				{
					if(m_aLotteryPick[k] == 0)
					{
						m_aLotteryPick[k] = i + 1;
						break;
					}
				}
			}
		}
	}

	CUIRect Rest = Inner;
	Rest.y = Inner.y + 3.0f * (CellH + 6.0f) + 10.0f;
	Rest.h = Inner.y + Inner.h - Rest.y;

	Rest.HSplitTop(16.0f, &DrawTitle, &Rest);
	Label(&DrawTitle, "DRAW", 9.0f, TEXTALIGN_ML, ColorRGBA(0.520f, 0.560f, 0.650f, 1.0f));
	Rest.HSplitTop(6.0f, nullptr, &Rest);

	const float BallSize = std::min(52.0f, Rest.h - 4.0f);
	Rest.HSplitTop(BallSize, &DrawRow, nullptr);
	const float BallStep = std::min((DrawRow.w - BallSize) / 4.0f, BallSize + 12.0f);
	for(int i = 0; i < 5; i++)
	{
		CUIRect Ball;
		Ball.w = BallSize;
		Ball.h = BallSize;
		Ball.x = DrawRow.x + i * BallStep;
		Ball.y = DrawRow.y;

		const bool Revealed = m_LotteryState == 0 && m_LotteryMatches >= 0 && m_aLotteryDraw[i] > 0;
		bool Hit = false;
		for(int k = 0; k < 5; k++)
			if(m_aLotteryPick[k] == m_aLotteryDraw[i])
				Hit = true;

		Shadow(&Ball, Revealed ? 0.45f : 0.20f);
		const vec2 BallCenter(Ball.x + Ball.w * 0.5f, Ball.y + Ball.h * 0.5f);

		if(m_LotteryState == 1)
		{
			// drawing: the ball boils until the timer runs out
			const float Phase = std::fmod(m_LotteryTimer * 3.0f + i * 0.37f, 1.0f);
			const float Jiggle = std::sin(m_LotteryTimer * 40.0f + i * 2.1f) * 1.5f;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(Lerp(ColorRGBA(0.200f, 0.130f, 0.300f, 1.0f), ColorRGBA(0.880f, 0.560f, 0.960f, 1.0f), Phase));
			Graphics()->DrawCircle(BallCenter.x + Jiggle, BallCenter.y, Ball.w * 0.5f, 32);
			Graphics()->QuadsEnd();
			Ring(vec2(BallCenter.x + Jiggle, BallCenter.y), Ball.w * 0.5f - 1.5f, 1.5f, ColorRGBA(0.880f, 0.560f, 0.960f, 0.8f), 32);
			continue;
		}

		if(!Revealed)
		{
			// idle: dim outline, no number
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(ColorRGBA(0.086f, 0.092f, 0.118f, 1.0f));
			Graphics()->DrawCircle(BallCenter.x, BallCenter.y, Ball.w * 0.5f, 32);
			Graphics()->QuadsEnd();
			Ring(BallCenter, Ball.w * 0.5f - 1.5f, 1.5f, ColorRGBA(0.200f, 0.210f, 0.260f, 1.0f), 32);
			Label(&Ball, "?", BallSize * 0.40f, TEXTALIGN_MC, ColorRGBA(0.360f, 0.390f, 0.460f, 1.0f));
			continue;
		}

		if(Hit)
		{
			// matched number: gold ball with a white glossy core and a halo
			Ring(BallCenter, Ball.w * 0.5f + 3.0f, 2.0f, ColorRGBA(CASINO_GOLD.r, CASINO_GOLD.g, CASINO_GOLD.b, 0.45f), 32);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(Shade(CASINO_GOLD, 0.62f));
			Graphics()->DrawCircle(BallCenter.x, BallCenter.y, Ball.w * 0.5f, 32);
			Graphics()->SetColor(CASINO_GOLD);
			Graphics()->DrawCircle(BallCenter.x, BallCenter.y, Ball.w * 0.5f * 0.82f, 32);
			Graphics()->SetColor(Shade(CASINO_GOLD, 1.30f));
			Graphics()->DrawCircle(BallCenter.x - Ball.w * 0.10f, BallCenter.y - Ball.h * 0.11f, Ball.w * 0.5f * 0.38f, 24);
			Graphics()->QuadsEnd();
			str_format(aBuf, sizeof(aBuf), "%d", m_aLotteryDraw[i]);
			Label(&Ball, aBuf, BallSize * 0.40f, TEXTALIGN_MC, ColorRGBA(0.080f, 0.070f, 0.020f, 1.0f));
		}
		else
		{
			// missed draw: quiet dark ball
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(ColorRGBA(0.130f, 0.140f, 0.180f, 1.0f));
			Graphics()->DrawCircle(BallCenter.x, BallCenter.y, Ball.w * 0.5f, 32);
			Graphics()->SetColor(ColorRGBA(0.170f, 0.185f, 0.235f, 1.0f));
			Graphics()->DrawCircle(BallCenter.x, BallCenter.y, Ball.w * 0.5f * 0.82f, 32);
			Graphics()->QuadsEnd();
			Ring(BallCenter, Ball.w * 0.5f - 1.5f, 1.5f, ColorRGBA(0.250f, 0.270f, 0.330f, 1.0f), 32);
			str_format(aBuf, sizeof(aBuf), "%d", m_aLotteryDraw[i]);
			Label(&Ball, aBuf, BallSize * 0.40f, TEXTALIGN_MC, ColorRGBA(0.560f, 0.600f, 0.680f, 1.0f));
		}
	}

	int Picked = 0;
	for(int k = 0; k < 5; k++)
		if(m_aLotteryPick[k] > 0)
			Picked++;

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, PickCard, DrawCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(96.0f, &PickCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(56.0f, &DrawCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "TICKET PRICE");
	{
		CUIRect Inner2 = BetCard;
		Inner2.Margin(10.0f, &Inner2);
		Inner2.HSplitTop(16.0f, nullptr, &Inner2);
		Inner2.HSplitTop(8.0f, nullptr, &Inner2); // align the stepper with the slots layout
		Inner2.HSplitTop(34.0f, &Inner2, nullptr);
		BetRow(&Inner2, &m_LotteryBet, 10, std::max(10, m_Credits), m_LotteryState == 0);
	}

	RailCard(&PickCard, "SELECTION");
	{
		CUIRect Inner2 = PickCard;
		Inner2.Margin(10.0f, &Inner2);
		Inner2.HSplitTop(20.0f, nullptr, &Inner2); // skip the card title: first arg would become the 20px band

		str_format(aBuf, sizeof(aBuf), "%d / 5", Picked);
		StatRow(&Inner2, "PICKED", aBuf, Picked == 5 ? ColorRGBA(0.700f, 0.960f, 0.800f, 1.0f) : CASINO_GOLD);

		const int Match = std::min(Picked, 5);
		str_format(aBuf, sizeof(aBuf), "%dx", s_aLotteryPay[Match]);
		StatRow(&Inner2, "MAX PAYOUT", aBuf, ColorRGBA(0.760f, 0.800f, 0.880f, 1.0f));
	}

	RailCard(&DrawCard, "");
	{
		CUIRect Inner2 = DrawCard;
		Inner2.Margin(10.0f, &Inner2);
		if(ActionButton(&Inner2, Picked == 5 ? "DRAW" : "PICK 5 NUMBERS", ColorRGBA(0.820f, 0.480f, 0.960f, 1.0f), Picked == 5 && m_Credits >= m_LotteryBet, 13.0f))
			LotteryDraw();
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner2 = ResultCard;
		Inner2.Margin(10.0f, &Inner2);
		Inner2.HSplitTop(18.0f, nullptr, &Inner2);

		CUIRect Box;
		Inner2.HSplitTop(24.0f, &Box, &Inner2);
		if(m_LotteryState == 1)
			ResultBanner(&Box, "DRAWING", false, true);
		else if(m_LotteryMatches >= 0)
		{
			str_format(aBuf, sizeof(aBuf), m_LotteryLastWin > 0 ? "%d MATCH%s  +%d" : "%d MATCH%s", m_LotteryMatches, m_LotteryMatches == 1 ? "" : "ES", m_LotteryLastWin);
			ResultBanner(&Box, aBuf, m_LotteryLastWin > 0);
		}
		else
			ResultBanner(&Box, "NO TICKET YET", false, true);
	}
}

void CCasino::RenderCrash(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "CRASH", "CASH OUT BEFORE THE CURVE COLLAPSES", "CR", ColorRGBA(0.320f, 0.860f, 0.580f, 1.0f));

	CUIRect Graph, Rail;
	Body.VSplitRight(262.0f, &Graph, &Rail);
	Graph.VSplitRight(14.0f, &Graph, nullptr);

	Frame(&Graph, ColorRGBA(0.060f, 0.070f, 0.092f, 1.0f), ColorRGBA(0.160f, 0.180f, 0.230f, 1.0f), 12.0f);

	CUIRect Plot = Graph;
	Plot.Margin(12.0f, &Plot);

	for(int i = 1; i < 6; i++)
	{
		CUIRect H, V;
		H.x = Plot.x;
		H.w = Plot.w;
		H.h = 1.0f;
		H.y = Plot.y + Plot.h * (float)i / 6.0f;
		H.Draw(ColorRGBA(0.130f, 0.150f, 0.190f, 1.0f), IGraphics::CORNER_NONE, 0.0f);

		V.y = Plot.y;
		V.h = Plot.h;
		V.w = 1.0f;
		V.x = Plot.x + Plot.w * (float)i / 6.0f;
		V.Draw(ColorRGBA(0.130f, 0.150f, 0.190f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
	}

	const float MaxMult = std::max(2.0f, m_CrashMultiplier * 1.18f);
	// three states: idle before the first bet, in flight, and finished (crashed or cashed)
	const bool CrashIdle = !m_CrashRunning && !m_CrashHasRound;
	const ColorRGBA CurveColor = m_CrashRunning ? ColorRGBA(0.320f, 0.900f, 0.600f, 1.0f) : (CrashIdle ? ColorRGBA(0.360f, 0.400f, 0.480f, 1.0f) : ColorRGBA(0.900f, 0.300f, 0.340f, 1.0f));

	const int Steps = 96;
	vec2 Prev(Plot.x, Plot.y + Plot.h);
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(int i = 1; i <= Steps; i++)
	{
		const float T = (float)i / (float)Steps;
		const float Mult = 1.0f + (m_CrashMultiplier - 1.0f) * (T * T * 0.35f + T * 0.65f);
		const float X = Plot.x + T * Plot.w;
		const float Y = Plot.y + Plot.h - ((Mult - 1.0f) / (MaxMult - 1.0f)) * Plot.h * 0.94f;

		Graphics()->SetColor(CurveColor.r, CurveColor.g, CurveColor.b, 0.14f);
		IGraphics::CFreeformItem Fill(Prev, vec2(X, Y), vec2(X, Plot.y + Plot.h), vec2(Prev.x, Plot.y + Plot.h));
		Graphics()->QuadsDrawFreeform(&Fill, 1);

		Graphics()->SetColor(CurveColor);
		IGraphics::CFreeformItem Line(Prev, vec2(X, Y), vec2(X, Y + 2.5f), vec2(Prev.x, Prev.y + 2.5f));
		Graphics()->QuadsDrawFreeform(&Line, 1);
		Prev = vec2(X, Y);
	}
	Graphics()->QuadsEnd();

	GlowDot(Prev, 7.0f, CurveColor);

	char aBuf[64];
	{
		CUIRect MultRect;
		MultRect.w = 340.0f;
		MultRect.h = 74.0f;
		MultRect.x = Plot.x + Plot.w * 0.5f - MultRect.w * 0.5f;
		MultRect.y = Plot.y + Plot.h * 0.34f;

		str_format(aBuf, sizeof(aBuf), "%.2fx", m_CrashMultiplier);
		Label(&MultRect, aBuf, 52.0f, TEXTALIGN_MC,
			m_CrashRunning ? ColorRGBA(0.980f, 1.000f, 0.990f, 1.0f) : (CrashIdle ? ColorRGBA(0.760f, 0.800f, 0.880f, 1.0f) : (m_CrashCashed ? ColorRGBA(0.700f, 0.980f, 0.800f, 1.0f) : ColorRGBA(1.000f, 0.520f, 0.560f, 1.0f))));

		CUIRect StateTag;
		StateTag.w = 200.0f;
		StateTag.h = 22.0f;
		StateTag.x = Plot.x + Plot.w * 0.5f - StateTag.w * 0.5f;
		StateTag.y = MultRect.y + MultRect.h + 2.0f;
		const char *pState = m_CrashRunning ? "LIVE" : (CrashIdle ? "READY" : (m_CrashCashed ? "CASHED OUT" : "CRASHED"));
		const ColorRGBA StateFill = m_CrashRunning ? ColorRGBA(0.070f, 0.180f, 0.115f, 1.0f) : (CrashIdle ? ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f) : (m_CrashCashed ? ColorRGBA(0.075f, 0.160f, 0.110f, 1.0f) : ColorRGBA(0.200f, 0.075f, 0.090f, 1.0f)));
		const ColorRGBA StateInk = m_CrashRunning ? ColorRGBA(0.700f, 0.980f, 0.800f, 1.0f) : (CrashIdle ? ColorRGBA(0.545f, 0.588f, 0.680f, 1.0f) : (m_CrashCashed ? ColorRGBA(0.700f, 0.980f, 0.800f, 1.0f) : ColorRGBA(1.000f, 0.660f, 0.680f, 1.0f)));
		Pill(&StateTag, pState, StateFill, StateInk, 10.0f);
	}

	{
		CUIRect HistRow;
		Graph.HSplitBottom(26.0f, nullptr, &HistRow);
		HistRow.HMargin(6.0f, &HistRow);
		for(int i = 0; i < 12; i++)
		{
			if(m_aCrashHistory[i] <= 0.0f)
				break;
			CUIRect Chip;
			Chip.w = 52.0f;
			Chip.h = 16.0f;
			Chip.x = HistRow.x + 8.0f + i * 56.0f;
			Chip.y = HistRow.y;

			const bool High = m_aCrashHistory[i] >= 2.0f;
			const ColorRGBA Fill = High ? ColorRGBA(0.070f, 0.160f, 0.110f, 1.0f) : ColorRGBA(0.170f, 0.075f, 0.085f, 1.0f);
			Frame(&Chip, Fill, Shade(Fill, 1.70f), 8.0f);
			str_format(aBuf, sizeof(aBuf), "%.2fx", m_aCrashHistory[i]);
			Label(&Chip, aBuf, 8.5f, TEXTALIGN_MC, High ? ColorRGBA(0.700f, 0.980f, 0.800f, 1.0f) : ColorRGBA(1.000f, 0.660f, 0.680f, 1.0f));
		}
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, ActionCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(66.0f, &ActionCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "STAKE");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_CrashBet, 10, std::max(10, m_Credits), !m_CrashRunning);
	}

	RailCard(&ActionCard, "");
	{
		CUIRect Inner = ActionCard;
		Inner.Margin(10.0f, &Inner);
		if(m_CrashRunning)
		{
			str_format(aBuf, sizeof(aBuf), "CASH OUT  %d", (int)((m_CrashRoundBet > 0 ? m_CrashRoundBet : m_CrashBet) * m_CrashMultiplier));
			if(ActionButton(&Inner, aBuf, ColorRGBA(0.320f, 0.880f, 0.580f, 1.0f), !m_CrashCashed, 16.0f))
				CrashCashOut();
		}
		else
		{
			if(ActionButton(&Inner, "PLACE BET", CASINO_GOLD, m_Credits >= m_CrashBet, 17.0f))
				CrashStart();
		}
	}

	RailCard(&ResultCard, "RESULT");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(18.0f, nullptr, &Inner);

		CUIRect Box;
		Inner.HSplitTop(34.0f, &Box, &Inner);
		if(m_CrashLastWin > 0)
		{
			str_format(aBuf, sizeof(aBuf), "CASHED OUT  +%d", m_CrashLastWin);
			ResultBanner(&Box, aBuf, true);
		}
		else if(m_CrashRunning)
		{
			Frame(&Box, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
			Label(&Box, "IN FLIGHT", 10.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
		}
		else if(CrashIdle)
			ResultBanner(&Box, "PLACE A BET", false, true);
		else
			ResultBanner(&Box, "BUSTED", false);
	}
}

void CCasino::RenderMines(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "MINES", "REVEAL SAFE CELLS  /  MULTIPLIER RISES WITH EVERY STEP", "MN", ColorRGBA(0.960f, 0.640f, 0.300f, 1.0f));

	CUIRect GridArea, Rail;
	Body.VSplitRight(262.0f, &GridArea, &Rail);
	GridArea.VSplitRight(14.0f, &GridArea, nullptr);

	Frame(&GridArea, ColorRGBA(0.062f, 0.068f, 0.088f, 1.0f), ColorRGBA(0.160f, 0.176f, 0.220f, 1.0f), 12.0f);

	CUIRect GridSpace = GridArea;
	GridSpace.Margin(16.0f, &GridSpace);

	const float CellSize = std::min(GridSpace.w, GridSpace.h) / 5.0f;
	CUIRect Grid;
	Grid.w = CellSize * 5.0f;
	Grid.h = CellSize * 5.0f;
	Grid.x = GridSpace.x + (GridSpace.w - Grid.w) * 0.5f;
	Grid.y = GridSpace.y + (GridSpace.h - Grid.h) * 0.5f;

	for(int i = 0; i < 25; i++)
	{
		CUIRect Cell;
		Cell.w = CellSize - 8.0f;
		Cell.h = CellSize - 8.0f;
		Cell.x = Grid.x + (i % 5) * CellSize + 4.0f;
		Cell.y = Grid.y + (i / 5) * CellSize + 4.0f;

		const bool Hovered = Ui()->MouseHovered(&Cell);
		ColorRGBA Top = ColorRGBA(0.132f, 0.146f, 0.186f, 1.0f);
		ColorRGBA Bottom = ColorRGBA(0.086f, 0.096f, 0.126f, 1.0f);
		ColorRGBA Border = ColorRGBA(0.190f, 0.208f, 0.258f, 1.0f);
		ColorRGBA Ink = ColorRGBA(0.400f, 0.440f, 0.520f, 1.0f);
		const char *pGlyph = "";

		if(m_aMinesRevealed[i])
		{
			if(m_aMinesField[i])
			{
				Top = ColorRGBA(0.420f, 0.110f, 0.130f, 1.0f);
				Bottom = ColorRGBA(0.230f, 0.055f, 0.070f, 1.0f);
				Border = ColorRGBA(0.880f, 0.300f, 0.340f, 1.0f);
				Ink = ColorRGBA(1.000f, 0.760f, 0.780f, 1.0f);
				pGlyph = "X";
			}
			else
			{
				Top = ColorRGBA(0.080f, 0.230f, 0.160f, 1.0f);
				Bottom = ColorRGBA(0.040f, 0.130f, 0.090f, 1.0f);
				Border = ColorRGBA(0.280f, 0.780f, 0.520f, 1.0f);
				Ink = ColorRGBA(0.720f, 1.000f, 0.840f, 1.0f);
				pGlyph = "+";
			}
		}
		else if(m_MinesBusted && m_aMinesField[i])
		{
			Top = ColorRGBA(0.250f, 0.085f, 0.100f, 1.0f);
			Bottom = ColorRGBA(0.140f, 0.045f, 0.055f, 1.0f);
			Border = ColorRGBA(0.520f, 0.200f, 0.220f, 1.0f);
			Ink = ColorRGBA(0.760f, 0.420f, 0.440f, 1.0f);
			pGlyph = "X";
		}
		else if(Hovered && m_MinesActive)
		{
			Top = ColorRGBA(0.200f, 0.220f, 0.276f, 1.0f);
			Bottom = ColorRGBA(0.140f, 0.154f, 0.196f, 1.0f);
			Border = ColorRGBA(0.420f, 0.460f, 0.540f, 1.0f);
		}

		Tile(&Cell, Top, Bottom, Border, 7.0f);

		if(pGlyph[0] != '\0')
			Label(&Cell, pGlyph, CellSize * 0.30f, TEXTALIGN_MC, Ink);
		else
		{
			GlowDot(vec2(Cell.x + Cell.w * 0.5f, Cell.y + Cell.h * 0.5f), 2.6f, ColorRGBA(0.260f, 0.290f, 0.350f, 1.0f));
		}

		if(m_MinesActive && !m_aMinesRevealed[i] && Ui()->MouseButtonClicked(0) && Hovered)
			MinesReveal(i);
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	CUIRect BetCard, MinesCard, StatCard, ActionCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(84.0f, &MinesCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(84.0f, &StatCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ActionCard, &Ctrl);

	RailCard(&BetCard, "STAKE");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_MinesBet, 10, std::max(10, m_Credits), !m_MinesActive);
	}

	RailCard(&MinesCard, "MINES");
	{
		CUIRect Inner = MinesCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(20.0f, nullptr, &Inner);
		Inner.HSplitTop(30.0f, &Inner, nullptr);

		for(int i = 0; i < 6; i++)
		{
			CUIRect Btn;
			Btn.w = (Inner.w - 5.0f * 5.0f) / 6.0f;
			Btn.h = Inner.h;
			Btn.x = Inner.x + i * (Btn.w + 5.0f);
			Btn.y = Inner.y;

			char aBuf[8];
			str_format(aBuf, sizeof(aBuf), "%d", i + 1);
			if(Button(&Btn, aBuf, m_MinesCount == i + 1, !m_MinesActive, 12.0f))
				m_MinesCount = i + 1;
		}
	}

	char aBuf[96];
	RailCard(&StatCard, "POTENTIAL");
	{
		CUIRect Inner = StatCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(12.0f, nullptr, &Inner);

		str_format(aBuf, sizeof(aBuf), "%.2fx", m_MinesMultiplier);
		StatRow(&Inner, "MULTIPLIER", aBuf, CASINO_GOLD);
		str_format(aBuf, sizeof(aBuf), "%d", (int)((m_MinesRoundBet > 0 ? m_MinesRoundBet : m_MinesBet) * m_MinesMultiplier));
		StatRow(&Inner, "PAYOUT", aBuf, ColorRGBA(0.700f, 0.960f, 0.800f, 1.0f));
		str_format(aBuf, sizeof(aBuf), "%d / 25", m_MinesRevealed);
		StatRow(&Inner, "REVEALED", aBuf, ColorRGBA(0.760f, 0.800f, 0.880f, 1.0f));
	}

	RailCard(&ActionCard, "");
	{
		CUIRect Inner = ActionCard;
		Inner.Margin(10.0f, &Inner);
		if(m_MinesActive)
		{
			if(ActionButton(&Inner, "CASH OUT", ColorRGBA(0.320f, 0.860f, 0.560f, 1.0f), m_MinesRevealed > 0, 16.0f))
				MinesCashOut();
		}
		else
		{
			if(ActionButton(&Inner, "START", CASINO_GOLD, m_Credits >= m_MinesBet, 17.0f))
				MinesStart();
		}
	}
}

void CCasino::RenderDice(CUIRect *pRect)
{
	CUIRect Area = *pRect;
	Area.Margin(18.0f, &Area);

	CUIRect Body = ScreenHeader(&Area, "DICE", "PICK A THRESHOLD  /  ROLL OVER OR UNDER  /  1% HOUSE EDGE", "DC", ColorRGBA(0.400f, 0.720f, 0.980f, 1.0f));

	CUIRect StageArea, Rail;
	Body.VSplitRight(262.0f, &StageArea, &Rail);
	StageArea.VSplitRight(14.0f, &StageArea, nullptr);

	Frame(&StageArea, ColorRGBA(0.060f, 0.068f, 0.090f, 1.0f), ColorRGBA(0.160f, 0.180f, 0.230f, 1.0f), 12.0f);

	const float Value = m_DiceRolling ? m_DiceDisplay : m_DiceRoll;

	char aBuf[96];
	CUIRect Readout;
	Readout.w = 360.0f;
	Readout.h = 96.0f;
	Readout.x = StageArea.x + StageArea.w * 0.5f - Readout.w * 0.5f;
	Readout.y = StageArea.y + StageArea.h * 0.16f;
	str_format(aBuf, sizeof(aBuf), "%.2f", Value);
	Label(&Readout, aBuf, 62.0f, TEXTALIGN_MC, m_DiceRolling ? ColorRGBA(0.560f, 0.600f, 0.680f, 1.0f) : ColorRGBA(0.980f, 0.990f, 1.000f, 1.0f));

	{
		CUIRect BarArea = StageArea;
		BarArea.HSplitTop(StageArea.h * 0.56f, nullptr, &BarArea);
		BarArea.Margin(24.0f, &BarArea);
		BarArea.HSplitTop(34.0f, &BarArea, nullptr);

		CUIRect Track;
		Track.x = BarArea.x;
		Track.y = BarArea.y;
		Track.w = BarArea.w;
		Track.h = 22.0f;

		Track.Draw(ColorRGBA(0.020f, 0.024f, 0.034f, 1.0f), IGraphics::CORNER_NONE, 0.0f);
		Track.DrawOutline(ColorRGBA(0.180f, 0.200f, 0.250f, 1.0f));

		const float ThreshX = Track.x + ((float)m_DiceThreshold / 100.0f) * Track.w;

		CUIRect WinZone, LoseZone;
		if(m_DiceOver)
		{
			WinZone.x = ThreshX;
			WinZone.w = Track.x + Track.w - ThreshX;
			LoseZone.x = Track.x;
			LoseZone.w = ThreshX - Track.x;
		}
		else
		{
			WinZone.x = Track.x;
			WinZone.w = ThreshX - Track.x;
			LoseZone.x = ThreshX;
			LoseZone.w = Track.x + Track.w - ThreshX;
		}
		WinZone.y = Track.y;
		WinZone.h = Track.h;
		LoseZone.y = Track.y;
		LoseZone.h = Track.h;

		if(WinZone.w > 1.0f)
		{
			Grad(&WinZone, ColorRGBA(0.120f, 0.420f, 0.280f, 1.0f), ColorRGBA(0.060f, 0.240f, 0.160f, 1.0f), 11.0f);
		}
		if(LoseZone.w > 1.0f)
		{
			Grad(&LoseZone, ColorRGBA(0.360f, 0.110f, 0.130f, 1.0f), ColorRGBA(0.220f, 0.060f, 0.075f, 1.0f), 11.0f);
		}

		CUIRect Marker;
		Marker.w = 3.0f;
		Marker.h = Track.h + 14.0f;
		Marker.x = ThreshX - 1.5f;
		Marker.y = Track.y - 7.0f;
		Marker.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f), IGraphics::CORNER_ALL, 1.5f);

		GlowDot(vec2(Track.x + (Value / 100.0f) * Track.w, Track.y + Track.h * 0.5f), 8.0f,
			m_DiceRolling ? ColorRGBA(0.560f, 0.600f, 0.680f, 1.0f) : ColorRGBA(1.0f, 0.900f, 0.560f, 1.0f));

		CUIRect Caption;
		Caption.x = Track.x;
		Caption.y = Track.y + Track.h + 8.0f;
		Caption.w = Track.w;
		Caption.h = 16.0f;
		str_format(aBuf, sizeof(aBuf), "ROLL %s %d", m_DiceOver ? "OVER" : "UNDER", m_DiceThreshold);
		Label(&Caption, aBuf, 10.0f, TEXTALIGN_MC, ColorRGBA(0.520f, 0.560f, 0.650f, 1.0f));
	}

	const float Probability = DiceWinProbability(m_DiceThreshold, m_DiceOver);
	const float Multiplier = 0.97f / std::max(Probability, 0.01f);
	{
		CUIRect Odds;
		Odds.w = 300.0f;
		Odds.h = 26.0f;
		Odds.x = StageArea.x + StageArea.w * 0.5f - Odds.w * 0.5f;
		Odds.y = StageArea.y + StageArea.h - 44.0f;
		str_format(aBuf, sizeof(aBuf), "%.1f%% WIN   -   PAYS %.2fx", Probability * 100.0f, Multiplier);
		Pill(&Odds, aBuf, ColorRGBA(0.086f, 0.100f, 0.136f, 1.0f), CASINO_ICE, 12.0f);
	}

	CUIRect Ctrl = Rail;
	Ctrl.Margin(2.0f, &Ctrl);

	// five cards share the rail, so the result card drops the title and the banner fills it
	CUIRect BetCard, DirCard, ThreshCard, RollCard, ResultCard;
	Ctrl.HSplitTop(74.0f, &BetCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(72.0f, &DirCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(70.0f, &ThreshCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(46.0f, &RollCard, &Ctrl);
	Ctrl.HSplitTop(8.0f, nullptr, &Ctrl);
	Ctrl.HSplitTop(Ctrl.h, &ResultCard, &Ctrl);

	RailCard(&BetCard, "STAKE");
	{
		CUIRect Inner = BetCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(16.0f, nullptr, &Inner);
		Inner.HSplitTop(8.0f, nullptr, &Inner); // align the stepper with the slots layout
		Inner.HSplitTop(34.0f, &Inner, nullptr);
		BetRow(&Inner, &m_DiceBet, 10, std::max(10, m_Credits), !m_DiceRolling);
	}

	RailCard(&DirCard, "DIRECTION");
	{
		CUIRect Inner = DirCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(20.0f, nullptr, &Inner);
		Inner.HSplitTop(32.0f, &Inner, nullptr);

		CUIRect OverBtn, UnderBtn;
		OverBtn = Inner;
		OverBtn.w = (Inner.w - 6.0f) * 0.5f;
		UnderBtn = OverBtn;
		UnderBtn.x = Inner.x + Inner.w - UnderBtn.w;

		if(Button(&OverBtn, "OVER", m_DiceOver, true, 12.0f))
			m_DiceOver = true;
		if(Button(&UnderBtn, "UNDER", !m_DiceOver, true, 12.0f))
			m_DiceOver = false;
	}

	RailCard(&ThreshCard, "THRESHOLD");
	{
		CUIRect Inner = ThreshCard;
		Inner.Margin(10.0f, &Inner);
		Inner.HSplitTop(20.0f, nullptr, &Inner);
		Inner.HSplitTop(30.0f, &Inner, nullptr);

		CUIRect Minus, Val, Plus;
		Inner.VSplitLeft(30.0f, &Minus, &Inner);
		Inner.VSplitRight(30.0f, &Inner, &Plus);
		Inner.VSplitLeft(4.0f, nullptr, &Val);
		Val.VSplitRight(4.0f, &Val, nullptr);

		if(Button(&Minus, "-", false, m_DiceThreshold > 2, 15.0f))
			m_DiceThreshold--;
		if(Button(&Plus, "+", false, m_DiceThreshold < 98, 15.0f))
			m_DiceThreshold++;

		Frame(&Val, ColorRGBA(0.060f, 0.068f, 0.088f, 1.0f), ColorRGBA(0.170f, 0.186f, 0.235f, 1.0f), 5.0f);
		str_format(aBuf, sizeof(aBuf), "%d", m_DiceThreshold);
		Label(&Val, aBuf, 15.0f, TEXTALIGN_MC, CASINO_GOLD);
	}

	RailCard(&RollCard, "");
	{
		CUIRect Inner = RollCard;
		Inner.Margin(10.0f, &Inner);
		if(ActionButton(&Inner, m_DiceRolling ? "ROLLING" : "ROLL", ColorRGBA(0.360f, 0.700f, 0.980f, 1.0f), !m_DiceRolling && m_Credits >= m_DiceBet, 17.0f))
			DiceRoll();
	}

	RailCard(&ResultCard, "");
	{
		CUIRect Inner = ResultCard;
		Inner.Margin(10.0f, &Inner);

		CUIRect Box;
		Inner.HSplitTop(std::clamp(Inner.h, 22.0f, 34.0f), &Box, &Inner);
		if(m_aDiceMsg[0] != '\0')
			ResultBanner(&Box, m_aDiceMsg, m_DiceLastWin > 0);
		else
		{
			Frame(&Box, ColorRGBA(0.082f, 0.092f, 0.122f, 1.0f), ColorRGBA(0.150f, 0.166f, 0.212f, 1.0f), 6.0f);
			Label(&Box, "SET YOUR THRESHOLD", 10.0f, TEXTALIGN_MC, ColorRGBA(0.420f, 0.455f, 0.540f, 1.0f));
		}
	}
}
