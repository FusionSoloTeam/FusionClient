#include "smtc.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>
#include <base/types.h>
#include <base/thread.h>

#include <cstring>

#if defined(CONF_FAMILY_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>

typedef void *HSTRING_;
typedef HRESULT(WINAPI *PFN_RoInitialize)(int);
typedef HRESULT(WINAPI *PFN_RoUninitialize)(void);
typedef HRESULT(WINAPI *PFN_RoGetActivationFactory)(HSTRING_, REFIID, void **);
typedef HRESULT(WINAPI *PFN_WindowsCreateString)(const wchar_t *, UINT32, HSTRING_ *);
typedef PCWSTR(WINAPI *PFN_GetStringRawBuffer)(HSTRING_, UINT32 *);
typedef HRESULT(WINAPI *PFN_WindowsDeleteString)(HSTRING_);

static PFN_RoInitialize s_pfnRoInitialize;
static PFN_RoUninitialize s_pfnRoUninitialize;
static PFN_RoGetActivationFactory s_pfnRoGetActivationFactory;
static PFN_WindowsCreateString s_pfnWindowsCreateString;
static PFN_GetStringRawBuffer s_pfnGetStringRawBuffer;
static PFN_WindowsDeleteString s_pfnWindowsDeleteString;

static const GUID SMTC_IID_IInspectable = {0xaf86e2e0, 0xb12d, 0x4c6a, {0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90}};
static const GUID SMTC_GUID_IUnknown = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID SMTC_GUID_IActivationFactory = {0x00000035, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID SMTC_GUID_IAgileObject = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};

enum
{
	SMTC_ASYNC_RESULTS_SLOT = 8,
	SMTC_TICKS_PER_SECOND = 10000000,
	SMTC_PENDING = 0x8000000E,
	SMTC_E_PENDING = 0x8000000A,
};

template <typename Fn>
static Fn SmtcVt(void *pObj, int Index)
{
	return (Fn)(*(void ***)pObj)[Index];
}

static void SmtcRelease(void *pObj)
{
	if(pObj != nullptr)
		SmtcVt<ULONG(WINAPI *)(void *)>(pObj, 2)(pObj);
}

static void SmtcToUtf8(PCWSTR pWide, char *pOut, int OutSize)
{
	pOut[0] = '\0';
	if(pWide == nullptr)
		return;
	if(WideCharToMultiByte(CP_UTF8, 0, pWide, -1, pOut, OutSize, nullptr, nullptr) <= 0)
		pOut[0] = '\0';
}

static void SmtcReadString(void *pObj, int Slot, char *pOut, int OutSize)
{
	pOut[0] = '\0';
	HSTRING_ hs = nullptr;
	HRESULT hr = SmtcVt<HRESULT(WINAPI *)(void *, HSTRING_ *)>(pObj, Slot)(pObj, &hs);
	if(SUCCEEDED(hr) && hs != nullptr)
	{
		SmtcToUtf8(s_pfnGetStringRawBuffer(hs, nullptr), pOut, OutSize);
		s_pfnWindowsDeleteString(hs);
	}
}

static bool SmtcAwait(void *pAsync, void **ppResult)
{
	if(pAsync == nullptr)
		return false;
	HRESULT hr = (HRESULT)SMTC_PENDING;
	for(int i = 0; i < 100; i++)
	{
		hr = SmtcVt<HRESULT(WINAPI *)(void *, void **)>(pAsync, SMTC_ASYNC_RESULTS_SLOT)(pAsync, ppResult);
		if(hr != (HRESULT)SMTC_PENDING && hr != (HRESULT)SMTC_E_PENDING)
			break;
		Sleep(5);
	}
	return SUCCEEDED(hr) && *ppResult != nullptr;
}

