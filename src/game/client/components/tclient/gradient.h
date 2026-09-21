#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_GRADIENT_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_GRADIENT_H

#include <game/client/component.h>

#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <vector>

class CGameClient;

// Hook invoked by the UI label renderer when "everything" gradient is enabled.
extern void (*BCGradient_ApplyEverythingHook)(CTextCursor *pCursor, const char *pText, int Length);

class CBcGradient : public CComponent
{
	int m_LastEverything = -1;
	int m_LastNick = -1;
	int m_LastClan = -1;

	void RefreshCachedText();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnRender() override;

	static float AnimatePhase(double GlobalTime);
	static void GetSkinToneColors(int ClientId, const CGameClient *pGameClient, ColorRGBA &Body, ColorRGBA &Feet);
	static void GetAnimatedEndpointColors(int ClientId, const CGameClient *pGameClient, float Phase, ColorRGBA &Color1, ColorRGBA &Color2);
	static std::vector<STextColorSplit> BuildAnimatedColorSplits(const char *pText, const ColorRGBA &Color1, const ColorRGBA &Color2, float Phase);
	static std::vector<STextColorSplit> BuildAnimatedTextSplits(const char *pText, int ClientId, CGameClient *pGameClient, float Phase);
	static void ApplyEverythingGradient(CTextCursor *pCursor, const char *pText, int Length, CGameClient *pGameClient);
};

#endif
