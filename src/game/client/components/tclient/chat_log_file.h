#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_CHAT_LOG_FILE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_CHAT_LOG_FILE_H

#include <base/hash.h>
#include <base/types.h>

#include <engine/storage.h>

#include <cstdint>
#include <string>
#include <vector>

inline constexpr const char *CHAT_LOG_SEALED_EXTENSION = ".fclog";
inline constexpr int CHAT_LOG_SEALED_NAME_LENGTH = 16;
inline constexpr const char *CHAT_LOG_LEGACY_EXTENSION = ".txt";
inline constexpr int CHAT_LOG_LEGACY_NAME_LENGTH = 14;

inline constexpr int CHAT_LOG_FLAG_SERVER = 1 << 0;

enum EChatLogIntegrity
{
	CHAT_LOG_INTEGRITY_VERIFIED = 0,
	CHAT_LOG_INTEGRITY_TAMPERED,
	CHAT_LOG_INTEGRITY_LEGACY,
	CHAT_LOG_INTEGRITY_UNREADABLE,
};

struct SChatLogMessage
{
	int64_t m_Timestamp;
	int m_ClientId;
	int m_Team;
	int m_Flags;
	std::string m_Name;
	std::string m_Text;
};

struct SChatLogChain
{
	char m_aDate[16];
	SHA256_DIGEST m_Head;
	int m_NumRecords;
	int64_t m_FileLength;
};

void ChatLogSealedPath(const char *pDirectory, const char *pDate, char *pBuf, int BufSize);
bool ChatLogSealedAppend(IStorage *pStorage, const char *pDirectory, SChatLogChain *pChain, int64_t Timestamp, int ClientId, int Team, int Flags, const char *pName, const char *pText);
EChatLogIntegrity ChatLogSealedRead(IStorage *pStorage, const char *pPath, const char *pExpectedDate, std::vector<SChatLogMessage> &vMessages, int MaxMessages, int *pTamperedIndex, SHA256_DIGEST *pHead);

#endif // GAME_CLIENT_COMPONENTS_TCLIENT_CHAT_LOG_FILE_H
