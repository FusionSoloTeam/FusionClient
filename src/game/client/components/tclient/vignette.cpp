#include "vignette.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <cmath>

void CVignette::EnsureVignetteTexture()
{
	const int Mode = g_Config.m_TcVignetteMode;
	const int Radius = g_Config.m_TcVignetteRadius;
	const int Smoothness = g_Config.m_TcVignetteSmoothness;
	const int Aspect = (int)(Graphics()->ScreenAspect() * 100.0f + 0.5f);
	if(m_VignetteTexture.IsValid() && m_VignetteTextureMode == Mode && m_VignetteTextureRadius == Radius && m_VignetteTextureSmoothness == Smoothness && m_VignetteTextureAspect == Aspect)
		return;

	constexpr int Size = 256;
	const float AspectF = Graphics()->ScreenAspect();
	const float Norm = 0.5f * std::sqrt(AspectF * AspectF + 1.0f);
	const float RadiusF = Radius / 100.0f;
	const float SmoothF = Smoothness / 100.0f;
	const float Inner = std::clamp(1.0f - RadiusF - SmoothF * 0.2f, 0.0f, 0.98f);

	CImageInfo Image;
	Image.m_Width = Size;
	Image.m_Height = Size;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.AllocateFillZero();

	for(int y = 0; y < Size; y++)
	{
		const float V = (y + 0.5f) / Size - 0.5f;
		for(int x = 0; x < Size; x++)
		{
			const float U = (x + 0.5f) / Size - 0.5f;
			float T;
			if(Mode == 1)
				T = std::fabs(U) * 2.0f;
			else if(Mode == 2)
				T = std::fabs(V) * 2.0f;
			else
				T = std::sqrt(U * U * AspectF * AspectF + V * V) / Norm;

			float S = 0.0f;
			if(T > Inner)
			{
				const float St = std::clamp((T - Inner) / (1.0f - Inner), 0.0f, 1.0f);
				S = St * St * (3.0f - 2.0f * St);
			}
			const uint8_t Alpha = (uint8_t)(std::pow(S, 0.8f) * 255.0f + 0.5f);
			uint8_t *pPixel = Image.m_pData + ((size_t)y * Size + x) * 4;
			pPixel[0] = 255;
			pPixel[1] = 255;
			pPixel[2] = 255;
			pPixel[3] = Alpha;
		}
	}

	const IGraphics::CTextureHandle NewTexture = Graphics()->LoadTextureRaw(Image, 0, "vignette-gradient");
	Image.Free();
	if(NewTexture.IsValid())
	{
		if(m_VignetteTexture.IsValid())
			Graphics()->UnloadTexture(&m_VignetteTexture);
		m_VignetteTexture = NewTexture;
		m_VignetteTextureMode = Mode;
		m_VignetteTextureRadius = Radius;
		m_VignetteTextureSmoothness = Smoothness;
		m_VignetteTextureAspect = Aspect;
	}
}

void CVignette::OnRender()
{
	if(!g_Config.m_TcVignette)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const float Strength = g_Config.m_TcVignetteStrength / 100.0f;
	const float Darken = g_Config.m_TcVignetteDarken / 100.0f;
	if(Strength > 0.0f && g_Config.m_TcVignetteBlur > 0)
		Graphics()->VignetteBlur();
	if(Strength <= 0.0f || Darken <= 0.0f)
		return;

	Ui()->MapScreen();
	EnsureVignetteTexture();
	if(!m_VignetteTexture.IsValid())
		return;

	const CUIRect *pScreen = Ui()->Screen();

	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcVignetteColor, true));

	Graphics()->TextureSet(m_VignetteTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, std::clamp(Strength * Darken * 0.85f, 0.0f, 1.0f));
	const IGraphics::CQuadItem Quad(pScreen->x, pScreen->y, pScreen->w, pScreen->h);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->TextureClear();
}

void CVignette::OnShutdown()
{
	if(m_VignetteTexture.IsValid())
	{
		Graphics()->UnloadTexture(&m_VignetteTexture);
		m_VignetteTextureMode = -1;
		m_VignetteTextureRadius = -1;
		m_VignetteTextureSmoothness = -1;
		m_VignetteTextureAspect = -1;
	}
}
