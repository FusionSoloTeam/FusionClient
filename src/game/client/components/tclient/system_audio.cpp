#include "system_audio.h"

#include <base/str.h>
#include <base/system.h>
#include <base/thread.h>

#include <engine/shared/config.h>

#if defined(CONF_FAMILY_WINDOWS)

#include <windows.h>

#define NOBITMAP
#define IStorage IStorage_WindowsCOM
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <audiopolicy.h>
#include <endpointvolume.h>
#undef IStorage

#include <tlhelp32.h>

#include <cmath>
#include <cstring>

typedef enum AUDIOCLIENT_ACTIVATION_TYPE
{
	AUDIOCLIENT_ACTIVATION_TYPE_DEFAULT = 0,
	AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK = 1
} AUDIOCLIENT_ACTIVATION_TYPE;

typedef enum PROCESS_LOOPBACK_MODE
{
	PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE = 0,
	PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE = 1
} PROCESS_LOOPBACK_MODE;

typedef struct AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS
{
	DWORD TargetProcessId;
	PROCESS_LOOPBACK_MODE ProcessLoopbackMode;
} AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS;

typedef struct AUDIOCLIENT_ACTIVATION_PARAMS
{
	AUDIOCLIENT_ACTIVATION_TYPE ActivationType;
	union
	{
		AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS ProcessLoopbackParams;
	};
} AUDIOCLIENT_ACTIVATION_PARAMS;

namespace
{
	const GUID s_MMDeviceEnumerator_Clsid = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
	const GUID s_IMMDeviceEnumerator_Iid = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
	const GUID s_IAudioClient_Iid = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
	const GUID s_IAudioCaptureClient_Iid = {0xC8ADBD64, 0xE71E, 0x48A0, {0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17}};
	const GUID s_IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
	const GUID s_IID_CompletionHandler = {0x41D949AB, 0x9862, 0x444A, {0x80, 0xF6, 0xC8, 0x61, 0x52, 0xDF, 0xC3, 0x24}};
	const GUID s_IID_IMarshal = {0x00000003, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
	const GUID s_IID_IAgileObject = {0x94EA2B94, 0xE9CC, 0x49E0, {0xC0, 0xFF, 0xEE, 0x64, 0xCA, 0x8F, 0x5B, 0x90}};

	const int LIST_MAX = 6;

	class CActivateHandler : public IActivateAudioInterfaceCompletionHandler
	{
	public:
		HANDLE m_hEvent = nullptr;
		IAudioClient *m_pClient = nullptr;

		void Init()
		{
			CoCreateFreeThreadedMarshaler(static_cast<IActivateAudioInterfaceCompletionHandler *>(this), &m_pFTM);
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override
		{
			if(IsEqualIID(riid, s_IID_IUnknown) || IsEqualIID(riid, s_IID_CompletionHandler))
			{
				*ppv = static_cast<IActivateAudioInterfaceCompletionHandler *>(this);
				AddRef();
				return S_OK;
			}
			if((IsEqualIID(riid, s_IID_IMarshal) || IsEqualIID(riid, s_IID_IAgileObject)) && m_pFTM != nullptr)
				return m_pFTM->QueryInterface(riid, ppv);
			*ppv = nullptr;
			return E_NOINTERFACE;
		}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return InterlockedIncrement(&m_Ref);
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG Ref = InterlockedDecrement(&m_Ref);
			if(Ref == 0)
				delete this;
			return Ref;
		}

		HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation *pOperation) override
		{
			IUnknown *pUnknown = nullptr;
			HRESULT Result = E_FAIL;
			const HRESULT GetResult = pOperation->GetActivateResult(&Result, &pUnknown);
			if(SUCCEEDED(GetResult) && SUCCEEDED(Result) && pUnknown != nullptr)
				m_pClient = (IAudioClient *)pUnknown;
			if(m_hEvent != nullptr)
				SetEvent(m_hEvent);
			return S_OK;
		}

	private:
		virtual ~CActivateHandler()
		{
			if(m_pClient != nullptr)
				m_pClient->Release();
			if(m_pFTM != nullptr)
				m_pFTM->Release();
			if(m_hEvent != nullptr)
				CloseHandle(m_hEvent);
		}
		IUnknown *m_pFTM = nullptr;
		LONG m_Ref = 1;
	};

	struct CStream
	{
		IAudioClient *m_pClient = nullptr;
		IAudioCaptureClient *m_pCapture = nullptr;
		bool m_Float32 = false;
		int m_Channels = 2;
		int m_SampleRate = 48000;
		float m_BassCoef = 0.0f;
		float m_BassState = 0.0f;
		float m_Alpha = 0.0f;
		float m_Env = 0.0f;
		DWORD m_Pid = 0;
	};

	void SetupStream(CStream &S)
	{
		S.m_BassCoef = 1.0f - std::exp(-2.0f * 3.14159265358979323846f * 160.0f / (float)S.m_SampleRate);
		S.m_Alpha = 1.0f - std::exp(-1.0f / (0.050f * (float)S.m_SampleRate));
		S.m_BassState = 0.0f;
		S.m_Env = 0.0f;
	}

	void StopStream(CStream &S)
	{
		if(S.m_pClient != nullptr)
			S.m_pClient->Stop();
		if(S.m_pCapture != nullptr)
			S.m_pCapture->Release();
		if(S.m_pClient != nullptr)
			S.m_pClient->Release();
		S.m_pClient = nullptr;
		S.m_pCapture = nullptr;
		S.m_Pid = 0;
		S.m_BassState = 0.0f;
		S.m_Env = 0.0f;
	}

	bool ActivateProcessClient(DWORD Pid, bool ExcludeTree, IAudioClient **ppClient)
	{
		CActivateHandler *pHandler = new CActivateHandler();
		pHandler->m_hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		pHandler->Init();

		AUDIOCLIENT_ACTIVATION_PARAMS Params;
		memset(&Params, 0, sizeof(Params));
		Params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
		Params.ProcessLoopbackParams.TargetProcessId = Pid;
		Params.ProcessLoopbackParams.ProcessLoopbackMode = ExcludeTree ? PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE : PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

		PROPVARIANT Var;
		memset(&Var, 0, sizeof(Var));
		Var.vt = VT_BLOB;
		Var.blob.cbSize = sizeof(Params);
		Var.blob.pBlobData = (BYTE *)&Params;

		IActivateAudioInterfaceAsyncOperation *pOperation = nullptr;
		const HRESULT Result = ActivateAudioInterfaceAsync(L"VAD\\Process_Loopback", s_IAudioClient_Iid, &Var, static_cast<IActivateAudioInterfaceCompletionHandler *>(pHandler), &pOperation);
		if(FAILED(Result))
		{
			pHandler->Release();
			return false;
		}
		if(pOperation != nullptr)
			pOperation->Release();
		const DWORD Wait = WaitForSingleObject(pHandler->m_hEvent, 5000);
		if(Wait == WAIT_OBJECT_0 && pHandler->m_pClient != nullptr)
		{
			*ppClient = pHandler->m_pClient;
			pHandler->m_pClient = nullptr;
			pHandler->Release();
			return true;
		}
		pHandler->Release();
		return false;
	}

