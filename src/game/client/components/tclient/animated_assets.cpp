#include "animated_assets.h"

#include "video_loader.h"

#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <generated/client_data.h>

#include <algorithm>
#include <cstdlib>

namespace {

double NowSeconds()
{
	return (double)time_get() / (double)time_freq();
}

constexpr const char *VCSKINS_GIF_DIR = "assets/skins";
constexpr const char *VCSKINS_ENTITY_NAME = "entities";

constexpr const char *const s_apAnimationExtensions[] = {".gif", ".mp4", ".m4v", ".mov", ".webm", ".mkv", ".avi"};

bool SheetEnabled(const char *pName)
{
	if(str_comp(pName, VCSKINS_ENTITY_NAME) == 0)
		return true;
	return g_Config.m_FcAnimatedAssetsGame != 0;
}

bool ScaleNearest(const CImageInfo &Src, size_t Width, size_t Height, CImageInfo &Dst)
{
	if(Src.m_pData == nullptr || Src.m_Format != CImageInfo::FORMAT_RGBA || Src.m_Width == 0 || Src.m_Height == 0)
		return false;

	Dst.Free();
	Dst.m_Width = Width;
	Dst.m_Height = Height;
	Dst.m_Format = CImageInfo::FORMAT_RGBA;
	Dst.Allocate();

	const size_t SrcStride = Src.m_Width * 4;
	const size_t DstStride = Dst.m_Width * 4;

	for(size_t y = 0; y < Height; ++y)
	{
		const size_t SrcY = (y * Src.m_Height) / Height;
		const uint8_t *pSrcRow = Src.m_pData + SrcY * SrcStride;
		uint8_t *pDstRow = Dst.m_pData + y * DstStride;
		for(size_t x = 0; x < Width; ++x)
		{
			const size_t SrcX = (x * Src.m_Width) / Width;
			const uint8_t *pSrc = pSrcRow + SrcX * 4;
			uint8_t *pDst = pDstRow + x * 4;
			pDst[0] = pSrc[0];
			pDst[1] = pSrc[1];
			pDst[2] = pSrc[2];
			pDst[3] = pSrc[3];
		}
	}

	return true;
}

}

void CVcAnimatedAssets::OnInit()
{
	ClearSheets();
	m_Time = NowSeconds();
}

void CVcAnimatedAssets::ClearSheets()
{
	m_vSheets.clear();
	m_vRecording.clear();
	m_Recording = false;
	m_Entities = SSheetAnimation();
	m_Time = NowSeconds();
}

int CVcAnimatedAssets::NumFrames() const
{
	int Total = 0;
	for(const SSheetAnimation &Sheet : m_vSheets)
		Total += (int)Sheet.m_Gif.NumFrames();
	Total += (int)m_Entities.m_Gif.NumFrames();
	return Total;
}

const char *CVcAnimatedAssets::SheetName(int Index) const
{
	if(Index < 0 || Index >= (int)m_vSheets.size())
		return "";
	return m_vSheets[(size_t)Index].m_aName;
}

bool CVcAnimatedAssets::SheetIsActive(int Index) const
{
	if(Index < 0 || Index >= (int)m_vSheets.size())
		return false;
	return !m_vSheets[(size_t)Index].m_Gif.Empty();
}

void CVcAnimatedAssets::BeginRecording()
{
	m_vRecording.clear();
	m_Recording = true;
	Graphics()->SetSpriteTextureRecorder(&m_vRecording);
}

