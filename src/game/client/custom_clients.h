#ifndef GAME_CLIENT_CUSTOM_CLIENTS_H
#define GAME_CLIENT_CUSTOM_CLIENTS_H

#include "custom_clients_ids.h"

#include <engine/graphics.h>

class IGraphics;
class IClient;
class IHttp;

constexpr int CUSTOM_CLIENT_ICON_MAX = 32;

enum ECustomClient
{
	CUSTOM_CLIENT_NONE = -1,
};

struct SCustomClientInfo
{
	int m_Id;
	int m_ColorBody;
	int m_ColorFeet;
	const char *m_pName;
	const char *m_pIconFile;
};

const SCustomClientInfo *CustomClientById(int Id);
const SCustomClientInfo *CustomClientByIcon(int Icon);
int CustomClientCount();
int CustomClientListId(int Index);

int CustomClientDetect(int ColorBody, int ColorFeet, const char *pSkinName);

const char *CustomClientName(int Id);
const char *CustomClientIconFile(int Id);

void CustomClientRenderIcon(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, vec2 Position, float Size, float Alpha = 1.0f);
float CustomClientIconWidth(float IconSize);

constexpr float CUSTOM_CLIENT_SCOREBOARD_ICON_SIZE = 16.0f;
constexpr float CUSTOM_CLIENT_SCOREBOARD_FONT_SIZE = 12.0f;
constexpr float CUSTOM_CLIENT_NAMEPLATE_ICON_SIZE = 32.0f;
constexpr float CUSTOM_CLIENT_NAMEPLATE_FONT_SIZE = 28.0f;

constexpr int CUSTOM_CLIENT_ANNOUNCE_TRIES = 4;

void CustomClientUpdateBestClient(IClient *pClient, IHttp *pHttp, bool Enabled);
bool CustomClientIsBestClient(int ClientId);
void CustomClientResetBestClient();

class CCustomClientIcons
{
public:
	void Init(IGraphics *pGraphics);
	void Unload();
	void Reload();

	IGraphics::CTextureHandle Texture(int Id) const;
	bool HasIcon(int Id) const;

private:
	IGraphics *m_pGraphics = nullptr;
	bool m_Loaded = false;
	IGraphics::CTextureHandle m_aTextures[CUSTOM_CLIENT_ICON_MAX];
};

#endif
