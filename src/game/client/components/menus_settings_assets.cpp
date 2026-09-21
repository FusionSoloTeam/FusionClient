#include "menus.h"

#include <base/fs.h>
#include <base/io.h>
#include <base/system.h>
#include <zlib.h>

#include <engine/font_icons.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <chrono>

using namespace std::chrono_literals;

typedef std::function<void()> TMenuAssetScanLoadedFunc;

struct SMenuAssetScanUser
{
	void *m_pUser;
	TMenuAssetScanLoadedFunc m_LoadedFunc;
};

// IDs of the tabs in the Assets menu
enum
{
	ASSETS_TAB_ENTITIES = 0,
	ASSETS_TAB_GAME = 1,
	ASSETS_TAB_EMOTICONS = 2,
	ASSETS_TAB_PARTICLES = 3,
	ASSETS_TAB_HUD = 4,
	ASSETS_TAB_EXTRAS = 5,
	ASSETS_TAB_AUDIO = 6,
	NUMBER_OF_ASSETS_TABS = 7,
};

void CMenus::LoadEntities(SCustomEntities *pEntitiesItem, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;

	char aPath[IO_MAX_PATH_LENGTH];
	if(str_comp(pEntitiesItem->m_aName, "default") == 0)
	{
		for(int i = 0; i < MAP_IMAGE_MOD_TYPE_COUNT; ++i)
		{
			str_format(aPath, sizeof(aPath), "editor/entities_clear/%s.png", gs_apModEntitiesNames[i]);
			pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
			if(!pEntitiesItem->m_RenderTexture.IsValid() || pEntitiesItem->m_RenderTexture.IsNullTexture())
				pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
		}
	}
	else
	{
		for(int i = 0; i < MAP_IMAGE_MOD_TYPE_COUNT; ++i)
		{
			str_format(aPath, sizeof(aPath), "assets/entities/%s/%s.png", pEntitiesItem->m_aName, gs_apModEntitiesNames[i]);
			pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
			if(pEntitiesItem->m_aImages[i].m_Texture.IsNullTexture())
			{
				str_format(aPath, sizeof(aPath), "assets/entities/%s.png", pEntitiesItem->m_aName);
				pEntitiesItem->m_aImages[i].m_Texture = pThis->Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
			}
			if(!pEntitiesItem->m_RenderTexture.IsValid() || pEntitiesItem->m_RenderTexture.IsNullTexture())
				pEntitiesItem->m_RenderTexture = pEntitiesItem->m_aImages[i].m_Texture;
		}
	}
}

int CMenus::EntitiesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	if(IsDir)
	{
		if(pName[0] == '.')
			return 0;

		// default is reserved
		if(str_comp(pName, "default") == 0)
			return 0;

		SCustomEntities EntitiesItem;
		str_copy(EntitiesItem.m_aName, pName);
		CMenus::LoadEntities(&EntitiesItem, pUser);
		pThis->m_vEntitiesList.push_back(EntitiesItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[IO_MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, aName);
			CMenus::LoadEntities(&EntitiesItem, pUser);
			pThis->m_vEntitiesList.push_back(EntitiesItem);
		}
	}

	pRealUser->m_LoadedFunc();

	return 0;
}

template<typename TName>
static void LoadAsset(TName *pAssetItem, const char *pAssetName, IGraphics *pGraphics)
{
	char aPath[IO_MAX_PATH_LENGTH];
	if(str_comp(pAssetItem->m_aName, "default") == 0)
	{
		str_format(aPath, sizeof(aPath), "%s.png", pAssetName);
		pAssetItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
	}
	else
	{
		str_format(aPath, sizeof(aPath), "assets/%s/%s.png", pAssetName, pAssetItem->m_aName);
		pAssetItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
		if(pAssetItem->m_RenderTexture.IsNullTexture())
		{
			str_format(aPath, sizeof(aPath), "assets/%s/%s/%s.png", pAssetName, pAssetItem->m_aName, pAssetName);
			pAssetItem->m_RenderTexture = pGraphics->LoadTexture(aPath, IStorage::TYPE_ALL);
		}
	}
}

template<typename TName>
static int AssetScan(const char *pName, int IsDir, int DirType, std::vector<TName> &vAssetList, const char *pAssetName, IGraphics *pGraphics, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	if(IsDir)
	{
		if(pName[0] == '.')
			return 0;

		// default is reserved
		if(str_comp(pName, "default") == 0)
			return 0;

		TName AssetItem;
		str_copy(AssetItem.m_aName, pName);
		LoadAsset(&AssetItem, pAssetName, pGraphics);
		vAssetList.push_back(AssetItem);
	}
	else
	{
		if(str_endswith(pName, ".png"))
		{
			char aName[IO_MAX_PATH_LENGTH];
			str_truncate(aName, sizeof(aName), pName, str_length(pName) - 4);
			// default is reserved
			if(str_comp(aName, "default") == 0)
				return 0;

			TName AssetItem;
			str_copy(AssetItem.m_aName, aName);
			LoadAsset(&AssetItem, pAssetName, pGraphics);
			vAssetList.push_back(AssetItem);
		}
	}

	pRealUser->m_LoadedFunc();

	return 0;
}

int CMenus::GameScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vGameList, "game", pGraphics, pUser);
}

int CMenus::EmoticonsScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vEmoticonList, "emoticons", pGraphics, pUser);
}

int CMenus::ParticlesScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vParticlesList, "particles", pGraphics, pUser);
}

int CMenus::HudScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vHudList, "hud", pGraphics, pUser);
}

int CMenus::ExtrasScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	IGraphics *pGraphics = pThis->Graphics();
	return AssetScan(pName, IsDir, DirType, pThis->m_vExtrasList, "extras", pGraphics, pUser);
}

