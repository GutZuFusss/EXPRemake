#include <engine/shared/config.h>
#include <generated/server_data.h>

#include <game/collision.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>
#include <game/server/entities/pickup.h>

#include "exp.h"

void CGameControllerEXP::TickEnvironment()
{
	// TELEPORTER
	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && GameServer()->m_apPlayers[id]->GetCharacter())
		{
			vec2 Dest = GameServer()->Collision()->Teleport(GameServer()->m_apPlayers[id]->GetCharacter()->GetPos().x, GameServer()->m_apPlayers[id]->GetCharacter()->GetPos().y);
			if(Dest.x >= 0 && Dest.y >= 0)
			{
				GameServer()->m_apPlayers[id]->GetCharacter()->m_Core.m_Pos = Dest;
				GameServer()->m_apPlayers[id]->GetCharacter()->m_Core.m_Vel = vec2(0, 0);
			}
		}
	}

	// HEALING AND GAS ZONES
	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && GameServer()->m_apPlayers[id]->GetCharacter())
		{
			if(GameServer()->Collision()->GetCollisionAt(GameServer()->m_apPlayers[id]->GetCharacter()->GetPos().x, GameServer()->m_apPlayers[id]->GetCharacter()->GetPos().y) & CCollision::COLFLAG_HEALING)
			{
				if((float)Server()->Tick() > GameServer()->m_apPlayers[id]->m_GameExp.m_RegenTimer)
				{
					if(GameServer()->m_apPlayers[id]->GetCharacter()->m_Health < 10)
					{
						GameServer()->m_apPlayers[id]->GetCharacter()->m_Health++;
						GameServer()->m_apPlayers[id]->m_GameExp.m_RegenTimer = (float)Server()->Tick() + GameServer()->Tuning()->m_RegenTimer*Server()->TickSpeed();
					}
					else if(GameServer()->m_apPlayers[id]->GetCharacter()->m_Armor < GameServer()->m_apPlayers[id]->m_GameExp.m_ArmorMax)
					{
						GameServer()->m_apPlayers[id]->GetCharacter()->m_Armor++;
						GameServer()->m_apPlayers[id]->m_GameExp.m_RegenTimer = (float)Server()->Tick() + GameServer()->Tuning()->m_RegenTimer*Server()->TickSpeed();
					}
				}
			}
			
			if(GameServer()->Collision()->GetCollisionAt(GameServer()->m_apPlayers[id]->GetCharacter()->GetPos().x, GameServer()->m_apPlayers[id]->GetCharacter()->GetPos().y) & CCollision::COLFLAG_POISON)
			{
				if((float)Server()->Tick() > GameServer()->m_apPlayers[id]->m_GameExp.m_PoisonTimer)
				{
					GameServer()->m_apPlayers[id]->GetCharacter()->TakeDamage(vec2(0, 0), 1, -1, WEAPON_WORLD);
					GameServer()->m_apPlayers[id]->m_GameExp.m_PoisonTimer = (float)Server()->Tick() + GameServer()->Tuning()->m_PoisonTimer*Server()->TickSpeed();
				}
			}
		}
	}

	// MINES
	for(int m = 0; m < m_CurMine; m++)
	{
		if(!m_aMines[m].m_Used)
			continue;
		
		if(m_aMines[m].m_Dead)
		{
			if((float)Server()->Tick() > m_aMines[m].m_TimerRespawn + GameServer()->Tuning()->m_RespawnTimer*Server()->TickSpeed())
				BuildMine(m);
		}
		else
		{
			//create tic-tic
			CCharacter *pClosest = GameServer()->m_World.ClosestCharacter(m_aMines[m].m_Pos, 400, NULL, true);
			if(pClosest)
			{
				int Mod = (int)(distance(pClosest->GetPos(), m_aMines[m].m_Pos)/8);
				if(Mod == 0 || Server()->Tick()%Mod == 0)
					GameServer()->CreateSound(m_aMines[m].m_Pos, SOUND_HOOK_ATTACH_GROUND);
			}
			
			//emote close players
			CCharacter *apCloseChars[MAX_CLIENTS];
			int n = GameServer()->m_World.FindEntities(m_aMines[m].m_Pos, 300.0f, (CEntity**)apCloseChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < n; i++)
			{
				if(!apCloseChars[i] || apCloseChars[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aMines[m].m_Pos, apCloseChars[i]->GetPos(), NULL, NULL, false)) 
					continue;
				apCloseChars[i]->m_EmoteType = EMOTE_SURPRISE;
				apCloseChars[i]->m_EmoteStop = Server()->Tick() + 1*Server()->TickSpeed();
				if(Server()->Tick() > apCloseChars[i]->GetPlayer()->m_LastEmote+Server()->TickSpeed()*2)
				{
					GameServer()->SendEmoticon(apCloseChars[i]->GetPlayer()->GetCID(), EMOTICON_EXCLAMATION); //!
					apCloseChars[i]->GetPlayer()->m_LastEmote = Server()->Tick();
				}
			}
			
			CCharacter *apCloseCharacters[MAX_CLIENTS];
			int num = GameServer()->m_World.FindEntities(m_aMines[m].m_Pos, 16.0f, (CEntity**)apCloseCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < num; i++)
			{
				if(!apCloseCharacters[i] || apCloseCharacters[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aMines[m].m_Pos, apCloseCharacters[i]->GetPos(), NULL, NULL, false)) 
					continue;
				DestroyMine(m);
				break;
			}
		}
	}

	// TRAPS
	for(int t = 0; t < m_CurTrap; t++)
	{
		if(!m_aTraps[t].m_Used)
			continue;
		if((float)Server()->Tick() < m_aTraps[t].m_Timer)
			continue;
		
		CCharacter *apCloseChars[MAX_CLIENTS];
		int num = GameServer()->m_World.FindEntities(m_aTraps[t].m_Pos, 600.0f, (CEntity**)apCloseChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < num; i++)
		{
			if(!apCloseChars[i] || apCloseChars[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aTraps[t].m_Pos, apCloseChars[i]->GetPos(), NULL, NULL, false)) 
				continue;
			if(apCloseChars[i]->GetPos().y > m_aTraps[t].m_Pos.y && apCloseChars[i]->GetPos().x > m_aTraps[t].m_Pos.x-64 && apCloseChars[i]->GetPos().x < m_aTraps[t].m_Pos.x+64)
			{
				new CProjectile(&GameServer()->m_World, WEAPON_GRENADE, -1, m_aTraps[t].m_Pos, vec2(0, 1), (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime), 5, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);
				m_aTraps[t].m_Timer = (float)Server()->Tick() + g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Firedelay*Server()->TickSpeed()/1000.0f;
				GameServer()->CreateSound(m_aTraps[t].m_Pos, SOUND_GRENADE_FIRE);
				break;
			}
		}
	}

	// TURRETS
	for(int t = 0; t < m_CurTurret; t++)
	{
		if(!m_aTurrets[t].m_Used)
			continue;
		
		if(m_aTurrets[t].m_Dead)
		{
			if((float)Server()->Tick() > m_aTurrets[t].m_TimerRespawn + GameServer()->Tuning()->m_RespawnTimer*Server()->TickSpeed())
				BuildTurret(t);
			continue;
		}
		
		if(m_aTurrets[t].m_Frozen)
		{
			if((float)Server()->Tick() > m_aTurrets[t].m_FrozenTimer)
				m_aTurrets[t].m_Frozen = false;
		}
		
		// LASER TURRETS
		if(m_aTurrets[t].m_Type == TURRET_TYPE_LASER)
		{
			new CLaser(&GameServer()->m_World, m_aTurrets[t].m_Pos, vec2(0, 0), 0.0f, -1, true, false);
			
			if(Server()->Tick() < m_aTurrets[t].m_Timer || m_aTurrets[t].m_Frozen)
				continue;
			
			CCharacter *apCloseChars[MAX_CLIENTS];
			int num = GameServer()->m_World.FindEntities(m_aTurrets[t].m_Pos, GameServer()->Tuning()->m_TurretLaserRadius, (CEntity**)apCloseChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < num; i++)
			{
				if(!apCloseChars[i] || apCloseChars[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aTurrets[t].m_Pos, apCloseChars[i]->GetPos(), NULL, NULL, false)) 
					continue;
				
				vec2 Direction = normalize(apCloseChars[i]->GetPos()-m_aTurrets[t].m_Pos);
				new CLaser(&GameServer()->m_World, m_aTurrets[t].m_Pos, Direction, GameServer()->Tuning()->m_TurretLaserRadius, -1, true, false);
				m_aTurrets[t].m_Timer = (float)Server()->Tick() + GameServer()->Tuning()->m_TurretReload*Server()->TickSpeed();
				GameServer()->CreateSound(m_aTurrets[t].m_Pos, SOUND_LASER_FIRE);
				
				break;
			}
		}
		
		// GUN TURRETS
		else if(m_aTurrets[t].m_Type == TURRET_TYPE_GUN)
		{
			if(Server()->Tick() < m_aTurrets[t].m_Timer || m_aTurrets[t].m_Frozen)
				continue;
			
			CCharacter *apCloseChars[MAX_CLIENTS];
			int num = GameServer()->m_World.FindEntities(m_aTurrets[t].m_Pos, 1000, (CEntity**)apCloseChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < num; i++)
			{
				if(!apCloseChars[i] || apCloseChars[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aTurrets[t].m_Pos, apCloseChars[i]->GetPos(), NULL, NULL, false)) 
					continue;
				
				vec2 Direction = normalize(apCloseChars[i]->GetPos()-m_aTurrets[t].m_Pos);
				new CProjectile(&GameServer()->m_World, WEAPON_GUN, -1, m_aTurrets[t].m_Pos, Direction, (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime), 1, 0, 0, -1, WEAPON_GUN);
				m_aTurrets[t].m_Timer = (float)Server()->Tick() + GameServer()->Tuning()->m_TurretReload*Server()->TickSpeed();
				GameServer()->CreateSound(m_aTurrets[t].m_Pos, SOUND_GUN_FIRE);
				
				break;
			}
		}
	}
}

