#include "gif_chat.h"

#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/version.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

static constexpr int MAX_ENTRIES = 6;
static constexpr int MAX_FRAMES = 90;
static constexpr size_t MAX_DIMENSION = 160;
static constexpr int64_t MAX_PAGE_SIZE = 4 * 1024 * 1024;
static constexpr int64_t MAX_GIF_SIZE = 12 * 1024 * 1024;
static constexpr float GIF_BOX_SIZE = 42.0f;

static double NowSeconds()
{
	return (double)time_get() / (double)time_freq();
}

static std::string ToLower(std::string Str)
{
	for(char &c : Str)
		c = (char)tolower((unsigned char)c);
	return Str;
}

static bool EndsWithCI(const std::string &Str, const char *pSuffix)
{
	const size_t Len = str_length(pSuffix);
	if(Str.size() < Len)
		return false;
	return str_comp_nocase(Str.c_str() + Str.size() - Len, pSuffix) == 0;
}

static std::string UrlHost(const std::string &Url)
{
	const size_t Scheme = Url.find("://");
	if(Scheme == std::string::npos)
		return "";
	size_t Start = Scheme + 3;
	if(Start < Url.size() && (Url[Start] == 'w' || Url[Start] == 'W'))
	{
		// strip the www prefix so the host matches the needle
		if(Url.compare(Start, 4, "www.") == 0)
			Start += 4;
	}
	const size_t End = Url.find('/', Start);
	return ToLower(Url.substr(Start, End == std::string::npos ? std::string::npos : End - Start));
}

static std::string UrlPath(const std::string &Url)
{
	const size_t Scheme = Url.find("://");
	if(Scheme == std::string::npos)
		return "";
	const size_t Start = Url.find('/', Scheme + 3);
	if(Start == std::string::npos)
		return "";
	std::string Path = Url.substr(Start);
	const size_t Query = Path.find('?');
	if(Query != std::string::npos)
		Path.resize(Query);
	const size_t Hash = Path.find('#');
	if(Hash != std::string::npos)
		Path.resize(Hash);
	return Path;
}

// chat text around the link would corrupt it, so only the token survives
static std::string ReadUrlToken(const std::string &Text, size_t Pos)
{
	static const char *const DELIMS = " \t\r\n\"'<>`()[]{}|\\^";
	size_t End = Pos;
	while(End < Text.size() && strchr(DELIMS, Text[End]) == nullptr)
		End++;
	std::string Token = Text.substr(Pos, End - Pos);
	// trailing chat punctuation corrupts the url, so strip it
	while(!Token.empty() && strchr(".,;:!?)]}>\"'", Token.back()) != nullptr)
		Token.pop_back();
	return Token;
}

bool CGifChat::ClassifyUrl(const std::string &Url, std::string &PageUrl, std::string &DirectGifUrl)
{
	PageUrl.clear();
	DirectGifUrl.clear();

	const std::string Lower = ToLower(Url);
	const std::string Host = UrlHost(Lower);
	const std::string LowerPath = UrlPath(Lower);
	const std::string Path = UrlPath(Url);
	const bool IsGif = EndsWithCI(LowerPath, ".gif");

	if(Host == "media.tenor.com")
	{
		if(IsGif)
			DirectGifUrl = Url;
		return !DirectGifUrl.empty();
	}
	if(Host == "tenor.com")
	{
		PageUrl = Url;
		return true;
	}
	if(Host == "media.giphy.com" || Host == "i.giphy.com")
	{
		if(IsGif)
			DirectGifUrl = Url;
		return !DirectGifUrl.empty();
	}
	if(Host == "giphy.com")
	{
		// giphy media links can be downloaded directly from the media host
		if(LowerPath.find("/media/") != std::string::npos && IsGif)
		{
			DirectGifUrl = "https://media.giphy.com" + Path;
			return true;
		}
		PageUrl = Url;
		return true;
	}
	return false;
}

