#include <base/io.h>
#include <base/str.h>
#include <base/time.h>
#include <base/types.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/components/tclient/chat_log_file.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

const char *const CHAT_LOG_DIRECTORY = "chatlog";
constexpr int CHAT_LOG_MAX_LINES = 20000;
constexpr int CHAT_LOG_MAX_BYTES = 4 * 1024 * 1024;
constexpr int CHAT_LOG_GLOBAL_MAX_MATCHES = 2000;

constexpr float CHAT_LOG_LINE_SIZE = 20.0f;
constexpr float CHAT_LOG_FONT_SIZE = 14.0f;
constexpr float CHAT_LOG_EDIT_FONT_SIZE = 12.0f;
constexpr float CHAT_LOG_HEADLINE_FONT_SIZE = 20.0f;
constexpr float CHAT_LOG_MARGIN = 5.0f;
constexpr float CHAT_LOG_MARGIN_LARGE = 30.0f;
constexpr float CHAT_LOG_FILE_LIST_WIDTH = 190.0f;

struct SChatLogFile
{
	std::string m_Name;
	std::string m_Date;
	std::string m_Label;
	bool m_Sealed = false;
};

int ChatLogScanCallback(const char *pFilename, int IsDir, int StorageType, void *pUser)
{
	auto *pFiles = static_cast<std::vector<SChatLogFile> *>(pUser);
	if(IsDir)
		return 0;

	SChatLogFile File;
	if(str_length(pFilename) == CHAT_LOG_SEALED_NAME_LENGTH && str_endswith(pFilename, CHAT_LOG_SEALED_EXTENSION))
		File.m_Sealed = true;
	else if(str_length(pFilename) == CHAT_LOG_LEGACY_NAME_LENGTH && str_endswith(pFilename, CHAT_LOG_LEGACY_EXTENSION))
		File.m_Sealed = false;
	else
		return 0;

	File.m_Name = pFilename;
	File.m_Date = std::string(pFilename, 10);
	File.m_Label = File.m_Sealed ? File.m_Date : File.m_Date + " (txt)";
	pFiles->push_back(std::move(File));
	return 0;
}

void ChatLogLoadSealed(IStorage *pStorage, const char *pName, std::vector<std::string> &vLines, int &Integrity, int &TamperedIndex)
{
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", CHAT_LOG_DIRECTORY, pName);

	const std::string Date(pName, 10);
	std::vector<SChatLogMessage> vMessages;
	SHA256_DIGEST Head;
	Integrity = ChatLogSealedRead(pStorage, aPath, Date.c_str(), vMessages, CHAT_LOG_MAX_LINES, &TamperedIndex, &Head);

	vLines.clear();
	vLines.reserve(vMessages.size());
	for(const SChatLogMessage &Message : vMessages)
	{
		char aTime[16];
		str_timestamp_ex((time_t)Message.m_Timestamp, aTime, sizeof(aTime), "%H:%M:%S");

		char aLine[512];
		if(Message.m_Flags & CHAT_LOG_FLAG_SERVER)
			str_format(aLine, sizeof(aLine), "[%s] *** %s", aTime, Message.m_Text.c_str());
		else if(Message.m_ClientId >= 0)
			str_format(aLine, sizeof(aLine), "[%s] %s%s: %s", aTime, Message.m_Team != 0 ? "[team] " : "", Message.m_Name.c_str(), Message.m_Text.c_str());
		else
			str_format(aLine, sizeof(aLine), "[%s] %s", aTime, Message.m_Text.c_str());

		vLines.emplace_back(aLine);
	}
}

void ChatLogLoadLegacy(IStorage *pStorage, const char *pName, std::vector<std::string> &vLines)
{
	vLines.clear();

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", CHAT_LOG_DIRECTORY, pName);
	IOHANDLE File = pStorage->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;

	std::string Content;
	char aBuf[4096];
	while(Content.size() < (size_t)CHAT_LOG_MAX_BYTES)
	{
		const unsigned NumRead = io_read(File, aBuf, sizeof(aBuf));
		if(NumRead == 0)
			break;
		Content.append(aBuf, NumRead);
	}
	io_close(File);

	size_t Start = 0;
	while(Start < Content.size() && (int)vLines.size() < CHAT_LOG_MAX_LINES)
	{
		size_t End = Content.find('\n', Start);
		if(End == std::string::npos)
			End = Content.size();

		std::string Line = Content.substr(Start, End - Start);
		if(!Line.empty() && Line.back() == '\r')
			Line.pop_back();
		if(!Line.empty())
			vLines.push_back(std::move(Line));

		if(End == Content.size())
			break;
		Start = End + 1;
	}
}

