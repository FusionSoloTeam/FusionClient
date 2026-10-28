// vibecoded :D

#include "lyrics.h"

#include <base/io.h>
#include <base/math.h>
#include <base/str.h>
#include <base/dbg.h>
#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>
#include <base/types.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/external/json-parser/json.h>
#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/shared/config.h>
#include <engine/http.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#ifdef CONF_FAMILY_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static int LyricsLayoutFontSize()
{
	return std::clamp((int)(g_Config.m_TcLyricsSize + 0.5f), 16, 120);
}
static constexpr int LYRICS_MAX_STORED_LINES = 600;
static constexpr int LYRICS_MAX_SPARKLES = 128;
static constexpr int LYRICS_MAX_ATTEMPTS = 3;

static const char *s_pTestLrc =
	"[00:00.0]gold glitter on the map\n"
	"[00:03.5]real 3d glyphs, not a hud label\n"
	"[00:07.0]silver rim and stepped extrusion\n"
	"[00:10.5]random tilt and slow drift\n"
	"[00:14.0]fade out when the line ends\n";

static float Hash01(unsigned Seed)
{
	Seed ^= Seed >> 16;
	Seed *= 0x7feb352d;
	Seed ^= Seed >> 15;
	Seed *= 0x846ca68b;
	Seed ^= Seed >> 16;
	return (float)(Seed & 0xFFFFFF) / (float)0xFFFFFF;
}

static void TrimInPlace(char *pStr)
{
	if(pStr == nullptr)
		return;
	char *pStart = pStr;
	while(*pStart == ' ' || *pStart == '\t' || *pStart == '\r' || *pStart == '\n')
		pStart++;
	const int Len = str_length(pStart);
	int End = Len;
	while(End > 0 && (pStart[End - 1] == ' ' || pStart[End - 1] == '\t' || pStart[End - 1] == '\r' || pStart[End - 1] == '\n'))
		End--;
	if(pStart != pStr)
		mem_move(pStr, pStart, End);
	pStr[End] = '\0';
}

static bool IsLrcMetaTag(const char *pTag)
{
	static const char *const s_apPrefixes[] = {"bpm:", "ar:", "ti:", "al:", "by:", "re:", "ve:", "length:"};
	for(const char *pPrefix : s_apPrefixes)
	{
		if(str_startswith_nocase(pTag, pPrefix))
			return true;
	}
	return false;
}

static bool NextLine(const char *&pCursor, char *pLine, int LineSize)
{
	const char *pEnd = pCursor;
	while(*pEnd != '\0' && *pEnd != '\n' && *pEnd != '\r')
		pEnd++;
	if(*pCursor == '\0')
		return false;
	int Len = (int)(pEnd - pCursor);
	if(Len > LineSize - 1)
		Len = LineSize - 1;
	mem_copy(pLine, pCursor, Len);
	pLine[Len] = '\0';
	pCursor = pEnd;
	while(*pCursor == '\n' || *pCursor == '\r')
		pCursor++;
	return true;
}

