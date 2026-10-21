/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "sounds.h"

#include <base/time.h>

#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/sound.h>

#include <generated/client_data.h>

#include <base/fs.h>

#include <game/client/components/camera.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <cmath>

CSoundLoading::CSoundLoading(CGameClient *pGameClient, bool Render) :
	m_pGameClient(pGameClient),
	m_Render(Render)
{
	Abortable(true);
}

void CSoundLoading::Run()
{
	for(int s = 0; s < g_pData->m_NumSounds; s++)
	{
		const char *pLoadingCaption = Localize("Loading DDNet Client");
		const char *pLoadingContent = Localize("Loading sound files");

		for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; i++)
		{
			if(State() == IJob::STATE_ABORTED)
				return;

			int Id = -1;
			if(g_Config.m_ClAssetAudio[0] != '\0')
			{
				char aCustom[IO_MAX_PATH_LENGTH];
				const char *pOriginal = g_pData->m_aSounds[s].m_aSounds[i].m_pFilename;
				str_format(aCustom, sizeof(aCustom), "assets/audio/%s/%s", g_Config.m_ClAssetAudio, pOriginal);
				if(m_pGameClient->Storage()->FileExists(aCustom, IStorage::TYPE_ALL))
					Id = m_pGameClient->Sound()->LoadWV(aCustom);
				else
				{
					str_format(aCustom, sizeof(aCustom), "assets/audio/%s/%s", g_Config.m_ClAssetAudio, fs_filename(pOriginal));
					if(m_pGameClient->Storage()->FileExists(aCustom, IStorage::TYPE_ALL))
						Id = m_pGameClient->Sound()->LoadWV(aCustom);
				}
			}
			if(Id < 0)
				Id = m_pGameClient->Sound()->LoadWV(g_pData->m_aSounds[s].m_aSounds[i].m_pFilename);
			g_pData->m_aSounds[s].m_aSounds[i].m_Id = Id;
			// try to render a frame
			if(m_Render)
				m_pGameClient->m_Menus.RenderLoading(pLoadingCaption, pLoadingContent, 0);
		}

		if(m_Render)
			m_pGameClient->m_Menus.RenderLoading(pLoadingCaption, pLoadingContent, 1);
	}
}

void CSounds::UpdateChannels()
{
	const float NewGuiSoundVolume = g_Config.m_SndChatVolume / 100.0f;
	if(NewGuiSoundVolume != m_GuiSoundVolume)
	{
		m_GuiSoundVolume = NewGuiSoundVolume;
		Sound()->SetChannel(CSounds::CHN_GUI, m_GuiSoundVolume, 0.0f);
	}

	const float NewGameSoundVolume = g_Config.m_SndGameVolume / 100.0f;
	if(NewGameSoundVolume != m_GameSoundVolume)
	{
		m_GameSoundVolume = NewGameSoundVolume;
		Sound()->SetChannel(CSounds::CHN_WORLD, 0.9f * m_GameSoundVolume, 1.0f);
		Sound()->SetChannel(CSounds::CHN_GLOBAL, m_GameSoundVolume, 0.0f);
	}

	const float NewMapSoundVolume = g_Config.m_SndMapVolume / 100.0f;
	if(NewMapSoundVolume != m_MapSoundVolume)
	{
		m_MapSoundVolume = NewMapSoundVolume;
		Sound()->SetChannel(CSounds::CHN_MAPSOUND, m_MapSoundVolume, 1.0f);
	}

	const float NewBackgroundMusicVolume = g_Config.m_SndBackgroundMusicVolume / 100.0f;
	if(NewBackgroundMusicVolume != m_BackgroundMusicVolume)
	{
		m_BackgroundMusicVolume = NewBackgroundMusicVolume;
		Sound()->SetChannel(CSounds::CHN_MUSIC, m_BackgroundMusicVolume, 1.0f);
	}
}

