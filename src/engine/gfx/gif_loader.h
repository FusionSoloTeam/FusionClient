#ifndef ENGINE_GFX_GIF_LOADER_H
#define ENGINE_GFX_GIF_LOADER_H

#include <base/types.h>

#include <engine/image.h>

#include <vector>

class SGifFrame
{
public:
	CImageInfo m_Image;
	int m_DelayMs = 100;
};

class CGifAnimation
{
public:
	std::vector<SGifFrame> m_vFrames;
	size_t m_Width = 0;
	size_t m_Height = 0;

	void Free();
	bool Empty() const { return m_vFrames.empty(); }
	size_t NumFrames() const { return m_vFrames.size(); }
};

class CGifLoader
{
public:
	CGifLoader() = delete;

	static bool Load(const uint8_t *pData, size_t DataSize, const char *pContextName, CGifAnimation &Animation);
};

#endif
