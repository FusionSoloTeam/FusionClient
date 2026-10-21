#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_SYSTEM_AUDIO_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_SYSTEM_AUDIO_H

#include <atomic>

class CSystemAudio
{
public:
	~CSystemAudio();
	void SetEnabled(bool Enabled);
	float Level() const { return m_Level.load(std::memory_order_relaxed); }
	float Beat() const { return m_Beat.load(std::memory_order_relaxed); }
	float BeatInterval() const { return m_BeatInterval.load(std::memory_order_relaxed); }

private:
	std::atomic<bool> m_WantEnabled{false};
	std::atomic<float> m_Level{0.0f};
	std::atomic<float> m_Beat{0.0f};
	std::atomic<float> m_BeatInterval{0.0f};
	void *m_pThread = nullptr;

	static void ThreadEntry(void *pUser);
	void ThreadRun();
};

#endif