void ChatLogBuildMatches(const std::vector<std::string> &vLines, const char *pSearch, std::vector<int> &vMatches)
{
	vMatches.clear();
	for(size_t i = 0; i < vLines.size(); i++)
	{
		if(pSearch[0] == '\0' || str_find_nocase(vLines[i].c_str(), pSearch) != nullptr)
			vMatches.push_back((int)i);
	}
}

void ChatLogBuildGlobal(IStorage *pStorage, const std::vector<SChatLogFile> &vFiles, const char *pSearch, std::vector<std::string> &vMatches, std::vector<int> &vFileIndices, std::vector<int> &vLineIndices, int &Days, int &BadFiles)
{
	vMatches.clear();
	vFileIndices.clear();
	vLineIndices.clear();
	Days = 0;
	BadFiles = 0;

	std::vector<std::string> vLines;
	char aLine[512];
	for(size_t f = 0; f < vFiles.size() && (int)vMatches.size() < CHAT_LOG_GLOBAL_MAX_MATCHES; f++)
	{
		int Integrity;
		int TamperedIndex;
		if(vFiles[f].m_Sealed)
		{
			ChatLogLoadSealed(pStorage, vFiles[f].m_Name.c_str(), vLines, Integrity, TamperedIndex);
		}
		else
		{
			ChatLogLoadLegacy(pStorage, vFiles[f].m_Name.c_str(), vLines);
			Integrity = CHAT_LOG_INTEGRITY_LEGACY;
			TamperedIndex = -1;
		}

		if(Integrity == CHAT_LOG_INTEGRITY_TAMPERED || Integrity == CHAT_LOG_INTEGRITY_UNREADABLE)
			BadFiles++;

		int NumDayMatches = 0;
		for(size_t i = 0; i < vLines.size() && (int)vMatches.size() < CHAT_LOG_GLOBAL_MAX_MATCHES; i++)
		{
			if(str_find_nocase(vLines[i].c_str(), pSearch) == nullptr)
				continue;

			str_format(aLine, sizeof(aLine), "%s %s", vFiles[f].m_Date.c_str(), vLines[i].c_str());
			vMatches.emplace_back(aLine);
			vFileIndices.push_back((int)f);
			vLineIndices.push_back((int)i);
			NumDayMatches++;
		}

		if(NumDayMatches > 0)
			Days++;
	}
}

}