static bool SmtcLoadEntryPoints()
{
	HMODULE hCombase = LoadLibraryA("combase.dll");
	if(hCombase == nullptr)
		return false;
	s_pfnRoInitialize = (PFN_RoInitialize)GetProcAddress(hCombase, "RoInitialize");
	s_pfnRoUninitialize = (PFN_RoUninitialize)GetProcAddress(hCombase, "RoUninitialize");
	s_pfnRoGetActivationFactory = (PFN_RoGetActivationFactory)GetProcAddress(hCombase, "RoGetActivationFactory");
	s_pfnWindowsCreateString = (PFN_WindowsCreateString)GetProcAddress(hCombase, "WindowsCreateString");
	s_pfnGetStringRawBuffer = (PFN_GetStringRawBuffer)GetProcAddress(hCombase, "WindowsGetStringRawBuffer");
	s_pfnWindowsDeleteString = (PFN_WindowsDeleteString)GetProcAddress(hCombase, "WindowsDeleteString");
	return s_pfnRoInitialize && s_pfnRoUninitialize && s_pfnRoGetActivationFactory && s_pfnWindowsCreateString &&
	       s_pfnGetStringRawBuffer && s_pfnWindowsDeleteString;
}

static void *SmtcCreateManager()
{
	const wchar_t *pClassName = L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";
	HSTRING_ hClass = nullptr;
	if(FAILED(s_pfnWindowsCreateString(pClassName, (UINT32)wcslen(pClassName), &hClass)) || hClass == nullptr)
		return nullptr;

	void *pFactory = nullptr;
	HRESULT hr = s_pfnRoGetActivationFactory(hClass, SMTC_IID_IInspectable, &pFactory);
	s_pfnWindowsDeleteString(hClass);
	if(FAILED(hr) || pFactory == nullptr)
		return nullptr;

	ULONG NumIids = 0;
	GUID *pIids = nullptr;
	GUID StaticsIid = {};
	bool Found = false;
	if(SUCCEEDED(SmtcVt<HRESULT(WINAPI *)(void *, ULONG *, GUID **)>(pFactory, 3)(pFactory, &NumIids, &pIids)) && pIids != nullptr)
	{
		for(ULONG i = 0; i < NumIids; i++)
		{
			if(memcmp(&pIids[i], &SMTC_IID_IInspectable, sizeof(GUID)) == 0 ||
				memcmp(&pIids[i], &SMTC_GUID_IUnknown, sizeof(GUID)) == 0 ||
				memcmp(&pIids[i], &SMTC_GUID_IActivationFactory, sizeof(GUID)) == 0 ||
				memcmp(&pIids[i], &SMTC_GUID_IAgileObject, sizeof(GUID)) == 0)
				continue;
			StaticsIid = pIids[i];
			Found = true;
		}
		CoTaskMemFree(pIids);
	}
	if(!Found)
	{
		SmtcRelease(pFactory);
		return nullptr;
	}

	void *pStatics = nullptr;
	hr = SmtcVt<HRESULT(WINAPI *)(void *, REFIID, void **)>(pFactory, 0)(pFactory, StaticsIid, &pStatics);
	SmtcRelease(pFactory);
	if(FAILED(hr) || pStatics == nullptr)
		return nullptr;

	void *pAsync = nullptr;
	hr = SmtcVt<HRESULT(WINAPI *)(void *, void **)>(pStatics, 6)(pStatics, &pAsync);
	SmtcRelease(pStatics);
	if(FAILED(hr))
		return nullptr;

	void *pManager = nullptr;
	if(!SmtcAwait(pAsync, &pManager))
	{
		SmtcRelease(pAsync);
		return nullptr;
	}
	SmtcRelease(pAsync);
	return pManager;
}
#endif

CSmtc::~CSmtc()
{
	Stop();
}

void CSmtc::Start()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!m_WantRunning.exchange(true) && m_pThread == nullptr)
		m_pThread = thread_init(ThreadEntry, this, "SMTC");
#endif
}

void CSmtc::Stop()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(m_WantRunning.exchange(false) && m_pThread != nullptr)
	{
		thread_wait(m_pThread);
		m_pThread = nullptr;
	}
#endif
}

void CSmtc::SetStatus(const char *pStatus)
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	m_Status = pStatus;
}

bool CSmtc::HasTrack() const
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return m_HasTrack;
}