static void NormalizeMatchKey(char *pOut, int OutSize, const char *pIn)
{
	int Out = 0;
	for(const char *p = pIn; *p != '\0' && Out < OutSize - 1; p++)
	{
		const unsigned char C = (unsigned char)*p;
		if(C < 0x80)
		{
			if(C >= 'A' && C <= 'Z')
				pOut[Out++] = (char)(C - 'A' + 'a');
			else if((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9'))
				pOut[Out++] = (char)C;
		}
		else
		{
			pOut[Out++] = (char)C;
		}
	}
	pOut[Out] = '\0';
}

static bool TitlesMatch(const char *pSong, const char *pCandidate)
{
	char aSong[256];
	char aCandidate[256];
	NormalizeMatchKey(aSong, sizeof(aSong), pSong);
	NormalizeMatchKey(aCandidate, sizeof(aCandidate), pCandidate);
	if(aSong[0] == '\0' || aCandidate[0] == '\0')
		return false;
	if(str_comp(aSong, aCandidate) == 0)
		return true;
	const int SongLen = str_length(aSong);
	const int CandidateLen = str_length(aCandidate);
	if(SongLen >= 3 && str_comp_num(aCandidate, aSong, SongLen) == 0)
		return true;
	if(CandidateLen >= 4 && str_find(aSong, aCandidate) != nullptr && CandidateLen * 10 >= SongLen * 7)
		return true;
	if(SongLen >= 4 && str_find(aCandidate, aSong) != nullptr && SongLen * 10 >= CandidateLen * 7)
		return true;
	return false;
}

static bool ArtistsMatch(const char *pSong, const char *pCandidate)
{
	char aSong[256];
	char aCandidate[256];
	NormalizeMatchKey(aSong, sizeof(aSong), pSong);
	NormalizeMatchKey(aCandidate, sizeof(aCandidate), pCandidate);
	if(aSong[0] == '\0' || aCandidate[0] == '\0')
		return true;
	if(str_comp(aSong, aCandidate) == 0)
		return true;
	if(str_length(aSong) >= 3 && str_find(aCandidate, aSong) != nullptr)
		return true;
	if(str_length(aCandidate) >= 3 && str_find(aSong, aCandidate) != nullptr)
		return true;
	return false;
}

static const char *JsonString(const json_value &Value)
{
	if(Value.type == json_string && Value.u.string.ptr != nullptr)
		return Value.u.string.ptr;
	return "";
}

static float JsonNumber(const json_value &Value)
{
	if(Value.type == json_double)
		return (float)Value.u.dbl;
	if(Value.type == json_integer)
		return (float)Value.u.integer;
	return 0.0f;
}

static void UrlEncode(char *pOut, int OutSize, const char *pIn)
{
	static const char s_aHex[] = "0123456789ABCDEF";
	int Out = 0;
	for(const char *p = pIn; *p != '\0' && Out + 4 < OutSize; p++)
	{
		const unsigned char C = (unsigned char)*p;
		if((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.' || C == '~')
		{
			pOut[Out++] = (char)C;
		}
		else if(C == ' ')
		{
			pOut[Out++] = '+';
		}
		else
		{
			pOut[Out++] = '%';
			pOut[Out++] = s_aHex[C >> 4];
			pOut[Out++] = s_aHex[C & 0xF];
		}
	}
	pOut[Out] = '\0';
}

#ifdef CONF_FAMILY_WINDOWS
static const char *s_apPlayerProcesses[] = {
	"spotify.exe", "aimp.exe", "aimp64.exe", "foobar2000.exe", "vlc.exe", "musicbee.exe",
	"yandexmusic.exe", "deezer.exe", "tidal.exe", "itunes.exe", "wmplayer.exe",
	"mpc-hc.exe", "mpc-hc64.exe", "applemusic.exe", "amazon music.exe", "soundcloud.exe"};

static const char *s_apBrowserProcesses[] = {
	"chrome.exe", "msedge.exe", "firefox.exe", "opera.exe", "brave.exe", "yandex.exe", "vivaldi.exe"};

static const char *s_apTitleSuffixes[] = {
	" - YouTube Music", " - YouTube", " | Spotify", " - Spotify", " - SoundCloud",
	" - Yandex Music", " - Яндекс Музыка", " - VLC media player", " - AIMP", " - MusicBee",
	" - Google Chrome", " - Microsoft Edge", " - Mozilla Firefox", " - Opera", " - Brave"};

static void Utf16ToUtf8(const wchar_t *pIn, char *pOut, int OutSize)
{
	const int Written = WideCharToMultiByte(CP_UTF8, 0, pIn, -1, pOut, OutSize, nullptr, nullptr);
	if(Written <= 0)
		pOut[0] = '\0';
}

static bool StripTitleSuffix(char *pTitle)
{
	bool Stripped = false;
	for(const char *pSuffix : s_apTitleSuffixes)
	{
		const int SuffixLen = str_length(pSuffix);
		const int TitleLen = str_length(pTitle);
		if(TitleLen > SuffixLen && str_comp_nocase(&pTitle[TitleLen - SuffixLen], pSuffix) == 0)
		{
			pTitle[TitleLen - SuffixLen] = '\0';
			Stripped = true;
		}
	}
	return Stripped;
}

struct SFindSongContext
{
	char m_aTitle[256];
	int m_Priority;
};

static BOOL CALLBACK FindSongWindowProc(HWND hWnd, LPARAM lParam)
{
	SFindSongContext *pCtx = (SFindSongContext *)lParam;
	if(pCtx->m_Priority >= 2)
		return FALSE;
	if(!IsWindowVisible(hWnd))
		return TRUE;

	wchar_t aWideTitle[512];
	if(GetWindowTextW(hWnd, aWideTitle, (int)(sizeof(aWideTitle) / sizeof(aWideTitle[0]))) <= 0)
		return TRUE;

	char aTitle[512];
	Utf16ToUtf8(aWideTitle, aTitle, sizeof(aTitle));
	TrimInPlace(aTitle);
	if(aTitle[0] == '\0' || str_find(aTitle, " - ") == nullptr)
		return TRUE;

	DWORD Pid = 0;
	GetWindowThreadProcessId(hWnd, &Pid);
	if(Pid == 0 || Pid == GetCurrentProcessId())
		return TRUE;

	char aProcess[128] = "";
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Pid);
	if(hProcess != nullptr)
	{
		wchar_t aWidePath[512];
		DWORD Len = (DWORD)(sizeof(aWidePath) / sizeof(aWidePath[0]));
		if(QueryFullProcessImageNameW(hProcess, 0, aWidePath, &Len))
		{
			const wchar_t *pBase = aWidePath;
			for(const wchar_t *p = aWidePath; *p != L'\0'; p++)
				if(*p == L'\\' || *p == L'/')
					pBase = p + 1;
			Utf16ToUtf8(pBase, aProcess, sizeof(aProcess));
		}
		CloseHandle(hProcess);
	}
	if(aProcess[0] == '\0')
		return TRUE;
	for(char *p = aProcess; *p != '\0'; p++)
		*p = (char)tolower((unsigned char)*p);

	int Priority = 0;
	for(const char *pPlayer : s_apPlayerProcesses)
		if(str_comp(pPlayer, aProcess) == 0)
			Priority = 2;

	if(Priority == 0)
	{
		bool IsBrowser = false;
		for(const char *pBrowser : s_apBrowserProcesses)
			if(str_comp(pBrowser, aProcess) == 0)
				IsBrowser = true;
		if(!IsBrowser || !StripTitleSuffix(aTitle))
			return TRUE;
		Priority = 1;
	}
	else
	{
		bool Stripped = true;
		while(Stripped)
			Stripped = StripTitleSuffix(aTitle);
	}

	TrimInPlace(aTitle);
	if(aTitle[0] == '\0')
		return TRUE;

	str_copy(pCtx->m_aTitle, aTitle, sizeof(pCtx->m_aTitle));
	pCtx->m_Priority = Priority;
	return TRUE;
}
#endif

static bool FindPlayingSong(char *pOut, int OutSize)
{
	pOut[0] = '\0';
#ifdef CONF_FAMILY_WINDOWS
	SFindSongContext Ctx;
	Ctx.m_aTitle[0] = '\0';
	Ctx.m_Priority = 0;
	EnumWindows(FindSongWindowProc, (LPARAM)&Ctx);
	if(Ctx.m_aTitle[0] == '\0')
		return false;
	str_copy(pOut, Ctx.m_aTitle, OutSize);
	return true;
#else
	(void)OutSize;
	return false;
#endif
}

CLyrics::~CLyrics()
{
	if(m_pRequest)
		m_pRequest->Abort();
}

void CLyrics::SetStatus(const char *pFormat, ...)
{
	va_list Args;
	va_start(Args, pFormat);
	str_format_v(m_aStatus, sizeof(m_aStatus), pFormat, Args);
	va_end(Args);
}

void CLyrics::ClearState()
{
	if(m_pRequest)
	{
		m_pRequest->Abort();
		m_pRequest.reset();
	}
	m_RequestDone = false;
	m_vLines.clear();
	m_vDrawn.clear();
	m_vSparkles.clear();
	m_HasLyrics = false;
	m_LastSmtcPos = -1.0f;
	m_HasPosAnchor = false;
	m_LastPosTime = -1.0f;
	m_SmtcChanges = 0;
	m_LastSmtcChange = 0.0f;
	m_LastLineIndex = -2;
	m_RequestAttempts = 0;
	m_RequestRetryAt = 0.0f;
	m_SongDuration = 0.0f;
}

void CLyrics::ResetSong()
{
	ClearState();
	m_Position = g_Config.m_TcLyricsOffset / 1000.0f;
}

void CLyrics::ResetState()
{
	ClearState();
	m_Position = 0.0f;
	m_NextTrackScan = 0.0f;
	m_BeatSmooth = 0.0f;
	m_aSongKey[0] = '\0';
	m_aSongTitle[0] = '\0';
	m_aLoadedFile[0] = '\0';
}

float CLyrics::ScreenPxPerUnit() const
{
	float ViewW, ViewH;
	const float Zoom = maximum(0.001f, GameClient()->m_Camera.m_RenderZoom);
	Graphics()->CalcScreenParams(Graphics()->WorldAspect(), Zoom, &ViewW, &ViewH);
	return Graphics()->ScreenWidth() / maximum(1.0f, ViewW);
}

vec2 CLyrics::LineOrigin(const SDrawnLine &Line) const
{
	const vec2 Origin = GameClient()->m_Camera.m_Center + Line.m_ScreenOffset / ScreenPxPerUnit();
	if(!std::isfinite(Origin.x) || !std::isfinite(Origin.y))
		return GameClient()->m_Camera.m_Center;
	return Origin;
}

float CLyrics::LineScale(const SDrawnLine &Line) const
{
	return (g_Config.m_TcLyricsSize / (float)Line.m_LayoutFontSize) / ScreenPxPerUnit();
}

IGraphics::CTextureHandle CLyrics::EnsureGlyphAtlas(int Index)
{
	Index = std::clamp(Index, 0, 1);
	const unsigned Generation = TextRender()->GetFontTextureGeneration();
	int Dimension = 0;
	const uint8_t *pData = TextRender()->GetFontTextureData(Index, &Dimension);
	if(pData == nullptr || Dimension <= 0)
		return TextRender()->GetFontTexture(Index);

	if(m_aGlyphAtlas[Index].IsValid() && m_aGlyphAtlasGeneration[Index] == Generation && m_aGlyphAtlasDimension[Index] == Dimension)
		return m_aGlyphAtlas[Index];

	const size_t PixelCount = (size_t)Dimension * (size_t)Dimension;
	uint8_t *pRgba = (uint8_t *)malloc(PixelCount * 4);
	if(pRgba == nullptr)
		return TextRender()->GetFontTexture(Index);
	for(size_t i = 0; i < PixelCount; i++)
	{
		const uint8_t Coverage = pData[i];
		pRgba[i * 4 + 0] = 255;
		pRgba[i * 4 + 1] = 255;
		pRgba[i * 4 + 2] = 255;
		pRgba[i * 4 + 3] = Coverage;
	}

	CImageInfo Image;
	Image.m_Width = Dimension;
	Image.m_Height = Dimension;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.m_pData = pRgba;
	const IGraphics::CTextureHandle NewTexture = Graphics()->LoadTextureRaw(Image, 0, "lyrics-glyph-atlas");
	free(pRgba);

	if(NewTexture.IsValid())
	{
		if(m_aGlyphAtlas[Index].IsValid())
			Graphics()->UnloadTexture(&m_aGlyphAtlas[Index]);
		m_aGlyphAtlas[Index] = NewTexture;
		m_aGlyphAtlasDimension[Index] = Dimension;
		m_aGlyphAtlasGeneration[Index] = Generation;
		return m_aGlyphAtlas[Index];
	}
	return TextRender()->GetFontTexture(Index);
}

void CLyrics::UnloadGlyphAtlases()
{
	for(auto &Texture : m_aGlyphAtlas)
	{
		if(Texture.IsValid())
			Graphics()->UnloadTexture(&Texture);
	}
	for(int i = 0; i < 2; i++)
	{
		m_aGlyphAtlasDimension[i] = 0;
		m_aGlyphAtlasGeneration[i] = 0;
	}
}

void CLyrics::OnInit()
{
}

void CLyrics::OnReset()
{
	ResetState();
	UnloadGlyphAtlases();
	m_aSongKey[0] = '\0';
	m_aStatus[0] = '\0';
}

void CLyrics::OnUpdate()
{
	if(!g_Config.m_TcLyrics)
	{
		m_Smtc.Stop();
		if(!m_vLines.empty() || !m_vDrawn.empty() || m_pRequest)
			ResetState();
		return;
	}

	if(!m_Smtc.IsRunning())
		m_Smtc.Start();

	if(m_RequestRetryAt > 0.0f && !m_pRequest && Client()->LocalTime() >= m_RequestRetryAt)
	{
		m_RequestRetryAt = 0.0f;
		StartRequest();
	}

	if(g_Config.m_TcLyricsTest)
	{
		if(str_comp(m_aSongKey, "<test>") != 0)
		{
			ResetSong();
			str_copy(m_aSongKey, "<test>", sizeof(m_aSongKey));
			ApplyLrcDoc(s_pTestLrc);
			SetStatus("test pattern");
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lyrics", "test pattern loaded");
		}
		else
		{
			UpdateHttp();
		}
	}
	else
	{
		UpdateTrack();
		UpdateHttp();
	}

	const float FrameTime = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	UpdatePosition(FrameTime);

	if(m_HasLyrics && !m_vLines.empty())
	{
		const float SongTime = m_Position;
		int Index = -1;
		for(int i = 0; i < (int)m_vLines.size(); i++)
		{
			if(m_vLines[i].m_Time <= SongTime)
				Index = i;
			else
				break;
		}
		if(Index < m_LastLineIndex - 1)
		{
			m_vDrawn.clear();
			m_LastLineIndex = -1;
		}
		if(Index != m_LastLineIndex)
		{
			SpawnLine(Index);
			m_LastLineIndex = Index;
		}
	}

	const float Now = Client()->LocalTime();
	for(size_t i = 0; i < m_vDrawn.size();)
	{
		if(Now > m_vDrawn[i].m_EndTime + 0.05f)
			m_vDrawn.erase(m_vDrawn.begin() + i);
		else
			i++;
	}
}

float CLyrics::SongLength() const
{
	if(m_SongDuration > 5.0f)
		return m_SongDuration + 1.0f;
	if(m_HasLyrics && !m_vLines.empty())
		return m_vLines.back().m_Time + 6.0f;
	return 0.0f;
}

void CLyrics::UpdatePosition(float FrameTime)
{
	const float Now = Client()->LocalTime();
	float Delta = m_LastPosTime > 0.0f ? Now - m_LastPosTime : FrameTime;
	m_LastPosTime = Now;
	Delta = std::clamp(Delta, 0.0f, 0.25f);

	const bool LivePosition = m_HasPosAnchor && Now - m_LastSmtcTime < 5.0;
	const bool Paused = m_SmtcChanges >= 2 && Now - m_LastSmtcChange > 8.0f;
	if(!Paused)
		m_Position += Delta;

	if(LivePosition)
	{
		if(m_SongDuration > 5.0f && m_Position > m_SongDuration + 3.0f)
			m_Position = m_SongDuration + 3.0f;
	}
	else
	{
		const float Length = SongLength();
		if(Length > 0.0f && m_Position >= Length)
		{
			m_Position = g_Config.m_TcLyricsOffset / 1000.0f;
			m_LastLineIndex = -2;
			m_vDrawn.clear();
			m_HasPosAnchor = false;
			m_LastSmtcPos = -1.0f;
		}
	}

	if(m_Position < 0.0f)
		m_Position = 0.0f;
}

void CLyrics::SyncSmtcPosition(float SmtcPosition)
{
	if(SmtcPosition < 0.0f)
		return;

	const double Now = Client()->LocalTime();
	if(!m_HasPosAnchor)
	{
		m_Position = SmtcPosition;
		m_LastSmtcPos = SmtcPosition;
		m_LastSmtcTime = Now;
		m_HasPosAnchor = true;
		return;
	}
	if(fabsf(SmtcPosition - m_LastSmtcPos) <= 0.05f)
		return;

	const float Expected = m_LastSmtcPos + (float)(Now - m_LastSmtcTime);
	m_LastSmtcPos = SmtcPosition;
	m_LastSmtcTime = Now;
	m_SmtcChanges++;
	m_LastSmtcChange = (float)Now;
	if(fabsf(SmtcPosition - Expected) > 2.0f)
		m_Position = SmtcPosition;
}

void CLyrics::UpdateTrack()
{
	const float Now = Client()->LocalTime();

	if(g_Config.m_TcLyricsFile[0] != '\0')
	{
		if(str_comp(m_aLoadedFile, g_Config.m_TcLyricsFile) == 0)
			return;
		str_copy(m_aLoadedFile, g_Config.m_TcLyricsFile);
		ResetSong();
		str_copy(m_aSongKey, m_aLoadedFile);
		if(LoadLrcFile(m_aLoadedFile))
			SetStatus("Lyrics loaded from %s", m_aLoadedFile);
		else
			SetStatus("Could not read %s", m_aLoadedFile);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lyrics", m_aStatus);
		return;
	}

	if(m_aLoadedFile[0] != '\0')
	{
		m_aLoadedFile[0] = '\0';
		ResetSong();
		m_aSongKey[0] = '\0';
	}

	char aSong[256] = "";
	char aArtist[256] = "";
	char aAlbum[256] = "";
	float SmtcDuration = 0.0f;
	float SmtcPosition = -1.0f;
	bool FromSmtc = false;
	if(m_Smtc.HasTrack())
	{
		m_Smtc.Snapshot(aSong, sizeof(aSong), aArtist, sizeof(aArtist), aAlbum, sizeof(aAlbum), &SmtcDuration, &SmtcPosition);
		FromSmtc = aSong[0] != '\0';
	}

	char aKey[256];
	(void)SmtcDuration;
	if(FromSmtc)
	{
		if(aArtist[0] != '\0')
			str_format(aKey, sizeof(aKey), "%s - %s", aArtist, aSong);
		else
			str_copy(aKey, aSong, sizeof(aKey));
	}
	else
	{
		if(Now < m_NextTrackScan)
			return;
		m_NextTrackScan = Now + 0.75f;
		if(!FindPlayingSong(aSong, sizeof(aSong)))
			return;
		str_copy(aKey, aSong, sizeof(aKey));
	}

	if(aKey[0] == '\0')
		return;
	if(str_comp(aKey, m_aSongKey) == 0)
	{
		if(FromSmtc)
			SyncSmtcPosition(SmtcPosition);
		return;
	}

	ResetSong();
	str_copy(m_aSongKey, aKey, sizeof(m_aSongKey));
	{
		const char *pTitle = aKey;
		const char *pDash = nullptr;
		for(const char *p = aKey; (p = str_find(p, " - ")) != nullptr; p += 3)
			pDash = p;
		if(pDash != nullptr)
			pTitle = pDash + 3;
		str_copy(m_aSongTitle, pTitle, sizeof(m_aSongTitle));
	}
	str_copy(m_aSongArtist, aArtist, sizeof(m_aSongArtist));
	str_copy(m_aSongAlbum, aAlbum, sizeof(m_aSongAlbum));
	m_SongDuration = FromSmtc ? SmtcDuration : 0.0f;
	if(FromSmtc)
		SyncSmtcPosition(SmtcPosition);
	if(FromSmtc)
	{
		char aAppId[128];
		m_Smtc.AppId(aAppId, sizeof(aAppId));
		char aBuf[320];
		str_format(aBuf, sizeof(aBuf), "track from media session (%s): %s", aAppId, m_aSongKey);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lyrics", aBuf);
	}
	else
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lyrics", "track found");

	if(LoadCachedLyrics())
	{
		SetStatus("%s", m_aSongKey);
		return;
	}

	SetStatus("%s - looking up lyrics", m_aSongKey);

	m_RequestAttempts = 0;
	StartRequest();
}

static int JsonItemCount(const json_value &Json)
{
	return Json.type == json_array ? (int)Json.u.array.length : 1;
}

static const json_value &JsonItemAt(const json_value &Json, int Index)
{
	return Json.type == json_array ? Json[Index] : Json;
}

int CLyrics::PickLyricsIndex(const json_value &Json, bool NeedTiming) const
{
	int Best = -1;
	float BestScore = 1e9f;
	const int Count = JsonItemCount(Json);
	for(int i = 0; i < Count; i++)
	{
		const json_value &Item = JsonItemAt(Json, i);
		const char *pCandidateTitle = JsonString(Item["trackName"]);
		const char *pCandidateArtist = JsonString(Item["artistName"]);
		const char *pMatchTitle = m_aSongTitle[0] != '\0' ? m_aSongTitle : m_aSongKey;
		if(pCandidateTitle[0] != '\0' && !TitlesMatch(pMatchTitle, pCandidateTitle))
			continue;
		if(m_aSongArtist[0] != '\0' && pCandidateArtist[0] != '\0' && !ArtistsMatch(m_aSongArtist, pCandidateArtist))
			continue;
		const float CandidateDuration = JsonNumber(Item["duration"]);
		if(m_SongDuration > 5.0f && CandidateDuration > 5.0f && fabsf(CandidateDuration - m_SongDuration) > 20.0f)
			continue;

		const json_value &Synced = Item["syncedLyrics"];
		const json_value &FileDoc = Item["lyricsfile"];
		const json_value &Plain = Item["plainLyrics"];
		const bool Usable = NeedTiming ?
			(Synced.type == json_string && Synced.u.string.ptr != nullptr && Synced.u.string.length > 0) ||
				(FileDoc.type == json_string && FileDoc.u.string.ptr != nullptr && str_find(FileDoc.u.string.ptr, "start_ms:") != nullptr) :
			Plain.type == json_string && Plain.u.string.ptr != nullptr && Plain.u.string.length > 0;
		if(!Usable)
			continue;

		float Score = (float)i;
		if(m_SongDuration > 1.0f && CandidateDuration > 1.0f)
			Score += fabsf(CandidateDuration - m_SongDuration) * 2.0f;
		if(Score < BestScore)
		{
			BestScore = Score;
			Best = i;
		}
	}
	return Best;
}

void CLyrics::UpdateHttp()
{
	if(!m_pRequest || m_RequestDone)
		return;
	if(!m_pRequest->Done())
	{
		if(Client()->LocalTime() - m_RequestStart > 20.0f)
		{
			m_pRequest->Abort();
			m_RequestDone = true;
			SetStatus("%s - lyrics request timed out", m_aSongKey);
			m_pRequest.reset();
		}
		return;
	}
	m_RequestDone = true;

	if(m_pRequest->State() == EHttpState::DONE)
	{
		json_value *pJson = m_pRequest->ResultJson();
		bool Found = false;
		bool PlainEstimate = false;
		if(pJson != nullptr && (pJson->type == json_array || pJson->type == json_object))
		{
			int Index = PickLyricsIndex(*pJson, true);
			if(Index < 0)
			{
				Index = PickLyricsIndex(*pJson, false);
				PlainEstimate = Index >= 0;
			}

			if(Index >= 0)
			{
				const json_value &Item = JsonItemAt(*pJson, Index);
				char aMatched[320];
				str_format(aMatched, sizeof(aMatched), "matched: %s - %s", JsonString(Item["artistName"]), JsonString(Item["trackName"]));
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lyrics", aMatched);
				if(PlainEstimate)
				{
					float Duration = m_SongDuration;
					const json_value &Dur = Item["duration"];
					if(Duration <= 1.0f)
					{
						if(Dur.type == json_double)
							Duration = (float)Dur.u.dbl;
						else if(Dur.type == json_integer)
							Duration = (float)Dur.u.integer;
					}
					ApplyPlainLyrics(Item["plainLyrics"].u.string.ptr, Duration);
				}
				else
				{
					const json_value &Synced = Item["syncedLyrics"];
					if(Synced.type == json_string && Synced.u.string.ptr != nullptr)
						ApplyLrcDoc(Synced.u.string.ptr);
					if(!m_HasLyrics)
					{
						const json_value &FileDoc = Item["lyricsfile"];
						if(FileDoc.type == json_string && FileDoc.u.string.ptr != nullptr)
							ApplyLyricsFileDoc(FileDoc.u.string.ptr);
					}
				}
				Found = m_HasLyrics;
				if(Found)
					SaveCachedFromLines();
			}
		}
		if(pJson != nullptr)
			json_value_free(pJson);
		if(!Found && !m_HasLyrics)
		{
			SetStatus("%s - no matching lyrics found", m_aSongKey);
			ScheduleRetry();
		}
		else if(Found && PlainEstimate)
		{
			SetStatus("%s - plain lyrics (approximate timing)", m_aSongKey);
		}
		else if(Found)
		{
			SetStatus("%s", m_aSongKey);
		}
	}
	else
	{
		SetStatus("%s - lyrics request failed (state %d)", m_aSongKey, (int)m_pRequest->State());
		ScheduleRetry();
	}
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lyrics", m_aStatus);

	m_pRequest.reset();
}

void CLyrics::ScheduleRetry()
{
	if(m_HasLyrics)
		return;
	if(m_RequestAttempts >= LYRICS_MAX_ATTEMPTS)
		return;
	m_RequestRetryAt = Client()->LocalTime() + 2.5f;
}

void CLyrics::StartRequest()
{
	m_RequestAttempts++;
	m_RequestDone = false;
	m_RequestStart = Client()->LocalTime();

	char aTitle[256];
	str_copy(aTitle, m_aSongKey, sizeof(aTitle));
	char aArtist[256] = "";
	char *pDash = (char *)str_find(aTitle, " - ");
	if(pDash != nullptr)
	{
		const int ArtistLen = (int)(pDash - aTitle);
		mem_copy(aArtist, aTitle, ArtistLen);
		aArtist[ArtistLen] = '\0';
		mem_move(aTitle, pDash + 3, str_length(pDash + 3) + 1);
	}
	if(m_aSongArtist[0] != '\0')
		str_copy(aArtist, m_aSongArtist, sizeof(aArtist));

	char aEncTitle[512];
	char aEncArtist[512];
	char aEncAlbum[512];
	char aEncQuery[640];
	UrlEncode(aEncTitle, sizeof(aEncTitle), aTitle);
	UrlEncode(aEncArtist, sizeof(aEncArtist), aArtist);
	UrlEncode(aEncAlbum, sizeof(aEncAlbum), m_aSongAlbum);
	UrlEncode(aEncQuery, sizeof(aEncQuery), m_aSongKey);

	const int Variant = (m_RequestAttempts - 1) % 3;
	const int Duration = (int)(m_SongDuration + 0.5f);
	char aUrl[1200];
	if(Variant <= 1 && aArtist[0] != '\0')
	{
		if(Variant == 0 && m_aSongAlbum[0] != '\0' && Duration > 0)
			str_format(aUrl, sizeof(aUrl), "https://lrclib.net/api/get?track_name=%s&artist_name=%s&album_name=%s&duration=%d", aEncTitle, aEncArtist, aEncAlbum, Duration);
		else if(Variant == 0 && Duration > 0)
			str_format(aUrl, sizeof(aUrl), "https://lrclib.net/api/get?track_name=%s&artist_name=%s&duration=%d", aEncTitle, aEncArtist, Duration);
		else
			str_format(aUrl, sizeof(aUrl), "https://lrclib.net/api/get?track_name=%s&artist_name=%s", aEncTitle, aEncArtist);
	}
	else
	{
		str_format(aUrl, sizeof(aUrl), "https://lrclib.net/api/search?q=%s", aEncQuery);
	}

	auto pRequest = HttpGet(aUrl);
	pRequest->HeaderString("User-Agent", "FusionClient/1.0 (https://github.com/FusionSoloTeam/FusionClient)");
	pRequest->HeaderString("Lrclib-Client", "FusionClient/1.0 (https://github.com/FusionSoloTeam/FusionClient)");
	pRequest->MaxResponseSize(4 * 1024 * 1024);
	pRequest->Timeout(CTimeout{0, 15000, 1, 5});
	m_pRequest = std::shared_ptr<IHttpRequest>(std::move(pRequest));
	m_RequestRetryAt = 0.0f;
	Http()->Run(m_pRequest);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lyrics", "lyrics request sent");
}

void CLyrics::SaveCachedFromLines()
{
	if(m_vLines.empty())
		return;
	std::string Text;
	Text.reserve(m_vLines.size() * 64);
	for(const SLine &Line : m_vLines)
	{
		char aBuf[512];
		const int Minutes = (int)(Line.m_Time / 60.0f);
		const float Seconds = Line.m_Time - Minutes * 60.0f;
		str_format(aBuf, sizeof(aBuf), "[%02d:%05.2f]%s\n", Minutes, Seconds, Line.m_aText);
		Text.append(aBuf);
	}
	SaveCachedLyrics(Text.c_str());
}

bool CLyrics::ApplyLyricsFileDoc(const char *pText)
{
	std::vector<SLine> vLines;
	char aPendingText[256] = "";
	float PendingTime = -1.0f;
	const char *pCursor = pText;
	char aLine[512];
	while(NextLine(pCursor, aLine, sizeof(aLine)))
	{
		TrimInPlace(aLine);
		if(str_startswith_nocase(aLine, "- text:"))
		{
			str_copy(aPendingText, aLine + 7, sizeof(aPendingText));
			TrimInPlace(aPendingText);
			const int TextLen = str_length(aPendingText);
			if(TextLen >= 2 && aPendingText[0] == '\'' && aPendingText[TextLen - 1] == '\'')
			{
				aPendingText[TextLen - 1] = '\0';
				mem_move(aPendingText, aPendingText + 1, TextLen - 1);
			}
		}
		else if(str_startswith_nocase(aLine, "start_ms:"))
		{
			PendingTime = str_toint(aLine + 9) / 1000.0f;
			if(PendingTime >= 0.0f && aPendingText[0] != '\0')
			{
				SLine Line;
				Line.m_Time = PendingTime;
				str_copy(Line.m_aText, aPendingText, sizeof(Line.m_aText));
				vLines.push_back(Line);
			}
			aPendingText[0] = '\0';
			PendingTime = -1.0f;
		}
		if((int)vLines.size() >= LYRICS_MAX_STORED_LINES)
			break;
	}

	if(vLines.empty())
		return false;

	std::stable_sort(vLines.begin(), vLines.end(), [](const SLine &A, const SLine &B) { return A.m_Time < B.m_Time; });
	m_vLines = std::move(vLines);
	m_HasLyrics = true;
	m_PlainEstimate = false;
	m_LastLineIndex = -2;
	m_vDrawn.clear();
	return true;
}

void CLyrics::ApplyPlainLyrics(const char *pText, float Duration)
{
	std::vector<SLine> vLines;
	const char *pCursor = pText;
	char aLine[256];
	while((int)vLines.size() < 200 && NextLine(pCursor, aLine, sizeof(aLine)))
	{
		TrimInPlace(aLine);
		if(aLine[0] == '\0')
			continue;
		SLine Line;
		Line.m_Time = 0.0f;
		str_copy(Line.m_aText, aLine, sizeof(Line.m_aText));
		vLines.push_back(Line);
	}

	if(vLines.empty())
		return;

	if(Duration <= 0.0f)
		Duration = (float)vLines.size() * 3.2f;
	const float Start = Duration * 0.05f;
	const float End = Duration * 0.96f;
	const float Step = vLines.size() > 1 ? (End - Start) / (float)(vLines.size() - 1) : 0.0f;
	for(size_t i = 0; i < vLines.size(); i++)
		vLines[i].m_Time = Start + Step * (float)i;

	m_vLines = std::move(vLines);
	m_HasLyrics = true;
	m_PlainEstimate = true;
	m_LastLineIndex = -2;
	m_vDrawn.clear();
}

void CLyrics::ApplyLrcDoc(const char *pText)
{
	m_vLines.clear();
	m_HasLyrics = false;
	m_PlainEstimate = false;
	if(pText == nullptr)
		return;

	float OffsetSeconds = 0.0f;
	float LengthSeconds = 0.0f;
	const char *pCursor = pText;
	if(str_startswith(pCursor, "\xEF\xBB\xBF"))
		pCursor += 3;

	std::vector<SLine> vLines;
	char aLine[512];
	while(NextLine(pCursor, aLine, sizeof(aLine)))
	{
		char *p = aLine;
		TrimInPlace(p);

		float aTimes[16];
		int NumTimes = 0;
		while(p[0] == '[')
		{
			char *pClose = (char *)str_find(p, "]");
			if(pClose == nullptr)
				break;
			char aTag[64];
			const int TagLen = (int)(pClose - p - 1);
			if(TagLen <= 0 || TagLen >= (int)sizeof(aTag))
				break;
			mem_copy(aTag, p + 1, TagLen);
			aTag[TagLen] = '\0';

			if(str_startswith_nocase(aTag, "offset:"))
			{
				OffsetSeconds = (float)str_toint(aTag + 7) / 1000.0f;
			}
			else if(str_startswith_nocase(aTag, "length:"))
			{
				const char *pLength = aTag + 7;
				int Minutes = 0;
				int Seconds = 0;
				while(*pLength == ' ' || *pLength == ':')
					pLength++;
				if(sscanf(pLength, "%d:%d", &Minutes, &Seconds) == 2)
					LengthSeconds = Minutes * 60.0f + Seconds;
			}
			else if(!IsLrcMetaTag(aTag))
			{
				int Minutes = 0;
				int Seconds = 0;
				float Fraction = 0.0f;
				const char *pTag = aTag;
				while(*pTag == ' ')
					pTag++;
				if(sscanf(pTag, "%d:%d", &Minutes, &Seconds) == 2)
				{
					const char *pDot = str_find(pTag, ".");
					if(pDot != nullptr)
					{
						float Scale = 1.0f;
						float Digits = 0.0f;
						for(const char *pD = pDot + 1; *pD >= '0' && *pD <= '9'; pD++)
						{
							Digits = Digits * 10.0f + (*pD - '0');
							Scale *= 10.0f;
						}
						Fraction = Digits / Scale;
					}
					else
					{
						const char *pFirstColon = str_find(pTag, ":");
						const char *pSecondColon = pFirstColon != nullptr ? str_find(pFirstColon + 1, ":") : nullptr;
						if(pSecondColon != nullptr)
							Fraction = str_toint(pSecondColon + 1) / 100.0f;
					}
					if(NumTimes < 16)
						aTimes[NumTimes++] = Minutes * 60.0f + Seconds + Fraction;
				}
			}
			p = pClose + 1;
		}

		if(NumTimes == 0)
			continue;

		TrimInPlace(p);
		char aText[256];
		int Out = 0;
		for(const char *pSrc = p; *pSrc != '\0' && Out < (int)sizeof(aText) - 1;)
		{
			if(*pSrc == '<' && *(pSrc + 1) >= '0' && *(pSrc + 1) <= '9')
			{
				const char *pEnd = str_find(pSrc, ">");
				if(pEnd != nullptr)
				{
					pSrc = pEnd + 1;
					continue;
				}
			}
			aText[Out++] = *pSrc++;
		}
		aText[Out] = '\0';
		TrimInPlace(aText);
		if(aText[0] == '\0')
			continue;

		for(int i = 0; i < NumTimes; i++)
		{
			SLine Line;
			Line.m_Time = maximum(0.0f, aTimes[i] - OffsetSeconds);
			str_copy(Line.m_aText, aText, sizeof(Line.m_aText));
			vLines.push_back(Line);
			if((int)vLines.size() >= LYRICS_MAX_STORED_LINES)
				break;
		}
		if((int)vLines.size() >= LYRICS_MAX_STORED_LINES)
			break;
	}

	if(vLines.empty())
		return;

	std::stable_sort(vLines.begin(), vLines.end(), [](const SLine &A, const SLine &B) { return A.m_Time < B.m_Time; });
	m_vLines = std::move(vLines);
	m_HasLyrics = true;
	if(LengthSeconds > 5.0f && m_SongDuration <= 5.0f)
		m_SongDuration = LengthSeconds;
	if(!m_HasPosAnchor)
		m_Position = g_Config.m_TcLyricsOffset / 1000.0f;
	m_LastLineIndex = -2;
	m_vDrawn.clear();
}

bool CLyrics::LoadLrcFile(const char *pPath)
{
	IOHANDLE File = Storage()->OpenFile(pPath, IOFLAG_READ, IStorage::TYPE_ALL_OR_ABSOLUTE);
	if(!File)
		return false;
	const int Size = (int)io_length(File);
	if(Size <= 0 || Size > 2 * 1024 * 1024)
	{
		io_close(File);
		return false;
	}
	char *pBuffer = (char *)malloc(Size + 1);
	io_read(File, pBuffer, Size);
	pBuffer[Size] = '\0';
	io_close(File);
	ApplyLrcDoc(pBuffer);
	free(pBuffer);
	return m_HasLyrics;
}

static void CachePath(char *pOut, int OutSize, const char *pSongKey)
{
	char aName[256];
	str_copy(aName, pSongKey, sizeof(aName));
	str_sanitize_filename(aName);
	for(char *p = aName; *p != '\0'; p++)
		if(*p == ' ' || *p == '.')
			*p = '_';
	if(aName[0] == '\0')
		str_copy(aName, "track", sizeof(aName));
	str_format(pOut, OutSize, "lyrics/%s.lrc", aName);
}

bool CLyrics::LoadCachedLyrics()
{
	char aPath[512];
	CachePath(aPath, sizeof(aPath), m_aSongKey);
	if(!Storage()->FileExists(aPath, IStorage::TYPE_SAVE))
		return false;

	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	char aHeader[8] = "";
	const int Read = io_read(File, aHeader, 5);
	io_close(File);
	if(Read != 5 || str_comp_num(aHeader, "#tc2\n", 5) != 0)
	{
		Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
		return false;
	}
	return LoadLrcFile(aPath);
}

void CLyrics::SaveCachedLyrics(const char *pText)
{
	char aPath[512];
	CachePath(aPath, sizeof(aPath), m_aSongKey);
	Storage()->CreateFolder("lyrics", IStorage::TYPE_SAVE);
	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	io_write(File, "#tc2\n", 5);
	io_write(File, pText, str_length(pText));
	io_close(File);
}

void CLyrics::BuildLayout(SDrawnLine &Line) const
{
	Line.m_vGlyphs.clear();
	Line.m_LayoutFontSize = LyricsLayoutFontSize();
	Line.m_LayoutWidth = 0.0f;

	float X = 0.0f;
	const char *pCursor = Line.m_aText;
	while(*pCursor != '\0')
	{
		const char *pTmp = pCursor;
		const int Chr = str_utf8_decode(&pTmp);
		if(Chr <= 0)
			break;
		pCursor = pTmp;

		SGlyphMetrics Metrics;
		if(!TextRender()->GetGlyphMetrics(Line.m_LayoutFontSize, Chr, &Metrics))
		{
			X += Line.m_LayoutFontSize * 0.3f;
			continue;
		}
		SGlyphPlacement Placement;
		Placement.m_Metrics = Metrics;
		Placement.m_X = X;
		X += Metrics.m_AdvanceX;
		Line.m_vGlyphs.push_back(Placement);
	}
	Line.m_LayoutWidth = X;
}

void CLyrics::SpawnLine(int Index)
{
	if(Index < 0 || Index >= (int)m_vLines.size())
		return;

	const SLine &Source = m_vLines[Index];
	const float Now = Client()->LocalTime();
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "lyrics", Source.m_aText);

	const float ScreenW = Graphics()->ScreenWidth();
	const float ScreenH = Graphics()->ScreenHeight();

	SDrawnLine Line;
	str_copy(Line.m_aText, Source.m_aText, sizeof(Line.m_aText));
	if(Line.m_aText[0] == '\0')
		return;
	Line.m_LayoutFontSize = LyricsLayoutFontSize();
	BuildLayout(Line);
	if(Line.m_LayoutFontSize <= 0 || Line.m_LayoutWidth <= 0.0f)
		Line.m_LayoutWidth = str_length(Line.m_aText) * (float)maximum(1, LyricsLayoutFontSize()) * 0.52f;

	Line.m_SpawnTime = Now;
	float Hold = 5.0f;
	if(Index + 1 < (int)m_vLines.size())
		Hold = std::clamp(m_vLines[Index + 1].m_Time - Source.m_Time, 1.2f, 7.5f);
	Line.m_EndTime = Now + Hold;

	const float Drift = g_Config.m_TcLyricsDrift / 100.0f;
	Line.m_Seed = Hash01((unsigned)(Index * 2654435761u + 17));
	Line.m_Tilt = (Hash01((unsigned)(Index * 40503u + 991)) * 2.0f - 1.0f) * 0.10f;
	Line.m_SwayPhase = Hash01((unsigned)(Index * 7817u + 33)) * 6.2831853f;

	const float DepthVariation = (Hash01((unsigned)(Index * 15485863u + 7)) * 2.0f - 1.0f) * 0.35f;
	const float Zoom = maximum(0.001f, GameClient()->m_Camera.m_Zoom);
	Line.m_Velocity = vec2(
		(Hash01((unsigned)(Index * 31337u + 5)) * 2.0f - 1.0f) * 5.0f,
		-7.0f) *
			     Drift * ScreenPxPerUnit();

	const float TextScale = g_Config.m_TcLyricsSize / (float)maximum(1, Line.m_LayoutFontSize);
	const float TextHalfWidth = 0.5f * Line.m_LayoutWidth * TextScale;
	const float TextHalfHeight = 0.5f * g_Config.m_TcLyricsSize;
	Line.m_ScreenOffset = vec2(
		(Hash01((unsigned)(Index * 6151u + 3)) * 2.0f - 1.0f) * ScreenW * 0.30f / Zoom,
		(Hash01((unsigned)(Index * 24593u + 11)) * 2.0f - 1.0f) * ScreenH * 0.26f / Zoom + DepthVariation * ScreenH * 0.05f / Zoom);
	const float MaxOffsetX = maximum(0.0f, ScreenW * 0.5f / Zoom - TextHalfWidth);
	const float MaxOffsetY = maximum(0.0f, ScreenH * 0.5f / Zoom - TextHalfHeight);
	Line.m_ScreenOffset.x = std::clamp(Line.m_ScreenOffset.x, -MaxOffsetX, MaxOffsetX);
	Line.m_ScreenOffset.y = std::clamp(Line.m_ScreenOffset.y, -MaxOffsetY, MaxOffsetY);

	for(SDrawnLine &Old : m_vDrawn)
	{
		if(Old.m_EndTime > Now + 0.6f)
			Old.m_EndTime = Now + 0.6f;
	}

	m_vDrawn.push_back(std::move(Line));

	const int MaxLines = std::clamp(g_Config.m_TcLyricsLines, 1, 6);
	const int Excess = (int)m_vDrawn.size() - MaxLines;
	for(int i = 0; i < Excess && i < (int)m_vDrawn.size(); i++)
		m_vDrawn[i].m_EndTime = std::min(m_vDrawn[i].m_EndTime, Now + 0.35f);
}

void CLyrics::OnRender()
{
	if(!g_Config.m_TcLyrics)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(m_vDrawn.empty() && m_vSparkles.empty())
		return;
	if(GameClient()->m_Camera.m_Zoom <= 0.0f)
		return;

	const float Now = Client()->LocalTime();
	const float FrameTime = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);

	Graphics()->BlendNormal();

	float Beat = 0.0f;
	if(g_Config.m_TcLyricsBeat)
	{
		const float Raw = GameClient()->m_Outlines.SystemAudio().Beat();
		m_BeatSmooth = std::max(Raw, m_BeatSmooth - FrameTime * 6.0f);
		Beat = std::clamp(m_BeatSmooth, 0.0f, 1.0f);
		if(Beat > 0.6f && Now - m_LastBeatTime > 0.1f)
		{
			m_LastBeatTime = Now;
			if(g_Config.m_TcLyricsGlitter && !m_vDrawn.empty())
			{
				std::vector<vec2> vPoints;
				const SDrawnLine &Line = m_vDrawn.back();
				const vec2 Origin = LineOrigin(Line);
				const float Scale = LineScale(Line);
				for(const SGlyphPlacement &G : Line.m_vGlyphs)
					vPoints.push_back(Origin + vec2((G.m_X + G.m_Metrics.m_Width * 0.5f - Line.m_LayoutWidth * 0.5f) * Scale, 0.0f));
				if(g_Config.m_TcLyricsClean == 0)
					SpawnSparkles(vPoints, 10, Now);
			}
		}
	}

	std::vector<vec2> vGlitterPoints;
	for(SDrawnLine &Line : m_vDrawn)
		DrawLine(Line, Now, Beat, vGlitterPoints);

	if(g_Config.m_TcLyricsGlitter)
	{
		m_SparkleAccum += FrameTime * 26.0f;
		const int Amount = (int)m_SparkleAccum;
		m_SparkleAccum -= Amount;
		SpawnSparkles(vGlitterPoints, Amount, Now);
		DrawSparkles(Now);
	}

	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	for(size_t i = 0; i < m_vSparkles.size();)
	{
		if(Now - m_vSparkles[i].m_SpawnTime > m_vSparkles[i].m_Life)
			m_vSparkles.erase(m_vSparkles.begin() + i);
		else
			i++;
	}
}

void CLyrics::DrawLine(SDrawnLine &Line, float Now, float Beat, std::vector<vec2> &vGlitterPoints)
{
	if(Line.m_LayoutFontSize != LyricsLayoutFontSize())
		BuildLayout(Line);

	const float Age = Line.Age(Now);
	const float Remaining = Line.m_EndTime - Now;
	if(Remaining <= 0.0f)
		return;
	if(Line.m_LayoutFontSize <= 0 || Line.m_vGlyphs.empty())
		return;

	const float FadeInTime = 0.32f;
	const float FadeOutTime = 0.9f;
	const float FadeIn = std::clamp(Age / FadeInTime, 0.0f, 1.0f);
	const float FadeOut = std::clamp(Remaining / FadeOutTime, 0.0f, 1.0f);
	const float Alpha = std::min(FadeIn, FadeOut);
	if(Alpha <= 0.005f)
		return;

	const float PxPerUnit = ScreenPxPerUnit();
	float Scale = LineScale(Line);
	const float Width = Line.m_LayoutWidth * (g_Config.m_TcLyricsSize / (float)Line.m_LayoutFontSize);
	const float MaxWidth = Graphics()->ScreenWidth() * 0.72f;
	if(Width > MaxWidth && Width > 0.0f)
		Scale *= MaxWidth / Width;
	if(g_Config.m_TcLyricsClean == 0)
		Scale *= 1.0f + Beat * 0.05f;

	const bool Clean = g_Config.m_TcLyricsClean != 0;
	const float Drift = g_Config.m_TcLyricsDrift / 100.0f;
	vec2 Origin = LineOrigin(Line) + Line.m_Velocity * Age / PxPerUnit;
	Origin.x += sinf(Age * 0.85f + Line.m_SwayPhase) * 7.0f * Drift / PxPerUnit;
	Origin.y += sinf(Age * 0.55f + Line.m_SwayPhase * 1.7f) * 2.5f * Drift / PxPerUnit;

	const float Tilt = Line.m_Tilt * (g_Config.m_TcLyricsTilt / 100.0f);
	const float Cos = cosf(Tilt);
	const float Sin = sinf(Tilt);

	vec2 DepthDir(0.70f, -0.71f);
	DepthDir = vec2(DepthDir.x * Cos - DepthDir.y * Sin, DepthDir.x * Sin + DepthDir.y * Cos);
	DepthDir = normalize(DepthDir);

	const float Extrusion = Clean ? 0.0f : g_Config.m_TcLyricsExtrusion / 100.0f;
	const int Layers = (int)std::clamp(roundf(Extrusion * 6.0f), 0.0f, 12.0f);
	const float MaxDepth = g_Config.m_TcLyricsSize * 0.12f / PxPerUnit;
	const float Step = Layers > 0 ? minimum(g_Config.m_TcLyricsSize * 0.05f / PxPerUnit, MaxDepth / Layers) * (1.0f + Beat * 0.10f) : 0.0f;

	const ColorRGBA Gold = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcLyricsColor));

	for(int Layer = Layers; Layer >= 1; Layer--)
	{
		const float T = Layer / (float)Layers;
		const float Shade = 0.10f + 0.10f * (1.0f - T);
		const ColorRGBA Dark(Shade * 0.95f, Shade, Shade * 1.06f, 1.0f);
		DrawGlyphPass(Line, Origin, Scale, Tilt, DepthDir, Step, Layer, false, Dark, Alpha, false, nullptr);
	}

	DrawGlyphPass(Line, Origin, Scale, Tilt, DepthDir, Step, 0, false, Gold, Alpha, !Clean && g_Config.m_TcLyricsGlitter != 0, &vGlitterPoints);
}

