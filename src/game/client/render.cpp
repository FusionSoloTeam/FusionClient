/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "render.h"

#include "animstate.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/client_data7.h>
#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/components/tclient/skin_gradient.h>
#include <game/client/gameclient.h>
#include <game/mapitems.h>

#include <cmath>

static void VcSkinGradientSpriteSubset(int Id, float *pU0, float *pV0, float *pU1, float *pV1)
{
	const CDataSprite *pSprite = &client_data7::g_pData->m_aSprites[Id];
	const int GridX = pSprite->m_pSet->m_Gridx;
	const int GridY = pSprite->m_pSet->m_Gridy;
	*pU0 = pSprite->m_X / (float)GridX + 0.5f / (float)(GridX * 32);
	*pU1 = (pSprite->m_X + pSprite->m_W) / (float)GridX - 0.5f / (float)(GridX * 32);
	*pV0 = pSprite->m_Y / (float)GridY + 0.5f / (float)(GridY * 32);
	*pV1 = (pSprite->m_Y + pSprite->m_H) / (float)GridY - 0.5f / (float)(GridY * 32);
}

CSkinDescriptor::CSkinDescriptor()
{
	Reset();
}

void CSkinDescriptor::Reset()
{
	m_Flags = 0;
	m_aSkinName[0] = '\0';
	for(auto &Sixup : m_aSixup)
	{
		Sixup.Reset();
	}
}

bool CSkinDescriptor::IsValid() const
{
	return (m_Flags & (FLAG_SIX | FLAG_SEVEN)) != 0;
}

bool CSkinDescriptor::operator==(const CSkinDescriptor &Other) const
{
	if(m_Flags != Other.m_Flags)
	{
		return false;
	}

	if(m_Flags & FLAG_SIX)
	{
		if(str_comp(m_aSkinName, Other.m_aSkinName) != 0)
		{
			return false;
		}
	}

	if(m_Flags & FLAG_SEVEN)
	{
		for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
		{
			if(m_aSixup[Dummy] != Other.m_aSixup[Dummy])
			{
				return false;
			}
		}
	}

	return true;
}

void CSkinDescriptor::CSixup::Reset()
{
	for(auto &aSkinPartName : m_aaSkinPartNames)
	{
		aSkinPartName[0] = '\0';
	}
	m_BotDecoration = false;
	m_XmasHat = false;
}

bool CSkinDescriptor::CSixup::operator==(const CSixup &Other) const
{
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
	{
		if(str_comp(m_aaSkinPartNames[Part], Other.m_aaSkinPartNames[Part]) != 0)
		{
			return false;
		}
	}
	return m_BotDecoration == Other.m_BotDecoration &&
	       m_XmasHat == Other.m_XmasHat;
}

void CRenderTools::Init(IGraphics *pGraphics, ITextRender *pTextRender, CGameClient *pGameClient)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
	m_pGameClient = pGameClient;
	m_TeeQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, 64.f * 0.4f);

	// Feet
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);

	// Mirrored Feet
	Graphics()->QuadsSetSubsetFree(1, 0, 0, 0, 0, 1, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);
	Graphics()->QuadsSetSubsetFree(1, 0, 0, 0, 0, 1, 1, 1);
	Graphics()->QuadContainerAddSprite(m_TeeQuadContainerIndex, -32.f, -16.f, 64.f, 32.f);

	Graphics()->QuadContainerUpload(m_TeeQuadContainerIndex);
}

void CRenderTools::RenderCursor(vec2 Center, float Size, float Alpha) const
{
	Graphics()->WrapClamp();
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	IGraphics::CQuadItem QuadItem(Center.x, Center.y, Size, Size);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
	Graphics()->WrapNormal();
}

