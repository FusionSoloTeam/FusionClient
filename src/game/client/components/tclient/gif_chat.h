// gif previews for tenor/giphy chat links
#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_GIF_CHAT_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_GIF_CHAT_H

#include <base/time.h>

#include <engine/graphics.h>
#include <engine/http.h>

#include <engine/gfx/gif_loader.h>

#include <game/client/component.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

class CGifChat : public CComponent
{
public:
	struct CEntry
	{
		CGifAnimation m_Gif;
		IGraphics::CTextureHandle m_Texture;
		int m_UploadedFrame = -1;
		double m_FrameStart = 0.0;
		double m_LastUsed = 0.0;
		bool m_bVisible = false;
		int m_State = 0; // 0 resolve, 1 download, 2 ready, 3 failed
		std::string m_PageUrl;
		std::string m_GifUrl;
		std::shared_ptr<IHttpRequest> m_pRequest;
		size_t m_Width = 0;
		size_t m_Height = 0;
	};

	// returns the entry id for the gif link in a line, starting the load; -1 if none
	int OnChatText(const char *pText);

	CEntry *GetEntry(int Id);

	// draw size in chat units (300-high space) for a ready entry
	static bool GetDrawSize(const CEntry *pEntry, float &Width, float &Height);

	void InvalidateChatLines();

	void OnRender() override;
	void OnShutdown() override;
	int Sizeof() const override { return sizeof(*this); }

private:
	static bool ExtractGifUrl(const char *pText, std::string &PageUrl, std::string &DirectGifUrl);
	static bool ClassifyUrl(const std::string &Url, std::string &PageUrl, std::string &DirectGifUrl);

	void StartResolve(CEntry &Entry);
	void StartDownload(CEntry &Entry);
	void FinishRequest(CEntry &Entry);
	bool DecodeGif(const uint8_t *pData, size_t Length, CEntry &Entry);
	void ProcessEntry(CEntry &Entry);
	void UploadFrame(CEntry &Entry, int Frame);
	void EvictOldest();

	std::map<int, std::shared_ptr<CEntry>> m_vEntries;
	std::map<std::string, int> m_vByUrl;
	int m_NextId = 1;
	double m_Now = 0.0;
};

#endif