int CMenus::AudioScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	(void)DirType;
	auto *pRealUser = (SMenuAssetScanUser *)pUser;
	auto *pThis = (CMenus *)pRealUser->m_pUser;
	if(!IsDir || pName[0] == '.' || str_comp(pName, "default") == 0)
		return 0;
	SCustomAudio AudioItem;
	str_copy(AudioItem.m_aName, pName);
	pThis->m_vAudioList.push_back(AudioItem);
	pRealUser->m_LoadedFunc();
	return 0;
}

static std::vector<const CMenus::SCustomEntities *> gs_vpSearchEntitiesList;
static std::vector<const CMenus::SCustomGame *> gs_vpSearchGamesList;
static std::vector<const CMenus::SCustomEmoticon *> gs_vpSearchEmoticonsList;
static std::vector<const CMenus::SCustomParticle *> gs_vpSearchParticlesList;
static std::vector<const CMenus::SCustomHud *> gs_vpSearchHudList;
static std::vector<const CMenus::SCustomExtras *> gs_vpSearchExtrasList;
static std::vector<const CMenus::SCustomAudio *> gs_vpSearchAudioList;

static bool gs_aInitCustomList[NUMBER_OF_ASSETS_TABS] = {
	true,
};

static size_t gs_aCustomListSize[NUMBER_OF_ASSETS_TABS] = {
	0,
};

static CLineInputBuffered<64> s_aFilterInputs[NUMBER_OF_ASSETS_TABS];

static int s_CurCustomTab = ASSETS_TAB_ENTITIES;

static CUi::SSelectionPopupContext gs_AssetsContextMenu;
static CScrollRegion gs_AssetsContextMenuScroll;
static char gs_aAssetsContextName[64] = "";
static int gs_AssetsContextTab = -1;

static CUi::SConfirmPopupContext gs_AssetsDeleteConfirm;
static char gs_aAssetsDeleteName[64] = "";
static int gs_AssetsDeleteTab = -1;

static SPopupMenuId gs_AssetsRenamePopupId;
static int gs_AssetsRenameTab = -1;
static char gs_aAssetsRenameOld[64] = "";
static char gs_aAssetsRenameNew[64] = "";
static CLineInput gs_AssetsRenameInput(gs_aAssetsRenameNew, sizeof(gs_aAssetsRenameNew));
static float gs_AssetsRenameX = 0.0f;
static float gs_AssetsRenameY = 0.0f;
static CButtonContainer s_AssetsRenameApply;
static CButtonContainer s_AssetsRenameCancel;

static const char *AssetsTabTypeName(int Tab)
{
	if(Tab == ASSETS_TAB_ENTITIES)
		return "entities";
	if(Tab == ASSETS_TAB_GAME)
		return "game";
	if(Tab == ASSETS_TAB_EMOTICONS)
		return "emoticons";
	if(Tab == ASSETS_TAB_PARTICLES)
		return "particles";
	if(Tab == ASSETS_TAB_HUD)
		return "hud";
	if(Tab == ASSETS_TAB_EXTRAS)
		return "extras";
	if(Tab == ASSETS_TAB_AUDIO)
		return "audio";
	return "game";
}

static const CMenus::SCustomItem *GetCustomItem(int CurTab, size_t Index)
{
	if(CurTab == ASSETS_TAB_ENTITIES)
		return gs_vpSearchEntitiesList[Index];
	else if(CurTab == ASSETS_TAB_GAME)
		return gs_vpSearchGamesList[Index];
	else if(CurTab == ASSETS_TAB_EMOTICONS)
		return gs_vpSearchEmoticonsList[Index];
	else if(CurTab == ASSETS_TAB_PARTICLES)
		return gs_vpSearchParticlesList[Index];
	else if(CurTab == ASSETS_TAB_HUD)
		return gs_vpSearchHudList[Index];
	else if(CurTab == ASSETS_TAB_EXTRAS)
		return gs_vpSearchExtrasList[Index];
	else if(CurTab == ASSETS_TAB_AUDIO)
		return gs_vpSearchAudioList[Index];
	dbg_assert_failed("Invalid CurTab: %d", CurTab);
}

template<typename TName>
static void ClearAssetList(std::vector<TName> &vList, IGraphics *pGraphics)
{
	for(TName &Asset : vList)
	{
		pGraphics->UnloadTexture(&Asset.m_RenderTexture);
	}
	vList.clear();
}

void CMenus::ClearCustomItems(int CurTab)
{
	if(CurTab == ASSETS_TAB_ENTITIES)
	{
		for(auto &Entity : m_vEntitiesList)
		{
			for(auto &Image : Entity.m_aImages)
			{
				Graphics()->UnloadTexture(&Image.m_Texture);
			}
		}
		m_vEntitiesList.clear();

		// reload current entities
		GameClient()->m_MapImages.ChangeEntitiesPath(g_Config.m_ClAssetsEntities);
	}
	else if(CurTab == ASSETS_TAB_GAME)
	{
		ClearAssetList(m_vGameList, Graphics());

		// reload current game skin
		GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
	}
	else if(CurTab == ASSETS_TAB_EMOTICONS)
	{
		ClearAssetList(m_vEmoticonList, Graphics());

		// reload current emoticons skin
		GameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
	}
	else if(CurTab == ASSETS_TAB_PARTICLES)
	{
		ClearAssetList(m_vParticlesList, Graphics());

		// reload current particles skin
		GameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
	}
	else if(CurTab == ASSETS_TAB_HUD)
	{
		ClearAssetList(m_vHudList, Graphics());

		// reload current hud skin
		GameClient()->LoadHudSkin(g_Config.m_ClAssetHud);
	}
	else if(CurTab == ASSETS_TAB_EXTRAS)
	{
		ClearAssetList(m_vExtrasList, Graphics());

		// reload current DDNet particles skin
		GameClient()->LoadExtrasSkin(g_Config.m_ClAssetExtras);
	}
	else if(CurTab == ASSETS_TAB_AUDIO)
	{
		ClearAssetList(m_vAudioList, Graphics());
		GameClient()->LoadAudioAssets(g_Config.m_ClAssetAudio);
	}
	else
	{
		dbg_assert_failed("Invalid CurTab: %d", CurTab);
	}
	gs_aInitCustomList[CurTab] = true;
}

