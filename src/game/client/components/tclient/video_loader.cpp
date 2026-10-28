#include "video_loader.h"

#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <vector>

namespace {

constexpr const char *const s_apVideoExtensions[] = {".mp4", ".m4v", ".mov", ".webm", ".mkv", ".avi"};

constexpr size_t VIDEO_MAX_FRAMES = 512;
constexpr size_t VIDEO_MAX_TOTAL_PIXELS = 192ull * 1024 * 1024;
constexpr int VIDEO_MIN_FRAME_DELAY_MS = 20;
constexpr int VIDEO_DEFAULT_FRAME_DELAY_MS = 40;
constexpr int VIDEO_ALPHA_THRESHOLD = 24;

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

void ApplyAlphaKey(CImageInfo &Image, int AlphaMode)
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

bool CVideoLoader::Load(const char *pPath, size_t TargetWidth, size_t TargetHeight, size_t MaxFrames, int AlphaMode, CGifAnimation &Animation)
{
	if(pPath == nullptr || pPath[0] == '\0' || TargetWidth == 0 || TargetHeight == 0)
		return false;

	const int PreviousLogLevel = av_log_get_level();
	av_log_set_level(AV_LOG_FATAL);

	SVideoContext Video;
	int Result = avformat_open_input(&Video.m_pFormat, pPath, nullptr, nullptr);
	if(Result < 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_error("video", "failed to open '%s'", pPath);
		return false;
	}

	Result = avformat_find_stream_info(Video.m_pFormat, nullptr);
	if(Result < 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_error("video", "failed to read stream info of '%s'", pPath);
		return false;
	}

	const AVCodec *pCodec = nullptr;
	const int StreamIndex = av_find_best_stream(Video.m_pFormat, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if(StreamIndex < 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_error("video", "no video stream in '%s'", pPath);
		return false;
	}

	AVStream *pStream = Video.m_pFormat->streams[StreamIndex];
	if(pCodec == nullptr)
		pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
	if(pCodec == nullptr)
	{
		av_log_set_level(PreviousLogLevel);
		log_error("video", "no decoder for '%s'", pPath);
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
		log_error("video", "failed to open decoder for '%s'", pPath);
		return false;
	}

	const int SourceWidth = Video.m_pCodec->width;
	const int SourceHeight = Video.m_pCodec->height;
	if(SourceWidth <= 0 || SourceHeight <= 0)
	{
		av_log_set_level(PreviousLogLevel);
		log_error("video", "invalid dimensions in '%s'", pPath);
		return false;
	}

	const size_t Width = TargetWidth;
	const size_t Height = TargetHeight;
	const size_t PixelsPerFrame = Width * Height;

	const size_t MaxByBudget = maximum<size_t>(1, VIDEO_MAX_TOTAL_PIXELS / PixelsPerFrame);
	size_t FrameCap = minimum(MaxFrames == 0 ? VIDEO_MAX_FRAMES : MaxFrames, VIDEO_MAX_FRAMES);
	FrameCap = minimum(FrameCap, MaxByBudget);
	if(FrameCap == 0)
		FrameCap = 1;

	Video.m_pSws = sws_getContext(SourceWidth, SourceHeight, Video.m_pCodec->pix_fmt, (int)Width, (int)Height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
	if(Video.m_pSws == nullptr)
	{
		av_log_set_level(PreviousLogLevel);
		log_error("video", "failed to create scaler for '%s'", pPath);
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
	if(DelayMs < VIDEO_MIN_FRAME_DELAY_MS)
		DelayMs = VIDEO_MIN_FRAME_DELAY_MS;

	Video.m_pFrame = av_frame_alloc();
	Video.m_pPacket = av_packet_alloc();
	if(Video.m_pFrame == nullptr || Video.m_pPacket == nullptr)
	{
		av_log_set_level(PreviousLogLevel);
		return false;
	}

	std::vector<uint8_t> vBuffer(PixelsPerFrame * 4, 0);
	uint8_t *apDstData[4] = {vBuffer.data(), nullptr, nullptr, nullptr};
	const int aDstLinesize[4] = {(int)(Width * 4), 0, 0, 0};

	const auto AppendFrame = [&](AVFrame *pFrame) {
		sws_scale(Video.m_pSws, pFrame->data, pFrame->linesize, 0, SourceHeight, apDstData, aDstLinesize);

		SGifFrame Frame;
		Frame.m_DelayMs = DelayMs;
		Frame.m_Image.m_Width = Width;
		Frame.m_Image.m_Height = Height;
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
			if(Animation.m_vFrames.size() >= FrameCap)
				break;
		}

		if(Animation.m_vFrames.size() >= FrameCap)
			break;
	}

	if(Animation.m_vFrames.size() < FrameCap)
	{
		avcodec_send_packet(Video.m_pCodec, nullptr);
		while(avcodec_receive_frame(Video.m_pCodec, Video.m_pFrame) == 0)
		{
			AppendFrame(Video.m_pFrame);
			av_frame_unref(Video.m_pFrame);
			if(Animation.m_vFrames.size() >= FrameCap)
				break;
		}
	}

	av_log_set_level(PreviousLogLevel);

	if(Animation.m_vFrames.empty())
	{
		log_error("video", "no frames could be decoded from '%s'", pPath);
		return false;
	}

	Animation.m_Width = Width;
	Animation.m_Height = Height;

	log_info("video", "decoded '%s': %d frames, %dx%d -> %zux%zu, %dms delay, alpha mode %d",
		pPath, (int)Animation.m_vFrames.size(), SourceWidth, SourceHeight, Width, Height, DelayMs, AlphaMode);

	return true;
}
