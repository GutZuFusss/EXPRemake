/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PICKUP_H
#define GAME_SERVER_ENTITIES_PICKUP_H

#include <game/server/entity.h>

const int PickupPhysSize = 14;

class CPickup : public CEntity {

public:
	CPickup(CGameWorld *pGameWorld, int Type, int SubType = 0);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	
	const char *GetWeaponName(int wid);
	int RealPickup(int Type);
	int RealSubtype(int Type);
	void CreateRandomFromTurret(int TurretType, vec2 Pos);
	void MakeBossShield();

protected:
	int m_Type;
	int m_Subtype;
	int m_SpawnTick;
	float m_AnimationTimer;

	void TickAnims();
	void TickPotionAnim();
	void TickPickup();
	virtual void HandleRespawn(int RespawnTime);
};

#endif
