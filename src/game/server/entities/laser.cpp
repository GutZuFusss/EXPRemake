/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include "character.h"
#include "laser.h"

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, bool Turret, bool Freezer)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos)
{
	m_Owner = Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_IsTurret = Turret;
	m_TurretCollision = false;
	m_IsFreezer = false;
	GameWorld()->InsertEntity(this);
	DoBounce();
}


bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pOwnerChar);
	if(!pHit)
		return false;

	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	if(m_IsTurret)
	{
		if(!pHit->GetPlayer()->IsBot())
			pHit->TakeDamage(vec2(0, 0), GameServer()->Tuning()->m_TurretLaserDamage, m_Owner, WEAPON_WORLD);
	}
	else if(m_IsFreezer)
	{
		if(!GameServer()->m_pController->IsFriendlyFire(m_Owner, pHit->GetPlayer()->GetCID()))
			pHit->Freeze();
	}
	else
		pHit->TakeDamage(vec2(0.f, 0.f), g_pData->m_Weapons.m_aId[WEAPON_LASER].m_Damage, m_Owner, WEAPON_LASER);
	return true;
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0 || m_TurretCollision)
	{
		GameServer()->m_World.DestroyEntity(this);
		return;
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	if(GameServer()->Collision()->IntersectLine(m_Pos, To, 0x0, &To))
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			GameServer()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			m_Energy -= distance(m_From, m_Pos) + GameServer()->Tuning()->m_LaserBounceCost;
			m_Bounces++;

			if(m_Bounces > GameServer()->Tuning()->m_LaserBounceNum)
				m_Energy = -1;

			GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE);
		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}
}

void CLaser::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CLaser::Tick()
{
	// TURRETS COLLISION
	if(!m_IsTurret && GameServer()->m_apPlayers[m_Owner] && !GameServer()->m_apPlayers[m_Owner]->IsBot()
		&& Server()->Tick() > m_TurretHitTimer)
	{
		for(int b = 0; b < MAX_TURRETS; b++)
		{
			CTurret *t = &(((CGameControllerEXP*)GameServer()->m_pController)->m_aTurrets[b]);
			if(!t->m_Used || t->m_Dead)
				continue;
			
			float ClosestLen = distance(m_From, m_Pos) * 100.0f;
			vec2 IntersectPos = closest_point_on_line(m_From, m_Pos, t->m_Pos);
			float Len = distance(t->m_Pos, IntersectPos);
			if(Len < 32.0f && Len < ClosestLen)
			{
				if(m_IsFreezer)
					((CGameControllerEXP*)GameServer()->m_pController)->FreezeTurret(b);
				else
					((CGameControllerEXP*)GameServer()->m_pController)->HitTurret(b, m_Owner, 5);
				m_Pos = t->m_Pos;
				m_TurretCollision = true;
			}
		}
	}
	
	if(m_TurretCollision)
		m_TurretHitTimer = (float)Server()->Tick() + 0.1f*Server()->TickSpeed();
	if(Server()->Tick() > m_EvalTick+(Server()->TickSpeed()*GameServer()->Tuning()->m_LaserBounceDelay)/1000.0f)
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	pObj->m_StartTick = m_EvalTick;
}