void CMenus::AssetsRenameApply()
{
	if(gs_AssetsRenameTab < 0 || gs_aAssetsRenameNew[0] == '\0')
		return;
	if(str_comp(gs_aAssetsRenameOld, gs_aAssetsRenameNew) == 0)
		return;
	if(str_find(gs_aAssetsRenameNew, "/") || str_find(gs_aAssetsRenameNew, "\\") || str_find(gs_aAssetsRenameNew, ".."))
		return;

	const char *pType = AssetsTabTypeName(gs_AssetsRenameTab);
	char aOldFolder[IO_MAX_PATH_LENGTH];
	char aNewFolder[IO_MAX_PATH_LENGTH];
	char aOldFile[IO_MAX_PATH_LENGTH];
	char aNewFile[IO_MAX_PATH_LENGTH];
	str_format(aOldFolder, sizeof(aOldFolder), "assets/%s/%s", pType, gs_aAssetsRenameOld);
	str_format(aNewFolder, sizeof(aNewFolder), "assets/%s/%s", pType, gs_aAssetsRenameNew);
	str_format(aOldFile, sizeof(aOldFile), "assets/%s/%s.png", pType, gs_aAssetsRenameOld);
	str_format(aNewFile, sizeof(aNewFile), "assets/%s/%s.png", pType, gs_aAssetsRenameNew);

	if(Storage()->FolderExists(aOldFolder, IStorage::TYPE_SAVE))
		Storage()->RenameFile(aOldFolder, aNewFolder, IStorage::TYPE_SAVE);
	else
		Storage()->RenameFile(aOldFile, aNewFile, IStorage::TYPE_SAVE);
	ClearCustomItems(gs_AssetsRenameTab);
}

CUi::EPopupMenuFunctionResult CMenus::PopupAssetsRename(void *pContext, CUIRect View, bool Active)
{
	CMenus *pMenus = (CMenus *)pContext;
	CUIRect Title, InputRow, Buttons;
	View.HSplitTop(20.0f, &Title, &View);
	pMenus->Ui()->DoLabel(&Title, Localize("Rename"), 14.0f, TEXTALIGN_ML);
	View.HSplitTop(6.0f, nullptr, &View);
	View.HSplitTop(24.0f, &InputRow, &View);
	pMenus->Ui()->DoEditBox(&gs_AssetsRenameInput, &InputRow, 12.0f);
	View.HSplitBottom(22.0f, &View, &Buttons);
	CUIRect ApplyButton, CancelButton;
	Buttons.VSplitMid(&ApplyButton, &CancelButton, 6.0f);
	if(pMenus->DoButton_Menu(&s_AssetsRenameApply, Localize("Rename"), 0, &ApplyButton) || (Active && pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		pMenus->AssetsRenameApply();
		return CUi::POPUP_CLOSE_CURRENT;
	}
	if(pMenus->DoButton_Menu(&s_AssetsRenameCancel, Localize("Cancel"), 0, &CancelButton) || (Active && pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)))
		return CUi::POPUP_CLOSE_CURRENT;
	return CUi::POPUP_KEEP_OPEN;
}

void CMenus::AssetsContextAction(int Tab, const char *pName, int Selection)
{
	const bool IsDefault = str_comp(pName, "default") == 0;
	const char *pType = AssetsTabTypeName(Tab);
	char aFolder[IO_MAX_PATH_LENGTH];
	char aFile[IO_MAX_PATH_LENGTH];
	str_format(aFolder, sizeof(aFolder), "assets/%s/%s", pType, pName);
	str_format(aFile, sizeof(aFile), "assets/%s/%s.png", pType, pName);

	if(IsDefault)
		Selection = Tab == ASSETS_TAB_AUDIO ? 2 : (Selection == 0 ? 3 : 2);

	if(Selection == 0)
	{
		gs_AssetsDeleteConfirm.Reset();
		gs_AssetsDeleteConfirm.YesNoButtons();
		str_format(gs_AssetsDeleteConfirm.m_aMessage, sizeof(gs_AssetsDeleteConfirm.m_aMessage), Localize("Delete \"%s\"?"), pName);
		Ui()->ShowPopupConfirm(Ui()->MouseX(), Ui()->MouseY(), &gs_AssetsDeleteConfirm);
		str_copy(gs_aAssetsDeleteName, pName);
		gs_AssetsDeleteTab = Tab;
	}
	else if(Selection == 1)
	{
		if(Ui()->IsPopupOpen(&gs_AssetsRenamePopupId))
			Ui()->ClosePopupMenu(&gs_AssetsRenamePopupId);
		gs_AssetsRenameTab = Tab;
		str_copy(gs_aAssetsRenameOld, pName);
		str_copy(gs_aAssetsRenameNew, pName);
		gs_AssetsRenameInput.Set(gs_aAssetsRenameNew);
		gs_AssetsRenameX = Ui()->MouseX();
		gs_AssetsRenameY = Ui()->MouseY();
		Ui()->DoPopupMenu(&gs_AssetsRenamePopupId, gs_AssetsRenameX, gs_AssetsRenameY, 230.0f, 96.0f, this, PopupAssetsRename);
	}
	else if(Selection == 2)
	{
		char aComplete[IO_MAX_PATH_LENGTH];
		char aTarget[IO_MAX_PATH_LENGTH];
		if(IsDefault)
			str_format(aTarget, sizeof(aTarget), "assets/%s", pType);
		else if(Storage()->FolderExists(aFolder, IStorage::TYPE_SAVE))
			str_copy(aTarget, aFolder);
		else
			str_copy(aTarget, aFile);
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, aTarget, aComplete, sizeof(aComplete));
		Client()->ViewFile(aComplete);
	}
	else if(Selection == 3)
	{
		int EditorType = ASSETS_EDITOR_TYPE_GAME;
		if(Tab == ASSETS_TAB_ENTITIES)
			EditorType = ASSETS_EDITOR_TYPE_ENTITIES;
		else if(Tab == ASSETS_TAB_EMOTICONS)
			EditorType = ASSETS_EDITOR_TYPE_EMOTICONS;
		else if(Tab == ASSETS_TAB_PARTICLES)
			EditorType = ASSETS_EDITOR_TYPE_PARTICLES;
		else if(Tab == ASSETS_TAB_HUD)
			EditorType = ASSETS_EDITOR_TYPE_HUD;
		else if(Tab == ASSETS_TAB_EXTRAS)
			EditorType = ASSETS_EDITOR_TYPE_EXTRAS;

		m_AssetsEditorState.m_Type = EditorType;
		AssetsEditorReloadAssets(EditorType);
		const auto &vAssets = m_AssetsEditorState.m_avAssets[EditorType];
		int Index = 0;
		for(size_t i = 0; i < vAssets.size(); ++i)
		{
			if(str_comp(vAssets[i].m_aName, pName) == 0)
			{
				Index = (int)i;
				break;
			}
		}
		m_AssetsEditorState.m_aMainAssetIndex[EditorType] = Index;
		m_AssetsEditorState.m_aDonorAssetIndex[EditorType] = Index;
		m_AssetsEditorState.m_VisualsEditorInitialized = false;
		m_AssetsEditorState.m_VisualsEditorOpen = true;
		m_AssetsEditorState.m_FullscreenOpen = true;
	}
}

