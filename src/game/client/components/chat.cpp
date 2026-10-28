/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "chat.h"

#include <base/color.h>
#include <base/io.h>
#include <base/log.h>
#include <base/log_color.h>
#include <base/mem.h>
#include <base/time.h>

#include <engine/editor.h>
#include <engine/external/regex.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/csv.h>
#include <engine/textrender.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/animstate.h>
#include <game/client/components/censor.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/components/tclient/colored_parts.h>
#include <game/client/components/tclient/gradient.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

static constexpr float CHAT_SLIDE_MS = 220.0f;
static constexpr float CHAT_REVEAL_MS = 700.0f;

namespace
{
bool NotifyNameInList(const char *pName, const char *pList)
{
	if(!pName[0] || !pList[0])
		return false;
	const size_t NameLen = str_length(pName);
	while(*pList)
	{
		while(*pList == ',' || *pList == ' ')
			pList++;
		const char *pEnd = pList;
		while(*pEnd && *pEnd != ',')
			pEnd++;
		if(pEnd > pList && (size_t)(pEnd - pList) == NameLen && str_comp_nocase(pName, pList) == 0)
			return true;
		pList = pEnd;
	}
	return false;
}

void NotifyReplaceNick(const char *pTemplate, const char *pName, char *pOut, int OutSize)
{
	const char *pAt = str_find(pTemplate, "@nick");
	if(!pAt || OutSize <= 1)
	{
		str_copy(pOut, pTemplate, OutSize);
		return;
	}
	str_copy(pOut, pTemplate, minimum<int>((int)(pAt - pTemplate) + 1, OutSize));
	str_append(pOut, pName, OutSize);
	str_append(pOut, pAt + 5, OutSize);
}
}

char CChat::ms_aDisplayText[MAX_CHAT_LENGTH] = "";

CChat::CLine::CLine()
{
	m_TextContainerIndex.Reset();
	m_QuadContainerIndex = -1;
}

void CChat::CLine::Reset(CChat &This)
{
	This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	This.Graphics()->DeleteQuadContainer(m_QuadContainerIndex);
	m_Initialized = false;
	m_Time = 0;
	m_aText[0] = '\0';
	m_aName[0] = '\0';
	m_Friend = false;
	m_TimesRepeated = 0;
	m_pManagedTeeRenderInfo = nullptr;
	m_pTranslateResponse = nullptr;
}

CChat::CChat()
{
	m_Mode = MODE_NONE;

	m_Input.SetCalculateOffsetCallback([this]() { return m_IsInputCensored; });
	m_Input.SetDisplayTextCallback([this](char *pStr, size_t NumChars) {
		m_IsInputCensored = false;
		if(
			g_Config.m_ClStreamerMode &&
			(str_startswith(pStr, "/login ") ||
				str_startswith(pStr, "/register ") ||
				str_startswith(pStr, "/code ") ||
				str_startswith(pStr, "/timeout ") ||
				str_startswith(pStr, "/save ") ||
				str_startswith(pStr, "/load ")))
		{
			bool Censor = false;
			const size_t NumLetters = std::min(NumChars, sizeof(ms_aDisplayText) - 1);
			for(size_t i = 0; i < NumLetters; ++i)
			{
				if(Censor)
					ms_aDisplayText[i] = '*';
				else
					ms_aDisplayText[i] = pStr[i];
				if(pStr[i] == ' ')
				{
					Censor = true;
					m_IsInputCensored = true;
				}
			}
			ms_aDisplayText[NumLetters] = '\0';
			return ms_aDisplayText;
		}
		return pStr;
	});
}

void CChat::RegisterCommand(const char *pName, const char *pParams, const char *pHelpText)
{
	// Don't allow duplicate commands.
	for(const auto &Command : m_vServerCommands)
		if(str_comp(Command.m_aName, pName) == 0)
			return;

	m_vServerCommands.emplace_back(pName, pParams, pHelpText);
	m_ServerCommandsNeedSorting = true;
}

void CChat::UnregisterCommand(const char *pName)
{
	m_vServerCommands.erase(std::remove_if(m_vServerCommands.begin(), m_vServerCommands.end(), [pName](const CCommand &Command) { return str_comp(Command.m_aName, pName) == 0; }), m_vServerCommands.end());
}

void CChat::RebuildChat()
{
	for(auto &Line : m_aLines)
	{
		if(!Line.m_Initialized)
			continue;
		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		// recalculate sizes
		Line.m_aYOffset[0] = -1.0f;
		Line.m_aYOffset[1] = -1.0f;
	}
}

void CChat::ClearLines()
{
	for(auto &Line : m_aLines)
		Line.Reset(*this);
	m_PrevScoreBoardShowed = false;
	m_PrevShowChat = false;
}

void CChat::OnWindowResize()
{
	RebuildChat();
}

void CChat::Reset()
{
	ClearLines();

	m_Show = false;
	m_CompletionUsed = false;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = nullptr;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;
	m_CurrentLine = 0;
	m_IsInputCensored = false;
	m_EditingNewLine = true;
	m_ServerSupportsCommandInfo = false;
	m_ServerCommandsNeedSorting = false;
	m_aCurrentInputText[0] = '\0';
	DisableMode();
	m_vServerCommands.clear();

	m_SmoothText.clear();
	m_vSmoothGhosts.clear();
	m_vSmoothCharSpawnTime.clear();
	m_SmoothDeleteTimer = 0.0f;
	m_SmoothCaretX = -1.0f;
	m_SmoothSendTime = -1.0f;
	m_SmoothWasDeleting = false;
	m_ChatSlide = 0.0f;
	m_WasRevealing = false;

	for(int64_t &LastSoundPlayed : m_aLastSoundPlayed)
		LastSoundPlayed = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
		Reset();
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat *)pUserData)->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		((CChat *)pUserData)->EnableMode(1);
	else
		log_error("chat", "expected all or team as mode");

	if(pResult->GetString(1)[0] || g_Config.m_ClChatReset)
		((CChat *)pUserData)->m_Input.Set(pResult->GetString(1));
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Echo(pResult->GetString(0));
}

void CChat::ConClearChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->ClearLines();
}

void CChat::ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	((CChat *)pUserData)->RebuildChat();
}

void CChat::ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentWidth();
	pChat->RebuildChat();
}

void CChat::ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentFontSize();
	pChat->RebuildChat();
}

void CChat::Echo(const char *pString)
{
	AddLine(CLIENT_MSG, 0, pString);
}

static const char *ChatLayoutFixChar(int Code)
{
	switch(Code)
	{
	case '.': return "/";
	case 0x430: return "f";
	case 0x431: return ",";
	case 0x432: return "d";
	case 0x433: return "u";
	case 0x434: return "l";
	case 0x435: return "t";
	case 0x451: return "`";
	case 0x436: return ";";
	case 0x437: return "p";
	case 0x438: return "b";
	case 0x439: return "q";
	case 0x43A: return "r";
	case 0x43B: return "k";
	case 0x43C: return "v";
	case 0x43D: return "y";
	case 0x43E: return "j";
	case 0x43F: return "g";
	case 0x440: return "h";
	case 0x441: return "c";
	case 0x442: return "n";
	case 0x443: return "e";
	case 0x444: return "a";
	case 0x445: return "[";
	case 0x446: return "w";
	case 0x447: return "x";
	case 0x448: return "i";
	case 0x449: return "o";
	case 0x44A: return "]";
	case 0x44B: return "s";
	case 0x44C: return "m";
	case 0x44D: return "'";
	case 0x44E: return ".";
	case 0x44F: return "z";
	case 0x410: return "F";
	case 0x411: return "<";
	case 0x412: return "D";
	case 0x413: return "U";
	case 0x414: return "L";
	case 0x415: return "T";
	case 0x401: return "~";
	case 0x416: return ":";
	case 0x417: return "P";
	case 0x418: return "B";
	case 0x419: return "Q";
	case 0x41A: return "R";
	case 0x41B: return "K";
	case 0x41C: return "V";
	case 0x41D: return "Y";
	case 0x41E: return "J";
	case 0x41F: return "G";
	case 0x420: return "H";
	case 0x421: return "C";
	case 0x422: return "N";
	case 0x423: return "E";
	case 0x424: return "A";
	case 0x425: return "{";
	case 0x426: return "W";
	case 0x427: return "X";
	case 0x428: return "I";
	case 0x429: return "O";
	case 0x42A: return "}";
	case 0x42B: return "S";
	case 0x42C: return "M";
	case 0x42D: return "\"";
	case 0x42E: return ">";
	case 0x42F: return "Z";
	}
	return nullptr;
}

static void ChatFirstToken(const char *pLine, char *pOut, int OutSize)
{
	int Len = 0;
	while(pLine[Len] != '\0' && pLine[Len] != ' ' && Len < OutSize - 1)
	{
		pOut[Len] = pLine[Len];
		Len++;
	}
	pOut[Len] = '\0';
}

static bool ChatTranslateLayoutToken(const char *pToken, char *pOut, int OutSize)
{
	if(pToken == nullptr || pOut == nullptr || OutSize < 2 || pToken[0] != '.')
		return false;

	int Out = 0;
	for(const char *p = pToken; *p != '\0' && Out < OutSize - 8;)
	{
		const int Code = str_utf8_decode(&p);
		const char *pMapped = ChatLayoutFixChar(Code);
		if(pMapped != nullptr)
		{
			for(int k = 0; pMapped[k] != '\0' && Out < OutSize - 1; k++)
				pOut[Out++] = pMapped[k];
		}
		else
		{
			char aCode[8];
			const int Len = str_utf8_encode(aCode, Code);
			for(int k = 0; k < Len && Out < OutSize - 1; k++)
				pOut[Out++] = aCode[k];
		}
	}
	pOut[Out] = '\0';
	return pOut[0] == '/' && pOut[1] != '\0';
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT | CFGFLAG_STORE, ConEcho, this, "Echo the text in chat window");
	Console()->Register("clear_chat", "", CFGFLAG_CLIENT | CFGFLAG_STORE, ConClearChat, this, "Clear chat messages");
}

