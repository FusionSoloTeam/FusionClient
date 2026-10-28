#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H

#include <game/client/component.h>

#include "system_audio.h"

#include <vector>

class CTile;
class CTeleTile;

class COutlines : public CComponent
{
private:
	ivec2 m_MapDataSize;
	int *m_pMapData = nullptr;

	std::vector<int *> m_apLodData;
	std::vector<ivec2> m_aLodSizes;
	CSystemAudio m_SystemAudio;

	void BuildLodPyramid();
	void DestroyLodPyramid();
	float GetTileAnimationProgress(int Type, int x, int y, int LodLevel) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnMapLoad() override;
	void OnRender() override;
	~COutlines() override;

	CSystemAudio &SystemAudio() { return m_SystemAudio; }
};

#endif
