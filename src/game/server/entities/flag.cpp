/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/server/gamemodes/exp/exp.h>

#include "character.h"
#include "flag.h"

CFlag::CFlag(CGameWorld *pGameWorld, int Team, vec2 StandPos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG, StandPos, ms_PhysSize)
{
	m_Team = Team;
	m_StandPos = StandPos;

	GameServer()->m_World.InsertEntity(this);

	Reset();
}

void CFlag::Reset()
{
	m_pCarrier = 0;
	m_AtStand = true;
	m_Pos = m_StandPos;
	m_Vel = vec2(0, 0);
	m_GrabTick = 0;
}

void CFlag::Grab(CCharacter *pChar)
{
	m_AtStand = false;
	m_pCarrier = pChar;
	if(m_AtStand)
		m_GrabTick = Server()->Tick();
}

void CFlag::Drop()
{
	m_pCarrier = 0;
	m_Vel = vec2(0, 0);
	m_DropTick = Server()->Tick();
}

void CFlag::Tick()
{
	if(m_pCarrier)
	{
		// update flag position
		m_Pos = m_pCarrier->GetPos();
	}
	else
	{
		// flag hits death-tile or left the game layer, reset it
		if((GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y) & CCollision::COLFLAG_DEATH)
			|| GameLayerClipped(m_Pos))
		{
			Reset();
			GameServer()->m_pController->OnFlagReturn(this);
		}

		if(!m_AtStand)
		{
			if(Server()->Tick() > m_DropTick + Server()->TickSpeed()*30)
			{
				Reset();
				GameServer()->m_pController->OnFlagReturn(this);
			}
			else
			{
				m_Vel.y += GameServer()->m_World.m_Core.m_Tuning.m_Gravity;
				GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(ms_PhysSize, ms_PhysSize), 0.5f);
			}
		}
	}
}

void CFlag::TickPaused()
{
	m_DropTick++;
	if(m_GrabTick)
		m_GrabTick++;
}

void CFlag::Snap(int SnappingClient)
{
	if(m_Team == 0)
	{
		if(NetworkClipped(SnappingClient))
			return;

		//if(GameServer()->m_apPlayers[SnappingClient] && GameServer()->m_apPlayers[SnappingClient]->GetCharacter() && distance(GameServer()->m_apPlayers[SnappingClient]->GetCharacter()->GetPos(), m_Pos) > 550)
		//	return;
	}
	else
	{
		if(((CGameControllerEXP*)GameServer()->m_pController)->m_Boss.m_Exist && !GameServer()->m_apPlayers[SnappingClient]->m_GameExp.m_BossKiller)
			return;
	}

	CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, m_Team, sizeof(CNetObj_Flag));
	if(!pFlag)
		return;

	pFlag->m_X = (int)m_Pos.x;
	pFlag->m_Y = (int)m_Pos.y;
	pFlag->m_Team = m_Team;
}