void CChat::OnInit()
{
	Reset();
	Console()->Chain("cl_chat_old", ConchainChatOld, this);
	Console()->Chain("cl_chat_size", ConchainChatFontSize, this);
	Console()->Chain("cl_chat_width", ConchainChatWidth, this);
}

static bool ChatCursorEnabled()
{
	return g_Config.m_TcChatCursor || g_Config.m_TcChatTagCursor;
}

bool CChat::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!IsActive() || !ChatCursorEnabled())
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);

	const vec2 UiSize = vec2(600.0f * Graphics()->ScreenAspect(), 600.0f);
	const vec2 WindowSize = vec2(maximum((float)Graphics()->WindowWidth(), 1.0f), maximum((float)Graphics()->WindowHeight(), 1.0f));
	m_ChatCursorPos = vec2(
		std::clamp(m_ChatCursorPos.x + x * UiSize.x / WindowSize.x, 0.0f, UiSize.x - 1.0f),
		std::clamp(m_ChatCursorPos.y + y * UiSize.y / WindowSize.y, 0.0f, UiSize.y - 1.0f));

	return true;
}

bool CChat::OnInput(const IInput::CEvent &Event)
{
	if(m_Mode == MODE_NONE)
		return false;

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		DisableMode();
		GameClient()->OnRelease();
		if(g_Config.m_ClChatReset)
		{
			m_Input.Clear();
			m_pHistoryEntry = nullptr;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_ServerCommandsNeedSorting)
		{
			std::sort(m_vServerCommands.begin(), m_vServerCommands.end());
			m_ServerCommandsNeedSorting = false;
		}

		char aLayoutFix[512];
		if(GetLayoutFixCommand(aLayoutFix, sizeof(aLayoutFix)))
		{
			const char *pRest = m_Input.GetString();
			while(*pRest != '\0' && *pRest != ' ')
				pRest++;

			char aFixedLine[MAX_CHAT_LENGTH];
			str_format(aFixedLine, sizeof(aFixedLine), "%s%s", aLayoutFix, pRest);
			m_Input.Set(aFixedLine);
		}

		if(GameClient()->m_BindChat.ChatDoBinds(m_Input.GetString()))
			; // Do nothing as bindchat was executed
		else if(GameClient()->m_TClient.ChatDoSpecId(m_Input.GetString()))
			; // Do nothing as specid was executed
		else if(GameClient()->m_HeartList.OnChatInput(m_Input.GetString()))
			;
		else if(GameClient()->m_Translate.ChatDoTranslateOutgoing(m_Mode == MODE_TEAM ? 1 : 0, m_Input.GetString()))
			; // Do nothing as outgoing translate was queued
		else
		{
			StartSmoothSend(m_Input.GetString());
			SendChatQueued(m_Input.GetString());
		}
		m_pHistoryEntry = nullptr;
		DisableMode();
		GameClient()->OnRelease();
		m_Input.Clear();
	}
	if(g_Config.m_TcChatScroll)
	{
		const int InitializedLines = GetInitializedLineCount();
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_UP && InitializedLines > 0)
			m_HistoryScrollOffset = minimum(m_HistoryScrollOffset + 1, InitializedLines);
		else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_DOWN && m_HistoryScrollOffset > 0)
			--m_HistoryScrollOffset;
	}
	if(g_Config.m_TcChatTagCursor && (Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_MOUSE_1)
		TagPlayerUnderCursor();
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		const bool ShiftPressed = Input()->ShiftIsPressed();

		// fill the completion buffer
		if(!m_CompletionUsed)
		{
			const char *pCursor = m_Input.GetString() + m_Input.GetCursorOffset();
			for(size_t Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor - 1) != ' '; --pCursor, ++Count)
				;
			m_PlaceholderOffset = pCursor - m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_truncate(m_aCompletionBuffer, sizeof(m_aCompletionBuffer), m_Input.GetString() + m_PlaceholderOffset, m_PlaceholderLength);

			if(g_Config.m_TcChatFixLayout)
			{
				char aFixedToken[MAX_CHAT_LENGTH];
				if(ChatTranslateLayoutToken(m_aCompletionBuffer, aFixedToken, sizeof(aFixedToken)))
					str_copy(m_aCompletionBuffer, aFixedToken, sizeof(m_aCompletionBuffer));
			}
		}

		if(!m_CompletionUsed && m_aCompletionBuffer[0] != '/')
		{
			// Create the completion list of player names through which the player can iterate
			const char *PlayerName, *FoundInput;
			m_PlayerCompletionListLength = 0;
			for(auto &PlayerInfo : GameClient()->m_Snap.m_apInfoByName)
			{
				if(PlayerInfo)
				{
					PlayerName = GameClient()->m_aClients[PlayerInfo->m_ClientId].m_aName;
					FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
					if(FoundInput != nullptr)
					{
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_ClientId = PlayerInfo->m_ClientId;
						// The score for suggesting a player name is determined by the distance of the search input to the beginning of the player name
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_Score = (int)(FoundInput - PlayerName);
						m_PlayerCompletionListLength++;
					}
				}
			}
			std::stable_sort(m_aPlayerCompletionList, m_aPlayerCompletionList + m_PlayerCompletionListLength,
				[](const CRateablePlayer &Player1, const CRateablePlayer &Player2) -> bool {
					return Player1.m_Score < Player2.m_Score;
				});
		}

		if(GameClient()->m_BindChat.ChatDoAutocomplete(ShiftPressed))
		{
		}
		else if(m_aCompletionBuffer[0] == '/' && !m_vServerCommands.empty())
		{
			CCommand *pCompletionCommand = nullptr;

			const size_t NumCommands = m_vServerCommands.size();

			if(ShiftPressed && m_CompletionUsed)
				m_CompletionChosen--;
			else if(!ShiftPressed)
				m_CompletionChosen++;
			m_CompletionChosen = (m_CompletionChosen + 2 * NumCommands) % (2 * NumCommands);

			m_CompletionUsed = true;

			const char *pCommandStart = m_aCompletionBuffer + 1;
			for(size_t i = 0; i < 2 * NumCommands; ++i)
			{
				int SearchType;
				int Index;

				if(ShiftPressed)
				{
					SearchType = ((m_CompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen - i + NumCommands) % NumCommands;
				}
				else
				{
					SearchType = ((m_CompletionChosen + i) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen + i) % NumCommands;
				}

				auto &Command = m_vServerCommands[Index];

				if(str_startswith_nocase(Command.m_aName, pCommandStart))
				{
					pCompletionCommand = &Command;
					m_CompletionChosen = Index + SearchType * NumCommands;
					break;
				}
			}

			// insert the command
			if(pCompletionCommand)
			{
				char aBuf[MAX_CHAT_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// add the command
				str_append(aBuf, "/");
				str_append(aBuf, pCompletionCommand->m_aName);

				// add separator
				const char *pSeparator = pCompletionCommand->m_aParams[0] == '\0' ? "" : " ";
				str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->m_aName) + 1;
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
		else
		{
			// find next possible name
			const char *pCompletionString = nullptr;
			if(m_PlayerCompletionListLength > 0)
			{
				// We do this in a loop, if a player left the game during the repeated pressing of Tab, they are skipped
				CGameClient::CClientData *pCompletionClientData;
				for(int i = 0; i < m_PlayerCompletionListLength; ++i)
				{
					if(ShiftPressed && m_CompletionUsed)
					{
						m_CompletionChosen--;
					}
					else if(!ShiftPressed)
					{
						m_CompletionChosen++;
					}
					if(m_CompletionChosen < 0)
					{
						m_CompletionChosen += m_PlayerCompletionListLength;
					}
					m_CompletionChosen %= m_PlayerCompletionListLength;
					m_CompletionUsed = true;

					pCompletionClientData = &GameClient()->m_aClients[m_aPlayerCompletionList[m_CompletionChosen].m_ClientId];
					if(!pCompletionClientData->m_Active)
					{
						continue;
					}

					pCompletionString = pCompletionClientData->m_aName;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[MAX_CHAT_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// quote the name
				char aQuoted[128];
				if((m_Input.GetString()[0] == '/' || GameClient()->m_BindChat.CheckBindChat(m_Input.GetString())) && (str_find(pCompletionString, " ") || str_find(pCompletionString, "\"")))
				{
					// escape the name
					str_copy(aQuoted, "\"");
					char *pDst = aQuoted + str_length(aQuoted);
					str_escape(&pDst, pCompletionString, aQuoted + sizeof(aQuoted));
					str_append(aQuoted, "\"");

					pCompletionString = aQuoted;
				}

				// add the name
				str_append(aBuf, pCompletionString);

				// add separator
				const char *pSeparator = "";
				if(*(m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionString);
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT)
		{
			m_CompletionChosen = -1;
			m_CompletionUsed = false;
		}

		m_Input.ProcessInput(Event);
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if(m_EditingNewLine)
		{
			str_copy(m_aCurrentInputText, m_Input.GetString());
			m_EditingNewLine = false;
		}

		if(m_pHistoryEntry)
		{
			CHistoryEntry *pTest = m_History.Prev(m_pHistoryEntry);

			if(pTest)
				m_pHistoryEntry = pTest;
		}
		else
		{
			m_pHistoryEntry = m_History.Last();
		}

		if(m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if(m_pHistoryEntry)
		{
			m_Input.Set(m_pHistoryEntry->m_aText);
		}
		else if(!m_EditingNewLine)
		{
			m_Input.Set(m_aCurrentInputText);
			m_EditingNewLine = true;
		}
	}

	return true;
}

void CChat::EnableMode(int Team)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(m_Mode == MODE_NONE)
	{
		if(Team)
			m_Mode = MODE_TEAM;
		else
			m_Mode = MODE_ALL;

		m_CompletionChosen = -1;
		m_CompletionUsed = false;
		m_Input.Activate(EInputPriority::CHAT);
		m_ChatCursorPos = vec2(300.0f * Graphics()->ScreenAspect(), 300.0f);
	}
}

void CChat::DisableMode()
{
	if(m_Mode != MODE_NONE)
	{
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
	}
	m_HistoryScrollOffset = 0;
}

void CChat::TagPlayerUnderCursor()
{
	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CScreenRect WorldRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0.0f, 0.0f, Graphics()->WorldAspect(), GameClient()->m_Camera.m_RenderZoom);

	const vec2 UiSize = vec2(600.0f * Graphics()->ScreenAspect(), 600.0f);
	const vec2 WorldPos = vec2(
		WorldRect.m_TopLeft.x + (m_ChatCursorPos.x / UiSize.x) * WorldRect.Size().x,
		WorldRect.m_TopLeft.y + (m_ChatCursorPos.y / UiSize.y) * WorldRect.Size().y);

	int BestId = -1;
	float BestDist = CCharacterCore::PhysicalSize() * (float)std::clamp(g_Config.m_TcChatTagCursorHitbox, 50, 500) / 100.0f;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active || !GameClient()->m_aClients[i].m_Active)
			continue;
		if(i == GameClient()->m_aLocalIds[0] || (Client()->DummyConnected() && i == GameClient()->m_aLocalIds[1]))
			continue;
		const float Dist = distance(WorldPos, GameClient()->m_aClients[i].m_RenderPos);
		if(Dist <= BestDist)
		{
			BestDist = Dist;
			BestId = i;
		}
	}
	if(BestId < 0)
		return;

	char aName[MAX_NAME_LENGTH + 2];
	str_copy(aName, GameClient()->m_aClients[BestId].m_aName);
	str_append(aName, " ");
	m_Input.Insert(aName, m_Input.GetCursorOffset());
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(GameClient()->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

		/*
		if(g_Config.m_ClCensorChat)
		{
			char aMessage[MAX_CHAT_LENGTH];
			str_copy(aMessage, pMsg->m_pMessage);
			GameClient()->m_Censor.CensorMessage(aMessage);
			AddLine(pMsg->m_ClientId, pMsg->m_Team, aMessage);
		}
		else
			AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
		*/

		AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK &&
			pMsg->m_ClientId == SERVER_MSG)
		{
			StoreSave(pMsg->m_pMessage);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFO)
	{
		CNetMsg_Sv_CommandInfo *pMsg = (CNetMsg_Sv_CommandInfo *)pRawMsg;
		if(!m_ServerSupportsCommandInfo)
		{
			m_vServerCommands.clear();
			m_ServerSupportsCommandInfo = true;
		}
		RegisterCommand(pMsg->m_pName, pMsg->m_pArgsFormat, pMsg->m_pHelpText);
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		CNetMsg_Sv_CommandInfoRemove *pMsg = (CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		UnregisterCommand(pMsg->m_pName);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHit = str_utf8_find_nocase(pLine, pName);

	while(pHit)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHit || pHit[-1] == ' ') && (pHit[Length] == 0 || pHit[Length] == ' ' || pHit[Length] == '.' || pHit[Length] == '!' || pHit[Length] == ',' || pHit[Length] == '?' || pHit[Length] == ':'))
			return true;

		pHit = str_utf8_find_nocase(pHit + 1, pName);
	}

	return false;
}

static constexpr const char *SAVES_HEADER[] = {
	"Time",
	"Player",
	"Map",
	"Code",
};

// TODO: remove this in a few releases (in 2027 or later)
//       it got deprecated by CGameClient::StoreSave
void CChat::StoreSave(const char *pText)
{
	const char *pStart = str_find(pText, "Team successfully saved by ");
	const char *pMid = str_find(pText, ". Use '/load ");
	const char *pOn = str_find(pText, "' on ");
	const char *pEnd = str_find(pText, pOn ? " to continue" : "' to continue");

	if(!pStart || !pMid || !pEnd || pMid < pStart || pEnd < pMid || (pOn && (pOn < pMid || pEnd < pOn)))
		return;

	char aName[16];
	str_truncate(aName, sizeof(aName), pStart + 27, pMid - pStart - 27);

	char aSaveCode[64];

	str_truncate(aSaveCode, sizeof(aSaveCode), pMid + 13, (pOn ? pOn : pEnd) - pMid - 13);

	char aTimestamp[20];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE);

	const bool SavesFileExists = Storage()->FileExists(SAVES_FILE, IStorage::TYPE_SAVE);
	IOHANDLE File = Storage()->OpenFile(SAVES_FILE, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const char *apColumns[4] = {
		aTimestamp,
		aName,
		GameClient()->Map()->BaseName(),
		aSaveCode,
	};

	if(!SavesFileExists)
	{
		CsvWrite(File, 4, SAVES_HEADER);
	}
	CsvWrite(File, 4, apColumns);
	io_close(File);
}

void CChat::AddLine(int ClientId, int Team, const char *pLine)
{
	if(*pLine == 0)
		return;

	if(ClientId == SERVER_MSG && g_Config.m_TcAutoNotifyOnJoin && g_Config.m_TcAutoNotifyName[0] != '\0')
	{
		if(str_find_nocase(pLine, "entered and joined the game"))
		{
			const char *pStart = str_find(pLine, "'");
			const char *pEnd = pStart ? str_find(pStart + 1, "'") : nullptr;
			if(pStart && pEnd && pEnd > pStart + 1)
			{
				char aName[128];
				str_copy(aName, pStart + 1, minimum<int>((int)(pEnd - pStart), (int)sizeof(aName)));
				if(NotifyNameInList(aName, g_Config.m_TcAutoNotifyName))
				{
					char aMsg[256];
					NotifyReplaceNick(g_Config.m_TcAutoNotifyMsg, aName, aMsg, sizeof(aMsg));
					GameClient()->Echo(aMsg);
				}
			}
		}
	}

	if(ClientId == SERVER_MSG && g_Config.m_TcHoverMessage)
	{
		GameClient()->m_HoverNotification.Start(pLine);
		if(g_Config.m_TcHoverOnly || !g_Config.m_ClShowChatSystem)
			return;
	}

	if(ClientId >= 0 && g_Config.m_TcMuteMessagesConsole &&
		(GameClient()->m_aClients[ClientId].m_ChatIgnore || GameClient()->m_aClients[ClientId].m_Foe ||
			(g_Config.m_TcHideEnemyChat && GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[1])))
	{
		char aMutedBuf[512];
		str_format(aMutedBuf, sizeof(aMutedBuf), "%s: %s", GameClient()->m_aClients[ClientId].m_aName, pLine);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutedchat", aMutedBuf);
		return;
	}

	if((ClientId == SERVER_MSG && !g_Config.m_ClShowChatSystem) ||
		(ClientId >= 0 && (GameClient()->m_aClients[ClientId].m_aName[0] == '\0' || // unknown client
				  GameClient()->m_aClients[ClientId].m_ChatIgnore ||
				  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatFriends && !GameClient()->m_aClients[ClientId].m_Friend) ||
				  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatTeamMembersOnly && GameClient()->IsOtherTeam(ClientId) && GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK) ||
				  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_TcHideEnemyChat && GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[1]) ||
				  (GameClient()->m_Snap.m_LocalClientId != ClientId && GameClient()->m_aClients[ClientId].m_Foe))))
		return;

	// TClient
	if(ClientId == CLIENT_MSG && !g_Config.m_TcShowChatClient)
		return;

	// TClient
	if(([&]() {
		   if(ClientId == CLIENT_MSG || ClientId == SERVER_MSG)
			   return false;
		   for(const auto LocalClientId : GameClient()->m_aLocalIds)
			   if(LocalClientId == ClientId)
				   return false;
		   auto &Re = GameClient()->m_TClient.m_RegexChatIgnore;
		   return Re.error().empty() && Re.test(pLine);
	   })())
		return;

	// trim right and set maximum length to 256 utf8-characters
	int Length = 0;
	const char *pStr = pLine;
	const char *pEnd = nullptr;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = nullptr;
		}
		else if(pEnd == nullptr)
		{
			pEnd = pStrOld;
		}

		if(++Length >= MAX_CHAT_LENGTH)
		{
			*(const_cast<char *>(pStr)) = '\0';
			break;
		}
	}
	if(pEnd != nullptr)
		*(const_cast<char *>(pEnd)) = '\0';

	if(*pLine == 0)
		return;

	bool Highlighted = false;

	auto &&FChatMsgCheckAndPrint = [&](const CLine &Line) {
		ColorRGBA ChatLogColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		if(Line.m_Highlighted)
		{
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		}
		else
		{
			if(Line.m_Friend && g_Config.m_ClMessageFriend)
				ChatLogColor = (Line.m_ClientId >= 0 && GameClient()->m_HeartList.IsFriend(Line.m_ClientId)) ? GameClient()->m_HeartList.GetFriendColor(Line.m_ClientId) : color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
			else if(Line.m_Team)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
			else if(Line.m_ClientId == SERVER_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
			else if(Line.m_ClientId == CLIENT_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
			else // regular message
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		}

		const char *pFrom;
		if(Line.m_Whisper)
			pFrom = "chat/whisper";
		else if(Line.m_Team)
			pFrom = "chat/team";
		else if(Line.m_ClientId == SERVER_MSG)
			pFrom = "chat/server";
		else if(Line.m_ClientId == CLIENT_MSG)
			pFrom = "chat/client";
		else
			pFrom = "chat/all";

		log_info_color(color_cast<LOG_COLOR>(ChatLogColor), pFrom, "%s%s%s", Line.m_aName, Line.m_ClientId >= 0 ? ": " : "", Line.m_aText);
	};

	// Custom color for new line
	std::optional<ColorRGBA> CustomColor = std::nullopt;
	if(ClientId == CLIENT_MSG)
		CustomColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));

	CLine &PreviousLine = m_aLines[m_CurrentLine];

	// Team Number:
	// 0 = global; 1 = team; 2 = sending whisper; 3 = receiving whisper

	// If it's a client message, m_aText will have ": " prepended so we have to work around it.
	if(PreviousLine.m_Initialized &&
		PreviousLine.m_TeamNumber == Team &&
		PreviousLine.m_ClientId == ClientId &&
		str_comp(PreviousLine.m_aText, pLine) == 0 &&
		PreviousLine.m_CustomColor == CustomColor)
	{
		PreviousLine.m_TimesRepeated++;
		TextRender()->DeleteTextContainer(PreviousLine.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(PreviousLine.m_QuadContainerIndex);
		PreviousLine.m_Time = time();
		PreviousLine.m_aYOffset[0] = -1.0f;
		PreviousLine.m_aYOffset[1] = -1.0f;

		FChatMsgCheckAndPrint(PreviousLine);
		return;
	}

	m_CurrentLine = (m_CurrentLine + 1) % MAX_LINES;

	CLine &CurrentLine = m_aLines[m_CurrentLine];
	CurrentLine.Reset(*this);
	CurrentLine.m_Initialized = true;
	CurrentLine.m_Time = time();
	CurrentLine.m_aYOffset[0] = -1.0f;
	CurrentLine.m_aYOffset[1] = -1.0f;
	CurrentLine.m_ClientId = ClientId;
	CurrentLine.m_TeamNumber = Team;
	CurrentLine.m_Team = Team == 1;
	CurrentLine.m_Whisper = Team >= 2;
	CurrentLine.m_NameColor = -2;
	CurrentLine.m_CustomColor = CustomColor;

	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(ClientId >= 0 && ClientId != GameClient()->m_aLocalIds[0] && ClientId != GameClient()->m_aLocalIds[1])
		{
			for(int LocalId : GameClient()->m_aLocalIds)
			{
				Highlighted |= LocalId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[LocalId].m_aName);
			}
		}
	}
	else
	{
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= GameClient()->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_aName);
	}
	CurrentLine.m_Highlighted = Highlighted;

	str_copy(CurrentLine.m_aText, pLine);

	if(CurrentLine.m_ClientId == SERVER_MSG)
	{
		if(g_Config.m_TcChatServerPrefix)
			str_copy(CurrentLine.m_aName, g_Config.m_TcServerPrefix);
	}
	else if(CurrentLine.m_ClientId == CLIENT_MSG)
	{
		if(g_Config.m_TcChatClientPrefix)
			str_copy(CurrentLine.m_aName, g_Config.m_TcClientPrefix);
	}
	else
	{
		const auto &LineAuthor = GameClient()->m_aClients[CurrentLine.m_ClientId];

		if(LineAuthor.m_Active)
		{
			if(LineAuthor.m_Team == TEAM_SPECTATORS)
				CurrentLine.m_NameColor = TEAM_SPECTATORS;

			if(GameClient()->IsTeamPlay())
			{
				if(LineAuthor.m_Team == TEAM_RED)
					CurrentLine.m_NameColor = TEAM_RED;
				else if(LineAuthor.m_Team == TEAM_BLUE)
					CurrentLine.m_NameColor = TEAM_BLUE;
			}
		}

		if(Team == TEAM_WHISPER_SEND)
		{
			str_copy(CurrentLine.m_aName, "→");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, LineAuthor.m_aName);
			}
			CurrentLine.m_NameColor = TEAM_BLUE;
			CurrentLine.m_Highlighted = false;
			Highlighted = false;
		}
		else if(Team == TEAM_WHISPER_RECV)
		{
			str_copy(CurrentLine.m_aName, "←");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, LineAuthor.m_aName);
			}
			CurrentLine.m_NameColor = TEAM_RED;
			CurrentLine.m_Highlighted = true;
			Highlighted = true;
		}
		else
		{
			str_copy(CurrentLine.m_aName, LineAuthor.m_aName);
		}

		if(LineAuthor.m_Active)
		{
			CurrentLine.m_Friend = LineAuthor.m_Friend;
			CurrentLine.m_pManagedTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(LineAuthor);
		}
	}

	FChatMsgCheckAndPrint(CurrentLine);

	// play sound
	int64_t Now = time();
	if(ClientId == SERVER_MSG)
	{
		if(Now - m_aLastSoundPlayed[CHAT_SERVER] >= time_freq() * 3 / 10)
		{
			if(g_Config.m_SndServerMessage)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 1.0f);
				m_aLastSoundPlayed[CHAT_SERVER] = Now;
			}
		}
	}
	else if(ClientId == CLIENT_MSG)
	{
		// No sound yet
	}
	else if(Highlighted && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(Now - m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq() * 3 / 10)
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "%s: %s", CurrentLine.m_aName, CurrentLine.m_aText);
			Client()->Notify("DDNet Chat", aBuf);
			if(g_Config.m_SndHighlight)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 1.0f);
				m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;
			}

			if(g_Config.m_ClEditor)
			{
				GameClient()->Editor()->UpdateMentions();
			}
		}
	}
	else if(Team != TEAM_WHISPER_SEND)
	{
		if(Now - m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq() * 3 / 10)
		{
			bool PlaySound = CurrentLine.m_Team ? g_Config.m_SndTeamChat : g_Config.m_SndChat;
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
			{
				PlaySound &= (bool)g_Config.m_ClVideoShowChat;
			}
#endif
			if(PlaySound)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 1.0f);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
		}
	}

	// TClient
	GameClient()->m_Translate.AutoTranslate(CurrentLine);
}

