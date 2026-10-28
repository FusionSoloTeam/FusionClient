#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_PRACTICE_TAGS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_PRACTICE_TAGS_H

#include <base/vmath.h>

#include <game/client/component.h>

#include <deque>

class CPracticeTags : public CComponent
{
	std::deque<vec2> m_Tags;
	bool m_Dirty = false;

	void AddTag(vec2 Pos);
	void MakeSpaceFor(int Count);
	bool Save();
	bool Load();

public:
	enum EShape
	{
		SHAPE_CIRCLE = 0,
		SHAPE_SQUARE,
		SHAPE_TRIANGLE,
		SHAPE_CROSS,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnShutdown() override;

	void OnChatCommand(const char *pLine);
};

#endif