void CMenus::RenderSettingsFusionChatLog(CUIRect MainView)
{
	static std::vector<SChatLogFile> s_vFiles;
	static bool s_Scanned = false;
	static int s_SelectedFile = -1;
	static std::vector<std::string> s_vLines;
	static std::string s_LoadedFile;
	static std::vector<int> s_vMatches;
	static std::string s_MatchesFor;
	static bool s_MatchesDirty = true;
	static int s_SelectedLine = -1;
	static int s_Integrity = CHAT_LOG_INTEGRITY_VERIFIED;
	static int s_TamperedIndex = -1;
	static bool s_ScrollToSelectedLine = false;
	static bool s_PreviewFocused = false;
	static bool s_GlobalSearch = false;
	static std::vector<std::string> s_vGlobalMatches;
	static std::vector<int> s_vGlobalFiles;
	static std::vector<int> s_vGlobalLines;
	static std::string s_GlobalFor;
	static bool s_GlobalDirty = true;
	static int s_GlobalDays = 0;
	static int s_GlobalBadFiles = 0;
	static int s_GlobalSelected = -1;
	static bool s_GlobalScrollToSelected = false;
	static int s_PendingJumpLine = -1;
	static CListBox s_FileList;
	static CListBox s_GlobalList;
	static CListBox s_LineList;
	static CLineInput s_SearchInput;
	static char s_aSearch[64] = "";
	static CButtonContainer s_OpenFolderButton;
	static CButtonContainer s_RefreshButton;

	CUIRect Header, Toolbar, Options, Files, Preview;
	MainView.HSplitTop(CHAT_LOG_MARGIN, nullptr, &MainView);
	MainView.HSplitTop(CHAT_LOG_HEADLINE_FONT_SIZE, &Header, &MainView);
	MainView.HSplitTop(CHAT_LOG_MARGIN, nullptr, &MainView);
	MainView.HSplitTop(CHAT_LOG_LINE_SIZE, &Toolbar, &MainView);
	MainView.HSplitTop(CHAT_LOG_MARGIN, nullptr, &MainView);
	MainView.HSplitTop(CHAT_LOG_LINE_SIZE, &Options, &MainView);
	MainView.HSplitTop(CHAT_LOG_MARGIN, nullptr, &MainView);

	MainView.VSplitLeft(CHAT_LOG_FILE_LIST_WIDTH, &Files, &Preview);
	Files.VSplitRight(CHAT_LOG_MARGIN, &Files, nullptr);
	Preview.VSplitLeft(CHAT_LOG_MARGIN_LARGE, nullptr, &Preview);

	CUIRect Title, OpenFolderButton;
	Header.VSplitRight(130.0f, &Title, &OpenFolderButton);
	Ui()->DoLabel(&Title, TCLocalize("Chat Logs"), CHAT_LOG_HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	if(DoButtonLineSize_Menu(&s_OpenFolderButton, TCLocalize("Open Folder"), 0, &OpenFolderButton, CHAT_LOG_LINE_SIZE, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		char aPath[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, CHAT_LOG_DIRECTORY, aPath, sizeof(aPath));
		Client()->ViewFile(aPath);
	}

	CUIRect SearchBox, RefreshButton;
	Toolbar.VSplitRight(110.0f, &SearchBox, &RefreshButton);
	SearchBox.VSplitRight(CHAT_LOG_MARGIN, &SearchBox, nullptr);
	s_SearchInput.SetBuffer(s_aSearch, sizeof(s_aSearch));
	Ui()->DoEditBox_Search(&s_SearchInput, &SearchBox, CHAT_LOG_EDIT_FONT_SIZE, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());
	if(DoButtonLineSize_Menu(&s_RefreshButton, TCLocalize("Refresh"), 0, &RefreshButton, CHAT_LOG_LINE_SIZE, false, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		s_Scanned = false;
		s_LoadedFile.clear();
	}

	CUIRect SaveLogs, SearchAll;
	Options.VSplitLeft(300.0f, &SaveLogs, &Options);
	if(DoButton_CheckBox(&g_Config.m_FcChatLog, TCLocalize("Save chat logs"), g_Config.m_FcChatLog, &SaveLogs))
		g_Config.m_FcChatLog ^= 1;
	Options.VSplitLeft(300.0f, &SearchAll, nullptr);
	if(DoButton_CheckBox(&s_GlobalSearch, TCLocalize("Search all days"), s_GlobalSearch, &SearchAll))
		s_GlobalSearch ^= 1;

	if(Ui()->MouseButtonClicked(0))
	{
		if(Ui()->MouseHovered(&Preview))
			s_PreviewFocused = true;
		else if(Ui()->MouseHovered(&Files))
			s_PreviewFocused = false;
	}

	if(!s_Scanned)
	{
		s_vFiles.clear();
		Storage()->ListDirectory(IStorage::TYPE_SAVE, CHAT_LOG_DIRECTORY, ChatLogScanCallback, &s_vFiles);
		std::sort(s_vFiles.begin(), s_vFiles.end(), [](const SChatLogFile &Left, const SChatLogFile &Right) {
			return Left.m_Date > Right.m_Date;
		});
		s_Scanned = true;
		s_GlobalDirty = true;

		if(s_SelectedFile >= (int)s_vFiles.size() || s_SelectedFile < -1)
			s_SelectedFile = -1;
		if(s_SelectedFile < 0 && !s_vFiles.empty())
			s_SelectedFile = 0;
	}

	const int PrevSelectedFile = s_SelectedFile;
	s_FileList.SetActive(!s_PreviewFocused);
	s_FileList.DoHeader(&Files, TCLocalize("Date"), CHAT_LOG_LINE_SIZE);
	s_FileList.DoStart(CHAT_LOG_LINE_SIZE, (int)s_vFiles.size(), 1, 8, s_SelectedFile);
	for(size_t i = 0; i < s_vFiles.size(); i++)
	{
		const CListboxItem Item = s_FileList.DoNextItem(&s_vFiles[i], (int)i == s_SelectedFile);
		if(!Item.m_Visible)
			continue;

		CUIRect Label = Item.m_Rect;
		Label.VSplitLeft(CHAT_LOG_MARGIN, nullptr, &Label);
		Label.VSplitRight(CHAT_LOG_MARGIN, &Label, nullptr);
		Ui()->DoLabel(&Label, s_vFiles[i].m_Label.c_str(), CHAT_LOG_FONT_SIZE, TEXTALIGN_ML, {.m_MaxWidth = Label.w, .m_EllipsisAtEnd = true});
	}
	s_SelectedFile = s_FileList.DoEnd();
	if(s_SelectedFile != PrevSelectedFile)
		s_GlobalSearch = false;

	const char *pSelectedName = s_SelectedFile >= 0 && s_SelectedFile < (int)s_vFiles.size() ? s_vFiles[s_SelectedFile].m_Name.c_str() : nullptr;
	if(pSelectedName == nullptr)
	{
		if(!s_LoadedFile.empty())
		{
			s_vLines.clear();
			s_vMatches.clear();
			s_LoadedFile.clear();
			s_MatchesFor.clear();
			s_MatchesDirty = true;
			s_SelectedLine = -1;
			s_Integrity = CHAT_LOG_INTEGRITY_VERIFIED;
			s_TamperedIndex = -1;
		}
	}
	else if(s_LoadedFile != pSelectedName)
	{
		if(s_vFiles[s_SelectedFile].m_Sealed)
		{
			ChatLogLoadSealed(Storage(), pSelectedName, s_vLines, s_Integrity, s_TamperedIndex);
		}
		else
		{
			ChatLogLoadLegacy(Storage(), pSelectedName, s_vLines);
			s_Integrity = CHAT_LOG_INTEGRITY_LEGACY;
			s_TamperedIndex = -1;
		}
		s_LoadedFile = pSelectedName;
		s_MatchesDirty = true;
		s_SelectedLine = -1;
		s_ScrollToSelectedLine = true;
	}

	if(s_MatchesDirty || s_MatchesFor != s_aSearch)
	{
		ChatLogBuildMatches(s_vLines, s_aSearch, s_vMatches);
		s_MatchesFor = s_aSearch;
		s_MatchesDirty = false;
		s_SelectedLine = s_vMatches.empty() ? -1 : (int)s_vMatches.size() - 1;
		s_ScrollToSelectedLine = true;
	}

	if(s_GlobalSearch && s_aSearch[0] != '\0' && (s_GlobalDirty || s_GlobalFor != s_aSearch))
	{
		ChatLogBuildGlobal(Storage(), s_vFiles, s_aSearch, s_vGlobalMatches, s_vGlobalFiles, s_vGlobalLines, s_GlobalDays, s_GlobalBadFiles);
		s_GlobalFor = s_aSearch;
		s_GlobalDirty = false;
		s_GlobalSelected = s_vGlobalMatches.empty() ? -1 : 0;
		s_GlobalScrollToSelected = true;
	}

	if(s_PendingJumpLine >= 0)
	{
		for(size_t i = 0; i < s_vMatches.size(); i++)
		{
			if(s_vMatches[i] == s_PendingJumpLine)
			{
				s_SelectedLine = (int)i;
				s_ScrollToSelectedLine = true;
				break;
			}
		}
		s_PendingJumpLine = -1;
	}

	CUIRect PreviewHeader;
	Preview.HSplitTop(CHAT_LOG_LINE_SIZE, &PreviewHeader, &Preview);

	const bool GlobalMode = s_GlobalSearch;
	const bool GlobalQuery = GlobalMode && s_aSearch[0] != '\0';

	CUIRect CountRect, StatusRect;
	PreviewHeader.VSplitRight(190.0f, &CountRect, &StatusRect);

	char aBuf[256] = "";
	char aStatus[128] = "";
	ColorRGBA StatusColor(0.45f, 0.9f, 0.45f, 1.0f);

	if(GlobalMode)
	{
		if(GlobalQuery)
		{
			if((int)s_vGlobalMatches.size() >= CHAT_LOG_GLOBAL_MAX_MATCHES)
				str_format(aBuf, sizeof(aBuf), "%d+ matches in %d days", (int)s_vGlobalMatches.size(), s_GlobalDays);
			else
				str_format(aBuf, sizeof(aBuf), "%d matches in %d days", (int)s_vGlobalMatches.size(), s_GlobalDays);

			if(s_GlobalBadFiles > 0)
			{
				str_format(aStatus, sizeof(aStatus), TCLocalize("Modified logs: %d"), s_GlobalBadFiles);
				StatusColor = ColorRGBA(1.0f, 0.35f, 0.35f, 1.0f);
			}
			else
			{
				str_copy(aStatus, TCLocalize("All logs verified"), sizeof(aStatus));
			}
		}
		else
		{
			str_copy(aBuf, TCLocalize("All days"), sizeof(aBuf));
		}
	}
	else if(pSelectedName != nullptr)
	{
		if(s_aSearch[0] != '\0')
			str_format(aBuf, sizeof(aBuf), "%s (%d of %d messages)", s_vFiles[s_SelectedFile].m_Date.c_str(), (int)s_vMatches.size(), (int)s_vLines.size());
		else
			str_format(aBuf, sizeof(aBuf), "%s (%d messages)", s_vFiles[s_SelectedFile].m_Date.c_str(), (int)s_vLines.size());

		if(s_Integrity == CHAT_LOG_INTEGRITY_TAMPERED)
		{
			str_format(aStatus, sizeof(aStatus), TCLocalize("Modified at message %d"), s_TamperedIndex + 1);
			StatusColor = ColorRGBA(1.0f, 0.35f, 0.35f, 1.0f);
		}
		else if(s_Integrity == CHAT_LOG_INTEGRITY_LEGACY)
		{
			str_copy(aStatus, TCLocalize("Legacy log (unsealed)"), sizeof(aStatus));
			StatusColor = ColorRGBA(1.0f, 0.8f, 0.35f, 1.0f);
		}
		else if(s_Integrity == CHAT_LOG_INTEGRITY_UNREADABLE)
		{
			str_copy(aStatus, TCLocalize("Unreadable log file"), sizeof(aStatus));
			StatusColor = ColorRGBA(1.0f, 0.35f, 0.35f, 1.0f);
		}
		else
		{
			str_copy(aStatus, TCLocalize("Chain verified"), sizeof(aStatus));
		}
	}

	if(aBuf[0] != '\0')
		Ui()->DoLabel(&CountRect, aBuf, CHAT_LOG_FONT_SIZE, TEXTALIGN_ML, {.m_MaxWidth = CountRect.w, .m_EllipsisAtEnd = true});
	if(aStatus[0] != '\0')
	{
		TextRender()->TextColor(StatusColor);
		Ui()->DoLabel(&StatusRect, aStatus, CHAT_LOG_FONT_SIZE, TEXTALIGN_MR, {.m_MaxWidth = StatusRect.w, .m_EllipsisAtEnd = true});
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	s_LineList.SetActive(!GlobalMode && s_PreviewFocused);
	s_GlobalList.SetActive(GlobalQuery && s_PreviewFocused);

	if(GlobalMode)
	{
		if(s_vFiles.empty())
		{
			Ui()->DoLabel(&Preview, TCLocalize("No chat logs found yet."), CHAT_LOG_FONT_SIZE, TEXTALIGN_MC, {.m_MaxWidth = Preview.w});
			return;
		}
		if(!GlobalQuery)
		{
			Ui()->DoLabel(&Preview, TCLocalize("Type to search all days."), CHAT_LOG_FONT_SIZE, TEXTALIGN_MC, {.m_MaxWidth = Preview.w});
			return;
		}
		if(s_vGlobalMatches.empty())
		{
			Ui()->DoLabel(&Preview, TCLocalize("No messages match the search."), CHAT_LOG_FONT_SIZE, TEXTALIGN_MC, {.m_MaxWidth = Preview.w});
			return;
		}

		if(s_GlobalScrollToSelected)
		{
			s_GlobalList.ScrollToSelected();
			s_GlobalScrollToSelected = false;
		}
		s_GlobalList.DoStart(CHAT_LOG_LINE_SIZE, (int)s_vGlobalMatches.size(), 1, 12, s_GlobalSelected, &Preview);
		int JumpTo = -1;
		for(size_t i = 0; i < s_vGlobalMatches.size(); i++)
		{
			const CListboxItem Item = s_GlobalList.DoNextItem((const void *)(intptr_t)(i + 1), (int)i == s_GlobalSelected);
			if(!Item.m_Visible)
				continue;

			if(Ui()->LastMouseButton(0) && !Ui()->MouseButton(0) && Ui()->MouseHovered(&Item.m_Rect))
				JumpTo = (int)i;

			CUIRect Label = Item.m_Rect;
			Label.VSplitLeft(CHAT_LOG_MARGIN, nullptr, &Label);
			Label.VSplitRight(CHAT_LOG_MARGIN, &Label, nullptr);
			Ui()->DoLabel(&Label, s_vGlobalMatches[i].c_str(), CHAT_LOG_FONT_SIZE, TEXTALIGN_ML, {.m_MaxWidth = Label.w, .m_EllipsisAtEnd = true});
		}
		s_GlobalSelected = s_GlobalList.DoEnd();

		if(JumpTo >= 0)
		{
			s_SelectedFile = s_vGlobalFiles[JumpTo];
			s_PendingJumpLine = s_vGlobalLines[JumpTo];
			s_GlobalSearch = false;
		}
		return;
	}

	if(s_ScrollToSelectedLine)
	{
		s_LineList.ScrollToSelected();
		s_ScrollToSelectedLine = false;
	}
	s_LineList.DoStart(CHAT_LOG_LINE_SIZE, (int)s_vMatches.size(), 1, 12, s_SelectedLine, &Preview);
	for(size_t i = 0; i < s_vMatches.size(); i++)
	{
		const CListboxItem Item = s_LineList.DoNextItem((const void *)(intptr_t)(s_vMatches[i] + 1), (int)i == s_SelectedLine);
		if(!Item.m_Visible)
			continue;

		CUIRect Label = Item.m_Rect;
		Label.VSplitLeft(CHAT_LOG_MARGIN, nullptr, &Label);
		Label.VSplitRight(CHAT_LOG_MARGIN, &Label, nullptr);
		Ui()->DoLabel(&Label, s_vLines[s_vMatches[i]].c_str(), CHAT_LOG_FONT_SIZE, TEXTALIGN_ML, {.m_MaxWidth = Label.w, .m_EllipsisAtEnd = true});
	}
	s_SelectedLine = s_LineList.DoEnd();

	if(s_vFiles.empty())
		Ui()->DoLabel(&Preview, TCLocalize("No chat logs found yet."), CHAT_LOG_FONT_SIZE, TEXTALIGN_MC, {.m_MaxWidth = Preview.w});
	else if(pSelectedName != nullptr && s_Integrity == CHAT_LOG_INTEGRITY_UNREADABLE)
		Ui()->DoLabel(&Preview, TCLocalize("Unreadable log file"), CHAT_LOG_FONT_SIZE, TEXTALIGN_MC, {.m_MaxWidth = Preview.w});
	else if(pSelectedName != nullptr && s_vLines.empty())
		Ui()->DoLabel(&Preview, TCLocalize("This chat log is empty."), CHAT_LOG_FONT_SIZE, TEXTALIGN_MC, {.m_MaxWidth = Preview.w});
	else if(pSelectedName != nullptr && s_vMatches.empty() && s_aSearch[0] != '\0')
		Ui()->DoLabel(&Preview, TCLocalize("No messages match the search."), CHAT_LOG_FONT_SIZE, TEXTALIGN_MC, {.m_MaxWidth = Preview.w});
}