void CVcAnimatedAssets::EndRecording(const char *pAssetName, const char *pAnimationPath, size_t SheetWidth, size_t SheetHeight)
{
	if(!m_Recording)
		return;

	m_Recording = false;
	Graphics()->SetSpriteTextureRecorder(nullptr);

	std::vector<SChannel> Channels;
	Channels.reserve(m_vRecording.size());
	for(const auto &Entry : m_vRecording)
	{
		const bool Duplicate = std::any_of(Channels.begin(), Channels.end(), [&Entry](const SChannel &Existing) {
			return Existing.m_Handle.Id() == Entry.first.Id() && Existing.m_pSprite == Entry.second;
		});
		if(!Duplicate)
			Channels.push_back({Entry.first, Entry.second});
	}
	m_vRecording.clear();

	if(!g_Config.m_FcAnimatedAssets || !SheetEnabled(pAssetName) || pAnimationPath == nullptr || pAnimationPath[0] == '\0')
	{
		RemoveSheet(pAssetName);
		return;
	}

	if(Channels.empty())
		return;

	if(SheetWidth == 0 || SheetHeight == 0)
	{
		for(const SChannel &Channel : Channels)
		{
			SheetWidth = maximum(SheetWidth, (size_t)(Channel.m_pSprite->m_X + Channel.m_pSprite->m_W));
			SheetHeight = maximum(SheetHeight, (size_t)(Channel.m_pSprite->m_Y + Channel.m_pSprite->m_H));
		}
	}

	SSheetAnimation Sheet;
	str_copy(Sheet.m_aName, pAssetName, sizeof(Sheet.m_aName));
	str_copy(Sheet.m_aGifPath, pAnimationPath, sizeof(Sheet.m_aGifPath));
	Sheet.m_vChannels = std::move(Channels);
	Sheet.m_SheetWidth = SheetWidth;
	Sheet.m_SheetHeight = SheetHeight;

	if(!LoadAnimationFor(pAssetName, pAssetName, Sheet.m_aGifPath, Sheet.m_Gif, SheetWidth, SheetHeight))
	{
		log_debug("fusion", "animated assets: could not decode '%s' for '%s'", Sheet.m_aGifPath, pAssetName);
		RemoveSheet(pAssetName);
		return;
	}

	log_info("fusion", "animated asset '%s': %zu sprites, %d frames, %zux%zu -> sheet %zux%zu",
		pAssetName, Sheet.m_vChannels.size(), (int)Sheet.m_Gif.NumFrames(),
		Sheet.m_Gif.m_Width, Sheet.m_Gif.m_Height, SheetWidth, SheetHeight);

	ApplySheetFrame(Sheet, 0);
	Sheet.m_FrameStart = NowSeconds();

	const auto It = std::find_if(m_vSheets.begin(), m_vSheets.end(), [pAssetName](const SSheetAnimation &Existing) {
		return str_comp(Existing.m_aName, pAssetName) == 0;
	});
	if(It != m_vSheets.end())
		*It = std::move(Sheet);
	else
		m_vSheets.push_back(std::move(Sheet));
}

void CVcAnimatedAssets::RemoveSheet(const char *pAssetName)
{
	m_vSheets.erase(std::remove_if(m_vSheets.begin(), m_vSheets.end(), [pAssetName](const SSheetAnimation &Sheet) {
		return str_comp(Sheet.m_aName, pAssetName) == 0;
	}), m_vSheets.end());
}

bool CVcAnimatedAssets::IsAnimationFile(const char *pPath)
{
	return AnimationExtension(pPath) != nullptr;
}

const char *CVcAnimatedAssets::AnimationExtension(const char *pPath)
{
	if(pPath == nullptr)
		return nullptr;

	for(const char *pExtension : s_apAnimationExtensions)
	{
		if(str_endswith_nocase(pPath, pExtension))
			return pExtension;
	}

	return nullptr;
}

const std::vector<const char *> &CVcAnimatedAssets::AnimationExtensions()
{
	static const std::vector<const char *> s_vExtensions(std::begin(s_apAnimationExtensions), std::end(s_apAnimationExtensions));
	return s_vExtensions;
}

bool CVcAnimatedAssets::ResolveAnimationPath(IStorage *pStorage, const char *pAssetName, const char *pCategory, char *pOutPath, size_t OutPathSize)
{
	if(pOutPath == nullptr || OutPathSize == 0)
		return false;
	pOutPath[0] = '\0';
	if(pStorage == nullptr || pCategory == nullptr || pCategory[0] == '\0')
		return false;

	constexpr int MAX_CANDIDATES = 5;
	char aaBases[MAX_CANDIDATES][IO_MAX_PATH_LENGTH];
	int NumBases = 0;

	const bool NameUsable = pAssetName != nullptr && pAssetName[0] != '\0' && str_comp(pAssetName, "default") != 0 &&
				str_find(pAssetName, "/") == nullptr && str_find(pAssetName, "\\") == nullptr;
	if(NameUsable)
	{
		str_format(aaBases[NumBases++], IO_MAX_PATH_LENGTH, "assets/%s/%s", pCategory, pAssetName);
		str_format(aaBases[NumBases++], IO_MAX_PATH_LENGTH, "assets/%s/%s/%s", pCategory, pAssetName, pCategory);
		str_format(aaBases[NumBases++], IO_MAX_PATH_LENGTH, "assets/%s/%s/%s", pCategory, pAssetName, pAssetName);
		str_format(aaBases[NumBases++], IO_MAX_PATH_LENGTH, "%s/%s", VCSKINS_GIF_DIR, pAssetName);
	}
	else
	{
		str_format(aaBases[NumBases++], IO_MAX_PATH_LENGTH, "%s/%s", VCSKINS_GIF_DIR, pCategory);
	}

	for(int Base = 0; Base < NumBases; ++Base)
	{
		for(const char *pExtension : s_apAnimationExtensions)
		{
			char aCandidate[IO_MAX_PATH_LENGTH];
			str_format(aCandidate, sizeof(aCandidate), "%s%s", aaBases[Base], pExtension);
			if(!pStorage->FileExists(aCandidate, IStorage::TYPE_ALL))
				continue;
			str_copy(pOutPath, aCandidate, OutPathSize);
			return true;
		}
	}

	return false;
}