void CLyrics::DrawGlyphPass(const SDrawnLine &Line, vec2 Origin, float Scale, float Tilt, vec2 DepthDir, float Step, int Layer, bool Outline, const ColorRGBA &BaseColor, float Alpha, bool Glitter, std::vector<vec2> *pvGlitterPoints)
{
	const int FontSize = Line.m_LayoutFontSize;
	if(FontSize <= 0 || Line.m_vGlyphs.empty())
		return;

	const float Cos = cosf(Tilt);
	const float Sin = sinf(Tilt);
	const vec2 LayerOffset = DepthDir * (Step * Layer);
	const float CenterX = Line.m_LayoutWidth * 0.5f;
	const float CenterY = FontSize * 0.5f;
	const float Time = Client()->LocalTime();
	const bool Flat = g_Config.m_TcLyricsClean != 0;

	Graphics()->TextureSet(EnsureGlyphAtlas(Outline || Flat ? TEXT_TEXTURE_OUTLINE : TEXT_TEXTURE_FILL));
	Graphics()->QuadsBegin();

	const float InvTextureSize = 1.0f / (float)maximum(1, TextRender()->GetFontTextureSize());

	unsigned GlyphIndex = 0;
	for(const SGlyphPlacement &Placement : Line.m_vGlyphs)
	{
		const SGlyphMetrics &Metrics = Placement.m_Metrics;
		GlyphIndex++;
		if(Metrics.m_Width <= 0.0f || Metrics.m_Height <= 0.0f)
			continue;

		const float Left = (Placement.m_X + Metrics.m_OffsetX - CenterX) * Scale;
		const float Bottom = (FontSize - Metrics.m_OffsetY - CenterY) * Scale;
		const float W = Metrics.m_Width * Scale;
		const float H = Metrics.m_Height * Scale;

		vec2 aCorners[4] = {
			vec2(Left, Bottom - H),
			vec2(Left + W, Bottom - H),
			vec2(Left + W, Bottom),
			vec2(Left, Bottom),
		};

		float Shade = 1.0f;
		if(Glitter)
		{
			const float Random = Hash01(GlyphIndex * 2654435761u + (unsigned)(Line.m_Seed * 65535.0f));
			const float Shimmer = Hash01(GlyphIndex * 7919u + (unsigned)(Time * 6.0f) * 131u);
			Shade = 0.82f + 0.18f * Random + 0.22f * Shimmer * Shimmer;
		}

		for(int c = 0; c < 4; c++)
		{
			vec2 P = aCorners[c];
			const vec2 Rotated(P.x * Cos - P.y * Sin, P.x * Sin + P.y * Cos);
			aCorners[c] = Origin + Rotated + LayerOffset;
		}

		if(pvGlitterPoints != nullptr && (GlyphIndex % 3) == 0 && pvGlitterPoints->size() < 96)
		{
			const float Random = Hash01(GlyphIndex * 104729u);
			const vec2 Mid = (aCorners[0] + aCorners[2]) * 0.5f;
			pvGlitterPoints->push_back(Mid + vec2((Random - 0.5f) * W, (Random - 0.5f) * H));
		}

		float TopBoost = 1.0f;
		float BottomShade = 1.0f;
		if(!Flat)
		{
			TopBoost = Outline ? 1.03f : (Layer > 0 ? 1.10f : 1.06f);
			BottomShade = Outline ? 0.95f : 0.88f;
		}
		const float AverageShade = (TopBoost + BottomShade) * 0.5f;
		const float R = std::clamp(BaseColor.r * Shade * AverageShade, 0.0f, 1.0f);
		const float G = std::clamp(BaseColor.g * Shade * AverageShade, 0.0f, 1.0f);
		const float B = std::clamp(BaseColor.b * Shade * AverageShade, 0.0f, 1.0f);

		Graphics()->SetColor(R, G, B, Alpha);
		const float U0 = Metrics.m_aUVs[0] * InvTextureSize;
		const float V0 = Metrics.m_aUVs[1] * InvTextureSize;
		const float U1 = Metrics.m_aUVs[2] * InvTextureSize;
		const float V1 = Metrics.m_aUVs[3] * InvTextureSize;
		Graphics()->QuadsSetSubsetFree(U0, V0, U1, V0, U0, V1, U1, V1);
		IGraphics::CFreeformItem Item(aCorners[0], aCorners[1], aCorners[3], aCorners[2]);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}

	Graphics()->QuadsEnd();
}