int CChat::GetInitializedLineCount() const
{
	int Count = 0;
	for(int i = 0; i < MAX_LINES; i++)
	{
		if(m_aLines[i].m_Initialized)
			Count++;
	}
	return Count;
}

struct SChatGradientRange
{
	int m_Start;
	int m_Length;
	int m_CharCount;
	int m_ClientId;
};

static bool ChatGradientWordChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || (unsigned char)c >= 0x80;
}

static void ChatCollectGradientRanges(CGameClient *pGameClient, const char *pText, std::vector<SChatGradientRange> &vRanges)
{
	const int TextLen = str_length(pText);
	if(TextLen <= 0)
		return;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		const auto &ClientData = pGameClient->m_aClients[ClientId];
		if(!ClientData.m_Active || ClientData.m_aName[0] == '\0' || pGameClient->m_HeartList.IsFriend(ClientId))
			continue;

		const int NameLen = str_length(ClientData.m_aName);
		if(NameLen < 2 || NameLen > TextLen)
			continue;

		const char *pFound = pText;
		while((pFound = str_find(pFound, ClientData.m_aName)) != nullptr)
		{
			const int Start = (int)(pFound - pText);
			const char CharBefore = Start > 0 ? pText[Start - 1] : '\0';
			const char CharAfter = pText[Start + NameLen];
			if(!ChatGradientWordChar(CharBefore) && !ChatGradientWordChar(CharAfter))
			{
				size_t Size = 0, CharCount = 0;
				str_utf8_stats(ClientData.m_aName, NameLen + 1, SIZE_MAX, &Size, &CharCount);
				vRanges.push_back({Start, NameLen, (int)CharCount, ClientId});
			}
			pFound += NameLen;
		}
	}

	std::sort(vRanges.begin(), vRanges.end(), [](const SChatGradientRange &Left, const SChatGradientRange &Right) { return Left.m_Start < Right.m_Start; });

	std::vector<SChatGradientRange> vFiltered;
	int LastEnd = 0;
	for(const SChatGradientRange &Range : vRanges)
	{
		if(Range.m_Start < LastEnd)
			continue;
		vFiltered.push_back(Range);
		LastEnd = Range.m_Start + Range.m_Length;
	}
	vRanges = std::move(vFiltered);
}