bool CVcAnimatedAssets::LoadAnimationFile(IStorage *pStorage, const char *pPath, CGifAnimation &Gif, size_t MaxFrames, size_t TargetWidth, size_t TargetHeight)
{
	if(pStorage == nullptr || pPath == nullptr || pPath[0] == '\0')
		return false;

	const char *pExtension = AnimationExtension(pPath);
	if(pExtension == nullptr)
		return false;

	if(str_comp_nocase(pExtension, ".gif") != 0)
	{
		if(TargetWidth == 0 || TargetHeight == 0)
		{
			log_warn("fusion", "no sheet size known for video '%s', skipping", pPath);
			return false;
		}

		return CVideoLoader::Load(pPath, TargetWidth, TargetHeight, MaxFrames == 0 ? (size_t)g_Config.m_FcAnimatedAssetsVideoFrames : MaxFrames, g_Config.m_FcAnimatedAssetsVideoAlpha, Gif);
	}

	void *pData = nullptr;
	unsigned DataSize = 0;
	if(!pStorage->ReadFile(pPath, IStorage::TYPE_ALL, &pData, &DataSize))
		return false;

	const bool Result = CGifLoader::Load((const uint8_t *)pData, (size_t)DataSize, pPath, Gif);
	free(pData);
	return Result;
}

IGraphics::CTextureHandle CVcAnimatedAssets::LoadAnimationTexture(IGraphics *pGraphics, IStorage *pStorage, const char *pPath)
{
	if(pGraphics == nullptr)
		return IGraphics::CTextureHandle();

	const char *pExtension = AnimationExtension(pPath);
	const bool IsVideo = pExtension != nullptr && str_comp_nocase(pExtension, ".gif") != 0;

	CGifAnimation Gif;
	if(!LoadAnimationFile(pStorage, pPath, Gif, 1, IsVideo ? 256 : 0, IsVideo ? 256 : 0))
		return IGraphics::CTextureHandle();

	if(Gif.m_vFrames.empty())
		return IGraphics::CTextureHandle();

	const IGraphics::CTextureHandle Texture = pGraphics->LoadTextureRaw(Gif.m_vFrames[0].m_Image, pGraphics->TextureLoadFlags(), pPath);
	Gif.Free();
	return Texture;
}

bool CVcAnimatedAssets::LoadAnimationFor(const char *pAssetName, const char *pCategory, const char *pPath, CGifAnimation &Gif, size_t SheetWidth, size_t SheetHeight)
{
	char aPath[IO_MAX_PATH_LENGTH];
	if(pPath != nullptr && pPath[0] != '\0')
		str_copy(aPath, pPath, sizeof(aPath));
	else if(!ResolveAnimationPath(GameClient()->Storage(), pAssetName, pCategory, aPath, sizeof(aPath)))
		return false;

	if(SheetWidth == 0 || SheetHeight == 0)
	{
		if(CVideoLoader::IsVideoFile(aPath))
			return false;
	}

	return LoadAnimationFile(GameClient()->Storage(), aPath, Gif, 0, SheetWidth, SheetHeight);
}