void CGameControllerEXP::BuildTurret(int t)
{
	m_aTurrets[t].m_Dead = false;
	m_aTurrets[t].m_Life = GameServer()->Tuning()->m_TurretLife;
	m_aTurrets[t].m_Timer = 0.0f;
	m_aTurrets[t].m_TimerRespawn = 0.0f;
	m_aTurrets[t].m_Frozen = false;
	GameServer()->CreatePlayerSpawn(m_aTurrets[t].m_Pos);
}

void CGameControllerEXP::HitTurret(int t, int Owner, int Dmg)
{
	// create damage indicator
	m_aTurrets[t].m_DamageTaken += Dmg;
	if(Server()->Tick() < m_aTurrets[t].m_DamageTakenTick+25)
		GameServer()->CreateDamageInd(m_aTurrets[t].m_Pos, m_aTurrets[t].m_DamageTaken*0.25f, Dmg);
	else
	{
		m_aTurrets[t].m_DamageTaken = 0;
		GameServer()->CreateDamageInd(m_aTurrets[t].m_Pos, 0, Dmg);
	}
	m_aTurrets[t].m_DamageTakenTick = Server()->Tick();
	
	GameServer()->CreateSound(GameServer()->m_apPlayers[Owner]->m_ViewPos, SOUND_HIT, CmaskOne(Owner));
	
	m_aTurrets[t].m_Life -= Dmg;
	if(m_aTurrets[t].m_Life <= 0)
		DestroyTurret(t, Owner);
	char aBuf[255];
	if(!(m_aTurrets[t].m_Life%5))
	{
		str_format(aBuf, sizeof(aBuf), "Turret [Health: %i]", m_aTurrets[t].m_Life);
		GameServer()->SendChatTarget(Owner, aBuf);
	}
}

void CGameControllerEXP::DestroyTurret(int t, int Killer)
{
	m_aTurrets[t].m_Dead = true;
	m_aTurrets[t].m_Life = 0;
	m_aTurrets[t].m_Timer = 0.0f;
	m_aTurrets[t].m_TimerRespawn = (float)Server()->Tick();
	
	GameServer()->m_apPlayers[Killer]->m_Score++;
	
	if(Server()->Tick()%100 < 75)
	{
		CPickup *p = new CPickup(&GameServer()->m_World, 0, m_aTurrets[t].m_Pos);
		p->CreateRandomFromTurret(m_aTurrets[t].m_Type);
	}
	
	GameServer()->CreateDeath(m_aTurrets[t].m_Pos, g_Config.m_SvMaxClients);
}

void CGameControllerEXP::FreezeTurret(int t)
{
	m_aTurrets[t].m_Frozen = true;
	m_aTurrets[t].m_FrozenTimer = (float)Server()->Tick() + 5.0f*Server()->TickSpeed();
}

void CGameControllerEXP::BuildMine(int m)
{
	m_aMines[m].m_Dead = false;
	m_aMines[m].m_TimerRespawn = 0.0f;
}

void CGameControllerEXP::DestroyMine(int m)
{
	m_aMines[m].m_Dead = true;
	m_aMines[m].m_TimerRespawn = (float)Server()->Tick();
	
	GameServer()->CreateExplosion(m_aMines[m].m_Pos, -1, WEAPON_WORLD, 5);
	GameServer()->CreateSound(m_aMines[m].m_Pos, SOUND_GRENADE_EXPLODE);
}

void CGameControllerEXP::BuildTrap(int t)
{
	m_aTraps[t].m_Timer = 0.0f;
}

void CGameControllerEXP::BuildDoor(int d)
{
	m_aDoors[d].m_CreateLaser = false;
	m_aDoors[d].m_Laser = new CLaserDoor(&GameServer()->m_World, m_aDoors[d].m_Pos, m_aDoors[d].m_Type, &m_aDoors[d]);
	GameServer()->Collision()->SetDoor(m_aDoors[d].m_Laser->m_From.x, m_aDoors[d].m_Laser->m_From.y, m_aDoors[d].m_Laser->GetPos().x, m_aDoors[d].m_Laser->GetPos().y);
}