static void ChatGradientMessageColors(const ColorRGBA &Base, ColorRGBA &Color1, ColorRGBA &Color2)
{
	Color1 = ColorRGBA(Base.r * 0.55f, Base.g * 0.55f, Base.b * 0.55f, Base.a);
	Color2 = ColorRGBA(
		Base.r + (1.0f - Base.r) * 0.65f,
		Base.g + (1.0f - Base.g) * 0.65f,
		Base.b + (1.0f - Base.b) * 0.65f,
		Base.a);
}

static std::vector<STextColorSplit> ChatBuildGradientSplits(const char *pText, const std::vector<SChatGradientRange> &vRanges, const ColorRGBA &Color1, const ColorRGBA &Color2, CGameClient *pGameClient, float Phase, bool RangesOnly)
{
	std::vector<STextColorSplit> vSplits;
	const int TextLen = str_length(pText);
	if(TextLen <= 0)
		return vSplits;

	size_t Size = 0, CharCount = 0;
	str_utf8_stats(pText, TextLen + 1, SIZE_MAX, &Size, &CharCount);
	if(CharCount <= 0)
		return vSplits;

	vSplits.reserve(CharCount);
	const char *pCursor = pText;
	int RangeIndex = 0;
	int RangeChar = 0;
	for(int CharIndex = 0; CharIndex < (int)CharCount; CharIndex++)
	{
		const int ByteOffset = (int)(pCursor - pText);
		const char *pPrev = pCursor;
		str_utf8_decode(&pCursor);
		const int ByteLen = (int)(pCursor - pPrev);

		while(RangeIndex < (int)vRanges.size() && ByteOffset >= vRanges[RangeIndex].m_Start + vRanges[RangeIndex].m_Length)
		{
			RangeIndex++;
			RangeChar = 0;
		}

		ColorRGBA Color;
		if(RangeIndex < (int)vRanges.size() && ByteOffset >= vRanges[RangeIndex].m_Start)
		{
			const SChatGradientRange &Range = vRanges[RangeIndex];
			const float t = Range.m_CharCount <= 1 ? 0.5f : (float)RangeChar / (float)(Range.m_CharCount - 1);
			ColorRGBA Body, Feet;
			CBcGradient::GetSkinToneColors(Range.m_ClientId, pGameClient, Body, Feet);
			Color = CBcGradient::SampleTwoColorGradient(Body, Feet, t, Phase);
			RangeChar++;
		}
		else
		{
			if(RangesOnly)
				continue;
			const float t = CharCount <= 1 ? 0.5f : (float)CharIndex / (float)(CharCount - 1);
			Color = CBcGradient::SampleTwoColorGradient(Color1, Color2, t, Phase);
		}

		vSplits.emplace_back(ByteOffset, ByteLen, Color);
	}
	return vSplits;
}

