#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_LYRICS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_LYRICS_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/external/json-parser/json.h>
#include <engine/textrender.h>

#include <game/client/component.h>

#include "smtc.h"

#include <memory>
#include <vector>

class CHttpRequest;

class CLyrics : public CComponent
{
public:
	~CLyrics() override;

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnReset() override;

	const char *Status() const { return m_aStatus; }
	bool HasLyrics() const { return m_HasLyrics; }
	int NumLines() const { return (int)m_vLines.size(); }
	int NumVisible() const { return (int)m_vDrawn.size(); }
	float Position() const { return m_Position; }

private:
	struct SLine
	{
		float m_Time;
		char m_aText[256];
	};

	struct SGlyphPlacement
	{
		SGlyphMetrics m_Metrics;
		float m_X;
	};

	struct SDrawnLine
	{
		std::vector<SGlyphPlacement> m_vGlyphs;
		int m_LayoutFontSize = 0;
		float m_LayoutWidth = 0.0f;

		char m_aText[256] = "";
		vec2 m_ScreenOffset = vec2(0.0f, 0.0f);
		vec2 m_Velocity = vec2(0.0f, 0.0f);
		float m_SpawnTime = 0.0f;
		float m_EndTime = 0.0f;
		float m_Tilt = 0.0f;
		float m_SwayPhase = 0.0f;
		float m_Seed = 0.0f;

		float Age(float Now) const { return Now - m_SpawnTime; }
	};

	struct SSparkle
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_SpawnTime;
		float m_Life;
		float m_Size;
	};

	float ScreenPxPerUnit() const;
	vec2 LineOrigin(const SDrawnLine &Line) const;
	float LineScale(const SDrawnLine &Line) const;

	void ClearState();
	void ResetState();
	void ResetSong();
	void UpdateTrack();
	void UpdateHttp();
	int PickLyricsIndex(const json_value &Json, bool NeedTiming) const;
	void StartRequest();
	void ScheduleRetry();
	void UpdatePosition(float FrameTime);
	void SyncSmtcPosition(float SmtcPosition);
	void SpawnLine(int Index);
	void SetStatus(const char *pFormat, ...);
	void BuildLayout(SDrawnLine &Line) const;
	bool LoadLrcFile(const char *pPath);
	bool LoadCachedLyrics();
	void SaveCachedLyrics(const char *pText);
	void SaveCachedFromLines();
	bool ApplyLyricsFileDoc(const char *pText);
	void ApplyPlainLyrics(const char *pText, float Duration);
	void ApplyLrcDoc(const char *pText);

	void DrawLine(SDrawnLine &Line, float Now, float Beat, std::vector<vec2> &vGlitterPoints);
	void DrawGlyphPass(const SDrawnLine &Line, vec2 Origin, float Scale, float Tilt, vec2 DepthDir, float Step, int Layer, bool Outline, const ColorRGBA &BaseColor, float Alpha, bool Glitter, std::vector<vec2> *pvGlitterPoints);
	void DrawSparkles(float Now);
	void SpawnSparkles(const std::vector<vec2> &vPoints, int Amount, float Now);

	// The engine glyph atlas is single channel, the quad shader would read it as fully opaque.
	// Keep an RGBA mirror of it so glyph quads get a proper alpha channel.
	IGraphics::CTextureHandle EnsureGlyphAtlas(int Index);
	void UnloadGlyphAtlases();

	IGraphics::CTextureHandle m_aGlyphAtlas[2];
	int m_aGlyphAtlasDimension[2] = {0, 0};
	unsigned m_aGlyphAtlasGeneration[2] = {0, 0};

	std::vector<SLine> m_vLines;
	std::vector<SDrawnLine> m_vDrawn;
	std::vector<SSparkle> m_vSparkles;

	std::shared_ptr<CHttpRequest> m_pRequest;
	bool m_RequestDone = false;
	bool m_HasLyrics = false;
	bool m_PlainEstimate = false;
	CSmtc m_Smtc;

	char m_aSongKey[256] = "";
	char m_aSongTitle[128] = "";
	char m_aSongArtist[128] = "";
	char m_aSongAlbum[128] = "";
	char m_aLoadedFile[128] = "";
	char m_aStatus[256] = "";

	float m_Position = 0.0f;
	float m_LastSmtcPos = -1.0f;
	double m_LastSmtcTime = 0.0;
	bool m_HasPosAnchor = false;
	float m_LastPosTime = -1.0f;
	int m_SmtcChanges = 0;
	float m_LastSmtcChange = 0.0f;
	float m_SongDuration = 0.0f;
	float m_NextTrackScan = 0.0f;
	float m_RequestStart = 0.0f;
	float m_RequestRetryAt = 0.0f;
	int m_RequestAttempts = 0;
	float m_BeatSmooth = 0.0f;
	float m_LastBeatTime = -100.0f;
	float m_SparkleAccum = 0.0f;
	int m_LastLineIndex = -2;
};

#endif
