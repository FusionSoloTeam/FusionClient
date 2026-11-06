#include "video_loader.h"

#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <vector>

#if defined(CONF_VIDEORECORDER)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}
#endif

#if defined(CONF_FAMILY_WINDOWS)
#ifdef NOGDI
#undef NOGDI
#endif
#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <string>
#endif

namespace {

constexpr const char *const s_apVideoExtensions[] = {".mp4", ".m4v", ".mov", ".webm", ".mkv", ".avi"};

constexpr size_t VIDEO_MAX_FRAMES = 512;
constexpr size_t VIDEO_MAX_TOTAL_PIXELS = 192ull * 1024 * 1024;
constexpr int VIDEO_MIN_FRAME_DELAY_MS = 20;
constexpr int VIDEO_MAX_FRAME_DELAY_MS = 1000;
constexpr int VIDEO_DEFAULT_FRAME_DELAY_MS = 40;
constexpr int VIDEO_ALPHA_THRESHOLD = 24;

[[maybe_unused]] size_t FrameCap(size_t MaxFrames, size_t PixelsPerFrame)
{
	const size_t MaxByBudget = maximum<size_t>(1, PixelsPerFrame == 0 ? 1 : VIDEO_MAX_TOTAL_PIXELS / PixelsPerFrame);
	const size_t Cap = minimum(MaxFrames == 0 ? VIDEO_MAX_FRAMES : MaxFrames, VIDEO_MAX_FRAMES);
	return maximum<size_t>(1, minimum(Cap, MaxByBudget));
}

[[maybe_unused]] void ApplyAlphaKey(CImageInfo &Image, int AlphaMode)
{
	if(AlphaMode != CVideoLoader::ALPHA_BLACK && AlphaMode != CVideoLoader::ALPHA_WHITE)
		return;

	const size_t Pixels = Image.m_Width * Image.m_Height;
	for(size_t i = 0; i < Pixels; ++i)
	{
		uint8_t *pPixel = Image.m_pData + i * 4;
		const int Luma = (77 * pPixel[0] + 150 * pPixel[1] + 29 * pPixel[2]) >> 8;

		if(AlphaMode == CVideoLoader::ALPHA_BLACK)
			pPixel[3] = Luma <= VIDEO_ALPHA_THRESHOLD ? 0 : 255;
		else
			pPixel[3] = Luma >= 255 - VIDEO_ALPHA_THRESHOLD ? 0 : 255;
	}
}

#if defined(CONF_VIDEORECORDER)

struct SVideoContext
{
	AVFormatContext *m_pFormat = nullptr;
	AVCodecContext *m_pCodec = nullptr;
	SwsContext *m_pSws = nullptr;
	AVFrame *m_pFrame = nullptr;
	AVPacket *m_pPacket = nullptr;

