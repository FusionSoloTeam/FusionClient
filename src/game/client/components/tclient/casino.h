#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_CASINO_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_CASINO_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui_rect.h>

#include <array>
#include <vector>

enum ECasinoGame
{
	CASINO_GAME_SLOT = 0,
	CASINO_GAME_WHEEL,
	CASINO_GAME_BLACKJACK,
	CASINO_GAME_CARIBBEAN,
	CASINO_GAME_HILO,
	CASINO_GAME_DRAGON_TIGER,
	CASINO_GAME_CRAPS,
	CASINO_GAME_SICBO,
	CASINO_GAME_LOTTERY,
	CASINO_GAME_CRASH,
	CASINO_GAME_MINES,
	CASINO_GAME_DICE,
	CASINO_GAME_COUNT,
};

struct SCasinoCard
{
	int m_Rank = 0;
	int m_Suit = 0;
};

struct SCasinoBlackjackHand
{
	std::vector<SCasinoCard> m_vCards;
	int m_Stake = 0;
	bool m_Doubled = false;
	bool m_Surrendered = false;
	bool m_Done = false;
	bool m_FromSplit = false;
};

struct SCasinoBlackjack
{
	std::vector<SCasinoCard> m_vDealer;
	std::vector<SCasinoBlackjackHand> m_vHands;
	int m_ActiveHand = 0;
	int m_Stake = 10;
	int m_State = 0;
	float m_Timer = 0.0f;
	int m_LastWin = 0;
	char m_aMessage[128] = "";
	bool m_HoleHidden = true;
};

struct SCasinoCaribbean
{
	std::vector<SCasinoCard> m_vPlayer;
	std::vector<SCasinoCard> m_vDealer;
	int m_Stake = 10;
	int m_RoundStake = 0; // ante actually debited, payouts never read the live field
	int m_State = 0;
	int m_LastWin = 0;
	char m_aMessage[128] = "";
};

struct SCasinoHiLo
{
	SCasinoCard m_Current;
	SCasinoCard m_Prev; // card shown before the last reveal, drives the flip animation
	SCasinoCard m_aHistory[8];
	signed char m_aHistDir[8] = {}; // +1 rose, -1 fell, 0 tie
	int m_HistCount = 0;
	float m_Multiplier = 1.0f;
	float m_PrevMult = 1.0f;
	float m_LastMult = 0.0f; // multiplier at the moment the round ended
	float m_MultAnim = 0.0f;
	float m_SettleAnim = 0.0f;
	int m_Stake = 10;
	int m_RoundStake = 0; // stake debited at start, payouts never read the live field
	int m_Streak = 0;
	int m_State = 0;
	int m_LastWin = 0;
	char m_aMessage[128] = "";
	float m_RevealTime = 0.0f;
	SCasinoCard m_HistPendingCard; // revealed card, joins the strip when the flip lands
	int m_HistPendingDir = 0;
	bool m_HistPending = false;
};

class CCasino : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;
	void OnConsoleInit() override;
	void OnReset() override;
	void Render(CUIRect View);
	// drops focus from the bet field, called when the casino page is left
	void ReleaseTextInput();