static bool CopyRawFileToStorage(IStorage *pStorage, const char *pSource, const char *pTargetStoragePath)
{
	IOHANDLE Source = io_open(pSource, IOFLAG_READ);
	if(!Source)
		return false;
	char aComplete[IO_MAX_PATH_LENGTH];
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, pTargetStoragePath, aComplete, sizeof(aComplete));
	fs_makedir_rec_for(aComplete);
	IOHANDLE Target = io_open(aComplete, IOFLAG_WRITE);
	if(!Target)
	{
		io_close(Source);
		return false;
	}
	char aBuffer[8192];
	while(true)
	{
		const int Read = io_read(Source, aBuffer, sizeof(aBuffer));
		if(Read <= 0)
			break;
		io_write(Target, aBuffer, Read);
	}
	io_close(Source);
	io_close(Target);
	return true;
}

static unsigned ZipReadLE16(const unsigned char *pData)
{
	return (unsigned)pData[0] | ((unsigned)pData[1] << 8);
}

static unsigned ZipReadLE32(const unsigned char *pData)
{
	return (unsigned)pData[0] | ((unsigned)pData[1] << 8) | ((unsigned)pData[2] << 16) | ((unsigned)pData[3] << 24);
}

static bool ZipEntryNameSafe(const char *pName)
{
	if(pName[0] == '\0' || pName[0] == '/' || pName[0] == '\\')
		return false;
	if(str_find(pName, "..") || str_find(pName, ":"))
		return false;
	return true;
}