	~SVideoContext()
	{
		if(m_pPacket != nullptr)
			av_packet_free(&m_pPacket);
		if(m_pFrame != nullptr)
			av_frame_free(&m_pFrame);
		if(m_pSws != nullptr)
			sws_freeContext(m_pSws);
		if(m_pCodec != nullptr)
			avcodec_free_context(&m_pCodec);
		if(m_pFormat != nullptr)
			avformat_close_input(&m_pFormat);
	}
};

bool LoadFfmpeg(const char *pRealPath, size_t TargetWidth, size_t TargetHeight, size_t MaxFrames, int AlphaMode, CGifAnimation &Animation)
{
	const int PreviousLogLevel = av_log_get_level();
	av_log_set_level(AV_LOG_FATAL);

	SVideoContext Video;
	int Result = avformat_open_input(&Video.m_pFormat, pRealPath, nullptr, nullptr);
	if(Result < 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_debug("video", "FFmpeg failed to open '%s'", pRealPath);
		return false;
	}

	Result = avformat_find_stream_info(Video.m_pFormat, nullptr);
	if(Result < 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_debug("video", "FFmpeg failed to read stream info of '%s'", pRealPath);
		return false;
	}

	const AVCodec *pCodec = nullptr;
	const int StreamIndex = av_find_best_stream(Video.m_pFormat, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if(StreamIndex < 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_debug("video", "FFmpeg found no video stream in '%s'", pRealPath);
		return false;
	}

	AVStream *pStream = Video.m_pFormat->streams[StreamIndex];
	if(pCodec == nullptr)
		pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
	if(pCodec == nullptr)
	{
		av_log_set_level(PreviousLogLevel);
		log_debug("video", "FFmpeg has no decoder for '%s'", pRealPath);
		return false;
	}

	Video.m_pCodec = avcodec_alloc_context3(pCodec);
	if(Video.m_pCodec == nullptr)
	{
		av_log_set_level(PreviousLogLevel);
		return false;
	}

	Result = avcodec_parameters_to_context(Video.m_pCodec, pStream->codecpar);
	if(Result < 0)
	{
		av_log_set_level(PreviousLogLevel);
		return false;
	}

	Video.m_pCodec->pkt_timebase = pStream->time_base;

	Result = avcodec_open2(Video.m_pCodec, pCodec, nullptr);
	if(Result < 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_debug("video", "FFmpeg failed to open decoder for '%s'", pRealPath);
		return false;
	}

	const int SourceWidth = Video.m_pCodec->width;
	const int SourceHeight = Video.m_pCodec->height;
	if(SourceWidth <= 0 || SourceHeight <= 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_debug("video", "FFmpeg found invalid dimensions in '%s'", pRealPath);
		return false;
	}

	const size_t PixelsPerFrame = TargetWidth * TargetHeight;
	const size_t Cap = FrameCap(MaxFrames, PixelsPerFrame);

	Video.m_pSws = sws_getContext(SourceWidth, SourceHeight, Video.m_pCodec->pix_fmt, (int)TargetWidth, (int)TargetHeight, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
	if(Video.m_pSws == nullptr)
	{
		av_log_set_level(PreviousLogLevel);
		log_debug("video", "FFmpeg failed to create scaler for '%s'", pRealPath);
		return false;
	}

	AVRational Rate = pStream->avg_frame_rate;
	if(Rate.num <= 0 || Rate.den <= 0)
		Rate = pStream->r_frame_rate;

	int DelayMs = VIDEO_DEFAULT_FRAME_DELAY_MS;
	if(Rate.num > 0 && Rate.den > 0)
	{
		const double Fps = av_q2d(Rate);
		if(Fps > 0.5 && Fps < 1000.0)
			DelayMs = (int)(1000.0 / Fps + 0.5);
	}
	DelayMs = minimum(maximum(DelayMs, VIDEO_MIN_FRAME_DELAY_MS), VIDEO_MAX_FRAME_DELAY_MS);

	Video.m_pFrame = av_frame_alloc();
	Video.m_pPacket = av_packet_alloc();
	if(Video.m_pFrame == nullptr || Video.m_pPacket == nullptr)
	{
		av_log_set_level(PreviousLogLevel);
		return false;
	}

	std::vector<uint8_t> vBuffer(PixelsPerFrame * 4, 0);
	uint8_t *apDstData[4] = {vBuffer.data(), nullptr, nullptr, nullptr};
	const int aDstLinesize[4] = {(int)(TargetWidth * 4), 0, 0, 0};

	const auto AppendFrame = [&](AVFrame *pFrame) {
		sws_scale(Video.m_pSws, pFrame->data, pFrame->linesize, 0, SourceHeight, apDstData, aDstLinesize);

		SGifFrame Frame;
		Frame.m_DelayMs = DelayMs;
		Frame.m_Image.m_Width = TargetWidth;
		Frame.m_Image.m_Height = TargetHeight;
		Frame.m_Image.m_Format = CImageInfo::FORMAT_RGBA;
		Frame.m_Image.Allocate();
		mem_copy(Frame.m_Image.m_pData, vBuffer.data(), PixelsPerFrame * 4);
		ApplyAlphaKey(Frame.m_Image, AlphaMode);

		Animation.m_vFrames.push_back(std::move(Frame));
	};

	for(;;)
	{
		const int ReadResult = av_read_frame(Video.m_pFormat, Video.m_pPacket);
		if(ReadResult < 0)
			break;

		if(Video.m_pPacket->stream_index != StreamIndex)
		{
			av_packet_unref(Video.m_pPacket);
			continue;
		}

		const int SendResult = avcodec_send_packet(Video.m_pCodec, Video.m_pPacket);
		av_packet_unref(Video.m_pPacket);
		if(SendResult < 0)
			continue;

		while(avcodec_receive_frame(Video.m_pCodec, Video.m_pFrame) == 0)
		{
			AppendFrame(Video.m_pFrame);
			av_frame_unref(Video.m_pFrame);
			if(Animation.m_vFrames.size() >= Cap)
				break;
		}

		if(Animation.m_vFrames.size() >= Cap)
			break;
	}

	if(Animation.m_vFrames.size() < Cap)
	{
		avcodec_send_packet(Video.m_pCodec, nullptr);
		while(avcodec_receive_frame(Video.m_pCodec, Video.m_pFrame) == 0)
		{
			AppendFrame(Video.m_pFrame);
			av_frame_unref(Video.m_pFrame);
			if(Animation.m_vFrames.size() >= Cap)
				break;
		}
	}

	av_log_set_level(PreviousLogLevel);

	if(Animation.m_vFrames.empty())
	{
		log_debug("video", "FFmpeg decoded no frames from '%s'", pRealPath);
		return false;
	}

	Animation.m_Width = TargetWidth;
	Animation.m_Height = TargetHeight;

	log_info("video", "FFmpeg decoded '%s': %d frames, %dx%d -> %zux%zu, %dms delay, alpha mode %d",
		pRealPath, (int)Animation.m_vFrames.size(), SourceWidth, SourceHeight, TargetWidth, TargetHeight, DelayMs, AlphaMode);

	return true;
}

#endif

#if defined(CONF_FAMILY_WINDOWS)

class CCom
{
public:
	CCom()
	{
		m_CoInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
		m_MfStarted = SUCCEEDED(MFStartup(MF_VERSION));
	}
	~CCom()
	{
		if(m_MfStarted)
			MFShutdown();
		if(m_CoInitialized)
			CoUninitialize();
	}

	bool Ok() const { return m_MfStarted; }

private:
	bool m_CoInitialized = false;
	bool m_MfStarted = false;
};

class CWidePath
{
public:
	bool Set(const char *pUtf8)
	{
		const int Length = MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, nullptr, 0);
		if(Length <= 0)
			return false;
		std::vector<WCHAR> vWide(Length);
		if(MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, vWide.data(), Length) <= 0)
			return false;
		for(WCHAR &WideChar : vWide)
		{
			if(WideChar == L'/')
				WideChar = L'\\';
		}
		m_aPath.assign(vWide.data());
		if(m_aPath.length() >= MAX_PATH && m_aPath.compare(0, 4, L"\\\\?\\") != 0)
			m_aPath = L"\\\\?\\" + m_aPath;
		return !m_aPath.empty();
	}

	const WCHAR *Get() const { return m_aPath.c_str(); }

private:
	std::wstring m_aPath;
};

void ScaleBilinear(const uint8_t *pSrc, int SrcPitch, size_t SrcWidth, size_t SrcHeight, size_t DstWidth, size_t DstHeight, uint8_t *pDst)
{
	const double ScaleX = (double)SrcWidth / (double)DstWidth;
	const double ScaleY = (double)SrcHeight / (double)DstHeight;

	for(size_t y = 0; y < DstHeight; ++y)
	{
		double SrcY = (y + 0.5) * ScaleY - 0.5;
		SrcY = minimum(maximum(SrcY, 0.0), (double)(SrcHeight - 1));
		const size_t Y0 = (size_t)SrcY;
		const size_t Y1 = minimum(Y0 + 1, SrcHeight - 1);
		const float fy = (float)(SrcY - Y0);
		const uint8_t *pRow0 = pSrc + Y0 * (size_t)SrcPitch;
		const uint8_t *pRow1 = pSrc + Y1 * (size_t)SrcPitch;
		uint8_t *pDstRow = pDst + y * DstWidth * 4;

		for(size_t x = 0; x < DstWidth; ++x)
		{
			double SrcX = (x + 0.5) * ScaleX - 0.5;
			SrcX = minimum(maximum(SrcX, 0.0), (double)(SrcWidth - 1));
			const size_t X0 = (size_t)SrcX;
			const size_t X1 = minimum(X0 + 1, SrcWidth - 1);
			const float fx = (float)(SrcX - X0);

			const uint8_t *apPixels[4] = {pRow0 + X0 * 4, pRow0 + X1 * 4, pRow1 + X0 * 4, pRow1 + X1 * 4};
			const float afWeights[4] = {(1.0f - fx) * (1.0f - fy), fx * (1.0f - fy), (1.0f - fx) * fy, fx * fy};

			for(int Channel = 0; Channel < 3; ++Channel)
			{
				float Value = 0.0f;
				for(int i = 0; i < 4; ++i)
					Value += (float)apPixels[i][Channel] * afWeights[i];
				pDstRow[x * 4 + (2 - Channel)] = (uint8_t)minimum(maximum((int)(Value + 0.5f), 0), 255);
			}
			pDstRow[x * 4 + 3] = 255;
		}
	}
}

bool ReadFrame(IMFSample *pSample, size_t SourceWidth, size_t SourceHeight, size_t TargetWidth, size_t TargetHeight, CImageInfo &Image)
{
	IMFMediaBuffer *pBuffer = nullptr;
	if(FAILED(pSample->GetBufferByIndex(0, &pBuffer)))
		return false;

	IMF2DBuffer *p2DBuffer = nullptr;
	const bool Have2D = SUCCEEDED(pBuffer->QueryInterface(IID_IMF2DBuffer, reinterpret_cast<void **>(&p2DBuffer)));

	BYTE *pScanline0 = nullptr;
	LONG Pitch = 0;
	BYTE *pData = nullptr;
	DWORD DataSize = 0;
	HRESULT LockResult;
	if(Have2D)
		LockResult = p2DBuffer->Lock2D(&pScanline0, &Pitch);
	else
		LockResult = pBuffer->Lock(&pData, nullptr, &DataSize);

	bool Result = false;
	if(SUCCEEDED(LockResult))
	{
		const uint8_t *pPixels = Have2D ? pScanline0 : pData;
		int PixelPitch = Pitch;
		if(!Have2D || PixelPitch <= 0)
			PixelPitch = (int)(SourceWidth * 4);
		if(pPixels != nullptr)
		{
			ScaleBilinear(pPixels, PixelPitch, SourceWidth, SourceHeight, TargetWidth, TargetHeight, Image.m_pData);
			Result = true;
		}

		if(Have2D)
			p2DBuffer->Unlock2D();
		else
			pBuffer->Unlock();
	}

	if(p2DBuffer != nullptr)
		p2DBuffer->Release();
	pBuffer->Release();
	return Result;
}

bool LoadMediaFoundation(const char *pRealPath, size_t TargetWidth, size_t TargetHeight, size_t MaxFrames, int AlphaMode, CGifAnimation &Animation)
{
	CWidePath Wide;
	if(!Wide.Set(pRealPath))
		return false;

	CCom Com;
	if(!Com.Ok())
	{
		log_error("video", "could not initialize Media Foundation");
		return false;
	}

	IMFAttributes *pAttributes = nullptr;
	IMFSourceReader *pReader = nullptr;
	IMFMediaType *pRequested = nullptr;
	IMFMediaType *pCurrent = nullptr;

	HRESULT Hr = MFCreateAttributes(&pAttributes, 2);
	if(SUCCEEDED(Hr))
	{
		pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
		pAttributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, TRUE);
		Hr = MFCreateSourceReaderFromURL(Wide.Get(), pAttributes, &pReader);
	}
	if(SUCCEEDED(Hr))
		Hr = MFCreateMediaType(&pRequested);
	if(SUCCEEDED(Hr))
	{
		pRequested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		pRequested->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
		Hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pRequested);
	}
	if(SUCCEEDED(Hr))
		Hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrent);

	size_t SourceWidth = 0;
	size_t SourceHeight = 0;
	int DelayMs = VIDEO_DEFAULT_FRAME_DELAY_MS;
	if(SUCCEEDED(Hr))
	{
		UINT32 Width = 0;
		UINT32 Height = 0;
		if(FAILED(MFGetAttributeSize(pCurrent, MF_MT_FRAME_SIZE, &Width, &Height)) || Width == 0 || Height == 0)
			Hr = E_FAIL;
		SourceWidth = Width;
		SourceHeight = Height;

		UINT32 Numerator = 0;
		UINT32 Denominator = 0;
		if(SUCCEEDED(MFGetAttributeRatio(pCurrent, MF_MT_FRAME_RATE, &Numerator, &Denominator)) && Numerator > 0 && Denominator > 0)
		{
			const double Fps = (double)Numerator / (double)Denominator;
			if(Fps > 0.5 && Fps < 1000.0)
				DelayMs = (int)(1000.0 / Fps + 0.5);
		}
		DelayMs = minimum(maximum(DelayMs, VIDEO_MIN_FRAME_DELAY_MS), VIDEO_MAX_FRAME_DELAY_MS);
	}

	if(FAILED(Hr))
	{
		log_debug("video", "Media Foundation could not read '%s' (hr=0x%08lx)", pRealPath, (unsigned long)Hr);
		if(pCurrent != nullptr)
			pCurrent->Release();
		if(pRequested != nullptr)
			pRequested->Release();
		if(pReader != nullptr)
			pReader->Release();
		if(pAttributes != nullptr)
			pAttributes->Release();
		return false;
	}

	const size_t Cap = FrameCap(MaxFrames, TargetWidth * TargetHeight);
	LONGLONG PreviousTime = -1;
	bool Failed = false;

	while(!Failed && Animation.m_vFrames.size() < Cap)
	{
		DWORD StreamFlags = 0;
		LONGLONG Time = 0;
		IMFSample *pSample = nullptr;
		if(FAILED(pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &StreamFlags, &Time, &pSample)))
		{
			Failed = Animation.m_vFrames.empty();
			break;
		}

		if(StreamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
		{
			IMFMediaType *pNewType = nullptr;
			if(SUCCEEDED(pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pNewType)))
			{
				UINT32 Width = 0;
				UINT32 Height = 0;
				MFGetAttributeSize(pNewType, MF_MT_FRAME_SIZE, &Width, &Height);
				if(Width != SourceWidth || Height != SourceHeight)
				{
					pNewType->Release();
					if(pSample != nullptr)
						pSample->Release();
					break;
				}
				pNewType->Release();
			}
		}

		if(StreamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			if(pSample != nullptr)
				pSample->Release();
			break;
		}

		if(pSample == nullptr)
			continue;

		int FrameDelay = DelayMs;
		if(PreviousTime >= 0 && Time > PreviousTime)
			FrameDelay = (int)minimum(maximum((Time - PreviousTime + 5000) / 10000, (LONGLONG)VIDEO_MIN_FRAME_DELAY_MS), (LONGLONG)VIDEO_MAX_FRAME_DELAY_MS);
		PreviousTime = Time;

		SGifFrame Frame;
		Frame.m_DelayMs = FrameDelay;
		Frame.m_Image.m_Width = TargetWidth;
		Frame.m_Image.m_Height = TargetHeight;
		Frame.m_Image.m_Format = CImageInfo::FORMAT_RGBA;
		Frame.m_Image.Allocate();

		if(ReadFrame(pSample, SourceWidth, SourceHeight, TargetWidth, TargetHeight, Frame.m_Image))
		{
			ApplyAlphaKey(Frame.m_Image, AlphaMode);
			Animation.m_vFrames.push_back(std::move(Frame));
		}
		else
		{
			Frame.m_Image.Free();
			Failed = Animation.m_vFrames.empty();
		}
		pSample->Release();
	}

	pCurrent->Release();
	pRequested->Release();
	pReader->Release();
	pAttributes->Release();

	if(Animation.m_vFrames.empty())
	{
		log_debug("video", "Media Foundation decoded no frames from '%s'", pRealPath);
		return false;
	}

	Animation.m_Width = TargetWidth;
	Animation.m_Height = TargetHeight;

	log_info("video", "Media Foundation decoded '%s': %d frames, %zux%zu -> %zux%zu, %dms delay, alpha mode %d",
		pRealPath, (int)Animation.m_vFrames.size(), SourceWidth, SourceHeight, TargetWidth, TargetHeight, DelayMs, AlphaMode);

	return true;
}

