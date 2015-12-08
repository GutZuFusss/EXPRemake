#ifndef GAME_SERVER_ENTITY_DOOR_H
#define GAME_SERVER_ENTITY_DOOR_H

#include <game/server/entity.h>

class CLaserDoor : public CEntity
{
	vec2 m_Dir;
	float m_Energy;
	class CDoor *m_Ref;
	
	void Create();
	
public:
	
	vec2 m_From;
	
	CLaserDoor(CGameWorld *pGameWorld, vec2 Pos, int Type, class CDoor *r);
	
	virtual void Destroy();
	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif
