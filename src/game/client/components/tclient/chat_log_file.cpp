#include "chat_log_file.h"

#include <base/hash_ctxt.h>
#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <algorithm>
#include <cstring>

namespace
{

constexpr char CHAT_LOG_MAGIC[8] = {'F', 'C', 'C', 'H', 'A', 'T', 'L', '1'};
constexpr uint32_t CHAT_LOG_VERSION = 1;
constexpr int CHAT_LOG_HEADER_SIZE = 32;
constexpr int CHAT_LOG_DATE_SIZE = 12;
constexpr int CHAT_LOG_BODY_HEADER_SIZE = 18;
constexpr int CHAT_LOG_MAX_BODY_SIZE = 64 * 1024;
constexpr int CHAT_LOG_MAX_FILE_SIZE = 32 * 1024 * 1024;
constexpr int CHAT_LOG_MAX_NAME_LENGTH = 255;

void WriteLE(unsigned char *pBuf, uint64_t Value, int Size)
{
	for(int i = 0; i < Size; i++)
		pBuf[i] = (unsigned char)((Value >> (8 * i)) & 0xff);
}

uint64_t ReadLE(const unsigned char *pBuf, int Size)
{
	uint64_t Value = 0;
	for(int i = 0; i < Size; i++)
		Value |= (uint64_t)pBuf[i] << (8 * i);
	return Value;
}

void ChatLogWriteHeader(unsigned char *pBuf, const char *pDate)
{
	std::memset(pBuf, 0, CHAT_LOG_HEADER_SIZE);
	std::memcpy(pBuf, CHAT_LOG_MAGIC, sizeof(CHAT_LOG_MAGIC));
	WriteLE(pBuf + 8, CHAT_LOG_VERSION, 4);
	WriteLE(pBuf + 12, CHAT_LOG_HEADER_SIZE, 4);
	str_copy((char *)pBuf + 16, pDate, CHAT_LOG_DATE_SIZE);
}

SHA256_DIGEST ChatLogGenesis(const char *pDate)
{
	unsigned char aBuf[CHAT_LOG_HEADER_SIZE];
	ChatLogWriteHeader(aBuf, pDate);
	return sha256(aBuf, sizeof(aBuf));
}

SHA256_DIGEST ChatLogChain(const SHA256_DIGEST &Previous, const unsigned char *pBody, int BodySize)
{
	SHA256_CTX Context;
	sha256_init(&Context);
	sha256_update(&Context, Previous.data, SHA256_DIGEST_LENGTH);
	sha256_update(&Context, pBody, BodySize);
	return sha256_finish(&Context);
}

bool ChatLogReadAll(IStorage *pStorage, const char *pPath, std::vector<unsigned char> &vData, bool *pTruncated)
{
	IOHANDLE File = pStorage->OpenFile(pPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;

	vData.clear();
	*pTruncated = false;
	unsigned char aBuf[4096];
	while(true)
	{
		if((int)vData.size() >= CHAT_LOG_MAX_FILE_SIZE)
		{
			*pTruncated = true;
			break;
		}
		const unsigned NumRead = io_read(File, aBuf, sizeof(aBuf));
		if(NumRead == 0)
			break;
		vData.insert(vData.end(), aBuf, aBuf + NumRead);
	}
	io_close(File);
	return true;
}

EChatLogIntegrity ChatLogParse(const unsigned char *pData, size_t Size, const char *pExpectedDate, std::vector<SChatLogMessage> &vMessages, int MaxMessages, int *pTamperedIndex, SHA256_DIGEST *pHead, int *pNumRecords, bool Truncated)
{
	*pTamperedIndex = -1;
	*pHead = {};
	*pNumRecords = 0;

	if(Size < (size_t)CHAT_LOG_HEADER_SIZE ||
		std::memcmp(pData, CHAT_LOG_MAGIC, sizeof(CHAT_LOG_MAGIC)) != 0 ||
		ReadLE(pData + 8, 4) != CHAT_LOG_VERSION ||
		ReadLE(pData + 12, 4) != CHAT_LOG_HEADER_SIZE)
	{
		return CHAT_LOG_INTEGRITY_UNREADABLE;
	}

	char aDate[CHAT_LOG_DATE_SIZE];
	std::memcpy(aDate, pData + 16, CHAT_LOG_DATE_SIZE - 1);
	aDate[CHAT_LOG_DATE_SIZE - 1] = '\0';
	if(aDate[0] == '\0')
		return CHAT_LOG_INTEGRITY_UNREADABLE;
	if(pExpectedDate != nullptr && str_comp(aDate, pExpectedDate) != 0)
		return CHAT_LOG_INTEGRITY_UNREADABLE;

	SHA256_DIGEST Head = ChatLogGenesis(aDate);
	bool Tampered = false;
	const size_t MinRecordSize = 4 + CHAT_LOG_BODY_HEADER_SIZE + SHA256_DIGEST_LENGTH;
	size_t Offset = CHAT_LOG_HEADER_SIZE;

	while(Offset < Size)
	{
		if(Size - Offset < MinRecordSize)
		{
			Tampered = Tampered || !Truncated;
			break;
		}

		const uint32_t BodySize = (uint32_t)ReadLE(pData + Offset, 4);
		if(BodySize < CHAT_LOG_BODY_HEADER_SIZE || BodySize > CHAT_LOG_MAX_BODY_SIZE ||
			Size - Offset - 4 < (size_t)BodySize + SHA256_DIGEST_LENGTH)
		{
			Tampered = Tampered || !Truncated;
			break;
		}

		const unsigned char *pBody = pData + Offset + 4;
		SHA256_DIGEST Stored;
		std::memcpy(Stored.data, pBody + BodySize, SHA256_DIGEST_LENGTH);

		if(ChatLogChain(Head, pBody, (int)BodySize) != Stored)
			Tampered = true;

		Head = Stored;

		const int NameLength = (int)ReadLE(pBody + 14, 2);
		const int TextLength = (int)ReadLE(pBody + 16, 2);
		const bool BodyValid = (int)BodySize == CHAT_LOG_BODY_HEADER_SIZE + NameLength + TextLength;
		if(!BodyValid)
			Tampered = true;

		if(BodyValid && *pNumRecords < MaxMessages)
		{
			SChatLogMessage Message;
			Message.m_Timestamp = (int64_t)ReadLE(pBody, 8);
			Message.m_ClientId = (int)(int32_t)(uint32_t)ReadLE(pBody + 8, 4);
			Message.m_Team = pBody[12];
			Message.m_Flags = pBody[13];
			Message.m_Name.assign((const char *)pBody + CHAT_LOG_BODY_HEADER_SIZE, NameLength);
			Message.m_Text.assign((const char *)pBody + CHAT_LOG_BODY_HEADER_SIZE + NameLength, TextLength);
			vMessages.push_back(std::move(Message));
		}

		if(Tampered && *pTamperedIndex < 0)
			*pTamperedIndex = *pNumRecords;

		(*pNumRecords)++;
		Offset += 4 + BodySize + SHA256_DIGEST_LENGTH;
	}

	if(Tampered && *pTamperedIndex < 0)
		*pTamperedIndex = *pNumRecords;

	*pHead = Head;
	return Tampered ? CHAT_LOG_INTEGRITY_TAMPERED : CHAT_LOG_INTEGRITY_VERIFIED;
}

bool ChatLogRestore(IStorage *pStorage, const char *pPath, const char *pDate, SChatLogChain *pChain)
{
	pChain->m_aDate[0] = '\0';
	pChain->m_NumRecords = 0;
	pChain->m_FileLength = 0;
	pChain->m_Head = ChatLogGenesis(pDate);

	std::vector<unsigned char> vData;
	bool Truncated = false;
	if(!ChatLogReadAll(pStorage, pPath, vData, &Truncated))
	{
		str_copy(pChain->m_aDate, pDate, sizeof(pChain->m_aDate));
		return true;
	}

	std::vector<SChatLogMessage> vMessages;
	int TamperedIndex = -1;
	SHA256_DIGEST Head;
	if(ChatLogParse(vData.data(), vData.size(), pDate, vMessages, 0, &TamperedIndex, &Head, &pChain->m_NumRecords, Truncated) == CHAT_LOG_INTEGRITY_UNREADABLE)
		return false;

	pChain->m_Head = Head;
	pChain->m_FileLength = (int64_t)vData.size();
	str_copy(pChain->m_aDate, pDate, sizeof(pChain->m_aDate));
	return true;
}

}

void ChatLogSealedPath(const char *pDirectory, const char *pDate, char *pBuf, int BufSize)
{
	str_format(pBuf, BufSize, "%s/%s%s", pDirectory, pDate, CHAT_LOG_SEALED_EXTENSION);
}

bool ChatLogSealedAppend(IStorage *pStorage, const char *pDirectory, SChatLogChain *pChain, int64_t Timestamp, int ClientId, int Team, int Flags, const char *pName, const char *pText)
{
	char aDate[16];
	str_timestamp_ex((time_t)Timestamp, aDate, sizeof(aDate), "%Y-%m-%d");

	const int NameLength = std::min<int>(str_length(pName), CHAT_LOG_MAX_NAME_LENGTH);
	const int TextLength = std::min<int>(str_length(pText), 0xffff);
	const int BodySize = CHAT_LOG_BODY_HEADER_SIZE + NameLength + TextLength;
	if(BodySize > CHAT_LOG_MAX_BODY_SIZE)
		return false;

	unsigned char aBody[CHAT_LOG_MAX_BODY_SIZE];
	WriteLE(aBody, (uint64_t)Timestamp, 8);
	WriteLE(aBody + 8, (uint32_t)(int32_t)ClientId, 4);
	aBody[12] = (unsigned char)(Team & 0xff);
	aBody[13] = (unsigned char)(Flags & 0xff);
	WriteLE(aBody + 14, (uint32_t)NameLength, 2);
	WriteLE(aBody + 16, (uint32_t)TextLength, 2);
	std::memcpy(aBody + CHAT_LOG_BODY_HEADER_SIZE, pName, NameLength);
	std::memcpy(aBody + CHAT_LOG_BODY_HEADER_SIZE + NameLength, pText, TextLength);

	char aPath[IO_MAX_PATH_LENGTH];
	ChatLogSealedPath(pDirectory, aDate, aPath, sizeof(aPath));

	if(!pStorage->FileExists(aPath, IStorage::TYPE_SAVE))
		pStorage->CreateFolder(pDirectory, IStorage::TYPE_SAVE);

	if(str_comp(pChain->m_aDate, aDate) != 0 && !ChatLogRestore(pStorage, aPath, aDate, pChain))
		return false;

	IOHANDLE File = pStorage->OpenFile(aPath, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return false;

	if(io_length(File) != pChain->m_FileLength)
	{
		io_close(File);
		if(!ChatLogRestore(pStorage, aPath, aDate, pChain))
			return false;
		File = pStorage->OpenFile(aPath, IOFLAG_APPEND, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		if(io_length(File) != pChain->m_FileLength)
		{
			io_close(File);
			return false;
		}
	}

	if(pChain->m_FileLength == 0)
	{
		unsigned char aHeader[CHAT_LOG_HEADER_SIZE];
		ChatLogWriteHeader(aHeader, aDate);
		if(io_write(File, aHeader, sizeof(aHeader)) != sizeof(aHeader))
		{
			io_close(File);
			return false;
		}
		pChain->m_FileLength += CHAT_LOG_HEADER_SIZE;
	}

	const SHA256_DIGEST Chain = ChatLogChain(pChain->m_Head, aBody, BodySize);

	unsigned char aRecordHeader[4];
	WriteLE(aRecordHeader, (uint32_t)BodySize, sizeof(aRecordHeader));

	if(io_write(File, aRecordHeader, sizeof(aRecordHeader)) != sizeof(aRecordHeader) ||
		io_write(File, aBody, BodySize) != (unsigned)BodySize ||
		io_write(File, Chain.data, SHA256_DIGEST_LENGTH) != SHA256_DIGEST_LENGTH)
	{
		io_close(File);
		return false;
	}
	io_close(File);

	pChain->m_Head = Chain;
	pChain->m_NumRecords++;
	pChain->m_FileLength += 4 + BodySize + SHA256_DIGEST_LENGTH;
	return true;
}

EChatLogIntegrity ChatLogSealedRead(IStorage *pStorage, const char *pPath, const char *pExpectedDate, std::vector<SChatLogMessage> &vMessages, int MaxMessages, int *pTamperedIndex, SHA256_DIGEST *pHead)
{
	std::vector<unsigned char> vData;
	bool Truncated = false;
	if(!ChatLogReadAll(pStorage, pPath, vData, &Truncated))
	{
		*pTamperedIndex = -1;
		*pHead = {};
		return CHAT_LOG_INTEGRITY_UNREADABLE;
	}

	int NumRecords = 0;
	return ChatLogParse(vData.data(), vData.size(), pExpectedDate, vMessages, MaxMessages, pTamperedIndex, pHead, &NumRecords, Truncated);
}