#endif

}

bool CVideoLoader::IsVideoFile(const char *pPath)
{
	if(pPath == nullptr)
		return false;

	for(const char *pExtension : s_apVideoExtensions)
	{
		if(str_endswith_nocase(pPath, pExtension))
			return true;
	}

	return false;
}

bool CVideoLoader::Load(const char *pRealPath, size_t TargetWidth, size_t TargetHeight, size_t MaxFrames, int AlphaMode, CGifAnimation &Animation)
{
	if(pRealPath == nullptr || pRealPath[0] == '\0' || TargetWidth == 0 || TargetHeight == 0)
		return false;

#if defined(CONF_FAMILY_WINDOWS)
	if(LoadMediaFoundation(pRealPath, TargetWidth, TargetHeight, MaxFrames, AlphaMode, Animation))
		return true;
#endif
#if defined(CONF_VIDEORECORDER)
	if(LoadFfmpeg(pRealPath, TargetWidth, TargetHeight, MaxFrames, AlphaMode, Animation))
		return true;
#endif
#if !defined(CONF_FAMILY_WINDOWS) && !defined(CONF_VIDEORECORDER)
	(void)MaxFrames;
	(void)AlphaMode;
	log_error("video", "this build cannot decode '%s', it has no video decoding support", pRealPath);
#endif

	if(Animation.m_vFrames.empty())
		log_error("video", "failed to decode '%s'", pRealPath);
	return false;
}
