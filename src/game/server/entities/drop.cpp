#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#include <game/server/gamemodes/exp/environment.h>
#include <game/server/gamemodes/exp/exp.h>

#include "drop.h"

CDrop::CDrop(CGameWorld *pGameWorld, vec2 Pos, int Type, int SubType) : CPickup(pGameWorld, Type, SubType) {	
	CPickup::CPickup(pGameWorld, Type, SubType);
	m_Pos = Pos;
	m_Lifetime = Server()->Tick();
	m_SpawnTick = -1;
}

void CDrop::Reset() {
	CPickup::Reset();
	GameWorld()->DestroyEntity(this);
}

void CDrop::Tick() {
	CPickup::Tick();
	if (ShouldDespawn()) {
		Despawn();
	}
}

bool CDrop::ShouldDespawn() {
	return Server()->Tick() > m_Lifetime + GameServer()->Tuning()->m_PickupLifetime * Server()->TickSpeed();
}

void CDrop::Despawn() {
	GameWorld()->DestroyEntity(this);
}

void CDrop::HandleRespawn(int RespawnTime) {
	GameServer()->m_World.DestroyEntity(this);
}

void CDrop::Snap(int SnappingClient) {
	CPickup::Snap(SnappingClient);
}