int CSounds::GetSampleId(int SetId)
{
	if(!g_Config.m_SndEnable || !Sound()->IsSoundEnabled() || m_WaitForSoundJob || SetId < 0 || SetId >= g_pData->m_NumSounds)
		return -1;

	CDataSoundset *pSet = &g_pData->m_aSounds[SetId];
	if(!pSet->m_NumSounds)
		return -1;

	if(pSet->m_NumSounds == 1)
		return pSet->m_aSounds[0].m_Id;

	// return random one
	int Id;
	do
	{
		Id = rand() % pSet->m_NumSounds;
	} while(Id == pSet->m_Last);
	pSet->m_Last = Id;
	return pSet->m_aSounds[Id].m_Id;
}

void CSounds::OnInit()
{
	UpdateChannels();
	ClearQueue();

	// load sounds
	if(g_Config.m_ClThreadsoundloading)
	{
		m_pSoundJob = std::make_shared<CSoundLoading>(GameClient(), false);
		GameClient()->Engine()->AddJob(m_pSoundJob);
		m_WaitForSoundJob = true;
		GameClient()->m_Menus.RenderLoading(Localize("Loading DDNet Client"), Localize("Loading sound files"), 0);
	}
	else
	{
		CSoundLoading(GameClient(), true).Run();
		m_WaitForSoundJob = false;
	}
}

void CSounds::ReloadSounds()
{
	if(m_pSoundJob && m_pSoundJob->State() == IJob::STATE_RUNNING)
		return;
	Sound()->StopAll();
	for(int s = 0; s < g_pData->m_NumSounds; s++)
	{
		for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; i++)
		{
			if(g_pData->m_aSounds[s].m_aSounds[i].m_Id < 0)
				continue;
			Sound()->UnloadSample(g_pData->m_aSounds[s].m_aSounds[i].m_Id);
			g_pData->m_aSounds[s].m_aSounds[i].m_Id = -1;
		}
	}
	CSoundLoading(GameClient(), false).Run();
}

void CSounds::OnReset()
{
	m_NumEchoes = 0;
	m_OcclusionTraces = 0;
	m_EnclosureTime = -1.0f;
	if(Client()->State() >= IClient::STATE_ONLINE)
	{
		Sound()->StopAll();
		ClearQueue();
	}
}

void CSounds::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
		OnReset();
}

void CSounds::OnRender()
{
	m_OcclusionTraces = 0;

	// check for sound initialisation
	if(m_WaitForSoundJob)
	{
		if(m_pSoundJob->State() == IJob::STATE_DONE)
			m_WaitForSoundJob = false;
		else
			return;
	}

	Sound()->SetListenerPosition(GameClient()->m_Camera.m_Center);
	UpdateChannels();
	UpdateEchoes();

	// play sound from queue
	if(m_QueuePos > 0)
	{
		int64_t Now = time();
		if(m_QueueWaitTime <= Now)
		{
			Play(m_aQueue[0].m_Channel, m_aQueue[0].m_SetId, 1.0f);
			m_QueueWaitTime = Now + time_freq() * 3 / 10; // wait 300ms before playing the next one
			if(--m_QueuePos > 0)
				mem_move(m_aQueue, m_aQueue + 1, m_QueuePos * sizeof(CQueueEntry));
		}
	}
}

void CSounds::ClearQueue()
{
	mem_zero(m_aQueue, sizeof(m_aQueue));
	m_QueuePos = 0;
	m_QueueWaitTime = time();
}

void CSounds::Enqueue(int Channel, int SetId)
{
	if(GameClient()->m_SuppressEvents)
		return;
	if(m_QueuePos >= QUEUE_SIZE)
		return;
	if(Channel != CHN_MUSIC && g_Config.m_ClEditor)
		return;

	m_aQueue[m_QueuePos].m_Channel = Channel;
	m_aQueue[m_QueuePos++].m_SetId = SetId;
}

void CSounds::PlayAndRecord(int Channel, int SetId, float Volume, vec2 Position)
{
	// TODO: Volume and position are currently not recorded for sounds played with this function
	// TODO: This also causes desync sounds during demo playback of demos recorded on high ping servers:
	//       https://github.com/ddnet/ddnet/issues/1282
	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundId = SetId;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_NOSEND | MSGFLAG_RECORD);

	PlayAt(Channel, SetId, Volume, Position);
}

