#include "above_below_player.h"

#include "game/client/gameclient.h"
#include "game/mapitems.h"
#include "engine/shared/config.h"

float CAboveBelowPlayer::EaseInOutQuad(const float T)
{
	return T == 0.0f ? 0.0f : T == 1.0f ? 1.0f : ((T < 0.5f) ? (2.0f * T * T) : (1.0f - std::pow(-2.0f * T + 2.0f, 2) / 2.0f));
}

CAboveBelowPlayer::CAboveBelowPlayer()
{
	CAboveBelowPlayer::OnReset();
}

void CAboveBelowPlayer::OnReset()
{
	m_AboveAnim = m_HideAnim;
	m_SameAnim = m_HideAnim;
	m_BelowAnim = m_HideAnim;
	m_PlayerAbove = false;
	m_PlayerSame = false;
	m_PlayerBelow = false;
}

static bool IsWallBetween(const CCollision *pCollision, vec2 From, vec2 To)
{
	const float Dist = distance(From, To);
	const int Steps = (int)(Dist * 2.0f);
	for(int i = 1; i < Steps; i++)
	{
		const vec2 Pos = mix(From, To, (float)i / (float)Steps) * 32.0f;
		const int Index = pCollision->GetCollisionAt(Pos.x, Pos.y);
		if(Index == TILE_SOLID || Index == TILE_NOHOOK)
			return true;
	}
	return false;
}