bool CGifChat::ExtractGifUrl(const char *pText, std::string &PageUrl, std::string &DirectGifUrl)
{
	PageUrl.clear();
	DirectGifUrl.clear();
	if(pText == nullptr)
		return false;

	const std::string Text = pText;
	for(size_t Pos = 0; Pos < Text.size();)
	{
		const size_t Http = Text.find("http", Pos);
		if(Http == std::string::npos)
			break;
		Pos = Http + 4;
		if(Text.compare(Http, 7, "http://") != 0 && Text.compare(Http, 8, "https://") != 0)
			continue;

		const std::string Url = ReadUrlToken(Text, Http);
		if(ClassifyUrl(Url, PageUrl, DirectGifUrl))
			return true;
	}
	return false;
}

// pulls the direct .gif url out of a tenor/giphy page
static bool FindMediaGifInHtml(const std::string &Html, const std::string &HostNeedle, std::string &OutUrl)
{
	OutUrl.clear();
	// json escapes slashes as \/, so unescape them for the needle match
	std::string Clean = Html;
	size_t EscPos = 0;
	while((EscPos = Clean.find("\\/", EscPos)) != std::string::npos)
	{
		Clean.erase(EscPos, 1);
	}
	const std::string &Haystack = Clean;
	std::string Best;
	for(size_t Pos = 0; Pos + HostNeedle.size() <= Haystack.size();)
	{
		const size_t Found = Haystack.find(HostNeedle, Pos);
		if(Found == std::string::npos)
			break;
		Pos = Found + HostNeedle.size();

		// the host may be mid-url, so we grab the whole token and prepend the scheme
		size_t End = Pos;
		while(End < Haystack.size() && Haystack[End] != '"' && Haystack[End] != '\'' &&
			!isspace((unsigned char)Haystack[End]) && Haystack[End] != '<' && Haystack[End] != '>')
			End++;
		std::string Token = Haystack.substr(Found, End - Found);
		// html entities and the query string would defeat the .gif check, so strip them
		std::string Check = Token;
		const size_t Amp = Check.find("&amp;");
		if(Amp != std::string::npos)
			Check = Check.substr(0, Amp);
		const size_t Query = Check.find('?');
		if(Query != std::string::npos)
			Check.resize(Query);
		if(!EndsWithCI(Check, ".gif"))
			continue;

		const std::string Full = "https://" + Token;
		if(EndsWithCI(Check, "/tenor.gif") || EndsWithCI(Check, "/giphy.gif"))
		{
			OutUrl = Full;
			return true;
		}
		if(Best.empty())
			Best = Full;
	}
	if(!Best.empty())
	{
		OutUrl = Best;
		return true;
	}
	return false;
}

static bool ScaleImage(const CImageInfo &Src, size_t DstW, size_t DstH, CImageInfo &Dst)
{
	if(Src.m_pData == nullptr || Src.m_Format != CImageInfo::FORMAT_RGBA || Src.m_Width == 0 || Src.m_Height == 0)
		return false;

	Dst.m_Width = DstW;
	Dst.m_Height = DstH;
	Dst.m_Format = CImageInfo::FORMAT_RGBA;
	Dst.Allocate();
	if(Dst.m_pData == nullptr)
		return false;

	for(size_t y = 0; y < DstH; y++)
	{
		const size_t Sy = (size_t)((double)y * Src.m_Height / DstH);
		for(size_t x = 0; x < DstW; x++)
		{
			const size_t Sx = (size_t)((double)x * Src.m_Width / DstW);
			const uint8_t *pSrc = Src.m_pData + 4 * (Sy * Src.m_Width + Sx);
			uint8_t *pDst = Dst.m_pData + 4 * (y * DstW + x);
			pDst[0] = pSrc[0];
			pDst[1] = pSrc[1];
			pDst[2] = pSrc[2];
			pDst[3] = pSrc[3];
		}
	}
	return true;
}