static int ChatTextLength(const char *pText)
{
	return (int)str_utf8_offset_bytes_to_chars(pText, str_length(pText));
}

static void SmoothRevealSplits(CTextCursor *pCursor, const char *pText, const ColorRGBA &Color, float Progress, int &CharIndex, int TotalChars)
{
	const int CharOffset = pCursor->m_CharCount;
	const std::vector<STextColorSplit> vBaseSplits = pCursor->m_vColorSplits;
	std::vector<STextColorSplit> vSplits;
	size_t BaseIndex = 0;
	for(const char *p = pText; *p != '\0';)
	{
		const char *pCharStart = p;
		str_utf8_decode(&p);
		const int CharLen = (int)(p - pCharStart);
		if(CharLen <= 0)
			break;
		const int Offset = (int)(pCharStart - pText) + CharOffset;
		const float CharT = TotalChars > 1 ? minimum(1.0f, (float)CharIndex / (float)TotalChars) : 0.0f;
		const float Alpha = std::clamp((Progress - CharT * 0.70f) / 0.30f, 0.0f, 1.0f);
		ColorRGBA CharColor = Color;
		while(BaseIndex < vBaseSplits.size() && Offset >= vBaseSplits[BaseIndex].m_CharIndex + vBaseSplits[BaseIndex].m_Length)
			BaseIndex++;
		if(BaseIndex < vBaseSplits.size() && Offset >= vBaseSplits[BaseIndex].m_CharIndex)
			CharColor = vBaseSplits[BaseIndex].m_Color;
		vSplits.emplace_back(Offset, CharLen, CharColor.WithMultipliedAlpha(Alpha));
		CharIndex++;
	}
	pCursor->m_vColorSplits = std::move(vSplits);
}