void CRenderTools::RenderIcon(int ImageId, int SpriteId, const CUIRect *pRect, const ColorRGBA *pColor) const
{
	Graphics()->TextureSet(g_pData->m_aImages[ImageId].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SelectSprite(SpriteId);
	if(pColor)
		Graphics()->SetColor(pColor->r * pColor->a, pColor->g * pColor->a, pColor->b * pColor->a, pColor->a);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CRenderTools::GetRenderTeeAnimScaleAndBaseSize(const CTeeRenderInfo *pInfo, float &AnimScale, float &BaseSize)
{
	AnimScale = pInfo->m_Size * 1.0f / 64.0f;
	BaseSize = pInfo->m_Size;
}

void CRenderTools::GetRenderTeeBodyScale(float BaseSize, float &BodyScale)
{
	BodyScale = g_Config.m_ClFatSkins ? BaseSize * 1.3f : BaseSize;
	BodyScale /= 64.0f;
}

void CRenderTools::GetRenderTeeFeetScale(float BaseSize, float &FeetScaleWidth, float &FeetScaleHeight)
{
	FeetScaleWidth = BaseSize / 64.0f;
	FeetScaleHeight = (BaseSize / 2) / 32.0f;
}

void CRenderTools::GetRenderTeeBodySize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &BodyOffset, float &Width, float &Height)
{
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float BodyScale;
	GetRenderTeeBodyScale(BaseSize, BodyScale);

	Width = pInfo->m_SkinMetrics.m_Body.WidthNormalized() * 64.0f * BodyScale;
	Height = pInfo->m_SkinMetrics.m_Body.HeightNormalized() * 64.0f * BodyScale;
	BodyOffset.x = pInfo->m_SkinMetrics.m_Body.OffsetXNormalized() * 64.0f * BodyScale;
	BodyOffset.y = pInfo->m_SkinMetrics.m_Body.OffsetYNormalized() * 64.0f * BodyScale;
}

void CRenderTools::GetRenderTeeFeetSize(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &FeetOffset, float &Width, float &Height)
{
	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);

	float FeetScaleWidth, FeetScaleHeight;
	GetRenderTeeFeetScale(BaseSize, FeetScaleWidth, FeetScaleHeight);

	Width = pInfo->m_SkinMetrics.m_Feet.WidthNormalized() * 64.0f * FeetScaleWidth;
	Height = pInfo->m_SkinMetrics.m_Feet.HeightNormalized() * 32.0f * FeetScaleHeight;
	FeetOffset.x = pInfo->m_SkinMetrics.m_Feet.OffsetXNormalized() * 64.0f * FeetScaleWidth;
	FeetOffset.y = pInfo->m_SkinMetrics.m_Feet.OffsetYNormalized() * 32.0f * FeetScaleHeight;
}

void CRenderTools::GetRenderTeeOffsetToRenderedTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, vec2 &TeeOffsetToMid)
{
	if(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_BODY).IsValid())
	{
		TeeOffsetToMid = vec2(0.0f, pInfo->m_Size * 0.12f);
		return;
	}

	float AnimScale, BaseSize;
	GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);
	vec2 BodyPos = vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;

	float AssumedScale = BaseSize / 64.0f;

	// just use the lowest feet
	vec2 FeetPos;
	const CAnimKeyframe *pFoot = pAnim->GetFrontFoot();
	FeetPos = vec2(pFoot->m_X * AnimScale, pFoot->m_Y * AnimScale);
	pFoot = pAnim->GetBackFoot();
	FeetPos = vec2(FeetPos.x, std::max(FeetPos.y, pFoot->m_Y * AnimScale));

	vec2 BodyOffset;
	float BodyWidth, BodyHeight;
	GetRenderTeeBodySize(pAnim, pInfo, BodyOffset, BodyWidth, BodyHeight);

	// -32 is the assumed min relative position for the quad
	float MinY = -32.0f * AssumedScale;
	// the body pos shifts the body away from center
	MinY += BodyPos.y;
	// the actual body is smaller though, because it doesn't use the full skin image in most cases
	MinY += BodyOffset.y;

	vec2 FeetOffset;
	float FeetWidth, FeetHeight;
	GetRenderTeeFeetSize(pAnim, pInfo, FeetOffset, FeetWidth, FeetHeight);

	// MaxY builds up from the MinY
	float MaxY = MinY + BodyHeight;
	// if the body is smaller than the total feet offset, use feet
	// since feet are smaller in height, respect the assumed relative position
	MaxY = std::max(MaxY, (-16.0f * AssumedScale + FeetPos.y) + FeetOffset.y + FeetHeight);

	// now we got the full rendered size
	float FullHeight = (MaxY - MinY);

	// next step is to calculate the offset that was created compared to the assumed relative position
	float MidOfRendered = MinY + FullHeight / 2.0f;

	// TODO: x coordinate is ignored for now, bcs it's not really used yet anyway
	TeeOffsetToMid.x = 0;
	// negative value, because the calculation that uses this offset should work with addition.
	TeeOffsetToMid.y = -MidOfRendered;
}

