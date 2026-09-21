#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_REACT_BYPASS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_REACT_BYPASS_H

#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include <game/client/components/tclient/warlist.h>

#include <string>
#include <vector>

class CReactBypass : public CComponent
{
public:
	enum
	{
		KIND_MUTE = 0,
		KIND_FOE,
		KIND_WAR,
		KIND_HEART,
	};

	struct STempEntry
	{
		char m_aOldName[MAX_NAME_LENGTH] = "";
		char m_aNewName[MAX_NAME_LENGTH] = "";
		char m_aType[MAX_WARLIST_TYPE_LENGTH] = "";
		char m_aReason[MAX_WARLIST_REASON_LENGTH] = "";
		int m_Kind = KIND_MUTE;
	};

private:
	std::vector<STempEntry> m_vTemp;
	std::vector<std::string> m_vMuteNames;
	char m_aaPrevNames[MAX_CLIENTS][MAX_NAME_LENGTH];
	int m_aPrevIgnore[MAX_CLIENTS];

	bool IsMuteName(const char *pName) const;
	void AddMuteName(const char *pName);
	void AddTemp(const char *pOld, const char *pNew, int Kind, const char *pType, const char *pReason);
	void RemoveTempFor(const char *pName);
	void ReactToRename(int ClientId, const char *pOld, const char *pNew);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnNewSnapshot() override;
};

#endif