void CAboveBelowPlayer::OnRender()
{
	if(!g_Config.m_TcNotifyWhenAbovePosPlayer && !g_Config.m_TcNotifyWhenBelowPosPlayer && !g_Config.m_TcNotifyWhenSamePosPlayer)
		return;

	const int LocalClientId = GameClient()->m_Snap.m_SpecInfo.m_Active ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
	m_PlayerAbove = false;
	m_PlayerSame = false;
	m_PlayerBelow = false;

	if(LocalClientId == SPEC_FREEVIEW)
	{
		return;
	}

	vec2 m_LocalPos;
	int m_LinesNum = 0;

	if(GameClient()->m_aClients[LocalClientId].m_SpecCharPresent)
	{
		m_LocalPos = GameClient()->m_aClients[LocalClientId].m_SpecChar / 32.0f;
	}
	else
	{
		const CNetObj_Character *pLocalChar = &GameClient()->m_Snap.m_aCharacters[LocalClientId].m_Cur;
		m_LocalPos = vec2(pLocalChar->m_X, pLocalChar->m_Y) / 32.0f;
	}

	const CCollision *pCollision = GameClient()->Collision();
	const auto UpdateOffset = [](int &Offset, int Diff) {
		if(Offset < 0 || Diff < Offset)
			Offset = Diff;
	};

	int OffAbove = -1;
	int OffSame = -1;
	int OffBelow = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		if(i == LocalClientId)
			continue;

		if(!GameClient()->m_Teams.CanCollide(LocalClientId, i))
			continue;

		const CNetObj_Character *pCurChar = &GameClient()->m_Snap.m_aCharacters[i].m_Cur;
		const vec2 PlayerPos = vec2(pCurChar->m_X, pCurChar->m_Y) / 32.0f;
		const float DiffTiles = absolute(PlayerPos.x - m_LocalPos.x);
		const int Diff = (int)lroundf(DiffTiles * 100.0f);
		const bool WithinSkew = g_Config.m_TcAboveBelowSkew > 0 ? Diff <= g_Config.m_TcAboveBelowSkew : DiffTiles == 0.0f;
		if(!WithinSkew || IsWallBetween(pCollision, m_LocalPos, PlayerPos))
			continue;

		if(PlayerPos.y == m_LocalPos.y && g_Config.m_TcNotifyWhenSamePosPlayer)
		{
			m_PlayerSame = true;
			UpdateOffset(OffSame, Diff);
		}
		if(PlayerPos.y < m_LocalPos.y && g_Config.m_TcNotifyWhenBelowPosPlayer)
		{
			m_PlayerBelow = true;
			UpdateOffset(OffBelow, Diff);
		}
		if(PlayerPos.y > m_LocalPos.y && g_Config.m_TcNotifyWhenAbovePosPlayer)
		{
			m_PlayerAbove = true;
			UpdateOffset(OffAbove, Diff);
		}
	}

	m_LinesNum = (m_PlayerAbove ? 1 : m_AboveAnim > m_HideAnim ? 1 : 0) + (m_PlayerSame ? 1 : m_SameAnim > m_HideAnim ? 1 : 0) + (m_PlayerBelow ? 1 : m_BelowAnim > m_HideAnim ? 1 : 0);

	if(!m_LinesNum)
		return;

	Ui()->MapScreen();
	const CUIRect *pScreen = Ui()->Screen();
	const float LineSize = 20.0f;
	const float FontSize = 14.0f;
	const float Margin = 5.0f;
	float LineHeight = 20 * m_LinesNum + 5 * (m_LinesNum - 1);
	// Line screen/4 - w, 20px - h, Marign - 5px
	CUIRect Line, CurLine;
	Line.w = pScreen->w / 6.0f;
	Line.h = LineHeight;
	Line.x = (pScreen->w - Line.w) * g_Config.m_TcNotifyWhenPosPlayerPosX / 100.0f;
	Line.y = (pScreen->h - Line.h) * g_Config.m_TcNotifyWhenPosPlayerPosY / 100.0f;

	if(m_PlayerAbove || m_AboveAnim > m_HideAnim)
	{
		Line.HSplitTop(LineSize, &CurLine, &Line);
		const float AnimSpeed = 0.2f;
		if(m_PlayerAbove)
			m_AboveAnim += Client()->RenderFrameTime() / AnimSpeed;
		else
			m_AboveAnim -= Client()->RenderFrameTime() / AnimSpeed * 2.0f;
		m_AboveAnim = std::clamp(m_AboveAnim, m_HideAnim, 1.0f);
		if(m_AboveAnim > 0.0f)
		{
			CurLine.Draw(ColorRGBA(0.25f, 0.70f, 0.40f, 0.5f * EaseInOutQuad(m_AboveAnim)), IGraphics::CORNER_ALL, CurLine.h / 3.0f);
			SLabelProperties Props;
			Props.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f * EaseInOutQuad(m_AboveAnim)));
			char aAbove[64];
			if(g_Config.m_TcAboveBelowSkew > 0 && OffAbove >= 0)
				str_format(aAbove, sizeof(aAbove), "/\\ You Above /\\ (%d)", OffAbove);
			else
				str_copy(aAbove, "/\\ You Above /\\");
			Ui()->DoLabel(&CurLine, aAbove, FontSize, TEXTALIGN_MC, Props);
			Line.HSplitTop(Margin, nullptr, &Line);
		}
	}
	if(m_PlayerSame || m_SameAnim > m_HideAnim)
	{
		Line.HSplitTop(LineSize, &CurLine, &Line);
		const float AnimSpeed = 0.2f;
		if(m_PlayerSame)
			m_SameAnim += Client()->RenderFrameTime() / AnimSpeed;
		else
			m_SameAnim -= Client()->RenderFrameTime() / AnimSpeed * 2.0f;
		m_SameAnim = std::clamp(m_SameAnim, m_HideAnim, 1.0f);
		if(m_SameAnim > 0.0f)
		{
			CurLine.Draw(ColorRGBA(0.90f, 0.65f, 0.20f, 0.5f * EaseInOutQuad(m_SameAnim)), IGraphics::CORNER_ALL, CurLine.h / 3.0f);
			SLabelProperties Props;
			Props.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f * EaseInOutQuad(m_SameAnim)));
			char aSame[64];
			if(g_Config.m_TcAboveBelowSkew > 0 && OffSame >= 0)
				str_format(aSame, sizeof(aSame), "= Same = (%d)", OffSame);
			else
				str_copy(aSame, "= Same =");
			Ui()->DoLabel(&CurLine, aSame, FontSize, TEXTALIGN_MC, Props);
			Line.HSplitTop(Margin, nullptr, &Line);
		}
	}
	if(m_PlayerBelow || m_BelowAnim > m_HideAnim)
	{
		Line.HSplitTop(LineSize, &CurLine, &Line);
		const float AnimSpeed = 0.2f;
		if(m_PlayerBelow)
			m_BelowAnim += Client()->RenderFrameTime() / AnimSpeed;
		else
			m_BelowAnim -= Client()->RenderFrameTime() / AnimSpeed * 2.0f;
		m_BelowAnim = std::clamp(m_BelowAnim, m_HideAnim, 1.0f);
		if(m_BelowAnim > 0.0f)
		{
			CurLine.Draw(ColorRGBA(0.85f, 0.30f, 0.30f, 0.5f * EaseInOutQuad(m_BelowAnim)), IGraphics::CORNER_ALL, CurLine.h / 3.0f);
			SLabelProperties Props;
			Props.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f * EaseInOutQuad(m_BelowAnim)));
			char aBelow[64];
			if(g_Config.m_TcAboveBelowSkew > 0 && OffBelow >= 0)
				str_format(aBelow, sizeof(aBelow), "\\/ You Below \\/ (%d)", OffBelow);
			else
				str_copy(aBelow, "\\/ You Below \\/");
			Ui()->DoLabel(&CurLine, aBelow, FontSize, TEXTALIGN_MC, Props);
		}
	}
}