static bool ExtractZipToStorage(IStorage *pStorage, const char *pZipPath, const char *pTargetDir)
{
	IOHANDLE File = io_open(pZipPath, IOFLAG_READ);
	if(!File)
		return false;
	const int64_t FileSize = io_length(File);
	if(FileSize < 22)
	{
		io_close(File);
		return false;
	}

	const int64_t TailSize = minimum<int64_t>(FileSize, 66000);
	std::vector<unsigned char> vTail((size_t)TailSize);
	io_seek(File, FileSize - TailSize, IOSEEK_START);
	io_read(File, vTail.data(), (unsigned)vTail.size());

	int64_t EocdPos = -1;
	for(int64_t i = TailSize - 22; i >= 0; --i)
	{
		if(ZipReadLE32(&vTail[(size_t)i]) == 0x06054b50)
		{
			EocdPos = i;
			break;
		}
	}
	if(EocdPos < 0)
	{
		io_close(File);
		return false;
	}

	const unsigned NumEntries = ZipReadLE16(&vTail[(size_t)EocdPos + 10]);
	const unsigned CdOffset = ZipReadLE32(&vTail[(size_t)EocdPos + 16]);

	int64_t Position = CdOffset;
	int Extracted = 0;
	for(unsigned EntryIndex = 0; EntryIndex < NumEntries; ++EntryIndex)
	{
		unsigned char aHeader[46];
		io_seek(File, Position, IOSEEK_START);
		if(io_read(File, aHeader, sizeof(aHeader)) != (int)sizeof(aHeader))
			break;
		if(ZipReadLE32(aHeader) != 0x02014b50)
			break;

		const unsigned Method = ZipReadLE16(&aHeader[10]);
		const unsigned CompSize = ZipReadLE32(&aHeader[20]);
		const unsigned UncompSize = ZipReadLE32(&aHeader[24]);
		const unsigned NameLength = ZipReadLE16(&aHeader[28]);
		const unsigned ExtraLength = ZipReadLE16(&aHeader[30]);
		const unsigned CommentLength = ZipReadLE16(&aHeader[32]);
		const unsigned LocalOffset = ZipReadLE32(&aHeader[42]);

		if(NameLength == 0 || NameLength >= IO_MAX_PATH_LENGTH)
			break;
		char aName[IO_MAX_PATH_LENGTH];
		if(io_read(File, aName, NameLength) != (int)NameLength)
			break;
		aName[NameLength] = '\0';
		Position += 46 + NameLength + ExtraLength + CommentLength;

		if(aName[NameLength - 1] == '/')
			continue;
		if(!ZipEntryNameSafe(aName))
			continue;
		if(UncompSize > 64u * 1024u * 1024u || CompSize > 64u * 1024u * 1024u)
			continue;
		if(Method != 0 && Method != 8)
			continue;

		unsigned char aLocal[30];
		io_seek(File, LocalOffset, IOSEEK_START);
		if(io_read(File, aLocal, sizeof(aLocal)) != (int)sizeof(aLocal))
			continue;
		if(ZipReadLE32(aLocal) != 0x04034b50)
			continue;
		const unsigned LocalNameLength = ZipReadLE16(&aLocal[26]);
		const unsigned LocalExtraLength = ZipReadLE16(&aLocal[28]);
		const int64_t DataOffset = (int64_t)LocalOffset + 30 + LocalNameLength + LocalExtraLength;

		std::vector<unsigned char> vComp(CompSize);
		io_seek(File, DataOffset, IOSEEK_START);
		if(CompSize > 0 && io_read(File, vComp.data(), CompSize) != (int)CompSize)
			continue;

		std::vector<unsigned char> vData;
		if(Method == 0)
			vData = vComp;
		else
		{
			vData.resize(UncompSize);
			z_stream Stream{};
			Stream.next_in = vComp.data();
			Stream.avail_in = (uInt)vComp.size();
			Stream.next_out = vData.data();
			Stream.avail_out = (uInt)vData.size();
			if(inflateInit2(&Stream, -MAX_WBITS) != Z_OK)
				continue;
			const int Result = inflate(&Stream, Z_FINISH);
			inflateEnd(&Stream);
			if(Result != Z_STREAM_END)
				continue;
		}

		char aTarget[IO_MAX_PATH_LENGTH];
		str_format(aTarget, sizeof(aTarget), "%s/%s", pTargetDir, aName);
		char aComplete[IO_MAX_PATH_LENGTH];
		pStorage->GetCompletePath(IStorage::TYPE_SAVE, aTarget, aComplete, sizeof(aComplete));
		fs_makedir_rec_for(aComplete);
		IOHANDLE Out = io_open(aComplete, IOFLAG_WRITE);
		if(!Out)
			continue;
		if(!vData.empty())
			io_write(Out, vData.data(), (unsigned)vData.size());
		io_close(Out);
		++Extracted;
	}

	io_close(File);
	return Extracted > 0;
}

void CMenus::AssetsHandleDrop(const char *pPath)
{
	if(fs_is_dir(pPath))
		return;
	const char *pType = AssetsTabTypeName(s_CurCustomTab);

	char aStem[64];
	{
		char aName[IO_MAX_PATH_LENGTH];
		fs_split_file_extension(fs_filename(pPath), aName, sizeof(aName));
		str_copy(aStem, aName);
	}
	if(aStem[0] == '\0')
		return;

	if(str_endswith_nocase(pPath, ".zip"))
	{
		char aFolder[IO_MAX_PATH_LENGTH];
		str_format(aFolder, sizeof(aFolder), "assets/%s/%s", pType, aStem);
		if(ExtractZipToStorage(Storage(), pPath, aFolder))
			ClearCustomItems(s_CurCustomTab);
		return;
	}

	if(s_CurCustomTab == ASSETS_TAB_AUDIO)
	{
		if(str_endswith_nocase(pPath, ".wv") || str_endswith_nocase(pPath, ".opus") || str_endswith_nocase(pPath, ".wav"))
		{
			char aTarget[IO_MAX_PATH_LENGTH];
			str_format(aTarget, sizeof(aTarget), "assets/%s/%s/%s", pType, aStem, fs_filename(pPath));
			if(CopyRawFileToStorage(Storage(), pPath, aTarget))
				ClearCustomItems(s_CurCustomTab);
		}
		return;
	}

	if(str_endswith_nocase(pPath, ".png"))
	{
		char aInner[64];
		if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
			str_copy(aInner, "ddnet.png");
		else
			str_format(aInner, sizeof(aInner), "%s.png", pType);
		char aTarget[IO_MAX_PATH_LENGTH];
		str_format(aTarget, sizeof(aTarget), "assets/%s/%s/%s", pType, aStem, aInner);
		if(CopyRawFileToStorage(Storage(), pPath, aTarget))
			ClearCustomItems(s_CurCustomTab);
	}
}
template<typename TName, typename TCaller>
static void InitAssetList(std::vector<TName> &vAssetList, const char *pAssetPath, const char *pAssetName, FS_LISTDIR_CALLBACK pfnCallback, IGraphics *pGraphics, IStorage *pStorage, TCaller Caller)
{
	if(vAssetList.empty())
	{
		TName AssetItem;
		str_copy(AssetItem.m_aName, "default");
		LoadAsset(&AssetItem, pAssetName, pGraphics);
		vAssetList.push_back(AssetItem);

		// load assets
		pStorage->ListDirectory(IStorage::TYPE_ALL, pAssetPath, pfnCallback, Caller);
		std::sort(vAssetList.begin(), vAssetList.end());
	}
	if(vAssetList.size() != gs_aCustomListSize[s_CurCustomTab])
		gs_aInitCustomList[s_CurCustomTab] = true;
}

