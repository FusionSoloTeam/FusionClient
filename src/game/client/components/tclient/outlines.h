#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H

#include <game/client/component.h>

#include "system_audio.h"

class CTile;
class CTeleTile;

class COutlines : public CComponent
{
private:
	ivec2 m_MapDataSize;
	int *m_pMapData = nullptr;
	CSystemAudio m_SystemAudio;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnMapLoad() override;
	void OnRender() override;
	~COutlines() override { delete[] m_pMapData; }

	CSystemAudio &SystemAudio() { return m_SystemAudio; }
};

#endif
