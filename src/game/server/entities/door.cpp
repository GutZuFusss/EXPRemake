#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/gamemodes/exp/exp.h>

#include "character.h"
#include "door.h"

CLaserDoor::CLaserDoor(CGameWorld *pGameWorld, vec2 Pos, int Type, CDoor *r)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos = Pos;
	m_Ref = r;
	
	m_Dir = vec2(0, 0);
	if(Type == DOOR_TYPE_VERTICAL)
		m_Dir = vec2(0, 1);
	else if(Type == DOOR_TYPE_HORIZONTAL)
		m_Dir = vec2(1, 0);
	
	Create();
	
	GameServer()->m_World.InsertEntity(this);
}

void CLaserDoor::Create()
{
	vec2 To = m_Pos + m_Dir*10000.0f;
	vec2 OrgTo = To;
	
	if(GameServer()->Collision()->IntersectLine(m_Pos, To, NULL, &To, false))
	{
		//intersected
		m_From = m_Pos;
		m_Pos = To;
		
		vec2 TempPos = m_Pos;
		vec2 TempDir = m_Dir*4.0f;
		
		GameServer()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
		m_Pos = TempPos;
		m_Dir = normalize(TempDir);
	}
	else
	{
		m_From = m_Pos;
		m_Pos = To;
	}
}

void CLaserDoor::Destroy()
{
	m_Ref->m_CreateLaser = true;
	m_Ref->m_Laser = NULL;
	CEntity::Destroy();
}

void CLaserDoor::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CLaserDoor::Tick()
{
	// nothing (:
}

void CLaserDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	
	if(GameServer()->m_apPlayers[SnappingClient]->GetCharacter() && GameServer()->m_apPlayers[SnappingClient]->GetCharacter()->DoorOpen())
		return;
	
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	pObj->m_StartTick = Server()->Tick()-1;
}