static void DownscaleAnimation(CGifAnimation &Gif)
{
	if(Gif.m_vFrames.empty())
		return;

	const size_t W = Gif.m_vFrames[0].m_Image.m_Width;
	const size_t H = Gif.m_vFrames[0].m_Image.m_Height;
	if(W == 0 || H == 0)
		return;

	const size_t MaxSide = maximum(W, H);
	if(MaxSide <= MAX_DIMENSION && Gif.NumFrames() <= MAX_FRAMES)
		return;

	if(Gif.NumFrames() > MAX_FRAMES)
		Gif.m_vFrames.resize(MAX_FRAMES);

	if(MaxSide > MAX_DIMENSION)
	{
		const double Scale = (double)MAX_DIMENSION / MaxSide;
		const size_t DstW = maximum<size_t>(1, (size_t)(W * Scale + 0.5));
		const size_t DstH = maximum<size_t>(1, (size_t)(H * Scale + 0.5));
		for(SGifFrame &Frame : Gif.m_vFrames)
		{
			CImageInfo Scaled;
			if(ScaleImage(Frame.m_Image, DstW, DstH, Scaled))
			{
				Frame.m_Image.Free();
				Frame.m_Image = std::move(Scaled);
			}
		}
	}
}

void CGifChat::StartResolve(CEntry &Entry)
{
	Entry.m_pRequest = HttpGet(Entry.m_PageUrl.c_str());
	Entry.m_pRequest->WriteToMemory();
	Entry.m_pRequest->Timeout(CTimeout{10000, 0, 500, 10});
	Entry.m_pRequest->IpResolve(IPRESOLVE::V4);
	Entry.m_pRequest->MaxResponseSize(MAX_PAGE_SIZE);
	Entry.m_pRequest->HeaderString("User-Agent", FUSION_NAME "/" FUSION_VERSION);
	Http()->Run(Entry.m_pRequest);
	Entry.m_State = 0;
}

void CGifChat::StartDownload(CEntry &Entry)
{
	Entry.m_pRequest = HttpGet(Entry.m_GifUrl.c_str());
	Entry.m_pRequest->WriteToMemory();
	Entry.m_pRequest->Timeout(CTimeout{15000, 0, 800, 10});
	Entry.m_pRequest->IpResolve(IPRESOLVE::V4);
	Entry.m_pRequest->MaxResponseSize(MAX_GIF_SIZE);
	Entry.m_pRequest->HeaderString("User-Agent", FUSION_NAME "/" FUSION_VERSION);
	Http()->Run(Entry.m_pRequest);
	Entry.m_State = 1;
}

void CGifChat::FinishRequest(CEntry &Entry)
{
	std::shared_ptr<IHttpRequest> pTask = std::move(Entry.m_pRequest);
	Entry.m_pRequest = nullptr;
	if(!pTask)
		return;

	const bool Ok = pTask->State() == EHttpState::DONE && pTask->StatusCode() == 200;
	unsigned char *pData = nullptr;
	size_t Length = 0;
	if(Ok)
		pTask->Result(&pData, &Length);
	log_info("gifchat", "request done state=%d http=%d ok=%d len=%zu", Entry.m_State, (int)pTask->State(), (int)Ok, Length);

	// owned by the request (freed in ~IHttpRequest); do not free
	if(Entry.m_State == 0)
	{
		// resolving the page: find the gif url in the html
		if(!Ok || pData == nullptr || Length == 0)
		{
			Entry.m_State = 3;
			log_info("gifchat", "resolve failed -> state 3");
			return;
		}
		// tenor .gif links serve the binary directly, skipping the html parse
		if(Length >= 6 && memcmp(pData, "GIF8", 4) == 0)
		{
			log_info("gifchat", "resolve returned gif binary, decoding");
			Entry.m_GifUrl = Entry.m_PageUrl;
			const bool Decoded = DecodeGif(pData, Length, Entry);
			if(!Decoded)
			{
				Entry.m_State = 3;
				log_info("gifchat", "direct gif decode failed -> state 3");
			}
			return;
		}
		const std::string Html((const char *)pData, Length);
		if(FindMediaGifInHtml(Html, ToLower(UrlHost(Entry.m_PageUrl)) == "tenor.com" ? "media.tenor.com/" : "media.giphy.com/", Entry.m_GifUrl))
		{
			log_info("gifchat", "resolved media url %s", Entry.m_GifUrl.c_str());
			StartDownload(Entry);
		}
		else
		{
			Entry.m_State = 3;
			log_info("gifchat", "no media url in page -> state 3");
		}
		return;
	}

	// direct gif download
	if(!Ok || pData == nullptr || Length == 0)
	{
		Entry.m_State = 3;
		log_info("gifchat", "gif download failed -> state 3");
		return;
	}

	const bool Decoded = DecodeGif(pData, Length, Entry);
	if(!Decoded)
	{
		Entry.m_State = 3;
		log_info("gifchat", "gif decode failed -> state 3");
	}
}

