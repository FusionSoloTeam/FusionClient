#include "uwu_chat.h"

#include <base/str.h>

#include <engine/shared/config.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace
{

constexpr int CP_SOFT_SIGN = 0x44C; // ь

bool Chance(int Percent)
{
	if(Percent <= 0)
		return false;
	if(Percent >= 100)
		return true;
	return (rand() % 100) < Percent;
}

int RandomBelow(int Max)
{
	if(Max <= 1)
		return 0;
	return rand() % Max;
}

void AppendCodepoint(std::string &Out, int Code)
{
	if(Code <= 0)
		return;
	char aBuf[8];
	const int Len = str_utf8_encode(aBuf, Code);
	Out.append(aBuf, Len);
}

int LowerCyrillic(int Code)
{
	if(Code >= 0x410 && Code <= 0x42F)
		return Code + 0x20;
	if(Code == 0x401) // Ё
		return 0x451;
	return Code;
}

int UpperCyrillic(int Code)
{
	if(Code >= 0x430 && Code <= 0x44F)
		return Code - 0x20;
	if(Code == 0x451) // ё
		return 0x401;
	return Code;
}

int LowerLatin(int Code)
{
	if(Code >= 'A' && Code <= 'Z')
		return Code + ('a' - 'A');
	return Code;
}

bool IsCyrillicVowel(int Code)
{
	switch(LowerCyrillic(Code))
	{
	case 0x430: // а
	case 0x435: // е
	case 0x451: // ё
	case 0x438: // и
	case 0x43E: // о
	case 0x443: // у
	case 0x44B: // ы
	case 0x44D: // э
	case 0x44E: // ю
	case 0x44F: // я
		return true;
	default:
		return false;
	}
}

bool IsCyrillicConsonant(int Code)
{
	switch(LowerCyrillic(Code))
	{
	case 0x431: // б
	case 0x432: // в
	case 0x433: // г
	case 0x434: // д
	case 0x436: // ж
	case 0x437: // з
	case 0x439: // й
	case 0x43A: // к
	case 0x43B: // л
	case 0x43C: // м
	case 0x43D: // н
	case 0x43F: // п
	case 0x440: // р
	case 0x441: // с
	case 0x442: // т
	case 0x444: // ф
	case 0x445: // х
	case 0x446: // ц
	case 0x447: // ч
	case 0x448: // ш
	case 0x449: // щ
		return true;
	default:
		return false;
	}
}

bool IsLatinVowel(int Code)
{
	switch(LowerLatin(Code))
	{
	case 'a':
	case 'e':
	case 'i':
	case 'o':
	case 'u':
	case 'y':
		return true;
	default:
		return false;
	}
}

bool IsLatinConsonant(int Code)
{
	if(Code < 'a' || Code > 'z')
	{
		if(Code < 'A' || Code > 'Z')
			return false;
	}
	switch(LowerLatin(Code))
	{
	case 'b':
	case 'c':
	case 'd':
	case 'f':
	case 'g':
	case 'h':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'p':
	case 'q':
	case 'r':
	case 's':
	case 't':
	case 'v':
	case 'w':
	case 'x':
	case 'z':
		return true;
	default:
		return false;
	}
}

bool IsLetter(int Code)
{
	return IsCyrillicVowel(Code) || IsCyrillicConsonant(Code) || Code == CP_SOFT_SIGN || Code == 0x44A || IsLatinVowel(Code) || IsLatinConsonant(Code);
}

bool IsTrailingPunctuation(int Code)
{
	switch(Code)
	{
	case '.':
	case ',':
	case '!':
	case '?':
	case ';':
	case ':':
	case ')':
	case '(':
	case '"':
	case '\'':
	case '-':
	case '~':
	case 0x2026: // …
	case 0x2013: // –
	case 0x2014: // —
	case 0x00AB: // «
	case 0x00BB: // »
		return true;
	default:
		return false;
	}
}

// case is preserved so the rewrite stays readable
int ShiftVowel(int Code)
{
	const int Lower = LowerCyrillic(Code);
	int Shifted;
	switch(Lower)
	{
	case 0x430: Shifted = 0x44F; break; // а → я
	case 0x44F: Shifted = 0x430; break; // я → а
	case 0x43E: Shifted = 0x451; break; // о → ё
	case 0x451: Shifted = 0x43E; break; // ё → о
	case 0x443: Shifted = 0x44E; break; // у → ю
	case 0x44E: Shifted = 0x443; break; // ю → у
	case 0x44B: Shifted = 0x438; break; // ы → и
	case 0x438: Shifted = 0x44B; break; // и → ы
	case 0x44D: Shifted = 0x435; break; // э → е
	case 0x435: Shifted = 0x44D; break; // е → э
	default:
		return Code;
	}
	return Code != Lower ? UpperCyrillic(Shifted) : Shifted;
}

// only legible shifts: р→л, л→р, ц→ць, r→l, l→r
void PushShiftedConsonant(std::vector<int> &vOut, int Code)
{
	const int Lower = LowerCyrillic(Code);
	const bool Upper = Code != Lower;
	switch(Lower)
	{
	case 0x440: // р
		vOut.push_back(Upper ? 0x41B : 0x43B); // л / Л
		return;
	case 0x43B: // л
		vOut.push_back(Upper ? 0x420 : 0x440); // р / Р
		return;
	case 0x446: // ц
		vOut.push_back(Upper ? 0x426 : 0x446); // ц / Ц
		vOut.push_back(CP_SOFT_SIGN);
		return;
	default:
		break;
	}

	const int Latin = LowerLatin(Code);
	const bool LatinUpper = Code >= 'A' && Code <= 'Z';
	if(Latin == 'r')
	{
		vOut.push_back(LatinUpper ? 'L' : 'l');
		return;
	}
	if(Latin == 'l')
	{
		vOut.push_back(LatinUpper ? 'R' : 'r');
		return;
	}
	vOut.push_back(Code);
}

std::string TransformWord(const std::string &Word, bool FirstWord)
{
	// leading ^ codes are not letters and must stay intact
	size_t Pos = 0;
	std::string Prefix;
	while(Pos + 1 < Word.size() && Word[Pos] == '^')
	{
		Prefix.append(Word, Pos, 2);
		Pos += 2;
	}

	std::vector<int> vCodes;
	const char *p = Word.c_str() + Pos;
	while(*p)
	{
		const int Code = str_utf8_decode(&p);
		if(Code <= 0)
			break;
		vCodes.push_back(Code);
	}

	size_t BodyEnd = vCodes.size();
	while(BodyEnd > 0 && IsTrailingPunctuation(vCodes[BodyEnd - 1]))
		--BodyEnd;

	std::string Suffix;
	for(size_t i = BodyEnd; i < vCodes.size(); i++)
		AppendCodepoint(Suffix, vCodes[i]);

	if(BodyEnd == 0)
		return Word;

	std::vector<int> vOut;
	vOut.reserve(BodyEnd + 4);
	for(size_t i = 0; i < BodyEnd; i++)
	{
		const int Code = vCodes[i];
		if(IsCyrillicVowel(Code))
		{
			if(Chance(g_Config.m_FcUwuChatVowels))
				vOut.push_back(ShiftVowel(Code));
			else
				vOut.push_back(Code);
		}
		else if(IsCyrillicConsonant(Code) || IsLatinConsonant(Code))
		{
			if(Chance(g_Config.m_FcUwuChatConsonants))
				PushShiftedConsonant(vOut, Code);
			else
				vOut.push_back(Code);
		}
		else
			vOut.push_back(Code);
	}

	// duplicate one letter to sell the stutter
	if(vOut.size() > 2 && Chance(g_Config.m_FcUwuChatDouble))
	{
		for(int Attempt = 0; Attempt < 4; Attempt++)
		{
			const int Index = 1 + RandomBelow((int)vOut.size() - 1);
			if(IsLetter(vOut[Index]))
			{
				vOut.insert(vOut.begin() + Index, vOut[Index]);
				break;
			}
		}
	}

	// soft sign after a consonant is what sells the cute
	if(!vOut.empty() && (IsCyrillicConsonant(vOut.back()) || IsLatinConsonant(vOut.back())) &&
		Chance(g_Config.m_FcUwuChatSoftSign))
		vOut.push_back(CP_SOFT_SIGN);

	std::string Result = Prefix;
	const int StutterChance = FirstWord ? g_Config.m_FcUwuChatStutter : g_Config.m_FcUwuChatStutter / 3;
	if(vOut.size() > 1 && IsLetter(vOut[0]) && Chance(StutterChance))
	{
		AppendCodepoint(Result, vOut[0]);
		Result += '-';
	}
	for(int Code : vOut)
		AppendCodepoint(Result, Code);
	Result += Suffix;
	return Result;
}

void SplitList(const char *pList, std::vector<std::string> &vOut)
{
	vOut.clear();
	if(pList == nullptr)
		return;
	const char *p = pList;
	while(*p)
	{
		while(*p == ' ' || *p == '\t' || *p == ',')
			++p;
		if(!*p)
			break;
		const char *pStart = p;
		while(*p && *p != ' ' && *p != '\t' && *p != ',')
			++p;
		vOut.emplace_back(pStart, (size_t)(p - pStart));
	}
}

} // namespace

