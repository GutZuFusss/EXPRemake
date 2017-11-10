#ifndef GAME_SERVER_ENTITIES_DROP_H
#define GAME_SERVER_ENTITIES_DROP_H

#include <game/server/entity.h>
#include <game/server/entities/pickup.h>

class CDrop : public CPickup {

public:
	CDrop(CGameWorld *pGameWorld, vec2 Pos, int Type, int SubType = 0);
	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	
private:
	float m_Lifetime;

	bool ShouldDespawn();
	void Despawn();

protected:
	virtual void HandleRespawn(int RespawnTime);
};
#endif