bool CGifChat::DecodeGif(const uint8_t *pData, size_t Length, CEntry &Entry)
{
	CGifAnimation Gif;
	const bool Loaded = CGifLoader::Load(pData, Length, Entry.m_GifUrl.c_str(), Gif);
	if(!Loaded || Gif.Empty())
	{
		Gif.Free();
		return false;
	}

	DownscaleAnimation(Gif);
	if(Gif.Empty())
	{
		Gif.Free();
		return false;
	}

	Entry.m_Gif = std::move(Gif);
	Entry.m_Width = Entry.m_Gif.m_Width;
	Entry.m_Height = Entry.m_Gif.m_Height;
	if(Entry.m_Width == 0 || Entry.m_Height == 0)
	{
		Entry.m_Gif.Free();
		return false;
	}

	Entry.m_Texture = Graphics()->LoadTextureRaw(Entry.m_Gif.m_vFrames[0].m_Image, Graphics()->TextureLoadFlags(), "chatgif");
	Entry.m_UploadedFrame = 0;
	Entry.m_FrameStart = NowSeconds();
	Entry.m_State = 2;
	log_info("gifchat", "loaded gif %s (%dx%d, %d frames)", Entry.m_PageUrl.c_str(), Entry.m_Width, Entry.m_Height, (int)Entry.m_Gif.NumFrames());
	log_info("gifchat", "invalidating chat lines");

	// chat line heights were measured without the gif; rebuild them
	InvalidateChatLines();
	return true;
}

void CGifChat::ProcessEntry(CEntry &Entry)
{
	if(Entry.m_pRequest && Entry.m_pRequest->Done())
		FinishRequest(Entry);
}

void CGifChat::UploadFrame(CEntry &Entry, int Frame)
{
	const int NumFrames = (int)Entry.m_Gif.NumFrames();
	if(NumFrames <= 0 || Frame < 0 || Frame >= NumFrames)
		return;
	const SGifFrame &F = Entry.m_Gif.m_vFrames[(size_t)Frame];
	if(F.m_Image.m_pData == nullptr || F.m_Image.m_Format != CImageInfo::FORMAT_RGBA)
		return;
	if(Graphics()->ReloadTexture(Entry.m_Texture, F.m_Image, "chatgif"))
		Entry.m_UploadedFrame = Frame;
}

void CGifChat::EvictOldest()
{
	while((int)m_vEntries.size() > MAX_ENTRIES)
	{
		int OldestId = -1;
		double Oldest = 1e30;
		for(const auto &Pair : m_vEntries)
		{
			if(Pair.second->m_LastUsed < Oldest)
			{
				Oldest = Pair.second->m_LastUsed;
				OldestId = Pair.first;
			}
		}
		if(OldestId < 0)
			return; // nothing evictable while everything is visible

		auto It = m_vEntries.find(OldestId);
		if(It == m_vEntries.end())
			return;
		for(auto UrlIt = m_vByUrl.begin(); UrlIt != m_vByUrl.end();)
		{
			if(UrlIt->second == OldestId)
				UrlIt = m_vByUrl.erase(UrlIt);
			else
				++UrlIt;
		}
		if(It->second->m_Texture.IsValid() && !It->second->m_Texture.IsNullTexture())
			Graphics()->UnloadTexture(&It->second->m_Texture);
		if(It->second->m_pRequest)
			It->second->m_pRequest->Abort();
		m_vEntries.erase(It);
	}
}