const char *UwuChat::Transform(int Team, const char *pText, char *pOut, int OutSize)
{
	if(!g_Config.m_FcUwuChat || pText == nullptr || pOut == nullptr || OutSize <= 1)
		return pText;
	if(Team == 1 && !g_Config.m_FcUwuChatTeam)
		return pText;

	// server commands and chatbinds must not be rewritten, they parse
	const char *p = str_utf8_skip_whitespaces(pText);
	if(*p == '\0' || *p == '/' || *p == '!')
		return pText;

	std::vector<std::string> vWords, vEmotes;
	SplitList(g_Config.m_FcUwuChatWordList, vWords);
	SplitList(g_Config.m_FcUwuChatEmoteList, vEmotes);

	std::string Out;
	bool FirstWord = true;
	while(*p)
	{
		// keep room for the closing emote and the trailing NUL
		if(Out.size() + 24 >= (size_t)OutSize)
			break;

		std::string Word;
		while(*p && *p != ' ')
		{
			const int Code = str_utf8_decode(&p);
			if(Code <= 0)
			{
				// drop the malformed byte so the loop can never spin in place
				++p;
				continue;
			}
			AppendCodepoint(Word, Code);
		}

		if(!Word.empty())
		{
			Out += TransformWord(Word, FirstWord);
			FirstWord = false;
		}

		while(*p == ' ')
			++p;
		if(!*p)
			break;

		if(!vWords.empty() && Chance(g_Config.m_FcUwuChatWords))
		{
			Out += ' ';
			Out += vWords[RandomBelow((int)vWords.size())];
			if(!vEmotes.empty() && Chance(g_Config.m_FcUwuChatEmotes))
				Out += vEmotes[RandomBelow((int)vEmotes.size())];
			Out += ' ';
		}
		else
			Out += ' ';
	}

	while(!Out.empty() && Out.back() == ' ')
		Out.pop_back();

	if(Out.empty())
		return pText;

	if(!vEmotes.empty() && Chance(g_Config.m_FcUwuChatEnd))
	{
		Out += ' ';
		Out += vEmotes[RandomBelow((int)vEmotes.size())];
	}

	str_copy(pOut, Out.c_str(), OutSize);
	return pOut;
}