void CSounds::Play(int Channel, int SetId, float Volume)
{
	PlaySample(Channel, GetSampleId(SetId), 0, Volume);
}

void CSounds::PlayAt(int Channel, int SetId, float Volume, vec2 Position)
{
	PlaySampleAt(Channel, GetSampleId(SetId), 0, Volume, Position);
}

void CSounds::Stop(int SetId)
{
	if(m_WaitForSoundJob || SetId < 0 || SetId >= g_pData->m_NumSounds)
		return;

	const CDataSoundset *pSet = &g_pData->m_aSounds[SetId];
	for(int i = 0; i < pSet->m_NumSounds; i++)
		if(pSet->m_aSounds[i].m_Id != -1)
			Sound()->Stop(pSet->m_aSounds[i].m_Id);
}

bool CSounds::IsPlaying(int SetId)
{
	if(m_WaitForSoundJob || SetId < 0 || SetId >= g_pData->m_NumSounds)
		return false;

	const CDataSoundset *pSet = &g_pData->m_aSounds[SetId];
	for(int i = 0; i < pSet->m_NumSounds; i++)
		if(pSet->m_aSounds[i].m_Id != -1 && Sound()->IsPlaying(pSet->m_aSounds[i].m_Id))
			return true;
	return false;
}

ISound::CVoiceHandle CSounds::PlaySample(int Channel, int SampleId, int Flags, float Volume)
{
	if(GameClient()->m_SuppressEvents || (Channel == CHN_MUSIC && !g_Config.m_SndMusic) || SampleId == -1)
		return ISound::CVoiceHandle();

	if(Channel == CHN_MUSIC)
		Flags |= ISound::FLAG_LOOP;

	return Sound()->Play(Channel, SampleId, Flags, Volume);
}

ISound::CVoiceHandle CSounds::PlaySampleAt(int Channel, int SampleId, int Flags, float Volume, vec2 Position)
{
	if(GameClient()->m_SuppressEvents || (Channel == CHN_MUSIC && !g_Config.m_SndMusic) || SampleId == -1)
		return ISound::CVoiceHandle();

	if(Channel == CHN_MUSIC)
		Flags |= ISound::FLAG_LOOP;

	float PlayVolume = Volume;
	if(g_Config.m_TcRaycastSounds && Channel == CHN_WORLD)
	{
		PlayVolume = ApplyOcclusion(SampleId, Volume, Position);
		if(PlayVolume <= 0.0f)
			return ISound::CVoiceHandle();
	}

	return Sound()->PlayAt(Channel, SampleId, Flags, PlayVolume, Position);
}

float CSounds::ApplyOcclusion(int SampleId, float Volume, vec2 Position)
{
	const vec2 ListenerPos = GameClient()->m_Camera.m_Center;
	const float Distance = distance(ListenerPos, Position);
	if(Distance > MAX_SOUND_DISTANCE)
		return 0.0f;

	const bool OcclusionEnabled = g_Config.m_TcRaycastSoundsOcclusion < 100;
	const bool TeammatesEnabled = g_Config.m_TcRaycastSoundsTeammates;
	int Walls = 0;
	if(Distance <= OCCLUSION_MAX_DISTANCE && m_OcclusionTraces < MAX_OCCLUSION_TRACES_PER_FRAME && (OcclusionEnabled || TeammatesEnabled))
	{
		m_OcclusionTraces++;
		Walls = CountWalls(ListenerPos, Position);
	}

	if(Walls > 0)
	{
		if(TeammatesEnabled && IsBlockedTeammateSound(Position))
			return 0.0f;

		Volume *= std::pow(g_Config.m_TcRaycastSoundsOcclusion / 100.0f, (float)Walls);
		if(Volume < 0.01f)
			return 0.0f;
	}

	if(Volume > 0.1f)
		ScheduleEchoes(SampleId, Volume, Position, Distance);
	return Volume;
}