template<typename TName>
static int InitSearchList(std::vector<const TName *> &vpSearchList, std::vector<TName> &vAssetList)
{
	vpSearchList.clear();
	int ListSize = vAssetList.size();
	for(int i = 0; i < ListSize; ++i)
	{
		const TName *pAsset = &vAssetList[i];

		// filter quick search
		if(!s_aFilterInputs[s_CurCustomTab].IsEmpty() && !str_utf8_find_nocase(pAsset->m_aName, s_aFilterInputs[s_CurCustomTab].GetString()))
			continue;

		vpSearchList.push_back(pAsset);
	}
	return vAssetList.size();
}

void CMenus::RenderSettingsCustom(CUIRect MainView)
{
	CUIRect TabBar, CustomList, QuickSearch, DirectoryButton, ReloadButton, AssetsEditorButton;

	char aDroppedAsset[IO_MAX_PATH_LENGTH];
	if(GameClient()->ConsumeDroppedFile(aDroppedAsset, sizeof(aDroppedAsset)))
		AssetsHandleDrop(aDroppedAsset);

	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / (float)NUMBER_OF_ASSETS_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_ASSETS_TABS] = {};
	const char *apTabNames[NUMBER_OF_ASSETS_TABS] = {
		Localize("Entities"),
		Localize("Game"),
		Localize("Emoticons"),
		Localize("Particles"),
		Localize("HUD"),
		Localize("Extras"),
		Localize("Audio")};

	for(int Tab = ASSETS_TAB_ENTITIES; Tab < NUMBER_OF_ASSETS_TABS; ++Tab)
	{
		CUIRect Button;
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == ASSETS_TAB_ENTITIES ? IGraphics::CORNER_L : (Tab == NUMBER_OF_ASSETS_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurCustomTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurCustomTab = Tab;
		}
	}

	CUIRect DropHintRect;
	MainView.HSplitTop(14.0f, &DropHintRect, &MainView);
	Ui()->DoLabel(&DropHintRect, Localize("Drag & drop .png files or .zip packs here to upload"), 10.0f, TEXTALIGN_ML);

	auto LoadStartTime = time_get_nanoseconds();
	SMenuAssetScanUser User;
	User.m_pUser = this;
	User.m_LoadedFunc = [&]() {
		if(time_get_nanoseconds() - LoadStartTime > 500ms)
			RenderLoading(Localize("Loading assets"), "", 0);
	};
	if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
	{
		if(m_vEntitiesList.empty())
		{
			SCustomEntities EntitiesItem;
			str_copy(EntitiesItem.m_aName, "default");
			LoadEntities(&EntitiesItem, &User);
			m_vEntitiesList.push_back(EntitiesItem);

			// load entities
			Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/entities", EntitiesScan, &User);
			std::sort(m_vEntitiesList.begin(), m_vEntitiesList.end());
		}
		if(m_vEntitiesList.size() != gs_aCustomListSize[s_CurCustomTab])
			gs_aInitCustomList[s_CurCustomTab] = true;
	}
	else if(s_CurCustomTab == ASSETS_TAB_GAME)
	{
		InitAssetList(m_vGameList, "assets/game", "game", GameScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
	{
		InitAssetList(m_vEmoticonList, "assets/emoticons", "emoticons", EmoticonsScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
	{
		InitAssetList(m_vParticlesList, "assets/particles", "particles", ParticlesScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_HUD)
	{
		InitAssetList(m_vHudList, "assets/hud", "hud", HudScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
	{
		InitAssetList(m_vExtrasList, "assets/extras", "extras", ExtrasScan, Graphics(), Storage(), &User);
	}
	else if(s_CurCustomTab == ASSETS_TAB_AUDIO)
	{
		if(m_vAudioList.empty())
		{
			SCustomAudio AudioItem;
			str_copy(AudioItem.m_aName, "default");
			m_vAudioList.push_back(AudioItem);
			Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/audio", AudioScan, &User);
			std::sort(m_vAudioList.begin(), m_vAudioList.end());
		}
		if(m_vAudioList.size() != gs_aCustomListSize[s_CurCustomTab])
			gs_aInitCustomList[s_CurCustomTab] = true;
	}
	else
	{
		dbg_assert_failed("Invalid s_CurCustomTab: %d", s_CurCustomTab);
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	// skin selector
	MainView.HSplitTop(MainView.h - 10.0f - ms_ButtonHeight, &CustomList, &MainView);
	if(gs_aInitCustomList[s_CurCustomTab])
	{
		int ListSize = 0;
		if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
		{
			gs_vpSearchEntitiesList.clear();
			ListSize = m_vEntitiesList.size();
			for(int i = 0; i < ListSize; ++i)
			{
				const SCustomEntities *pEntity = &m_vEntitiesList[i];

				// filter quick search
				if(!s_aFilterInputs[s_CurCustomTab].IsEmpty() && !str_utf8_find_nocase(pEntity->m_aName, s_aFilterInputs[s_CurCustomTab].GetString()))
					continue;

				gs_vpSearchEntitiesList.push_back(pEntity);
			}
		}
		else if(s_CurCustomTab == ASSETS_TAB_GAME)
		{
			ListSize = InitSearchList(gs_vpSearchGamesList, m_vGameList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
		{
			ListSize = InitSearchList(gs_vpSearchEmoticonsList, m_vEmoticonList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
		{
			ListSize = InitSearchList(gs_vpSearchParticlesList, m_vParticlesList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_HUD)
		{
			ListSize = InitSearchList(gs_vpSearchHudList, m_vHudList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
		{
			ListSize = InitSearchList(gs_vpSearchExtrasList, m_vExtrasList);
		}
		else if(s_CurCustomTab == ASSETS_TAB_AUDIO)
		{
			ListSize = InitSearchList(gs_vpSearchAudioList, m_vAudioList);
		}
		gs_aInitCustomList[s_CurCustomTab] = false;
		gs_aCustomListSize[s_CurCustomTab] = ListSize;
	}

	int OldSelected = -1;
	float Margin = 10;
	float TextureWidth = 150;
	float TextureHeight = 150;

	size_t SearchListSize = 0;

	if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
	{
		SearchListSize = gs_vpSearchEntitiesList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_GAME)
	{
		SearchListSize = gs_vpSearchGamesList.size();
		TextureHeight = 75;
	}
	else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
	{
		SearchListSize = gs_vpSearchEmoticonsList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
	{
		SearchListSize = gs_vpSearchParticlesList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_HUD)
	{
		SearchListSize = gs_vpSearchHudList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
	{
		SearchListSize = gs_vpSearchExtrasList.size();
	}
	else if(s_CurCustomTab == ASSETS_TAB_AUDIO)
	{
		SearchListSize = gs_vpSearchAudioList.size();
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(TextureHeight + 15.0f + 10.0f + Margin, SearchListSize, CustomList.w / (Margin + TextureWidth), 1, OldSelected, &CustomList, false);
	for(size_t i = 0; i < SearchListSize; ++i)
	{
		const SCustomItem *pItem = GetCustomItem(s_CurCustomTab, i);
		if(pItem == nullptr)
			continue;

		if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetsEntities) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_GAME)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetGame) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetEmoticons) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetParticles) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_HUD)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetHud) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetExtras) == 0)
				OldSelected = i;
		}
		else if(s_CurCustomTab == ASSETS_TAB_AUDIO)
		{
			if(str_comp(pItem->m_aName, g_Config.m_ClAssetAudio) == 0)
				OldSelected = i;
		}

		const CListboxItem Item = s_ListBox.DoNextItem(pItem, OldSelected >= 0 && (size_t)OldSelected == i);
		CUIRect ItemRect = Item.m_Rect;
		ItemRect.Margin(Margin / 2, &ItemRect);
		if(!Item.m_Visible)
			continue;

		if(!Ui()->IsPopupOpen() && Ui()->MouseHovered(&ItemRect) && Ui()->MouseButtonClicked(1))
		{
			const bool IsDefault = str_comp(pItem->m_aName, "default") == 0;
			gs_AssetsContextMenu.Reset();
			gs_AssetsContextMenu.m_pScrollRegion = &gs_AssetsContextMenuScroll;
			gs_AssetsContextMenu.m_Width = 190.0f;
			gs_AssetsContextMenu.m_EntryHeight = 18.0f;
			gs_AssetsContextMenu.m_FontSize = 12.0f;
			if(s_CurCustomTab == ASSETS_TAB_AUDIO)
			{
				if(IsDefault)
					gs_AssetsContextMenu.m_vEntries = {Localize("Show in folder")};
				else
					gs_AssetsContextMenu.m_vEntries = {Localize("Delete"), Localize("Rename"), Localize("Show in folder")};
			}
			else if(IsDefault)
				gs_AssetsContextMenu.m_vEntries = {Localize("Add to Frankenstein"), Localize("Show in folder")};
			else
				gs_AssetsContextMenu.m_vEntries = {Localize("Delete"), Localize("Rename"), Localize("Show in folder"), Localize("Add to Frankenstein")};
			Ui()->ShowPopupSelection(Ui()->MouseX(), Ui()->MouseY(), &gs_AssetsContextMenu);
			str_copy(gs_aAssetsContextName, pItem->m_aName);
			gs_AssetsContextTab = s_CurCustomTab;
		}
		CUIRect TextureRect;
		ItemRect.HSplitTop(15, &ItemRect, &TextureRect);
		TextureRect.HSplitTop(10, nullptr, &TextureRect);
		Ui()->DoLabel(&ItemRect, pItem->m_aName, ItemRect.h - 2, TEXTALIGN_MC);
		if(pItem->m_RenderTexture.IsValid())
		{
			Graphics()->WrapClamp();
			Graphics()->TextureSet(pItem->m_RenderTexture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1, 1, 1, 1);
			IGraphics::CQuadItem QuadItem(TextureRect.x + (TextureRect.w - TextureWidth) / 2, TextureRect.y + (TextureRect.h - TextureHeight) / 2, TextureWidth, TextureHeight);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
	}

	if(gs_AssetsDeleteConfirm.m_Result == CUi::SConfirmPopupContext::CONFIRMED && gs_aAssetsDeleteName[0] != '\0')
	{
		const char *pDelType = AssetsTabTypeName(gs_AssetsDeleteTab);
		char aDelFolder[IO_MAX_PATH_LENGTH];
		char aDelFile[IO_MAX_PATH_LENGTH];
		str_format(aDelFolder, sizeof(aDelFolder), "assets/%s/%s", pDelType, gs_aAssetsDeleteName);
		str_format(aDelFile, sizeof(aDelFile), "assets/%s/%s.png", pDelType, gs_aAssetsDeleteName);
		if(Storage()->FolderExists(aDelFolder, IStorage::TYPE_SAVE))
			Storage()->RemoveFolder(aDelFolder, IStorage::TYPE_SAVE);
		else
			Storage()->RemoveFile(aDelFile, IStorage::TYPE_SAVE);
		ClearCustomItems(gs_AssetsDeleteTab);
		gs_aAssetsDeleteName[0] = '\0';
	}
	else if(gs_AssetsDeleteConfirm.m_Result == CUi::SConfirmPopupContext::CANCELED && gs_aAssetsDeleteName[0] != '\0')
		gs_aAssetsDeleteName[0] = '\0';

	if(gs_AssetsContextMenu.m_SelectionIndex >= 0 && gs_aAssetsContextName[0] != '\0')
	{
		const int CtxSelection = gs_AssetsContextMenu.m_SelectionIndex;
		gs_AssetsContextMenu.m_SelectionIndex = -1;
		gs_AssetsContextMenu.m_pSelection = nullptr;
		AssetsContextAction(gs_AssetsContextTab, gs_aAssetsContextName, CtxSelection);
		gs_aAssetsContextName[0] = '\0';
	}
	else if(gs_aAssetsContextName[0] != '\0' && !Ui()->IsPopupOpen(&gs_AssetsContextMenu))
		gs_aAssetsContextName[0] = '\0';

	const int NewSelected = s_ListBox.DoEnd();	if(OldSelected != NewSelected)
	{
		if(GetCustomItem(s_CurCustomTab, NewSelected)->m_aName[0] != '\0')
		{
			if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
			{
				str_copy(g_Config.m_ClAssetsEntities, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
				GameClient()->m_MapImages.ChangeEntitiesPath(GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
			}
			else if(s_CurCustomTab == ASSETS_TAB_GAME)
			{
				str_copy(g_Config.m_ClAssetGame, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
			}
			else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
			{
				str_copy(g_Config.m_ClAssetEmoticons, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadEmoticonsSkin(g_Config.m_ClAssetEmoticons);
			}
			else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
			{
				str_copy(g_Config.m_ClAssetParticles, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadParticlesSkin(g_Config.m_ClAssetParticles);
			}
			else if(s_CurCustomTab == ASSETS_TAB_HUD)
			{
				str_copy(g_Config.m_ClAssetHud, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadHudSkin(g_Config.m_ClAssetHud);
			}
			else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
			{
				str_copy(g_Config.m_ClAssetExtras, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadExtrasSkin(g_Config.m_ClAssetExtras);
			}
			else if(s_CurCustomTab == ASSETS_TAB_AUDIO)
			{
				str_copy(g_Config.m_ClAssetAudio, GetCustomItem(s_CurCustomTab, NewSelected)->m_aName);
				GameClient()->LoadAudioAssets(g_Config.m_ClAssetAudio);
			}
		}
	}

	// Quick search
	MainView.HSplitBottom(ms_ButtonHeight, &MainView, &QuickSearch);
	QuickSearch.VSplitLeft(220.0f, &QuickSearch, &DirectoryButton);
	QuickSearch.HSplitTop(5.0f, nullptr, &QuickSearch);
	if(Ui()->DoEditBox_Search(&s_aFilterInputs[s_CurCustomTab], &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive()))
	{
		gs_aInitCustomList[s_CurCustomTab] = true;
	}

	DirectoryButton.HSplitTop(5.0f, nullptr, &DirectoryButton);
	DirectoryButton.VSplitRight(325.0f, nullptr, &DirectoryButton);
	DirectoryButton.VSplitRight(25.0f, &DirectoryButton, &ReloadButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, nullptr);
	DirectoryButton.VSplitRight(140.0f, &DirectoryButton, &AssetsEditorButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, nullptr);
	static CButtonContainer s_AssetsDirId;
	if(DoButton_Menu(&s_AssetsDirId, Localize("Assets directory"), 0, &DirectoryButton))
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		char aBufFull[IO_MAX_PATH_LENGTH + 7];
		if(s_CurCustomTab == ASSETS_TAB_ENTITIES)
			str_copy(aBufFull, "assets/entities");
		else if(s_CurCustomTab == ASSETS_TAB_GAME)
			str_copy(aBufFull, "assets/game");
		else if(s_CurCustomTab == ASSETS_TAB_EMOTICONS)
			str_copy(aBufFull, "assets/emoticons");
		else if(s_CurCustomTab == ASSETS_TAB_PARTICLES)
			str_copy(aBufFull, "assets/particles");
		else if(s_CurCustomTab == ASSETS_TAB_HUD)
			str_copy(aBufFull, "assets/hud");
		else if(s_CurCustomTab == ASSETS_TAB_EXTRAS)
			str_copy(aBufFull, "assets/extras");
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, aBufFull, aBuf, sizeof(aBuf));
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder(aBufFull, IStorage::TYPE_SAVE);
		Client()->ViewFile(aBuf);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_AssetsDirId, &DirectoryButton, Localize("Open the directory to add custom assets"));

	static CButtonContainer s_AssetsEditorBtnId;
	if(DoButton_Menu(&s_AssetsEditorBtnId, Localize("Assets editor"), 0, &AssetsEditorButton))
	{
		m_AssetsEditorState.m_VisualsEditorOpen = true;
		m_AssetsEditorState.m_FullscreenOpen = true;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_AssetsEditorBtnId, &AssetsEditorButton, Localize("Create custom assets from existing packs"));

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	static CButtonContainer s_AssetsReloadBtnId;
	if(DoButton_Menu(&s_AssetsReloadBtnId, FontIcon::ARROW_ROTATE_RIGHT, 0, &ReloadButton) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		ClearCustomItems(s_CurCustomTab);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CMenus::ConchainAssetsEntities(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetsEntities) != 0)
		{
			pThis->GameClient()->m_MapImages.ChangeEntitiesPath(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetGame(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetGame) != 0)
		{
			pThis->GameClient()->LoadGameSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetParticles(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetParticles) != 0)
		{
			pThis->GameClient()->LoadParticlesSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetEmoticons(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetEmoticons) != 0)
		{
			pThis->GameClient()->LoadEmoticonsSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetHud(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetHud) != 0)
		{
			pThis->GameClient()->LoadHudSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}

void CMenus::ConchainAssetExtras(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CMenus *pThis = (CMenus *)pUserData;
	if(pResult->NumArguments() == 1)
	{
		const char *pArg = pResult->GetString(0);
		if(str_comp(pArg, g_Config.m_ClAssetExtras) != 0)
		{
			pThis->GameClient()->LoadExtrasSkin(pArg);
		}
	}

	pfnCallback(pResult, pCallbackUserData);
}
