#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_NOTIFY_ON_MOVE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_NOTIFY_ON_MOVE_H
#include <game/client/component.h>
#include <base/vmath.h>

class CNotifyOnMove : public CComponent
{
	bool m_SpecNotifyMoved = false;
	float m_SpecNotifyAnim = 0.0f;
	bool m_SpecHasLastPos = false;
	vec2 m_SpecLastPos{};
	int m_SpecLastTargetId = -1000;
	bool m_SpecNotifyMovedRemove = false;

public:
	CNotifyOnMove();
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;
	bool OnInput(const IInput::CEvent &Event) override;
};

#endif