void CSmtc::Snapshot(char *pTitle, int TitleSize, char *pArtist, int ArtistSize, char *pAlbum, int AlbumSize, float *pDuration, float *pPosition) const
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	str_copy(pTitle, m_Title.c_str(), TitleSize);
	str_copy(pArtist, m_Artist.c_str(), ArtistSize);
	str_copy(pAlbum, m_Album.c_str(), AlbumSize);
	if(pDuration != nullptr)
		*pDuration = m_Duration;
	if(pPosition != nullptr)
		*pPosition = m_Position;
}

void CSmtc::AppId(char *pOut, int OutSize) const
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	str_copy(pOut, m_AppId.c_str(), OutSize);
}

void CSmtc::ThreadEntry(void *pUser)
{
	static_cast<CSmtc *>(pUser)->ThreadRun();
}

void CSmtc::ThreadRun()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!SmtcLoadEntryPoints())
	{
		SetStatus("combase.dll unavailable");
		return;
	}
	if(FAILED(s_pfnRoInitialize(1)))
	{
		SetStatus("RoInitialize failed");
		return;
	}

	void *pManager = SmtcCreateManager();
	if(pManager == nullptr)
	{
		SetStatus("no media session manager");
		s_pfnRoUninitialize();
		return;
	}
	SetStatus("media session ready");

	char aTitle[256];
	char aArtist[256];
	char aAlbum[256];
	char aAppId[128];
	while(m_WantRunning.load(std::memory_order_relaxed))
	{
		aTitle[0] = '\0';
		aArtist[0] = '\0';
		aAlbum[0] = '\0';
		aAppId[0] = '\0';
		float Duration = 0.0f;
		float Position = -1.0f;

		void *pSession = nullptr;
		HRESULT hr = SmtcVt<HRESULT(WINAPI *)(void *, void **)>(pManager, 6)(pManager, &pSession);
		if(SUCCEEDED(hr) && pSession != nullptr)
		{
			SmtcReadString(pSession, 6, aAppId, sizeof(aAppId));
			void *pAsync = nullptr;
			if(SUCCEEDED(SmtcVt<HRESULT(WINAPI *)(void *, void **)>(pSession, 7)(pSession, &pAsync)) && pAsync != nullptr)
			{
				void *pProps = nullptr;
				if(SmtcAwait(pAsync, &pProps))
				{
					SmtcReadString(pProps, 6, aTitle, sizeof(aTitle));
					SmtcReadString(pProps, 8, aArtist, sizeof(aArtist));
					SmtcReadString(pProps, 10, aAlbum, sizeof(aAlbum));
					SmtcRelease(pProps);
				}
				SmtcRelease(pAsync);
			}

			void *pTimeline = nullptr;
			if(SUCCEEDED(SmtcVt<HRESULT(WINAPI *)(void *, void **)>(pSession, 8)(pSession, &pTimeline)) && pTimeline != nullptr)
			{
				long long Start = 0;
				long long End = 0;
				long long TrackPosition = 0;
				SmtcVt<HRESULT(WINAPI *)(void *, long long *)>(pTimeline, 6)(pTimeline, &Start);
				SmtcVt<HRESULT(WINAPI *)(void *, long long *)>(pTimeline, 7)(pTimeline, &End);
				SmtcVt<HRESULT(WINAPI *)(void *, long long *)>(pTimeline, 10)(pTimeline, &TrackPosition);
				Duration = (float)(End / (double)SMTC_TICKS_PER_SECOND);
				if(TrackPosition > 0)
					Position = (float)(TrackPosition / (double)SMTC_TICKS_PER_SECOND);
				SmtcRelease(pTimeline);
			}
			SmtcRelease(pSession);
		}

		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_Title = aTitle;
			m_Artist = aArtist;
			m_Album = aAlbum;
			m_AppId = aAppId;
			m_Duration = Duration;
			m_Position = Position;
			m_HasTrack = aTitle[0] != '\0';
		}

		for(int i = 0; i < 40 && m_WantRunning.load(std::memory_order_relaxed); i++)
			Sleep(10);
	}

	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_HasTrack = false;
		m_Title.clear();
		m_Artist.clear();
		m_Album.clear();
		m_AppId.clear();
	}
	SmtcRelease(pManager);
	s_pfnRoUninitialize();
#endif
}