bool CVcAnimatedAssets::GetFrameImage(SSheetAnimation &Sheet, int FrameIndex, const CImageInfo *&pImage)
{
	pImage = nullptr;

	const int NumFrames = (int)Sheet.m_Gif.NumFrames();
	if(NumFrames <= 0)
		return false;

	FrameIndex %= NumFrames;
	if(FrameIndex < 0)
		FrameIndex += NumFrames;

	const CImageInfo &Src = Sheet.m_Gif.m_vFrames[(size_t)FrameIndex].m_Image;
	if(Src.m_pData == nullptr || Src.m_Format != CImageInfo::FORMAT_RGBA)
		return false;

	if(Sheet.m_SheetWidth == 0 || Sheet.m_SheetHeight == 0)
	{
		pImage = &Src;
		return true;
	}

	if(Src.m_Width == Sheet.m_SheetWidth && Src.m_Height == Sheet.m_SheetHeight)
	{
		pImage = &Src;
		return true;
	}

	if(!ScaleNearest(Src, Sheet.m_SheetWidth, Sheet.m_SheetHeight, Sheet.m_Scaled))
		return false;

	pImage = &Sheet.m_Scaled;
	return true;
}

void CVcAnimatedAssets::ApplySheetFrame(SSheetAnimation &Sheet, int FrameIndex)
{
	const int NumFrames = (int)Sheet.m_Gif.NumFrames();
	if(NumFrames <= 0)
		return;

	FrameIndex %= NumFrames;
	if(FrameIndex < 0)
		FrameIndex += NumFrames;

	if(Sheet.m_CurrentFrame == FrameIndex)
		return;

	const CImageInfo *pImage = nullptr;
	if(!GetFrameImage(Sheet, FrameIndex, pImage) || pImage == nullptr)
		return;

	const CImageInfo &Src = *pImage;

	int Applied = 0;
	for(const SChannel &Channel : Sheet.m_vChannels)
	{
		if(!Channel.m_Handle.IsValid() || Channel.m_Handle.IsNullTexture())
			continue;

		const CDataSprite *pSprite = Channel.m_pSprite;
		if(pSprite == nullptr || pSprite->m_W <= 0 || pSprite->m_H <= 0)
			continue;

		if((size_t)pSprite->m_X + (size_t)pSprite->m_W > Src.m_Width ||
			(size_t)pSprite->m_Y + (size_t)pSprite->m_H > Src.m_Height)
			continue;

		CImageInfo SpriteInfo;
		SpriteInfo.m_Width = (size_t)pSprite->m_W;
		SpriteInfo.m_Height = (size_t)pSprite->m_H;
		SpriteInfo.m_Format = CImageInfo::FORMAT_RGBA;
		SpriteInfo.Allocate();
		SpriteInfo.CopyRectFrom(Src, (size_t)pSprite->m_X, (size_t)pSprite->m_Y, SpriteInfo.m_Width, SpriteInfo.m_Height, 0, 0);

		if(Graphics()->ReloadTexture(Channel.m_Handle, SpriteInfo, pSprite->m_pName))
			Applied++;
	}

	if(Applied == 0)
		log_warn("fusion", "animated asset '%s': no sprite textures could be updated", Sheet.m_aName);

	Sheet.m_CurrentFrame = FrameIndex;
}

void CVcAnimatedAssets::ApplyEntitiesFrame(int FrameIndex)
{
	const int NumFrames = (int)m_Entities.m_Gif.NumFrames();
	if(NumFrames <= 0)
		return;

	FrameIndex %= NumFrames;
	if(FrameIndex < 0)
		FrameIndex += NumFrames;

	if(m_Entities.m_CurrentFrame == FrameIndex)
		return;

	const CImageInfo *pImage = nullptr;
	if(!GetFrameImage(m_Entities, FrameIndex, pImage) || pImage == nullptr)
		return;

	if(!GameClient()->m_MapImages.ApplyEntitiesFrame(*pImage))
		return;

	m_Entities.m_CurrentFrame = FrameIndex;
}

void CVcAnimatedAssets::TryLoadEntitiesAnimation()
{
	if(!m_Entities.m_Gif.Empty())
		return;
	if(m_Entities.m_aGifPath[0] == '\0')
		return;

	size_t Width = 0;
	size_t Height = 0;
	if(GameClient()->m_MapImages.GetEntitiesSheetSize(Width, Height))
	{
		m_Entities.m_SheetWidth = Width;
		m_Entities.m_SheetHeight = Height;
	}

	if(CVideoLoader::IsVideoFile(m_Entities.m_aGifPath) && (m_Entities.m_SheetWidth == 0 || m_Entities.m_SheetHeight == 0))
		return;

	if(!LoadAnimationFor(g_Config.m_ClAssetsEntities, "entities", m_Entities.m_aGifPath, m_Entities.m_Gif, m_Entities.m_SheetWidth, m_Entities.m_SheetHeight))
	{
		m_Entities.m_aGifPath[0] = '\0';
		return;
	}

	log_info("fusion", "animated entities loaded from '%s': %d frames", m_Entities.m_aGifPath, (int)m_Entities.m_Gif.NumFrames());
	m_Entities.m_CurrentFrame = -1;
	m_Entities.m_FrameStart = NowSeconds();
	ApplyEntitiesFrame(0);
}

