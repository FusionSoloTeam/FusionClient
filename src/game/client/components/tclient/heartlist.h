#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_HEARTLIST_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_HEARTLIST_H

#include <base/str.h>

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

enum
{
	MAX_HEART_TYPE_LENGTH = 16,
};

class CHeartType
{
public:
	char m_aName[MAX_HEART_TYPE_LENGTH] = "";
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	bool m_Removable = true;
	bool m_Prioritized = true;
	int m_Index = 0;

	CHeartType(const char *pName, ColorRGBA Color = ColorRGBA(1, 1, 1, 1), bool Removable = true)
	{
		str_copy(m_aName, pName);
		m_Color = Color;
		m_Removable = Removable;
	}

	bool operator==(const CHeartType &Other) const
	{
		return str_comp(m_aName, Other.m_aName) == 0;
	}
};

class CHeartEntry
{
public:
	// A heart entry is a single friend name bound to a heart type
	char m_aName[MAX_NAME_LENGTH] = "";
	CHeartType *m_pHeartType = nullptr;

	CHeartEntry(CHeartType *pHeartType)
	{
		m_pHeartType = pHeartType;
	}

	CHeartEntry(CHeartType *pHeartType, const char *pName)
	{
		m_pHeartType = pHeartType;
		str_copy(m_aName, pName);
	}

	bool operator==(const CHeartEntry &Other) const
	{
		return str_comp(m_aName, Other.m_aName) == 0;
	}
};

class CHeartDataCache
{
public:
	bool m_IsFriend = false;
	ColorRGBA m_Color = ColorRGBA(1, 1, 1, 1);
};

class CHeartList : public CComponent
{
	// Commands
	static void ConFriend(IConsole::IResult *pResult, void *pUserData);
	static void ConUnfriend(IConsole::IResult *pResult, void *pUserData);

	// Backend commands for config file
	static void ConAddHeartEntry(IConsole::IResult *pResult, void *pUserData);
	static void ConUpsertHeartType(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	CHeartList();
	~CHeartList() override;

	// Heart types (the custom hearts), "default" is index 0 and non-removable
	std::vector<CHeartType *> m_HeartTypes = {
		new CHeartType("default", ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false),
		new CHeartType("heart", ColorRGBA(1.0f, 0.2f, 0.2f, 1.0f)),
		new CHeartType("gold", ColorRGBA(1.0f, 0.84f, 0.0f, 1.0f)),
		new CHeartType("star", ColorRGBA(0.0f, 0.9f, 1.0f, 1.0f)),
		new CHeartType("blue", ColorRGBA(0.3f, 0.5f, 1.0f, 1.0f)),
		new CHeartType("pink", ColorRGBA(1.0f, 0.4f, 0.7f, 1.0f)),
		new CHeartType("green", ColorRGBA(0.3f, 1.0f, 0.4f, 1.0f)),
	};
	CHeartType *m_pHeartTypeDefault = m_HeartTypes[0];

	// Friend entries (a name bound to a heart)
	std::vector<CHeartEntry> m_vHeartEntries;
	CHeartDataCache m_aFriendData[MAX_CLIENTS];

	// Set when the chat should be rebuilt to pick up heart color changes
	bool m_NeedsChatRebuild = false;

	int Sizeof() const override { return sizeof(*this); }
	void OnNewSnapshot() override;
	void OnConsoleInit() override;

	// Adds to DDNet friends AND binds the given heart
	void AddFriend(const char *pName, const char *pHeart);
	void RemoveFriend(const char *pName);
	void RemoveFriendBinding(const char *pName);

	// Returns true if the message was a friendlist chat command (.friend/.unfriend)
	bool OnChatInput(const char *pMessage);

	void UpsertHeartType(int Index, const char *pName, ColorRGBA Color, bool Prioritized = true);
	void AddHeartType(const char *pName, ColorRGBA Color, bool Prioritized = true);
	void RemoveHeartType(const char *pName);
	CHeartType *FindHeartType(const char *pName);
	void MoveHeartType(int From, int To);

	void UpdateFriendPlayers();

	bool IsFriend(int ClientId) const { return m_aFriendData[ClientId].m_IsFriend; }
	ColorRGBA GetFriendColor(int ClientId) const { return m_aFriendData[ClientId].m_Color; }
};

#endif