void CLyrics::SpawnSparkles(const std::vector<vec2> &vPoints, int Amount, float Now)
{
	if(!g_Config.m_TcLyricsGlitter || Amount <= 0 || vPoints.empty())
		return;

	const float Size = g_Config.m_TcLyricsSize;
	for(int i = 0; i < Amount; i++)
	{
		if((int)m_vSparkles.size() >= LYRICS_MAX_SPARKLES)
			break;
		const vec2 Base = vPoints[rand() % vPoints.size()];
		SSparkle Sparkle;
		Sparkle.m_Pos = Base + vec2(random_float(-0.3f, 0.3f) * Size, random_float(-0.3f, 0.3f) * Size);
		Sparkle.m_Vel = vec2(random_float(-0.25f, 0.25f) * Size, random_float(-0.9f, 0.0f) * Size);
		Sparkle.m_SpawnTime = Now;
		Sparkle.m_Life = random_float(0.45f, 1.1f);
		Sparkle.m_Size = Size * random_float(0.02f, 0.065f);
		m_vSparkles.push_back(Sparkle);
	}
}

void CLyrics::DrawSparkles(float Now)
{
	if(m_vSparkles.empty())
		return;

	const ColorRGBA Gold = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcLyricsColor));
	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	for(const SSparkle &Sparkle : m_vSparkles)
	{
		const float Age = Now - Sparkle.m_SpawnTime;
		if(Age < 0.0f || Age > Sparkle.m_Life)
			continue;
		const float T = Age / Sparkle.m_Life;
		const float Fade = T < 0.25f ? T / 0.25f : 1.0f - (T - 0.25f) / 0.75f;
		const vec2 Pos = Sparkle.m_Pos + Sparkle.m_Vel * Age;
		Graphics()->SetColor(std::min(1.0f, Gold.r * 1.15f), std::min(1.0f, Gold.g * 1.1f), Gold.b, std::clamp(Fade, 0.0f, 1.0f) * 0.85f);
		Graphics()->DrawCircle(Pos.x, Pos.y, Sparkle.m_Size * (0.55f + 0.45f * Fade), 4);
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}
