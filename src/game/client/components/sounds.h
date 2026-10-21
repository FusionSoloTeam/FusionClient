/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SOUNDS_H
#define GAME_CLIENT_COMPONENTS_SOUNDS_H

#include <base/vmath.h>

#include <engine/shared/jobs.h>
#include <engine/sound.h>

#include <game/client/component.h>

class CSoundLoading : public IJob
{
	CGameClient *m_pGameClient;
	bool m_Render;

public:
	CSoundLoading(CGameClient *pGameClient, bool Render);
	void Run() override;
};

class CSounds : public CComponent
{
	enum
	{
		QUEUE_SIZE = 32,
		MAX_ECHOES = 24,
		MAX_OCCLUSION_WALLS = 4,
		MAX_OCCLUSION_TRACES_PER_FRAME = 12,
		MAX_ECHOES_PER_SOUND = 3,
	};
	class CQueueEntry
	{
	public:
		int m_Channel;
		int m_SetId;
	};
	CQueueEntry m_aQueue[QUEUE_SIZE];
	int m_QueuePos;
	int64_t m_QueueWaitTime;
	std::shared_ptr<CSoundLoading> m_pSoundJob;
	bool m_WaitForSoundJob;

	class CEcho
	{
	public:
		float m_Time;
		int m_SampleId;
		float m_Volume;
		vec2 m_Position;
	};
	CEcho m_aEchoes[MAX_ECHOES];
	int m_NumEchoes = 0;
	float m_Enclosure = 0.0f;
	float m_EnclosureTime = -1.0f;
	int m_OcclusionTraces = 0;

	void UpdateChannels();
	int GetSampleId(int SetId);

	static constexpr float MAX_SOUND_DISTANCE = 1500.0f;
	static constexpr float OCCLUSION_MAX_DISTANCE = 1000.0f;
	static constexpr float ECHO_MAX_DISTANCE = 1200.0f;
	static constexpr float SOUND_SOURCE_RADIUS = 56.0f;
	static constexpr float ENCLOSURE_RADIUS = 420.0f;

	int CountWalls(vec2 From, vec2 To) const;
	float ApplyOcclusion(int SampleId, float Volume, vec2 Position);
	float GetEnclosure(vec2 Pos);
	int FindSoundSource(vec2 Pos) const;
	bool IsBlockedTeammateSound(vec2 Pos) const;
	void ScheduleEchoes(int SampleId, float Volume, vec2 Position, float Distance);
	void UpdateEchoes();

	float m_GuiSoundVolume = -1.0f;
	float m_GameSoundVolume = -1.0f;
	float m_MapSoundVolume = -1.0f;
	float m_BackgroundMusicVolume = -1.0f;

public:
	void ReloadSounds();

	// sound channels
	enum
	{
		CHN_GUI = 0,
		CHN_MUSIC,
		CHN_WORLD,
		CHN_GLOBAL,
		CHN_MAPSOUND,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;

	void ClearQueue();
	void Enqueue(int Channel, int SetId);
	void Play(int Channel, int SetId, float Volume);
	void PlayAt(int Channel, int SetId, float Volume, vec2 Position);
	void PlayAndRecord(int Channel, int SetId, float Volume, vec2 Position);
	void Stop(int SetId);
	bool IsPlaying(int SetId);

	ISound::CVoiceHandle PlaySample(int Channel, int SampleId, int Flags, float Volume);
	ISound::CVoiceHandle PlaySampleAt(int Channel, int SampleId, int Flags, float Volume, vec2 Position);
};

#endif