void CRenderTools::RenderTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha, bool IsLocal) const
{
	RenderTee(pAnim, pInfo, Emote, Dir, Pos, Alpha, vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), 0.0f, 0.0f, IsLocal);
}

void CRenderTools::RenderTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha, vec2 BodyScale, vec2 FeetScale, float BodyAngle, float FeetAngle) const
{
	RenderTee(pAnim, pInfo, Emote, Dir, Pos, Alpha, BodyScale, FeetScale, BodyAngle, FeetAngle, false);
}

void CRenderTools::RenderTee(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha, vec2 BodyScale, vec2 FeetScale, float BodyAngle, float FeetAngle, bool IsLocal) const
{
	if(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_BODY).IsValid())
		RenderTee7(pAnim, pInfo, Emote, Dir, Pos, Alpha, BodyScale, FeetScale, BodyAngle, FeetAngle, IsLocal);
	else
		RenderTee6(pAnim, pInfo, Emote, Dir, Pos, Alpha, BodyScale, FeetScale, BodyAngle, FeetAngle, IsLocal);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0);
}

void CRenderTools::RenderTee7(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha, vec2 BodyScale, vec2 FeetScale, float BodyAngle, float FeetAngle, bool IsLocal) const
{
	vec2 Direction = Dir;
	vec2 Position = Pos;
	const bool IsBot = pInfo->m_aSixup[g_Config.m_ClDummy].m_BotTexture.IsValid();

	// TClient
	const float TinyBodyScale = 0.7f;
	const float TinyFeetScale = 0.85f;
	float SizeMultiplier = (g_Config.m_TcTinyTeeSize / 100.0f);
	bool TinyTee = g_Config.m_TcTinyTees && (IsLocal || g_Config.m_TcTinyTeesOthers);

	// first pass we draw the outline
	// second pass we draw the filling
	for(int Pass = 0; Pass < 2; Pass++)
	{
		bool OutLine = Pass == 0;

		for(int Filling = 0; Filling < 2; Filling++)
		{
			float AnimScale = pInfo->m_Size * 1.0f / 64.0f;
			float BaseSize = pInfo->m_Size;
			if(TinyTee) // TClient
			{
				BaseSize *= TinyBodyScale * SizeMultiplier;
				AnimScale *= TinyBodyScale * SizeMultiplier;
			}
			const vec2 TeeAnchor = Position + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
			const float TeeAnchorSize = BaseSize * BodyScale.x;
			if(Filling == 1)
			{
				vec2 BodyPos = Position + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
				IGraphics::CQuadItem BodyItem(BodyPos.x, BodyPos.y, BaseSize * BodyScale.x, BaseSize * BodyScale.y);
				IGraphics::CQuadItem Item;

				if(IsBot && !OutLine)
				{
					IGraphics::CQuadItem BotItem(BodyPos.x + (2.f / 3.f) * AnimScale, BodyPos.y + (-16 + 2.f / 3.f) * AnimScale, BaseSize * BodyScale.x, BaseSize * BodyScale.y);

					// draw bot visuals (background)
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotTexture);
					Graphics()->QuadsBegin();
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BOT_BACKGROUND);
					Item = BotItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();

					// draw bot visuals (foreground)
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotTexture);
					Graphics()->QuadsBegin();
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BOT_FOREGROUND);
					Item = BotItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotColor.WithAlpha(Alpha));
					Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BOT_GLOW);
					Item = BotItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}

				// draw decoration
				const IGraphics::CTextureHandle &DecorationTexture = pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_DECORATION);
				if(DecorationTexture.IsValid())
				{
					Graphics()->TextureSet(DecorationTexture);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_DECORATION].WithAlpha(Alpha));
					Graphics()->SelectSprite7(OutLine ? client_data7::SPRITE_TEE_DECORATION_OUTLINE : client_data7::SPRITE_TEE_DECORATION);
					Item = BodyItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}

				// draw body (behind marking)
				const IGraphics::CTextureHandle &BodyTexture = pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_BODY);
				Graphics()->TextureSet(BodyTexture);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);
				if(OutLine)
				{
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BODY_OUTLINE);
				}
				else
				{
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_BODY].WithAlpha(Alpha));
					Graphics()->SelectSprite7(client_data7::SPRITE_TEE_BODY);
				}
				Item = BodyItem;
				Graphics()->QuadsDraw(&Item, 1);
				Graphics()->QuadsEnd();

				if(!OutLine && VcSkinGradient::Enabled(IsLocal))
				{
					float GradientU0, GradientV0, GradientU1, GradientV1;
					VcSkinGradientSpriteSubset(client_data7::SPRITE_TEE_BODY, &GradientU0, &GradientV0, &GradientU1, &GradientV1);
					VcSkinGradient::RenderTeeOverlay(Graphics(),
						BodyTexture,
						vec2(BodyItem.m_X + BodyItem.m_Width * 0.5f, BodyItem.m_Y + BodyItem.m_Height * 0.5f),
						vec2(BodyItem.m_Width, BodyItem.m_Height),
						TeeAnchor, TeeAnchorSize,
						pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle,
						Alpha, GradientU0, GradientV0, GradientU1, GradientV1);
				}

				// draw marking
				const IGraphics::CTextureHandle &MarkingTexture = pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_MARKING);
				if(MarkingTexture.IsValid() && !OutLine)
				{
					Graphics()->TextureSet(MarkingTexture);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);
					ColorRGBA MarkingColor = pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_MARKING];
					Graphics()->SetColor(MarkingColor.r * MarkingColor.a, MarkingColor.g * MarkingColor.a, MarkingColor.b * MarkingColor.a, MarkingColor.a * Alpha);
					Graphics()->SelectSprite7(client_data7::SPRITE_TEE_MARKING);
					Item = BodyItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}

				// draw body (in front of marking)
				if(!OutLine)
				{
					Graphics()->TextureSet(BodyTexture);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					for(int t = 0; t < 2; t++)
					{
						Graphics()->SelectSprite7(t == 0 ? client_data7::SPRITE_TEE_BODY_SHADOW : client_data7::SPRITE_TEE_BODY_UPPER_OUTLINE);
						Item = BodyItem;
						Graphics()->QuadsDraw(&Item, 1);
					}
					Graphics()->QuadsEnd();
				}

				// draw eyes
				Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_EYES));
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);
				if(IsBot)
				{
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_BotColor.WithAlpha(Alpha));
					Emote = EMOTE_SURPRISE;
				}
				else
				{
					Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_EYES].WithAlpha(Alpha));
				}
				if(Pass == 1)
				{
					switch(Emote)
					{
					case EMOTE_PAIN:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_EYES_PAIN);
						break;
					case EMOTE_HAPPY:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_EYES_HAPPY);
						break;
					case EMOTE_SURPRISE:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_EYES_SURPRISE);
						break;
					case EMOTE_ANGRY:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_EYES_ANGRY);
						break;
					default:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_EYES_NORMAL);
						break;
					}

					float EyeScale = BaseSize * 0.60f;
					float h = Emote == EMOTE_BLINK ? BaseSize * 0.15f / 2.0f : EyeScale / 2.0f;
					vec2 Offset = vec2(Direction.x * 0.125f, -0.05f + Direction.y * 0.10f) * BaseSize;
					IGraphics::CQuadItem QuadItem(BodyPos.x + Offset.x, BodyPos.y + Offset.y, EyeScale, h);
					Graphics()->QuadsDraw(&QuadItem, 1);
				}
				Graphics()->QuadsEnd();

				// draw xmas hat
				if(!OutLine && pInfo->m_aSixup[g_Config.m_ClDummy].m_HatTexture.IsValid())
				{
					Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].m_HatTexture);
					Graphics()->QuadsBegin();
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);
					Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
					int Flag = Direction.x < 0.0f ? IGraphics::SPRITE_FLAG_FLIP_X : 0;
					switch(pInfo->m_aSixup[g_Config.m_ClDummy].m_HatSpriteIndex)
					{
					case 0:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HATS_TOP1, Flag);
						break;
					case 1:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HATS_TOP2, Flag);
						break;
					case 2:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HATS_SIDE1, Flag);
						break;
					case 3:
						Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HATS_SIDE2, Flag);
					}
					Item = BodyItem;
					Graphics()->QuadsDraw(&Item, 1);
					Graphics()->QuadsEnd();
				}
			}

			if(TinyTee) // TClient
			{
				BaseSize /= TinyBodyScale * SizeMultiplier;
				AnimScale /= TinyBodyScale * SizeMultiplier;
			}

			// draw feet
			Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_FEET));
			Graphics()->QuadsBegin();
			const CAnimKeyframe *pFoot = Filling ? pAnim->GetFrontFoot() : pAnim->GetBackFoot();

			float w = (BaseSize / 2.1f) * FeetScale.x;
			float h = (BaseSize / 2.1f) * FeetScale.y;

			if(TinyTee) // TClient
			{
				w *= TinyFeetScale * SizeMultiplier;
				h *= TinyFeetScale * SizeMultiplier;
			}

			Graphics()->QuadsSetRotation(pFoot->m_Angle * pi * 2 + FeetAngle);

			if(OutLine)
			{
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_FOOT_OUTLINE);
			}
			else
			{
				bool Indicate = !pInfo->m_GotAirJump && g_Config.m_ClAirjumpindicator;
				float ColorScale = 1.0f;
				if(Indicate)
					ColorScale = 0.5f;
				Graphics()->SetColor(
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].r * ColorScale,
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].g * ColorScale,
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].b * ColorScale,
					pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_FEET].a * Alpha);
				Graphics()->SelectSprite7(client_data7::SPRITE_TEE_FOOT);
			}

			IGraphics::CQuadItem QuadItem(Position.x + pFoot->m_X * AnimScale, Position.y + pFoot->m_Y * AnimScale, w, h);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->QuadsEnd();

			if(!OutLine && VcSkinGradient::Enabled(IsLocal) && g_Config.m_FcSkinGradientFeet)
			{
				float GradientU0, GradientV0, GradientU1, GradientV1;
				VcSkinGradientSpriteSubset(client_data7::SPRITE_TEE_FOOT, &GradientU0, &GradientV0, &GradientU1, &GradientV1);
				VcSkinGradient::RenderTeeOverlay(Graphics(),
					pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_FEET),
					vec2(QuadItem.m_X + QuadItem.m_Width * 0.5f, QuadItem.m_Y + QuadItem.m_Height * 0.5f),
					vec2(QuadItem.m_Width, QuadItem.m_Height),
					TeeAnchor, TeeAnchorSize,
					pFoot->m_Angle * pi * 2 + FeetAngle,
					Alpha, GradientU0, GradientV0, GradientU1, GradientV1, false);
			}
		}
	}
}