void CGifChat::OnRender()
{
	m_Now = NowSeconds();

	for(auto &Pair : m_vEntries)
		ProcessEntry(*Pair.second);

	EvictOldest();

	for(auto &Pair : m_vEntries)
	{
		CEntry &Entry = *Pair.second;
		const bool WasVisible = Entry.m_bVisible;
		Entry.m_bVisible = false;
		if(Entry.m_State != 2 || Entry.m_Gif.Empty() || !WasVisible)
			continue;

		Entry.m_LastUsed = m_Now;
		const int NumFrames = (int)Entry.m_Gif.NumFrames();
		if(NumFrames <= 1 || Entry.m_UploadedFrame < 0)
			continue;

		const int Current = Entry.m_UploadedFrame % NumFrames;
		double Delay = (double)Entry.m_Gif.m_vFrames[(size_t)Current].m_DelayMs / 1000.0;
		if(Delay <= 0.01)
			Delay = 0.1;
		Delay = maximum(Delay, 1.0 / 30.0);

		const double Elapsed = m_Now - Entry.m_FrameStart;
		if(Elapsed < Delay)
			continue;

		const int Steps = (int)(Elapsed / Delay);
		Entry.m_FrameStart += (double)Steps * Delay;
		UploadFrame(Entry, (Current + Steps) % NumFrames);
	}
}

void CGifChat::OnShutdown()
{
	for(auto &Pair : m_vEntries)
	{
		if(Pair.second->m_pRequest)
			Pair.second->m_pRequest->Abort();
		if(Pair.second->m_Texture.IsValid() && !Pair.second->m_Texture.IsNullTexture())
			Graphics()->UnloadTexture(&Pair.second->m_Texture);
	}
	m_vEntries.clear();
	m_vByUrl.clear();
}

int CGifChat::OnChatText(const char *pText)
{
	if(!g_Config.m_FcGifChat || pText == nullptr || pText[0] == '\0')
		return -1;

	std::string PageUrl, DirectGifUrl;
	if(!ExtractGifUrl(pText, PageUrl, DirectGifUrl))
		return -1;

	const std::string Key = PageUrl.empty() ? DirectGifUrl : PageUrl;
	auto Found = m_vByUrl.find(Key);
	if(Found != m_vByUrl.end())
		return Found->second;

	auto Entry = std::make_shared<CEntry>();
	Entry->m_PageUrl = Key;
	Entry->m_LastUsed = m_Now;
	const int Id = m_NextId++;
	log_info("gifchat", "new entry id=%d url=%s direct=%d", Id, Key.c_str(), DirectGifUrl.empty() ? 0 : 1);

	if(!DirectGifUrl.empty())
	{
		Entry->m_GifUrl = DirectGifUrl;
		Entry->m_State = 1;
		StartDownload(*Entry);
	}
	else
	{
		StartResolve(*Entry);
	}

	m_vEntries[Id] = Entry;
	m_vByUrl[Key] = Id;
	EvictOldest();
	return Id;
}

CGifChat::CEntry *CGifChat::GetEntry(int Id)
{
	if(Id < 0)
		return nullptr;
	auto It = m_vEntries.find(Id);
	if(It == m_vEntries.end())
		return nullptr;
	return It->second.get();
}

bool CGifChat::GetDrawSize(const CEntry *pEntry, float &Width, float &Height)
{
	Width = 0.0f;
	Height = 0.0f;
	if(pEntry == nullptr || pEntry->m_State != 2 || pEntry->m_Width == 0 || pEntry->m_Height == 0)
		return false;

	const double Scale = minimum((double)GIF_BOX_SIZE / pEntry->m_Width, (double)GIF_BOX_SIZE / pEntry->m_Height);
	Width = (float)(pEntry->m_Width * Scale);
	Height = (float)(pEntry->m_Height * Scale);
	return true;
}

void CGifChat::InvalidateChatLines()
{
	if(GameClient())
		GameClient()->m_Chat.InvalidateGifLines();
}