void CChat::OnPrepareLines(float y)
{
	float x = 5.0f;
	float FontSize = this->FontSize();

	const bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;
	const bool GradientNicknames = g_Config.m_TcGradientChat && g_Config.m_TcGradientChatNicknames;
	const bool GradientMessages = g_Config.m_TcGradientChat && g_Config.m_TcGradientChatMessages;
	bool ForceRecreate = IsScoreBoardOpen != m_PrevScoreBoardShowed || ShowLargeArea != m_PrevShowChat || g_Config.m_TcNameplateGradientEverything;
	float GradientPhase = 0.0f;
	if(GradientNicknames || GradientMessages)
	{
		GradientPhase = CBcGradient::AnimatePhase(Client()->GlobalTime());
		if(std::fabs(GradientPhase - m_LastGradientPhase) > 0.008f)
		{
			m_LastGradientPhase = GradientPhase;
			ForceRecreate = true;
		}
	}
	m_PrevScoreBoardShowed = IsScoreBoardOpen;
	m_PrevShowChat = ShowLargeArea;

	const int TeeSize = MessageTeeSize();
	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	float RealMsgPaddingTee = TeeSize + MESSAGE_TEE_PADDING_RIGHT;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
		RealMsgPaddingTee = 0;
	}

	int64_t Now = time();
	float LineWidth = (IsScoreBoardOpen ? std::max(85.0f, FontSize * 85.0f / 6.0f) : g_Config.m_ClChatWidth) - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	float Begin = x;
	float TextBegin = Begin + RealMsgPaddingX / 2.0f;
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	for(int i = m_HistoryScrollOffset; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		if(Line.m_TextContainerIndex.Valid() && !ForceRecreate)
			continue;

		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);

		char aClientId[16] = "";
		if(g_Config.m_ClShowIds && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			GameClient()->FormatClientId(Line.m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
		}

		char aCount[12];
		if(Line.m_ClientId < 0)
			str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
		else
			str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

		const char *pText = Line.m_aText;
		if(Config()->m_ClStreamerMode && Line.m_ClientId == SERVER_MSG)
		{
			if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load ") && str_endswith(Line.m_aText, "'"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***'";
			}
			else if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load") && str_endswith(Line.m_aText, "if it fails"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***' if save is successful or with '/load *** *** ***' if it fails";
			}
			else if(str_startswith(Line.m_aText, "Team successfully saved by ") && str_endswith(Line.m_aText, " to continue"))
			{
				pText = "Team successfully saved by ***. Use '/load *** *** ***' to continue";
			}
		}

		const CColoredParts ColoredParts(pText, Line.m_ClientId == CLIENT_MSG);
		if(!ColoredParts.Colors().empty() && ColoredParts.Colors()[0].m_Index == 0)
			Line.m_CustomColor = ColoredParts.Colors()[0].m_Color;
		pText = ColoredParts.Text();

		const char *pTranslatedError = nullptr;
		const char *pTranslatedText = nullptr;
		const char *pTranslatedLanguage = nullptr;
		if(Line.m_pTranslateResponse != nullptr && Line.m_pTranslateResponse->m_Text[0])
		{
			// If hidden and there is translated text
			if(pText != Line.m_aText)
			{
				pTranslatedError = TCLocalize("Translated text hidden due to streamer mode");
			}
			else if(Line.m_pTranslateResponse->m_Error)
			{
				pTranslatedError = Line.m_pTranslateResponse->m_Text;
			}
			else
			{
				pTranslatedText = Line.m_pTranslateResponse->m_Text;
				if(Line.m_pTranslateResponse->m_Language[0] != '\0')
					pTranslatedLanguage = Line.m_pTranslateResponse->m_Language;
			}
		}

		std::vector<SChatGradientRange> vGradientRanges;
		if(GradientNicknames && pText[0] != '\0')
			ChatCollectGradientRanges(GameClient(), pText, vGradientRanges);

		// get the y offset (calculate it if we haven't done that yet)
		if(Line.m_aYOffset[OffsetType] < 0.0f)
		{
			CTextCursor MeasureCursor;
			MeasureCursor.SetPosition(vec2(TextBegin, 0.0f));
			MeasureCursor.m_FontSize = FontSize;
			MeasureCursor.m_Flags = 0;
			MeasureCursor.m_LineWidth = LineWidth;

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				MeasureCursor.m_X += RealMsgPaddingTee;

				const auto &PrefixAuthor = GameClient()->m_aClients[Line.m_ClientId];
				if(g_Config.m_TcSpectatePrefix && PrefixAuthor.m_Team == TEAM_SPECTATORS)
					TextRender()->TextEx(&MeasureCursor, g_Config.m_TcSpecPrefix);
				if(g_Config.m_TcWarlistPrefixes && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId))
					TextRender()->TextEx(&MeasureCursor, g_Config.m_TcWarlistPrefix);
				else if(Line.m_Friend && g_Config.m_ClMessageFriend)
					TextRender()->TextEx(&MeasureCursor, g_Config.m_TcFriendPrefix);
			}

			TextRender()->TextEx(&MeasureCursor, aClientId);
			TextRender()->TextEx(&MeasureCursor, Line.m_aName);
			if(Line.m_TimesRepeated > 0)
				TextRender()->TextEx(&MeasureCursor, aCount);

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				TextRender()->TextEx(&MeasureCursor, ": ");
			}

			CTextCursor AppendCursor = MeasureCursor;
			AppendCursor.m_LongestLineWidth = 0.0f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				AppendCursor.m_StartX = MeasureCursor.m_X;
				AppendCursor.m_LineWidth -= MeasureCursor.m_LongestLineWidth;
			}

			if(pTranslatedText)
			{
				TextRender()->TextEx(&AppendCursor, pTranslatedText);
				if(pTranslatedLanguage)
				{
					TextRender()->TextEx(&AppendCursor, " [");
					TextRender()->TextEx(&AppendCursor, pTranslatedLanguage);
					TextRender()->TextEx(&AppendCursor, "]");
				}
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pText);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else if(pTranslatedError)
			{
				TextRender()->TextEx(&AppendCursor, pText);
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pTranslatedError);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else
			{
				TextRender()->TextEx(&AppendCursor, pText);
			}

			Line.m_aYOffset[OffsetType] = AppendCursor.Height() + RealMsgPaddingY;
		}

		y -= Line.m_aYOffset[OffsetType] * (i == m_HistoryScrollOffset ? m_SmoothSlotScale : 1.0f);

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		// the position the text was created
		Line.m_TextYOffset = y + RealMsgPaddingY / 2.0f;

		int CurRenderFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(CurRenderFlags | ETextRenderFlags::TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD);

		bool RevealBuild = false;
		float RevealProgress = 1.0f;
		int RevealTotalChars = 1;
		int RevealCharIndex = 0;
		if(g_Config.m_TcSmoothChatSend && m_HistoryScrollOffset == 0 && i == 0)
		{
			const int64_t RevealAgeMs = (time() - Line.m_Time) * 1000 / time_freq();
			if(RevealAgeMs >= 0 && RevealAgeMs < CHAT_REVEAL_MS)
			{
				RevealBuild = true;
				RevealProgress = (float)RevealAgeMs / CHAT_REVEAL_MS;
				RevealTotalChars = ChatTextLength(aClientId) + ChatTextLength(Line.m_aName) +
						   (Line.m_TimesRepeated > 0 ? ChatTextLength(aCount) : 0) +
						   (Line.m_ClientId >= 0 && Line.m_aName[0] != '\0' ? 2 : 0) + ChatTextLength(pText);
				if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
				{
					const auto &PrefixAuthor = GameClient()->m_aClients[Line.m_ClientId];
					if(g_Config.m_TcSpectatePrefix && PrefixAuthor.m_Team == TEAM_SPECTATORS)
						RevealTotalChars += ChatTextLength(g_Config.m_TcSpecPrefix);
					if(g_Config.m_TcWarlistPrefixes && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId))
						RevealTotalChars += ChatTextLength(g_Config.m_TcWarlistPrefix);
					else if(Line.m_Friend && g_Config.m_ClMessageFriend)
						RevealTotalChars += ChatTextLength(g_Config.m_TcFriendPrefix);
				}
				RevealTotalChars = std::max(1, RevealTotalChars);
			}
		}

		// reset the cursor
		CTextCursor LineCursor;
		LineCursor.SetPosition(vec2(TextBegin, Line.m_TextYOffset));
		LineCursor.m_FontSize = FontSize;
		LineCursor.m_LineWidth = LineWidth;

		auto AppendChatPiece = [&](CTextCursor *pCursor, const char *pPiece, bool Fade = true) {
			if(RevealBuild && Fade)
				SmoothRevealSplits(pCursor, pPiece, TextRender()->GetTextColor(), RevealProgress, RevealCharIndex, RevealTotalChars);
			if(BCGradient_ApplyEverythingHook)
				BCGradient_ApplyEverythingHook(pCursor, pPiece, -1);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, pCursor, pPiece);
			pCursor->m_vColorSplits.clear();
		};

		auto SetGradientSplits = [](CTextCursor *pCursor, std::vector<STextColorSplit> vSplits) {
			if(vSplits.empty())
				return;
			const int Offset = pCursor->m_CharCount;
			for(STextColorSplit &Split : vSplits)
				Split.m_CharIndex += Offset;
			pCursor->m_vColorSplits = std::move(vSplits);
		};

		// Message is from valid player
		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			LineCursor.m_X += RealMsgPaddingTee;

			const auto &PrefixAuthor = GameClient()->m_aClients[Line.m_ClientId];
			if(g_Config.m_TcSpectatePrefix && PrefixAuthor.m_Team == TEAM_SPECTATORS)
			{
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				AppendChatPiece(&LineCursor, g_Config.m_TcSpecPrefix);
			}
			if(g_Config.m_TcWarlistPrefixes && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId))
			{
				TextRender()->TextColor(GameClient()->m_WarList.GetPriorityColor(Line.m_ClientId));
				AppendChatPiece(&LineCursor, g_Config.m_TcWarlistPrefix);
			}
			else if(Line.m_Friend && g_Config.m_ClMessageFriend)
			{
				TextRender()->TextColor((Line.m_ClientId >= 0 && GameClient()->m_HeartList.IsFriend(Line.m_ClientId)) ? GameClient()->m_HeartList.GetFriendColor(Line.m_ClientId) : color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor)).WithAlpha(1.0f));
				AppendChatPiece(&LineCursor, g_Config.m_TcFriendPrefix);
			}
		}

		// render name
		ColorRGBA NameColor;
		if(Line.m_CustomColor)
			NameColor = *Line.m_CustomColor;
		else if(Line.m_ClientId >= 0 && GameClient()->m_HeartList.IsFriend(Line.m_ClientId))
			NameColor = GameClient()->m_HeartList.GetFriendColor(Line.m_ClientId);
		else if(Line.m_ClientId == SERVER_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_ClientId >= 0 && g_Config.m_TcWarList && g_Config.m_TcWarListChat && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId)) // TClient
			NameColor = GameClient()->m_WarList.GetPriorityColor(Line.m_ClientId);
		else if(Line.m_Team)
			NameColor = CalculateNameColor(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else if(Line.m_NameColor == TEAM_RED)
			NameColor = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
		else if(Line.m_NameColor == TEAM_BLUE)
			NameColor = ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f);
		else if(Line.m_NameColor == TEAM_SPECTATORS)
			NameColor = ColorRGBA(0.75f, 0.5f, 0.75f, 1.0f);
		else if(Line.m_ClientId >= 0 && g_Config.m_ClChatTeamColors && GameClient()->m_Teams.Team(Line.m_ClientId))
			NameColor = GameClient()->GetDDTeamColor(GameClient()->m_Teams.Team(Line.m_ClientId), 0.75f);
		else
			NameColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);

		TextRender()->TextColor(NameColor);
		AppendChatPiece(&LineCursor, aClientId);
		if(!RevealBuild && GradientNicknames && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0' && !Line.m_CustomColor && !GameClient()->m_HeartList.IsFriend(Line.m_ClientId))
			SetGradientSplits(&LineCursor, CBcGradient::BuildAnimatedTextSplits(Line.m_aName, Line.m_ClientId, GameClient(), GradientPhase));
		AppendChatPiece(&LineCursor, Line.m_aName);

		if(Line.m_TimesRepeated > 0)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
			AppendChatPiece(&LineCursor, aCount);
		}

		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			TextRender()->TextColor(NameColor);
			AppendChatPiece(&LineCursor, ": ");
		}

		ColorRGBA Color;
		if(Line.m_CustomColor)
			Color = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Highlighted)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		else if(Line.m_Team)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else // regular message
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		TextRender()->TextColor(Color);

		CTextCursor AppendCursor = LineCursor;
		AppendCursor.m_LongestLineWidth = 0.0f;
		if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
		{
			AppendCursor.m_StartX = LineCursor.m_X;
			AppendCursor.m_LineWidth -= LineCursor.m_LongestLineWidth;
		}

		if(pTranslatedText)
		{
			AppendChatPiece(&AppendCursor, pTranslatedText);
			if(pTranslatedLanguage)
			{
				ColorRGBA ColorLang = Color;
				ColorLang.r *= 0.8f;
				ColorLang.g *= 0.8f;
				ColorLang.b *= 0.8f;
				TextRender()->TextColor(ColorLang);
				AppendChatPiece(&AppendCursor, " [");
				AppendChatPiece(&AppendCursor, pTranslatedLanguage);
				AppendChatPiece(&AppendCursor, "]");
			}
			ColorRGBA ColorSub = Color;
			ColorSub.r *= 0.7f;
			ColorSub.g *= 0.7f;
			ColorSub.b *= 0.7f;
			TextRender()->TextColor(ColorSub);
			AppendChatPiece(&AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			AppendChatPiece(&AppendCursor, pText);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else if(pTranslatedError)
		{
			AppendChatPiece(&AppendCursor, pText);
			ColorRGBA ColorSub = Color;
			ColorSub.r = 0.7f;
			ColorSub.g = 0.6f;
			ColorSub.b = 0.6f;
			TextRender()->TextColor(ColorSub);
			AppendChatPiece(&AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			AppendChatPiece(&AppendCursor, pTranslatedError);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else
		{
			const bool CustomColored = !ColoredParts.Colors().empty();
			std::vector<STextColorSplit> vGradientSplits;
			if(!CustomColored && (GradientMessages || (GradientNicknames && !vGradientRanges.empty())))
			{
				ColorRGBA GradientColor1, GradientColor2;
				ChatGradientMessageColors(Color, GradientColor1, GradientColor2);
				vGradientSplits = ChatBuildGradientSplits(pText, vGradientRanges, GradientColor1, GradientColor2, GameClient(), GradientPhase, !GradientMessages);
				SetGradientSplits(&AppendCursor, vGradientSplits);
			}
			else
			{
				ColoredParts.AddSplitsToCursor(AppendCursor);
			}
			if(i == m_CurrentLine)
				m_vSmoothSentGradientSplits = vGradientSplits;
			AppendChatPiece(&AppendCursor, pText, !CustomColored);
			AppendCursor.m_vColorSplits.clear();
		}

		if(!g_Config.m_ClChatOld && (Line.m_aText[0] != '\0' || Line.m_aName[0] != '\0'))
		{
			float FullWidth = RealMsgPaddingX * 1.5f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				FullWidth += LineCursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth;
			}
			else
			{
				FullWidth += std::max(LineCursor.m_LongestLineWidth, AppendCursor.m_LongestLineWidth);
			}
			Graphics()->SetColor(1, 1, 1, 1);
			Line.m_QuadContainerIndex = Graphics()->CreateRectQuadContainer(Begin, y, FullWidth, Line.m_aYOffset[OffsetType], MessageRounding(), IGraphics::CORNER_ALL);
		}

		TextRender()->SetRenderFlags(CurRenderFlags);
		if(Line.m_TextContainerIndex.Valid())
			TextRender()->UploadTextContainer(Line.m_TextContainerIndex);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

static float SmoothChatEaseOutBack(float T)
{
	const float C1 = 1.70158f;
	const float C3 = C1 + 1.0f;
	const float P = T - 1.0f;
	return 1.0f + C3 * P * P * P + C1 * P * P;
}

void CChat::StartSmoothSend(const char *pText)
{
	if(!g_Config.m_TcSmoothChatSend)
		return;
	str_copy(m_aSmoothSentText, pText, sizeof(m_aSmoothSentText));
	m_SmoothSendTime = 0.0f;
	m_SmoothSendX = m_SmoothLastInputX;
	m_SmoothSendY = m_SmoothLastInputY;
	m_SmoothText.clear();
	m_vSmoothCharSpawnTime.clear();
	m_vSmoothGhosts.clear();
	m_vSmoothSentGradientSplits.clear();
	m_SmoothDeleteTimer = 0.0f;
	m_SmoothWasDeleting = false;
	m_SmoothCaretX = -1.0f;

	if(g_Config.m_TcGradientChat && g_Config.m_TcGradientChatMessages)
	{
		ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		ColorRGBA GradientColor1, GradientColor2;
		ChatGradientMessageColors(Color, GradientColor1, GradientColor2);
		std::vector<SChatGradientRange> vGradientRanges;
		ChatCollectGradientRanges(GameClient(), pText, vGradientRanges);
		const float GradientPhase = m_LastGradientPhase >= 0.0f ? m_LastGradientPhase : 0.0f;
		m_vSmoothSentGradientSplits = ChatBuildGradientSplits(pText, vGradientRanges, GradientColor1, GradientColor2, GameClient(), GradientPhase, false);
	}
}

void CChat::AddSmoothGhost(int CharIndex, float FontSize, float LineWidth, float Now)
{
	const int Offset = (int)str_utf8_offset_chars_to_bytes(m_SmoothText.c_str(), CharIndex);
	if(Offset < 0 || Offset >= (int)m_SmoothText.size())
		return;

	const char *pChar = m_SmoothText.c_str() + Offset;
	const char *pNext = pChar;
	str_utf8_decode(&pNext);
	const int Length = minimum((int)(pNext - pChar), (int)sizeof(SSmoothGhost::m_aText) - 1);

	SSmoothGhost Ghost;
	mem_copy(Ghost.m_aText, pChar, Length);
	Ghost.m_aText[Length] = '\0';
	Ghost.m_X = m_SmoothLastInputX + TextRender()->TextWidth(FontSize, m_SmoothText.c_str(), Offset, LineWidth);
	Ghost.m_Time = Now;
	m_vSmoothGhosts.push_back(Ghost);
}

STextBoundingBox CChat::RenderSmoothInput(const CUIRect *pRect, float FontSize, float LineWidth, float ScrollOffset)
{
	const float Now = (float)(time_get() / (double)time_freq());
	const float FrameTime = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);

	const char *pActual = m_Input.GetString();
	m_Input.MarkRendered();
	const STextBoundingBox BBox = TextRender()->TextBoundingBox(FontSize, pActual, -1, LineWidth, 0.0f);

	const int ActualChars = (int)str_utf8_offset_bytes_to_chars(pActual, str_length(pActual));

	if(!g_Config.m_TcSmoothChatTyping)
		m_SmoothText = pActual;

	const float BaseX = pRect->x;
	const float BaseY = pRect->y - ScrollOffset;
	m_SmoothLastInputX = BaseX;
	m_SmoothLastInputY = BaseY;

	int ShownChars = (int)str_utf8_offset_bytes_to_chars(m_SmoothText.c_str(), m_SmoothText.size());
	if(g_Config.m_TcSmoothChatTyping)
	{
		if(ActualChars > ShownChars && strncmp(pActual, m_SmoothText.c_str(), m_SmoothText.size()) == 0)
		{
			m_SmoothText = pActual;
			m_SmoothDeleteTimer = 0.0f;
		}
		else if(ActualChars < ShownChars)
		{
			if(ShownChars - ActualChars == 1)
			{
				AddSmoothGhost(ActualChars, FontSize, LineWidth, Now);
				m_SmoothText = pActual;
			}
			else
			{
				const float Step = m_SmoothWasDeleting ? 0.015f : 0.020f;
				m_SmoothDeleteTimer = minimum(m_SmoothDeleteTimer + FrameTime, Step);
				if(m_SmoothDeleteTimer >= Step)
				{
					m_SmoothDeleteTimer = 0.0f;
					AddSmoothGhost(ShownChars - 1, FontSize, LineWidth, Now);
					m_SmoothText.erase(str_utf8_offset_chars_to_bytes(m_SmoothText.c_str(), ShownChars - 1));
					ShownChars--;
				}
				m_SmoothWasDeleting = ShownChars > ActualChars;
			}
		}
		else
		{
			m_SmoothText = pActual;
			m_SmoothDeleteTimer = 0.0f;
		}
	}

	if((int)m_vSmoothCharSpawnTime.size() > ShownChars)
		m_vSmoothCharSpawnTime.resize(ShownChars);
	const int OldSpawnCount = (int)m_vSmoothCharSpawnTime.size();
	for(int i = OldSpawnCount; i < ShownChars; i++)
		m_vSmoothCharSpawnTime.push_back(Now + 0.03f * (i - OldSpawnCount));

	const float TypingDuration = m_SmoothWasDeleting ? 0.16f : 0.32f;
	float X = BaseX;
	int CharIndex = 0;
	for(const char *pCur = m_SmoothText.c_str(); *pCur != '\0'; CharIndex++)
	{
		const char *pCharStart = pCur;
		str_utf8_decode(&pCur);
		const int CharLen = (int)(pCur - pCharStart);
		if(CharLen <= 0)
			break;
		const float CharWidth = TextRender()->TextWidth(FontSize, pCharStart, CharLen, LineWidth);

		float Alpha = 1.0f;
		float YOffset = 0.0f;
		if(g_Config.m_TcSmoothChatTyping)
		{
			float Spawn = Now;
			if(CharIndex < (int)m_vSmoothCharSpawnTime.size())
				Spawn = m_vSmoothCharSpawnTime[CharIndex];
			const float T = std::clamp((Now - Spawn) / TypingDuration, 0.0f, 1.0f);
			YOffset = -12.0f * (1.0f - SmoothChatEaseOutBack(T));
			Alpha = std::clamp(T * 1.6f, 0.0f, 1.0f);
		}

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
		CTextCursor CharCursor;
		CharCursor.SetPosition(vec2(X, BaseY + YOffset));
		CharCursor.m_FontSize = FontSize;
		CharCursor.m_LineWidth = LineWidth;
		TextRender()->TextEx(&CharCursor, pCharStart, CharLen);

		X += CharWidth;
	}

	for(size_t i = 0; i < m_vSmoothGhosts.size();)
	{
		SSmoothGhost &Ghost = m_vSmoothGhosts[i];
		const float T = std::clamp((Now - Ghost.m_Time) / 0.45f, 0.0f, 1.0f);
		if(T >= 1.0f)
		{
			m_vSmoothGhosts.erase(m_vSmoothGhosts.begin() + i);
			continue;
		}
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f - T);
		CTextCursor GhostCursor;
		GhostCursor.SetPosition(vec2(Ghost.m_X, BaseY - 4.0f * T));
		GhostCursor.m_FontSize = FontSize;
		GhostCursor.m_LineWidth = LineWidth;
		TextRender()->TextEx(&GhostCursor, Ghost.m_aText);
		i++;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	if(g_Config.m_TcSmoothChatCaret && m_Input.IsActive())
	{
		const float TargetX = BaseX + TextRender()->TextWidth(FontSize, pActual, (int)m_Input.GetCursorOffset(), LineWidth);
		if(m_SmoothCaretX < 0.0f)
			m_SmoothCaretX = TargetX;
		else
			m_SmoothCaretX += (TargetX - m_SmoothCaretX) * std::clamp(FrameTime / 0.14f, 0.0f, 1.0f);

		const float CaretWidth = maximum(0.22f, FontSize * 0.055f);
		const float Radius = CaretWidth * 0.5f;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		IGraphics::CQuadItem CaretBody(m_SmoothCaretX, BaseY + Radius, CaretWidth, maximum(0.1f, FontSize - CaretWidth));
		Graphics()->QuadsDrawTL(&CaretBody, 1);
		Graphics()->DrawCircle(m_SmoothCaretX + Radius, BaseY + Radius, Radius, 12);
		Graphics()->DrawCircle(m_SmoothCaretX + Radius, BaseY + FontSize - Radius, Radius, 12);
		Graphics()->QuadsEnd();
		Graphics()->TextureClear();
	}

	return BBox;
}

void CChat::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend + time_freq() < time())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter - 1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				SendChat(pEntry->m_Team, pEntry->m_aText);
				break;
			}
		}
		--m_PendingChatCounter;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);

	float x = 5.0f;

	// TClient
	float y = 300.0f - (20.0f * FontSize() / 6.0f + (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f));
	// float y = 300.0f - 20.0f * FontSize() / 6.0f;

	float ScaledFontSize = FontSize() * (8.0f / 6.0f);
	if(m_Mode != MODE_NONE)
	{
		// render chat input
		CTextCursor InputCursor;
		InputCursor.SetPosition(vec2(x, y));
		InputCursor.m_FontSize = ScaledFontSize;
		InputCursor.m_LineWidth = Width - 190.0f;

		// TClient
		InputCursor.m_LineWidth = std::max(Width - 190.0f, 190.0f);

		if(m_Mode == MODE_ALL)
			TextRender()->TextEx(&InputCursor, Localize("All"));
		else if(m_Mode == MODE_TEAM)
			TextRender()->TextEx(&InputCursor, Localize("Team"));
		else
			TextRender()->TextEx(&InputCursor, Localize("Chat"));

		TextRender()->TextEx(&InputCursor, ": ");

		const float MessageMaxWidth = InputCursor.m_LineWidth - (InputCursor.m_X - InputCursor.m_StartX);
		const CUIRect ClippingRect = {InputCursor.m_X, InputCursor.m_Y, MessageMaxWidth, 2.25f * InputCursor.m_FontSize};
		const float XScale = Graphics()->ScreenWidth() / Width;
		const float YScale = Graphics()->ScreenHeight() / Height;
		Graphics()->ClipEnable((int)(ClippingRect.x * XScale), (int)(ClippingRect.y * YScale), (int)(ClippingRect.w * XScale), (int)(ClippingRect.h * YScale));

		float ScrollOffset = m_Input.GetScrollOffset();
		float ScrollOffsetChange = m_Input.GetScrollOffsetChange();

		m_Input.Activate(EInputPriority::CHAT); // Ensure that the input is active
		const CUIRect InputCursorRect = {InputCursor.m_X, InputCursor.m_Y - ScrollOffset, 0.0f, 0.0f};
		const bool WasChanged = m_Input.WasChanged();
		const bool WasCursorChanged = m_Input.WasCursorChanged();
		const bool Changed = WasChanged || WasCursorChanged;
		const bool SmoothChat = g_Config.m_TcSmoothChatTyping || g_Config.m_TcSmoothChatCaret || g_Config.m_TcSmoothChatSend;
		const STextBoundingBox BoundingBox = SmoothChat ? RenderSmoothInput(&InputCursorRect, InputCursor.m_FontSize, MessageMaxWidth, ScrollOffset) : m_Input.Render(&InputCursorRect, InputCursor.m_FontSize, TEXTALIGN_TL, Changed, MessageMaxWidth, 0.0f);

		Graphics()->ClipDisable();

		// Scroll up or down to keep the caret inside the clipping rect
		const float CaretPositionY = m_Input.GetCaretPosition().y - ScrollOffsetChange;
		if(CaretPositionY < ClippingRect.y)
			ScrollOffsetChange -= ClippingRect.y - CaretPositionY;
		else if(CaretPositionY + InputCursor.m_FontSize > ClippingRect.y + ClippingRect.h)
			ScrollOffsetChange += CaretPositionY + InputCursor.m_FontSize - (ClippingRect.y + ClippingRect.h);

		Ui()->DoSmoothScrollLogic(&ScrollOffset, &ScrollOffsetChange, ClippingRect.h, BoundingBox.m_H);

		m_Input.SetScrollOffset(ScrollOffset);
		m_Input.SetScrollOffsetChange(ScrollOffsetChange);

		// Autocompletion hint
		{
			const char *pGhost = nullptr;
			int GhostSkip = 0;
			if(m_Input.GetString()[0] == '/' && m_Input.GetString()[1] != '\0' && !m_vServerCommands.empty())
			{
				for(const auto &Command : m_vServerCommands)
				{
					if(str_startswith_nocase(Command.m_aName, m_Input.GetString() + 1))
					{
						pGhost = Command.m_aName;
						GhostSkip = str_length(m_Input.GetString() + 1);
						break;
					}
				}
			}
			else if(g_Config.m_TcChatFixLayout)
			{
				char aToken[512];
				char aFixedToken[512];
				ChatFirstToken(m_Input.GetString(), aToken, sizeof(aToken));
				if(ChatTranslateLayoutToken(aToken, aFixedToken, sizeof(aFixedToken)))
				{
					pGhost = FindCommandByPrefix(aFixedToken + 1);
					GhostSkip = str_length(aFixedToken + 1);
				}
			}

			if(pGhost != nullptr)
			{
				InputCursor.m_X += TextRender()->TextWidth(InputCursor.m_FontSize, m_Input.GetString(), -1, InputCursor.m_LineWidth);
				InputCursor.m_Y = m_Input.GetCaretPosition().y;
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.5f);
				TextRender()->TextEx(&InputCursor, pGhost + GhostSkip);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
		}
	}

	if(g_Config.m_TcSmoothChatSend && m_SmoothSendTime >= 0.0f)
	{
		m_SmoothSendTime += Client()->RenderFrameTime();
		const float T = std::clamp(m_SmoothSendTime / 0.60f, 0.0f, 1.0f);
		if(m_aSmoothSentText[0] != '\0')
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f - T);
			CTextCursor GhostCursor;
			GhostCursor.SetPosition(vec2(m_SmoothSendX, m_SmoothSendY + 20.0f * T));
			GhostCursor.m_FontSize = ScaledFontSize;
			GhostCursor.m_LineWidth = Width - 190.0f;
			GhostCursor.m_vColorSplits = m_vSmoothSentGradientSplits;
			for(STextColorSplit &Split : GhostCursor.m_vColorSplits)
				Split.m_Color.a *= 1.0f - T;
			TextRender()->TextEx(&GhostCursor, m_aSmoothSentText);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		if(T >= 1.0f)
			m_SmoothSendTime = -1.0f;
	}

