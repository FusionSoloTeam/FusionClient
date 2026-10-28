#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_VIDEO_LOADER_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_VIDEO_LOADER_H

#include <base/types.h>

#include <engine/gfx/gif_loader.h>

class CVideoLoader
{
public:
	CVideoLoader() = delete;

	enum
	{
		ALPHA_OPAQUE = 0,
		ALPHA_BLACK = 1,
		ALPHA_WHITE = 2,
	};

	static bool IsVideoFile(const char *pPath);

	static bool Load(const char *pPath, size_t TargetWidth, size_t TargetHeight, size_t MaxFrames, int AlphaMode, CGifAnimation &Animation);
};

#endif