	bool StartProcessStream(CStream &S, DWORD Pid, bool ExcludeTree)
	{
		IAudioClient *pClient = nullptr;
		if(!ActivateProcessClient(Pid, ExcludeTree, &pClient))
			return false;
		WAVEFORMATEX Format;
		memset(&Format, 0, sizeof(Format));
		Format.wFormatTag = WAVE_FORMAT_PCM;
		Format.nChannels = 2;
		Format.nSamplesPerSec = 48000;
		Format.wBitsPerSample = 16;
		Format.nBlockAlign = (WORD)(Format.nChannels * Format.wBitsPerSample / 8);
		Format.nAvgBytesPerSec = Format.nSamplesPerSec * Format.nBlockAlign;
		HRESULT Result = pClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 2000000, 0, &Format, nullptr);
		if(FAILED(Result))
		{
			pClient->Release();
			return false;
		}
		IAudioCaptureClient *pCapture = nullptr;
		Result = pClient->GetService(s_IAudioCaptureClient_Iid, (void **)&pCapture);
		if(FAILED(Result) || pCapture == nullptr)
		{
			pClient->Release();
			return false;
		}
		Result = pClient->Start();
		if(FAILED(Result))
		{
			pCapture->Release();
			pClient->Release();
			return false;
		}
		S.m_pClient = pClient;
		S.m_pCapture = pCapture;
		S.m_Float32 = false;
		S.m_Channels = 2;
		S.m_SampleRate = 48000;
		S.m_Pid = Pid;
		SetupStream(S);
		return true;
	}

	bool StartDeviceStream(CStream &S, IMMDevice *pDevice)
	{
		IAudioClient *pClient = nullptr;
		HRESULT Result = pDevice->Activate(s_IAudioClient_Iid, CLSCTX_ALL, nullptr, (void **)&pClient);
		if(FAILED(Result) || pClient == nullptr)
			return false;
		WAVEFORMATEX *pFormat = nullptr;
		Result = pClient->GetMixFormat(&pFormat);
		if(FAILED(Result) || pFormat == nullptr)
		{
			pClient->Release();
			return false;
		}
		bool Float32 = true;
		bool Supported = true;
		if(pFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
			Float32 = true;
		else if(pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pFormat->wBitsPerSample == 32)
			Float32 = true;
		else if(pFormat->wBitsPerSample == 16)
			Float32 = false;
		else
			Supported = false;
		if(!Supported)
		{
			CoTaskMemFree(pFormat);
			pClient->Release();
			return false;
		}
		Result = pClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 2000000, 0, pFormat, nullptr);
		if(FAILED(Result))
		{
			CoTaskMemFree(pFormat);
			pClient->Release();
			return false;
		}
		IAudioCaptureClient *pCapture = nullptr;
		Result = pClient->GetService(s_IAudioCaptureClient_Iid, (void **)&pCapture);
		if(FAILED(Result) || pCapture == nullptr)
		{
			CoTaskMemFree(pFormat);
			pClient->Release();
			return false;
		}
		Result = pClient->Start();
		if(FAILED(Result))
		{
			pCapture->Release();
			CoTaskMemFree(pFormat);
			pClient->Release();
			return false;
		}
		S.m_pClient = pClient;
		S.m_pCapture = pCapture;
		S.m_Float32 = Float32;
		S.m_Channels = pFormat->nChannels > 0 ? pFormat->nChannels : 2;
		S.m_SampleRate = pFormat->nSamplesPerSec > 0 ? (int)pFormat->nSamplesPerSec : 48000;
		SetupStream(S);
		CoTaskMemFree(pFormat);
		return true;
	}

	void DrainStream(CStream &S)
	{
		if(S.m_pCapture == nullptr)
			return;
		UINT32 PacketFrames = 0;
		if(S.m_pCapture->GetNextPacketSize(&PacketFrames) != S_OK)
			return;
		while(PacketFrames > 0)
		{
			BYTE *pData = nullptr;
			UINT32 NumFrames = 0;
			DWORD Flags = 0;
			if(S.m_pCapture->GetBuffer(&pData, &NumFrames, &Flags, nullptr, nullptr) != S_OK)
				break;
			const bool HasData = !(Flags & AUDCLNT_BUFFERFLAGS_SILENT) && pData != nullptr;
			for(UINT32 Frame = 0; Frame < NumFrames; Frame++)
			{
				float Sample = 0.0f;
				if(HasData)
				{
					if(S.m_Float32)
					{
						const float *pFloats = (const float *)pData;
						for(int Channel = 0; Channel < S.m_Channels; Channel++)
							Sample += pFloats[Frame * S.m_Channels + Channel];
					}
					else
					{
						const int16_t *pShorts = (const int16_t *)pData;
						for(int Channel = 0; Channel < S.m_Channels; Channel++)
							Sample += pShorts[Frame * S.m_Channels + Channel] / 32768.0f;
					}
					Sample /= (float)S.m_Channels;
				}
				S.m_BassState += (Sample - S.m_BassState) * S.m_BassCoef;
				S.m_Env += (S.m_BassState * S.m_BassState - S.m_Env) * S.m_Alpha;
			}
			S.m_pCapture->ReleaseBuffer(NumFrames);
			if(S.m_pCapture->GetNextPacketSize(&PacketFrames) != S_OK)
				break;
		}
	}

	DWORD FindProcessForToken(const char *pToken)
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if(hSnapshot == INVALID_HANDLE_VALUE)
			return 0;
		PROCESSENTRY32 Entry;
		Entry.dwSize = sizeof(Entry);
		DWORD aPids[16];
		DWORD aParents[16];
		int Num = 0;
		if(Process32First(hSnapshot, &Entry))
		{
			do
			{
				char aExe[64];
				int Out = 0;
				for(int k = 0; Entry.szExeFile[k] != 0 && Out < 63; k++)
					aExe[Out++] = Entry.szExeFile[k] > 0 && Entry.szExeFile[k] < 128 ? (char)Entry.szExeFile[k] : '?';
				aExe[Out] = '\0';
				bool Matches = str_comp_nocase(aExe, pToken) == 0;
				if(!Matches)
				{
					char aWithExe[80];
					str_format(aWithExe, sizeof(aWithExe), "%s.exe", pToken);
					Matches = str_comp_nocase(aExe, aWithExe) == 0;
				}
				if(Matches && Num < 16)
				{
					aPids[Num] = Entry.th32ProcessID;
					aParents[Num] = Entry.th32ParentProcessID;
					Num++;
				}
			} while(Process32Next(hSnapshot, &Entry));
		}
		CloseHandle(hSnapshot);
		DWORD Best = 0;
		for(int i = 0; i < Num; i++)
		{
			bool ParentMatches = false;
			for(int k = 0; k < Num; k++)
				if(aParents[i] == aPids[k])
					ParentMatches = true;
			if(!ParentMatches && (Best == 0 || aPids[i] < Best))
				Best = aPids[i];
		}
		if(Best == 0)
			for(int i = 0; i < Num; i++)
				if(Best == 0 || aPids[i] < Best)
					Best = aPids[i];
		return Best;
	}

	void RefreshProcessList(CStream *pList)
	{
		char aTokens[LIST_MAX][64];
		int NumTokens = 0;
		const char *p = g_Config.m_TcOutlinePulsateFilterList;
		while(*p != '\0' && NumTokens < LIST_MAX)
		{
			while(*p == ';' || *p == ',' || *p == ' ')
				p++;
			if(*p == '\0')
				break;
			int Length = 0;
			while(*p != '\0' && *p != ';' && *p != ',' && *p != ' ' && Length < 63)
				aTokens[NumTokens][Length++] = *p++;
			aTokens[NumTokens][Length] = '\0';
			if(Length > 0)
				NumTokens++;
		}
		for(int i = 0; i < LIST_MAX; i++)
		{
			DWORD Pid = i < NumTokens ? FindProcessForToken(aTokens[i]) : 0;
			if(Pid == GetCurrentProcessId())
				Pid = 0;
			if(Pid == pList[i].m_Pid)
				continue;
			StopStream(pList[i]);
			if(Pid != 0)
				StartProcessStream(pList[i], Pid, false);
		}
	}
}

