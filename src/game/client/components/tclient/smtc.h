#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_SMTC_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_SMTC_H

#include <atomic>
#include <mutex>
#include <string>

class CSmtc
{
public:
	~CSmtc();

	void Start();
	void Stop();
	bool IsRunning() const { return m_WantRunning.load(std::memory_order_relaxed); }

	bool HasTrack() const;
	void Snapshot(char *pTitle, int TitleSize, char *pArtist, int ArtistSize, char *pAlbum, int AlbumSize, float *pDuration, float *pPosition) const;
	void AppId(char *pOut, int OutSize) const;
	void SetStatus(const char *pStatus);

private:
	void *m_pThread = nullptr;
	std::atomic<bool> m_WantRunning{false};
	mutable std::mutex m_Mutex;
	std::string m_Title;
	std::string m_Artist;
	std::string m_Album;
	std::string m_AppId;
	std::string m_Status;
	float m_Duration = 0.0f;
	float m_Position = -1.0f;
	bool m_HasTrack = false;

	static void ThreadEntry(void *pUser);
	void ThreadRun();
};

#endif