private:
	static void ConOpenCasino(IConsole::IResult *pResult, void *pUser);

	int m_Credits = 0;
	int m_SelectedGame = 0;
	float m_GameListScroll = 0.0f;
	int m_AnimTime = 0;
	float m_TimeAccum = 0.0f;
	float m_SaveTimer = 0.0f;
	bool m_SavePending = false;
	bool m_DailyReady = false;
	bool m_DailyFrozen = false; // last-daily config was edited by hand, claims stay locked
	int m_DailyFrozenStamp = -1; // timestamp the frozen flag was computed for, avoids a hash per frame
	char m_aToast[128] = "";
	float m_ToastTime = 0.0f;

	unsigned int m_RngState = 0; // own stream, seeded from the OS on first use

	int m_TotalWagered = 0;
	int m_TotalWon = 0;
	int m_BiggestWin = 0;

	std::vector<SCasinoCard> m_vShoe;
	int m_ShoePos = 0;

	SCasinoBlackjack m_Blackjack;
	SCasinoCaribbean m_Caribbean;
	SCasinoHiLo m_HiLo;

	int m_aSlotReels[3][3] = {};
	int m_SlotBet = 10;
	int m_SlotRoundBet = 0; // per-line bet debited at spin time
	int m_SlotLines = 1;
	CLineInputNumber m_BetInput;
	int m_SlotState = 0;
	// per-reel spin: remaining time, travelled cells and the stop flash timer
	float m_aSlotReelTime[3] = {};
	float m_aSlotReelDuration[3] = {};
	float m_aSlotReelTravel[3] = {};
	float m_aSlotReelStopAnim[3] = {};
	int m_aSlotPrevReels[3][3] = {}; // symbols visible when the spin started
	unsigned int m_SlotSpinSeed = 0;
	int m_SlotLastWin = 0;
	int m_aSlotLineWin[5] = {};

	int m_WheelBet = 10;
	int m_WheelState = 0;
	int m_WheelResult = -1;
	float m_WheelAngle = 0.0f;
	float m_WheelSpinStart = 0.0f;
	float m_WheelSpinTarget = 0.0f;
	float m_WheelSpinTimer = 0.0f;
	float m_WheelSpinDuration = 1.0f;
	int m_WheelLastWin = 0;
	int m_WheelPendingWin = 0; // paid when the pointer lands

	int m_DragonTigerBet = 10;
	int m_DragonTigerChoice = 0;
	SCasinoCard m_DragonCard;
	SCasinoCard m_TigerCard;
	SCasinoCard m_DragonTigerPrevDragon; // pre-deal faces, drive the flip animation
	SCasinoCard m_DragonTigerPrevTiger;
	bool m_DragonTigerPrevValid = false;
	float m_DragonTigerReveal = 0.0f; // 0.9s countdown, dragon flips first, tiger follows
	float m_DragonTigerSettle = 0.0f;
	int m_aDragonTigerHist[8] = {}; // 0 dragon, 1 tiger, 2 tie
	unsigned char m_aDragonTigerHistWin[8] = {}; // 1 when the player pick wins
	int m_DragonTigerHistCount = 0;
	int m_DragonTigerHistPending = -1; // outcome waiting for the reveal to finish
	bool m_DragonTigerHistPendingWin = false;
	int m_DragonTigerState = 0;
	int m_DragonTigerLastWin = 0;
	int m_DragonTigerPendingWin = 0; // paid when the reveal ends
	char m_aDragonTigerMsg[128] = "";

	int m_CrapsBet = 10;
	int m_CrapsRoundBet = 0; // come-out wager, still in play across point rolls
	int m_CrapsChoice = 0;
	int m_CrapsPoint = 0;
	int m_CrapsState = 0;
	int m_aCrapsDice[2] = {1, 1};
	int m_CrapsLastWin = 0;
	float m_CrapsTimer = 0.0f;
	char m_aCrapsMsg[128] = "";

	int m_SicBoBet = 10;
	int m_SicBoChoice = 0;
	int m_SicBoTriple = 1;
	int m_SicBoSum = 10;
	int m_aSicBoDice[3] = {1, 1, 1};
	int m_SicBoState = 0;
	int m_SicBoLastWin = 0;
	float m_SicBoTimer = 0.0f;
	char m_aSicBoMsg[128] = "";

	int m_LotteryBet = 10;
	std::array<int, 5> m_aLotteryPick = {1, 2, 3, 4, 5};
	std::array<int, 5> m_aLotteryDraw = {};
	int m_LotteryState = 0;
	int m_LotteryMatches = -1;
	int m_LotteryLastWin = 0;
	float m_LotteryTimer = 0.0f;

	int m_CrashBet = 10;
	float m_CrashMultiplier = 1.0f;
	float m_CrashPoint = 2.0f;
	bool m_CrashRunning = false;
	bool m_CrashCashed = false;
	bool m_CrashHasRound = false; // a round has been played, so the screen is not "crashed" while idle
	int m_CrashRoundBet = 0;
	int m_CrashLastWin = 0;
	float m_CrashElapsed = 0.0f;
	std::array<float, 12> m_aCrashHistory = {};

	int m_MinesBet = 10;
	int m_MinesRoundBet = 0;
	int m_MinesCount = 3;
	std::array<bool, 25> m_aMinesField = {};
	std::array<bool, 25> m_aMinesRevealed = {};
	int m_MinesRevealed = 0;
	bool m_MinesActive = false;
	bool m_MinesBusted = false;
	float m_MinesMultiplier = 1.0f;

	int m_DiceBet = 10;
	int m_DiceThreshold = 50;
	bool m_DiceOver = true;
	float m_DiceRoll = 0.0f;
	float m_DiceDisplay = 0.0f;
	bool m_DiceRolling = false;
	float m_DiceTimer = 0.0f;
	int m_DiceLastWin = 0;
	char m_aDiceMsg[128] = "";

	void SyncFromConfig();
	void SyncToConfig();
	void AddCredits(int Amount);
	void TakeCredits(int Amount);
	void Toast(const char *pMessage);
	void Tick(float Dt);
	void CheckDaily();
	static void DailyStampHash(int Timestamp, char *pOut, size_t OutSize);
	bool DailyStampUntouched() const;
	void RecordBet(int Wagered);
	void RecordWin(int Won, int Staked = 0);
	int DaysSinceDaily() const;
	unsigned int NextRandom();
	int RandInt(int Max);
	float RandFloat();

	bool Button(CUIRect *pRect, const char *pText, bool Active = false, bool Enabled = true, float FontSize = 14.0f);
	void Label(const CUIRect *pRect, const char *pText, float Size, int Align, ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	void Shadow(const CUIRect *pRect, float Alpha = 0.35f);
	void Grad(const CUIRect *pRect, ColorRGBA Top, ColorRGBA Bottom, float Rounding);
	void Frame(const CUIRect *pRect, ColorRGBA Fill, ColorRGBA Border, float Rounding);
	void Tile(const CUIRect *pRect, ColorRGBA Top, ColorRGBA Bottom, ColorRGBA Border, float Rounding);
	void Pill(const CUIRect *pRect, const char *pText, ColorRGBA Fill, ColorRGBA TextColor, float FontSize);
	void Coin(const vec2 &Pos, float Radius);
	CUIRect ScreenHeader(CUIRect *pRect, const char *pTitle, const char *pSub, const char *pGlyph, ColorRGBA Accent);
	void Stage(const CUIRect *pRect, ColorRGBA Top, ColorRGBA Bottom, float Rounding = 10.0f);
	bool ActionButton(CUIRect *pRect, const char *pText, ColorRGBA Fill, bool Enabled, float FontSize = 17.0f);
	void ResultBanner(CUIRect *pRect, const char *pText, bool Win, bool Neutral = false);
	void StatRow(CUIRect *pRect, const char *pLabel, const char *pValue, ColorRGBA ValueColor);
	void RailCard(CUIRect *pRect, const char *pTitle);
	void Felt(const CUIRect *pRect, float Rounding = 10.0f);
	void Ring(const vec2 &Center, float Radius, float Thickness, ColorRGBA Color, int Segments = 64);
	void GlowDot(const vec2 &Pos, float Radius, ColorRGBA Color);
	void SlotSymbol(int Symbol, const CUIRect &Rect);
	void BetRow(CUIRect *pRect, int *pBet, int MinBet, int MaxBet, bool Enabled = true);
	void DrawCard(const CUIRect *pRect, const SCasinoCard &Card, bool Hidden = false, bool Highlight = false);
	void DrawDie(const CUIRect *pRect, int Value, bool Highlight = false);

	void ResetShoe();
	SCasinoCard DrawCardFromShoe();
	static int CardValue(const SCasinoCard &Card);
	static int BlackjackTotal(const std::vector<SCasinoCard> &vCards);
	static int EvaluatePokerHand(const std::vector<SCasinoCard> &vCards);
	static const char *PokerHandName(int Category);

	void RenderHeader(CUIRect *pRect);
	void RenderGameList(CUIRect *pRect);
	void RenderSlot(CUIRect *pRect);
	void RenderWheel(CUIRect *pRect);
	void RenderBlackjack(CUIRect *pRect);
	void RenderCaribbean(CUIRect *pRect);
	void RenderHiLo(CUIRect *pRect);
	void RenderDragonTiger(CUIRect *pRect);
	void RenderCraps(CUIRect *pRect);
	void RenderSicBo(CUIRect *pRect);
	void RenderLottery(CUIRect *pRect);
	void RenderCrash(CUIRect *pRect);
	void RenderMines(CUIRect *pRect);
	void RenderDice(CUIRect *pRect);

	void SlotSpin();
	void SlotSettle();
	void WheelSpin();
	void BlackjackDeal();
	void BlackjackHit();
	void BlackjackStand();
	void BlackjackDouble();
	void BlackjackSplit();
	void BlackjackSurrender();
	void BlackjackDealerPlay();
	void BlackjackSettle();
	void CaribbeanDeal();
	void CaribbeanPlay();
	void CaribbeanFold();
	void HiLoStart();
	void HiLoGuess(bool Higher);
	void HiLoCashOut();
	void HiLoPushHistory(const SCasinoCard &Card, int Dir);
	void DragonTigerDeal();
	void CrapsRoll();
	void SicBoRoll();
	void LotteryDraw();
	void CrashStart();
	void CrashCashOut();
	void MinesStart();
	void MinesReveal(int Index);
	void MinesCashOut();
	void DiceRoll();
};

#endif