CSystemAudio::~CSystemAudio()
{
	SetEnabled(false);
}

void CSystemAudio::SetEnabled(bool Enabled)
{
	if(Enabled)
	{
		if(!m_WantEnabled.exchange(true) && m_pThread == nullptr)
			m_pThread = thread_init(ThreadEntry, this, "SystemAudio");
	}
	else if(m_WantEnabled.exchange(false) && m_pThread != nullptr)
	{
		thread_wait(m_pThread);
		m_pThread = nullptr;
		m_Level.store(0.0f, std::memory_order_relaxed);
	}
}

void CSystemAudio::ThreadEntry(void *pUser)
{
	static_cast<CSystemAudio *>(pUser)->ThreadRun();
}

void CSystemAudio::ThreadRun()
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	IMMDeviceEnumerator *pEnumerator = nullptr;
	IMMDevice *pDevice = nullptr;

	HRESULT Result = CoCreateInstance(s_MMDeviceEnumerator_Clsid, nullptr, CLSCTX_ALL, s_IMMDeviceEnumerator_Iid, (void **)&pEnumerator);
	if(SUCCEEDED(Result) && pEnumerator != nullptr)
		Result = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

	CStream Device;
	CStream ExcludeOwn;
	if(SUCCEEDED(Result) && pDevice != nullptr)
	{
		StartDeviceStream(Device, pDevice);
		StartProcessStream(ExcludeOwn, GetCurrentProcessId(), true);
	}

	CStream aList[LIST_MAX];
	int64_t LastRefresh = 0;
	int64_t OwnRetryAt = 0;

	if(Device.m_pCapture != nullptr)
	{
		float GateState = 0.0f;

		while(m_WantEnabled.load(std::memory_order_relaxed))
		{
			DrainStream(Device);
			DrainStream(ExcludeOwn);
			for(int i = 0; i < LIST_MAX; i++)
				DrainStream(aList[i]);

			float Level;
			if(g_Config.m_TcOutlinePulsateFilter)
			{
				const int64_t Now = time_get();
				if(Now >= LastRefresh + 2000)
				{
					LastRefresh = Now;
					RefreshProcessList(aList);
				}
				if(ExcludeOwn.m_pCapture == nullptr && Now >= OwnRetryAt)
				{
					OwnRetryAt = Now + 5000;
					StartProcessStream(ExcludeOwn, GetCurrentProcessId(), true);
				}
				if(ExcludeOwn.m_pCapture != nullptr)
				{
					const float Primary = std::sqrt(ExcludeOwn.m_Env);
					float SubSq = 0.0f;
					for(int i = 0; i < LIST_MAX; i++)
					{
						if(aList[i].m_pCapture == nullptr)
							continue;
						const float SubLevel = std::sqrt(aList[i].m_Env) * 1.08f;
						SubSq += SubLevel * SubLevel;
					}
					float ResidualSq = Primary * Primary - SubSq;
					float Residual = ResidualSq > 0.0f ? std::sqrt(ResidualSq) : 0.0f;
					const float Sub = std::sqrt(SubSq);
					if(Sub > 0.0f && Residual < 0.30f * Sub)
						Residual = 0.0f;
					Level = std::min(1.0f, Residual * 16.0f);
				}
				else
				{
					Level = std::min(1.0f, std::sqrt(Device.m_Env) * 16.0f);
				}
			}
			else
			{
				Level = std::min(1.0f, std::sqrt(Device.m_Env) * 16.0f);
			}

			if(Level > GateState)
				GateState += (Level - GateState) * 0.5f;
			else
				GateState += (Level - GateState) * 0.1f;
			m_Level.store(GateState, std::memory_order_relaxed);

			Sleep(5);
		}
	}

	StopStream(Device);
	StopStream(ExcludeOwn);
	for(int i = 0; i < LIST_MAX; i++)
		StopStream(aList[i]);
	if(pDevice != nullptr)
		pDevice->Release();
	if(pEnumerator != nullptr)
		pEnumerator->Release();
	CoUninitialize();

	m_Level.store(0.0f, std::memory_order_relaxed);
}

#else

CSystemAudio::~CSystemAudio() {}

void CSystemAudio::SetEnabled(bool Enabled)
{
	(void)Enabled;
}

void CSystemAudio::ThreadEntry(void *pUser)
{
	(void)pUser;
}

void CSystemAudio::ThreadRun() {}

#endif
