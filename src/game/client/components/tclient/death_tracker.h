#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_DEATH_TRACKER_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_DEATH_TRACKER_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <vector>

class CDeathTracker : public CComponent
{
	struct SDeathMarker
	{
		vec2 m_Pos;
		int m_Count;
		int64_t m_Time;
	};

	std::vector<SDeathMarker> m_vMarkers;
	int m_aDeathCount[MAX_CLIENTS];

	void Clear();
	void AddMarker(int ClientId, vec2 Pos);

public:
	CDeathTracker();
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnMapLoad() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnRender() override;
};

#endif