void CRenderTools::RenderTee6(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha, vec2 BodyScale, vec2 FeetScale, float BodyAngle, float FeetAngle, bool IsLocal) const
{
	vec2 Direction = Dir;
	vec2 Position = Pos;

	const float TinyBodyScale = 0.7f;
	const float TinyFeetScale = 0.85f;
	float SizeMultiplier = (g_Config.m_TcTinyTeeSize / 100.0f);
	bool TinyTee = g_Config.m_TcTinyTees && (IsLocal || g_Config.m_TcTinyTeesOthers);

	const CSkin::CSkinTextures *pSkinTextures = pInfo->m_CustomColoredSkin ? &pInfo->m_ColorableRenderSkin : &pInfo->m_OriginalRenderSkin;

	// first pass we draw the outline
	// second pass we draw the filling
	for(int Pass = 0; Pass < 2; Pass++)
	{
		int OutLine = Pass == 0 ? 1 : 0;

		for(int Filling = 0; Filling < 2; Filling++)
		{
			float AnimScale, BaseSize;
			GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);
			const vec2 TeeAnchor = Position + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
			float TeeBodyScale;
			GetRenderTeeBodyScale(BaseSize, TeeBodyScale);
			const float TeeAnchorSize = 64.0f * TeeBodyScale * BodyScale.x;

			if(TinyTee)
			{
				BaseSize *= TinyBodyScale * SizeMultiplier;
				AnimScale *= TinyBodyScale * SizeMultiplier;
			}

			if(Filling == 1)
			{
				Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);

				// draw body
				Graphics()->SetColor(pInfo->m_ColorBody.r, pInfo->m_ColorBody.g, pInfo->m_ColorBody.b, Alpha);
				vec2 BodyPos = Position + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
				float RenderBodyScale;
				GetRenderTeeBodyScale(BaseSize, RenderBodyScale);
				Graphics()->TextureSet(OutLine == 1 ? pSkinTextures->m_BodyOutline : pSkinTextures->m_Body);
				Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, OutLine, BodyPos.x, BodyPos.y, RenderBodyScale * BodyScale.x, RenderBodyScale * BodyScale.y);

				if(OutLine == 0 && VcSkinGradient::Enabled(IsLocal))
				{
					VcSkinGradient::RenderTeeOverlayQuad(Graphics(), pSkinTextures->m_Body, BodyPos, vec2(32.0f, 32.0f), vec2(RenderBodyScale * BodyScale.x, RenderBodyScale * BodyScale.y), TeeAnchor, TeeAnchorSize, pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle, Alpha, 0.0f, 0.0f, 1.0f, 1.0f);
					Graphics()->QuadsSetRotation(pAnim->GetBody()->m_Angle * pi * 2 + BodyAngle);
					Graphics()->SetColor(pInfo->m_ColorBody.r, pInfo->m_ColorBody.g, pInfo->m_ColorBody.b, Alpha);
				}

				// draw eyes
				if(Pass == 1)
				{
					int QuadOffset = 2;
					int EyeQuadOffset = 0;
					int TeeEye = 0;

					switch(Emote)
					{
					case EMOTE_PAIN:
						EyeQuadOffset = 0;
						TeeEye = SPRITE_TEE_EYE_PAIN - SPRITE_TEE_EYE_NORMAL;
						break;
					case EMOTE_HAPPY:
						EyeQuadOffset = 1;
						TeeEye = SPRITE_TEE_EYE_HAPPY - SPRITE_TEE_EYE_NORMAL;
						break;
					case EMOTE_SURPRISE:
						EyeQuadOffset = 2;
						TeeEye = SPRITE_TEE_EYE_SURPRISE - SPRITE_TEE_EYE_NORMAL;
						break;
					case EMOTE_ANGRY:
						EyeQuadOffset = 3;
						TeeEye = SPRITE_TEE_EYE_ANGRY - SPRITE_TEE_EYE_NORMAL;
						break;
					default:
						EyeQuadOffset = 4;
						break;
					}

					float EyeScale = BaseSize * 0.40f * BodyScale.x;
					float h = (Emote == EMOTE_BLINK ? BaseSize * 0.15f : BaseSize * 0.40f) * BodyScale.y;
					float EyeSeparation = (0.075f - 0.010f * absolute(Direction.x)) * BaseSize * BodyScale.x;
					vec2 Offset = vec2(Direction.x * 0.125f * BodyScale.x, (-0.05f + Direction.y * 0.10f) * BodyScale.y) * BaseSize;

					Graphics()->TextureSet(pSkinTextures->m_aEyes[TeeEye]);
					Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, QuadOffset + EyeQuadOffset, BodyPos.x - EyeSeparation + Offset.x, BodyPos.y + Offset.y, EyeScale / (64.f * 0.4f), h / (64.f * 0.4f));
					Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, QuadOffset + EyeQuadOffset, BodyPos.x + EyeSeparation + Offset.x, BodyPos.y + Offset.y, -EyeScale / (64.f * 0.4f), h / (64.f * 0.4f));
				}
			}

			if(TinyTee)
			{
				BaseSize /= TinyBodyScale * SizeMultiplier;
				AnimScale /= TinyBodyScale * SizeMultiplier;
			}

			// draw feet
			const CAnimKeyframe *pFoot = Filling ? pAnim->GetFrontFoot() : pAnim->GetBackFoot();

			float w = BaseSize * FeetScale.x;
			float h = (BaseSize / 2) * FeetScale.y;

			if(TinyTee)
			{
				w *= TinyFeetScale * SizeMultiplier;
				h *= TinyFeetScale * SizeMultiplier;
			}

			int QuadOffset = 7;
			if(Dir.x < 0 && pInfo->m_FeetFlipped)
			{
				QuadOffset += 2;
			}

			Graphics()->QuadsSetRotation(pFoot->m_Angle * pi * 2 + FeetAngle);

			bool Indicate = !pInfo->m_GotAirJump && g_Config.m_ClAirjumpindicator;
			float ColorScale = 1.0f;

			if(!OutLine)
			{
				++QuadOffset;
				if(Indicate)
					ColorScale = 0.5f;
			}

			Graphics()->SetColor(pInfo->m_ColorFeet.r * ColorScale, pInfo->m_ColorFeet.g * ColorScale, pInfo->m_ColorFeet.b * ColorScale, Alpha);

			IGraphics::CTextureHandle FeetTexture;
			if(g_Config.m_TcWhiteFeet && pInfo->m_CustomColoredSkin)
			{
				CTeeRenderInfo WhiteFeetInfo;
				const CSkin *pSkin = GameClient()->m_Skins.Find(g_Config.m_TcWhiteFeetSkin);
				WhiteFeetInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
				WhiteFeetInfo.m_ColorFeet = ColorRGBA(1, 1, 1);
				const CSkin::CSkinTextures *pWhiteFeetTextures = &WhiteFeetInfo.m_OriginalRenderSkin;
				FeetTexture = OutLine == 1 ? pWhiteFeetTextures->m_FeetOutline : pWhiteFeetTextures->m_Feet;
			}
			else
			{
				FeetTexture = OutLine == 1 ? pSkinTextures->m_FeetOutline : pSkinTextures->m_Feet;
			}
			Graphics()->TextureSet(FeetTexture);

			Graphics()->RenderQuadContainerAsSprite(m_TeeQuadContainerIndex, QuadOffset, Position.x + pFoot->m_X * AnimScale, Position.y + pFoot->m_Y * AnimScale, w / 64.f, h / 32.f);

			if(OutLine == 0 && VcSkinGradient::Enabled(IsLocal) && g_Config.m_FcSkinGradientFeet)
			{
				const bool MirroredFeet = QuadOffset >= 9;
				VcSkinGradient::RenderTeeOverlayQuad(Graphics(), FeetTexture, vec2(Position.x + pFoot->m_X * AnimScale, Position.y + pFoot->m_Y * AnimScale), vec2(32.0f, 16.0f), vec2(w / 64.0f, h / 32.0f), TeeAnchor, TeeAnchorSize, pFoot->m_Angle * pi * 2 + FeetAngle, Alpha, MirroredFeet ? 1.0f : 0.0f, 0.0f, MirroredFeet ? 0.0f : 1.0f, 1.0f, false);
			}
		}
	}
}