int CSounds::CountWalls(vec2 From, vec2 To) const
{
	const float Total = distance(From, To);
	if(Total < 1.0f || Collision()->CheckPoint(From))
		return 0;

	const vec2 Dir = (To - From) / Total;
	int Walls = 0;
	vec2 Start = From;
	for(int i = 0; i < MAX_OCCLUSION_WALLS; i++)
	{
		vec2 Hit;
		if(Collision()->IntersectLine(Start, To, &Hit, nullptr) == 0)
			break;

		Walls++;

		vec2 P = Hit;
		int Step = 0;
		while(Collision()->CheckPoint(P) && Step++ < 128 && distance(P, To) > 1.0f)
			P += Dir * 8.0f;

		if(distance(P, To) < 2.0f)
			break;

		Start = P;
	}
	return Walls;
}

float CSounds::GetEnclosure(vec2 Pos)
{
	const float Now = Client()->LocalTime();
	if(m_EnclosureTime >= 0.0f && Now - m_EnclosureTime < 1.0f)
		return m_Enclosure;

	int Hits = 0;
	for(int i = 0; i < 8; i++)
	{
		const vec2 Dir = direction(i * (2.0f * pi / 8.0f));
		if(Collision()->IntersectLine(Pos, Pos + Dir * ENCLOSURE_RADIUS, nullptr, nullptr) != 0)
			Hits++;
	}

	m_Enclosure = Hits / 8.0f;
	m_EnclosureTime = Now;
	return m_Enclosure;
}

int CSounds::FindSoundSource(vec2 Pos) const
{
	int Best = -1;
	float BestDist = SOUND_SOURCE_RADIUS;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		const float Dist = distance(Pos, GameClient()->m_aClients[i].m_RenderPos);
		if(Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

bool CSounds::IsBlockedTeammateSound(vec2 Pos) const
{
	const int ClientId = FindSoundSource(Pos);
	if(ClientId < 0 || ClientId == GameClient()->m_aLocalIds[0] || ClientId == GameClient()->m_aLocalIds[1])
		return false;
	return !GameClient()->IsOtherTeam(ClientId);
}

void CSounds::ScheduleEchoes(int SampleId, float Volume, vec2 Position, float Distance)
{
	if(!g_Config.m_TcRaycastSoundsEcho || SampleId == -1 || Volume < 0.05f)
		return;
	if(Distance > ECHO_MAX_DISTANCE || m_NumEchoes + MAX_ECHOES_PER_SOUND > MAX_ECHOES)
		return;

	const float Enclosure = GetEnclosure(GameClient()->m_Camera.m_Center);
	if(Enclosure < 0.30f)
		return;

	const int Count = Enclosure > 0.7f ? MAX_ECHOES_PER_SOUND : 2;
	if(m_NumEchoes + Count > MAX_ECHOES)
		return;

	const float Now = Client()->LocalTime();
	const float BaseVolume = Volume * (g_Config.m_TcRaycastSoundsEchoVolume / 100.0f);
	for(int i = 0; i < Count; i++)
	{
		CEcho &Echo = m_aEchoes[m_NumEchoes++];
		Echo.m_Time = Now + 0.085f + 0.105f * i;
		Echo.m_SampleId = SampleId;
		Echo.m_Volume = BaseVolume * std::pow(0.55f, (float)i);
		Echo.m_Position = Position;
	}
}

void CSounds::UpdateEchoes()
{
	const float Now = Client()->LocalTime();
	for(int i = 0; i < m_NumEchoes;)
	{
		CEcho &Echo = m_aEchoes[i];
		if(Echo.m_Time <= Now)
		{
			Sound()->PlayAt(CHN_WORLD, Echo.m_SampleId, ISound::FLAG_POS, Echo.m_Volume, Echo.m_Position);
			Echo = m_aEchoes[--m_NumEchoes];
			continue;
		}
		i++;
	}
}