#if defined(CONF_VIDEORECORDER)
	if(!((g_Config.m_ClShowChat && !IVideo::Current()) || (g_Config.m_ClVideoShowChat && IVideo::Current())))
#else
	if(!g_Config.m_ClShowChat)
#endif
		return;

	y -= ScaledFontSize;

	if(g_Config.m_TcBetterChatNoInputGap)
	{
		const float InputLineHeight = 20.0f * FontSize() / 6.0f;
		const bool InputOpen = m_Mode != MODE_NONE;
		const float Target = InputOpen ? 0.0f : 1.0f;
		const float Speed = InputOpen ? 22.0f : 14.0f;
		const float FrameTime = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
		m_ChatSlide += (Target - m_ChatSlide) * (1.0f - std::exp(-FrameTime * Speed));
		if(std::fabs(Target - m_ChatSlide) < 0.002f)
			m_ChatSlide = Target;
		y += m_ChatSlide * InputLineHeight;
	}
	else
	{
		m_ChatSlide = 0.0f;
	}

	m_SmoothSlotScale = 1.0f;
	CLine &NewestLine = m_aLines[m_CurrentLine];
	bool Revealing = false;
	if(g_Config.m_TcSmoothChatSend && NewestLine.m_Initialized)
	{
		const int64_t AgeMs = (time() - NewestLine.m_Time) * 1000 / time_freq();
		if(AgeMs >= 0 && AgeMs < 1500)
		{
			m_SmoothSlotScale = std::clamp((float)AgeMs / CHAT_SLIDE_MS, 0.0f, 1.0f);
			Revealing = AgeMs < CHAT_REVEAL_MS && m_HistoryScrollOffset == 0;
		}
	}
	if((Revealing || m_WasRevealing) && NewestLine.m_TextContainerIndex.Valid())
		TextRender()->DeleteTextContainer(NewestLine.m_TextContainerIndex);
	m_WasRevealing = Revealing;

	OnPrepareLines(y);

	bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)

	int64_t Now = time();
	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
	}

	for(int i = m_HistoryScrollOffset; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		y -= Line.m_aYOffset[OffsetType] * (i == m_HistoryScrollOffset ? m_SmoothSlotScale : 1.0f);

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		float Blend = Now > Line.m_Time + 14 * time_freq() && !m_PrevShowChat ? 1.0f - (Now - Line.m_Time - 14 * time_freq()) / (2.0f * time_freq()) : 1.0f;

		// Draw backgrounds for messages in one batch
		if(!g_Config.m_ClChatOld)
		{
			Graphics()->TextureClear();
			if(Line.m_QuadContainerIndex != -1)
			{
				Graphics()->SetColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true)).WithMultipliedAlpha(Blend));
				Graphics()->RenderQuadContainerEx(Line.m_QuadContainerIndex, 0, -1, 0, ((y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset));
			}
		}

		if(Line.m_TextContainerIndex.Valid())
		{
			if(!g_Config.m_ClChatOld && Line.m_pManagedTeeRenderInfo != nullptr)
			{
				CTeeRenderInfo &TeeRenderInfo = Line.m_pManagedTeeRenderInfo->TeeRenderInfo();
				const int TeeSize = MessageTeeSize();
				TeeRenderInfo.m_Size = TeeSize;

				float RowHeight = FontSize() + RealMsgPaddingY;
				float OffsetTeeY = TeeSize / 2.0f;
				float FullHeightMinusTee = RowHeight - TeeSize;

				const CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeRenderInfo, OffsetToMid);
				vec2 TeeRenderPos(x + (RealMsgPaddingX + TeeSize) / 2.0f, y + OffsetTeeY + FullHeightMinusTee / 2.0f + OffsetToMid.y);
				RenderTools()->RenderTee(pIdleState, &TeeRenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), TeeRenderPos, Blend);
			}

			const ColorRGBA TextColor = TextRender()->DefaultTextColor().WithMultipliedAlpha(Blend);
			const ColorRGBA TextOutlineColor = TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Blend);

			TextRender()->RenderTextContainer(Line.m_TextContainerIndex, TextColor, TextOutlineColor, 0, (y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset);
		}
	}

	if(IsActive() && ChatCursorEnabled())
	{
		const CScreenRect PrevScreen = Graphics()->GetScreen();
		Ui()->MapScreen();
		RenderTools()->RenderCursor(m_ChatCursorPos, 24.0f);
		Graphics()->MapScreen(PrevScreen);
	}
}