void CVcAnimatedAssets::Refresh()
{
	if(!g_Config.m_FcAnimatedAssets)
		return;

	for(SSheetAnimation &Sheet : m_vSheets)
	{
		Sheet.m_Gif.Free();
		Sheet.m_Scaled.Free();
		Sheet.m_CurrentFrame = -1;
		if(LoadAnimationFor(Sheet.m_aName, Sheet.m_aName, Sheet.m_aGifPath, Sheet.m_Gif, Sheet.m_SheetWidth, Sheet.m_SheetHeight))
		{
			log_info("fusion", "animated asset '%s' reloaded from '%s': %d frames", Sheet.m_aName, Sheet.m_aGifPath, (int)Sheet.m_Gif.NumFrames());
			ApplySheetFrame(Sheet, 0);
		}
		Sheet.m_FrameStart = NowSeconds();
	}

	m_Entities.m_Gif.Free();
	m_Entities.m_Scaled.Free();
	m_Entities.m_CurrentFrame = -1;
	m_Entities.m_aGifPath[0] = '\0';

	char aEntitiesPath[IO_MAX_PATH_LENGTH];
	if(ResolveAnimationPath(GameClient()->Storage(), g_Config.m_ClAssetsEntities, "entities", aEntitiesPath, sizeof(aEntitiesPath)))
		str_copy(m_Entities.m_aGifPath, aEntitiesPath, sizeof(m_Entities.m_aGifPath));
	m_Entities.m_FrameStart = NowSeconds();

	TryLoadEntitiesAnimation();

}

void CVcAnimatedAssets::OnUpdate()
{
	if(!g_Config.m_FcAnimatedAssets)
		return;

	const double Now = NowSeconds();
	if(m_Time == 0.0)
	{
		m_Time = Now;
		return;
	}

	const double Speed = maximum(0.01, (double)g_Config.m_FcAnimatedAssetsSpeed / 100.0);
	const double MaxFps = maximum(1, g_Config.m_FcAnimatedAssetsFps);

	auto Advance = [&](SSheetAnimation &Sheet, bool Entities) {
		const int NumFrames = (int)Sheet.m_Gif.NumFrames();
		if(NumFrames <= 0)
			return;

		if(Sheet.m_CurrentFrame < 0)
		{
			if(Entities)
				ApplyEntitiesFrame(0);
			else
				ApplySheetFrame(Sheet, 0);
			Sheet.m_FrameStart = Now;
			return;
		}

		const int Current = Sheet.m_CurrentFrame % NumFrames;
		double Delay = (double)Sheet.m_Gif.m_vFrames[(size_t)Current].m_DelayMs / 1000.0;
		if(Delay <= 0.0)
			Delay = 0.1;
		Delay /= Speed;

		const double MinDelay = 1.0 / MaxFps;
		if(Delay < MinDelay)
			Delay = MinDelay;

		const double Elapsed = Now - Sheet.m_FrameStart;
		if(Elapsed < Delay)
			return;

		const int Steps = (int)(Elapsed / Delay);
		Sheet.m_FrameStart += (double)Steps * Delay;

		if(Entities)
			ApplyEntitiesFrame(Current + Steps);
		else
			ApplySheetFrame(Sheet, Current + Steps);
	};

	for(SSheetAnimation &Sheet : m_vSheets)
		Advance(Sheet, false);

	if(m_Entities.m_Gif.Empty())
		TryLoadEntitiesAnimation();

	if(!m_Entities.m_Gif.Empty())
	{
		if(m_Entities.m_SheetWidth == 0)
		{
			size_t Width = 0;
			size_t Height = 0;
			if(GameClient()->m_MapImages.GetEntitiesSheetSize(Width, Height))
			{
				m_Entities.m_SheetWidth = Width;
				m_Entities.m_SheetHeight = Height;
			}
		}
		Advance(m_Entities, true);
	}
}
