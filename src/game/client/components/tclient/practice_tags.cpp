#include "practice_tags.h"

#include <base/hash.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
constexpr int MAX_ITEMS_TO_LOAD = 65536;

IOHANDLE PracticeTagsOpenFile(CGameClient &This, int Flags)
{
	char aFilename[IO_MAX_PATH_LENGTH];
	const SHA256_DIGEST Sha256 = This.Map()->Sha256();
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(Sha256, aSha256, sizeof(aSha256));
	str_format(aFilename, sizeof(aFilename), "practicetags/%s_%s.csv", This.Map()->BaseName(), aSha256);

	if(Flags == IOFLAG_WRITE)
		This.Storage()->CreateFolder("practicetags", IStorage::TYPE_SAVE);
	return This.Storage()->OpenFile(aFilename, Flags, IStorage::TYPE_SAVE);
}

bool IsPracticeCommand(const char *pLine)
{
	static constexpr const char *s_apPracticeCommands[] = {
		"unpractice",
		"r",
		"rescue",
		"back",
		"rescuemode",
		"tp",
		"teleport",
		"tpxy",
		"lasttp",
		"tc",
		"telecursor",
		"totele",
		"totelecp",
		"unsolo",
		"solo",
		"undeep",
		"deep",
		"unlivefreeze",
		"livefreeze",
		"addweapon",
		"removeweapon",
		"shotgun",
		"grenade",
		"laser",
		"rifle",
		"jetpack",
		"setjumps",
		"weapons",
		"unshotgun",
		"ungrenade",
		"unlaser",
		"unrifle",
		"unjetpack",
		"unweapons",
		"ninja",
		"unninja",
		"infjump",
		"uninfjump",
		"endless",
		"unendless",
		"setswitch",
		"invincible",
		"collision",
		"hookcollision",
		"hitothers",
	};

	pLine = str_utf8_skip_whitespaces(pLine);
	if(pLine[0] != '/')
		return false;
	++pLine;
	const char *pEnd = pLine;
	while(*pEnd != '\0' && *pEnd != ' ' && *pEnd != '\t' && *pEnd != '\r' && *pEnd != '\n')
		++pEnd;
	const int Length = pEnd - pLine;
	for(const char *pCommand : s_apPracticeCommands)
	{
		if((int)str_length(pCommand) == Length && str_comp_nocase_num(pLine, pCommand, Length) == 0)
			return true;
	}
	return false;
}
}

void CPracticeTags::MakeSpaceFor(int Count)
{
	if(g_Config.m_TcPracticeTagMaxItems == 0)
	{
		if(!m_Tags.empty())
		{
			m_Tags.clear();
			m_Dirty = true;
		}
		return;
	}
	while((int)m_Tags.size() + Count > g_Config.m_TcPracticeTagMaxItems)
	{
		m_Tags.pop_front();
		m_Dirty = true;
	}
}

void CPracticeTags::AddTag(vec2 Pos)
{
	MakeSpaceFor(1);
	if(g_Config.m_TcPracticeTagMaxItems == 0)
		return;
	m_Tags.push_back(Pos);
	m_Dirty = true;
	Save();
}

bool CPracticeTags::Save()
{
	if(!m_Dirty)
		return true;
	if(GameClient()->Map() == nullptr || !GameClient()->Map()->IsLoaded())
		return false;

	IOHANDLE Handle = PracticeTagsOpenFile(*GameClient(), IOFLAG_WRITE);
	if(!Handle)
		return false;

	bool Success = true;
	for(const vec2 &Pos : m_Tags)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%.6f,%.6f", Pos.x, Pos.y);
		const unsigned Length = str_length(aBuf);
		if(io_write(Handle, aBuf, Length) != Length || !io_write_newline(Handle))
		{
			Success = false;
			break;
		}
	}
	io_close(Handle);
	if(Success)
		m_Dirty = false;
	return Success;
}

bool CPracticeTags::Load()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return false;
	if(GameClient()->Map() == nullptr || !GameClient()->Map()->IsLoaded())
		return false;

	IOHANDLE Handle = PracticeTagsOpenFile(*GameClient(), IOFLAG_READ);
	if(!Handle)
		return false;
	char *pContent = io_read_all_str(Handle);
	io_close(Handle);
	if(pContent == nullptr)
		return false;

	int Loaded = 0;
	for(char *pLine = std::strtok(pContent, "\r\n"); pLine != nullptr && Loaded < MAX_ITEMS_TO_LOAD; pLine = std::strtok(nullptr, "\r\n"))
	{
		float x, y;
		if(std::sscanf(pLine, "%f,%f", &x, &y) != 2 || !std::isfinite(x) || !std::isfinite(y))
			continue;
		m_Tags.emplace_back(x, y);
		++Loaded;
	}
	std::free(pContent);
	MakeSpaceFor(0);
	m_Dirty = false;
	return true;
}

void CPracticeTags::OnChatCommand(const char *pLine)
{
	if(!g_Config.m_TcPracticeTagEnabled || g_Config.m_TcPracticeTagMaxItems == 0 || Client()->State() != IClient::STATE_ONLINE || !IsPracticeCommand(pLine))
		return;

	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS || !GameClient()->m_aClients[LocalId].m_Active)
		return;
	const auto &Character = GameClient()->m_Snap.m_aCharacters[LocalId];
	if(!Character.m_Active)
		return;
	if(Character.m_HasExtendedDisplayInfo && (Character.m_ExtendedData.m_Flags & CHARACTERFLAG_PRACTICE_MODE) == 0)
		return;

	AddTag(GameClient()->m_aClients[LocalId].m_Predicted.m_Pos);
}

void CPracticeTags::OnRender()
{
	if(!g_Config.m_TcPracticeTagEnabled)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	MakeSpaceFor(0);
	if(m_Tags.empty())
		return;

	const float Size = g_Config.m_TcPracticeTagSize;
	const float Radius = Size / 2.0f;
	const ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcPracticeTagColor));
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	for(const vec2 &Pos : m_Tags)
	{
		switch(g_Config.m_TcPracticeTagShape)
		{
		case SHAPE_CIRCLE:
			Graphics()->DrawCircle(Pos.x, Pos.y, Radius, 32);
			break;
		case SHAPE_SQUARE:
		{
			IGraphics::CQuadItem Quad(Pos.x, Pos.y, Size, Size);
			Graphics()->QuadsDraw(&Quad, 1);
			break;
		}
		case SHAPE_TRIANGLE:
		{
			const IGraphics::CFreeformItem Triangle(
				Pos + vec2(0.0f, -Radius),
				Pos + vec2(-Radius, Radius),
				Pos + vec2(Radius, Radius),
				Pos + vec2(Radius, Radius));
			Graphics()->QuadsDrawFreeform(&Triangle, 1);
			break;
		}
		case SHAPE_CROSS:
		{
			const float Thickness = maximum(2.0f, Size * 0.25f);
			IGraphics::CQuadItem aQuads[] = {
				IGraphics::CQuadItem(Pos.x, Pos.y, Thickness, Size),
				IGraphics::CQuadItem(Pos.x, Pos.y, Size, Thickness),
			};
			Graphics()->QuadsDraw(aQuads, std::size(aQuads));
			break;
		}
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
}

void CPracticeTags::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE || OldState == IClient::STATE_DEMOPLAYBACK)
		Save();
	m_Tags.clear();
	m_Dirty = false;
	if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
		Load();
}

void CPracticeTags::OnShutdown()
{
	Save();
}