void CChat::EnsureCoherentFontSize() const
{
	// Adjust font size based on width
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatFontSize = g_Config.m_ClChatWidth / CHAT_FONTSIZE_WIDTH_RATIO;
}

void CChat::EnsureCoherentWidth() const
{
	// Adjust width based on font size
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatWidth = CHAT_FONTSIZE_WIDTH_RATIO * g_Config.m_ClChatFontSize;
}

// ----- send functions -----

const char *CChat::FindCommandByPrefix(const char *pName) const
{
	if(pName == nullptr || pName[0] == '\0')
		return nullptr;

	if(!m_vServerCommands.empty())
	{
		for(const auto &Command : m_vServerCommands)
		{
			if(str_startswith_nocase(Command.m_aName, pName))
				return Command.m_aName;
		}
		return nullptr;
	}

	static const char *s_apBuiltin[] = {"pause", "spec", "team", "w", "whisper", "kill", "emote", "me", "save", "load", "info", "rank", "top5", "lock", "practice", "unlock", "rescue", "swap", "r"};
	for(const char *pBuiltin : s_apBuiltin)
	{
		if(str_startswith_nocase(pBuiltin, pName))
			return pBuiltin;
	}
	return nullptr;
}

bool CChat::GetLayoutFixCommand(char *pOut, int OutSize) const
{
	if(pOut == nullptr || OutSize < 2)
		return false;
	pOut[0] = '\0';
	if(!g_Config.m_TcChatFixLayout)
		return false;

	char aToken[512];
	char aFixed[512];
	ChatFirstToken(m_Input.GetString(), aToken, sizeof(aToken));
	if(!ChatTranslateLayoutToken(aToken, aFixed, sizeof(aFixed)))
		return false;

	const char *pMatch = FindCommandByPrefix(aFixed + 1);
	if(pMatch == nullptr)
		return false;

	pOut[0] = '/';
	str_copy(pOut + 1, pMatch, OutSize - 1);
	return true;
}

void CChat::SendChat(int Team, const char *pLine)
{
	// don't send empty messages
	if(*str_utf8_skip_whitespaces(pLine) == '\0')
		return;

	if((!str_comp(pLine, "/spec") || !str_comp(pLine, "/pause")) && GameClient()->m_TClient.AntiUnSpec())
		return;

	GameClient()->m_PracticeTags.OnChatCommand(pLine);

	m_LastChatSend = time();

	if(GameClient()->Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_Say Msg7;
		Msg7.m_Mode = Team == 1 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
		Msg7.m_Target = -1;
		Msg7.m_pMessage = pLine;
		Client()->SendPackMsgActive(&Msg7, MSGFLAG_VITAL, true);
		return;
	}

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
}

void CChat::SendChatQueued(const char *pLine)
{
	if(!pLine || str_length(pLine) < 1)
		return;

	bool AddEntry = false;

	if(m_LastChatSend + time_freq() < time())
	{
		SendChat(m_Mode == MODE_ALL ? 0 : 1, pLine);
		AddEntry = true;
	}
	else if(m_PendingChatCounter < 3)
	{
		++m_PendingChatCounter;
		AddEntry = true;
	}

	if(AddEntry)
	{
		const int Length = str_length(pLine);
		CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry) + Length);
		pEntry->m_Team = m_Mode == MODE_ALL ? 0 : 1;
		str_copy(pEntry->m_aText, pLine, Length + 1);
	}
}
