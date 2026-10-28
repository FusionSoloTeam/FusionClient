#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_ANIMATED_ASSETS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_ANIMATED_ASSETS_H

#include <base/types.h>

#include <engine/gfx/gif_loader.h>
#include <engine/graphics.h>
#include <engine/image.h>

#include <game/client/component.h>

#include <utility>
#include <vector>

class IStorage;
struct CDataSprite;

class CVcAnimatedAssets : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;

	void BeginRecording();
	void EndRecording(const char *pAssetName, const char *pAnimationPath, size_t SheetWidth = 0, size_t SheetHeight = 0);
	void Refresh();

	bool HasAny() const { return !m_vSheets.empty() || EntitiesActive(); }
	int NumAnimatedSheets() const { return (int)m_vSheets.size(); }
	int NumFrames() const;
	const char *SheetName(int Index) const;
	bool SheetIsActive(int Index) const;
	bool EntitiesActive() const { return !m_Entities.m_Gif.Empty(); }

	static bool IsAnimationFile(const char *pPath);
	static const char *AnimationExtension(const char *pPath);
	static const std::vector<const char *> &AnimationExtensions();
	static bool ResolveAnimationPath(IStorage *pStorage, const char *pAssetName, const char *pCategory, char *pOutPath, size_t OutPathSize);
	static bool LoadAnimationFile(IStorage *pStorage, const char *pPath, CGifAnimation &Gif, size_t MaxFrames = 0, size_t TargetWidth = 0, size_t TargetHeight = 0);
	static IGraphics::CTextureHandle LoadAnimationTexture(IGraphics *pGraphics, IStorage *pStorage, const char *pPath);

private:
	struct SChannel
	{
		IGraphics::CTextureHandle m_Handle;
		const CDataSprite *m_pSprite;
	};

	struct SSheetAnimation
	{
		char m_aName[64] = "";
		char m_aGifPath[IO_MAX_PATH_LENGTH] = "";
		CGifAnimation m_Gif;
		std::vector<SChannel> m_vChannels;
		CImageInfo m_Scaled;
		size_t m_SheetWidth = 0;
		size_t m_SheetHeight = 0;
		int m_CurrentFrame = -1;
		double m_FrameStart = 0.0;
	};

	void ClearSheets();
	void RemoveSheet(const char *pAssetName);
	bool LoadAnimationFor(const char *pAssetName, const char *pCategory, const char *pPath, CGifAnimation &Gif, size_t SheetWidth = 0, size_t SheetHeight = 0);
	void TryLoadEntitiesAnimation();
	bool GetFrameImage(SSheetAnimation &Sheet, int FrameIndex, const CImageInfo *&pImage);
	void ApplySheetFrame(SSheetAnimation &Sheet, int FrameIndex);
	void ApplyEntitiesFrame(int FrameIndex);

	std::vector<SSheetAnimation> m_vSheets;
	std::vector<std::pair<IGraphics::CTextureHandle, const CDataSprite *>> m_vRecording;
	bool m_Recording = false;

	SSheetAnimation m_Entities;
	double m_Time = 0.0;
};

#endif
