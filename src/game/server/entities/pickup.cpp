/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#include <game/server/gamemodes/exp/environment.h>
#include <game/server/gamemodes/exp/exp.h>

#include "pickup.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_FromDrop = false;
	m_DieTimer = Server()->Tick();
	m_IsBossShield = false;

	m_Type = Type;
	m_Subtype = SubType;
	m_ProximityRadius = PickupPhysSize;

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	if(m_FromDrop)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;
}

void CPickup::Tick()
{
	if(m_IsBossShield)
		return;

	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			if(m_Type == POWERUP_WEAPON)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}

	if(m_FromDrop)
	{
		if(Server()->Tick() > m_DieTimer + GameServer()->Tuning()->m_PickupLifetime*Server()->TickSpeed())
		{
			GameWorld()->DestroyEntity(this);
			return;
		}
	}

	// Check if a player intersected us
	CCharacter *pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 20.0f, 0);
	if(pChr && pChr->IsAlive())
	{
		CPlayer *pPlayer = pChr->GetPlayer();

		// player picked us up, is someone was hooking us, let them go
		int RespawnTime = -1;
		switch (m_Type)
		{
			case POWERUP_HEALTH:
				if(pChr->IncreaseHealth(4))
				{
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				}
				break;

			case POWERUP_ARMOR:
				if(!pPlayer->IsBot() && pPlayer->m_GameExp.m_ArmorMax < 10)
				{
					if(pPlayer->m_GameExp.m_ArmorMax == 0)
						GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: ARMOR. Say /items for more info.");
					else
						GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: ARMOR.");

					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
					pPlayer->m_GameExp.m_ArmorMax += 1;
					pChr->m_Armor += 1;
				}
				break;

			case POWERUP_WEAPON:
				if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS)
				{
					if(pChr->GiveWeapon(m_Subtype, 10) && !pPlayer->IsBot())
					{
						char aMsg[64];
						str_format(aMsg, sizeof(aMsg), "Picked up: %s.", GetWeaponName(m_Subtype));
						GameServer()->SendChatTarget(pPlayer->GetCID(), aMsg);
						
						pPlayer->GetWeapon(WEAPON_GRENADE);

						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

						if(m_Subtype == WEAPON_GRENADE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);
						else if(m_Subtype == WEAPON_SHOTGUN)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
						else if(m_Subtype == WEAPON_RIFLE)
							GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);

						if(pChr->GetPlayer())
							GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);
					}
				}
				break;

			case POWERUP_NINJA:
				{
					// activate ninja on target player
					pChr->GiveNinja();
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

					// loop through all players, setting their emotes
					CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
					for(; pC; pC = (CCharacter *)pC->TypeNext())
					{
						if (pC != pChr)
							pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
					}

					pChr->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
					break;
				}

			case POWERUP_LIFE:
				{
					if(!pPlayer->IsBot())
					{
						if(pPlayer->m_GameExp.m_Items.m_Lives == 0)
							GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: LIFE. Say /items for more info.");
						else
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "Picked up: LIFE (%d)", pPlayer->m_GameExp.m_Items.m_Lives+1);
							GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
						}
						pPlayer->m_GameExp.m_Items.m_Lives++;
						
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
					}
				}
				break;
			
			case POWERUP_MINOR_POTION:
				{
					if(!pPlayer->IsBot())
					{
						if(pPlayer->m_GameExp.m_Items.m_MinorPotions == 0)
							GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: MINOR POTION. Say /items for more info.");
						else
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "Picked up: MINOR POTION (%d)", pPlayer->m_GameExp.m_Items.m_MinorPotions+1);
							GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
						}
						pPlayer->m_GameExp.m_Items.m_MinorPotions++;
						
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
					}
				}
				break;
			
			case POWERUP_GREATER_POTION:
				{
					if(!pPlayer->IsBot())
					{
						if(pPlayer->m_GameExp.m_Items.m_GreaterPotions == 0)
							GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: GREATER POTION. Say /items for more info.");
						else
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "Picked up: GREATER POTION (%d)", pPlayer->m_GameExp.m_Items.m_GreaterPotions+1);
							GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
						}
						pPlayer->m_GameExp.m_Items.m_GreaterPotions++;
						
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
					}
				}
				break;

			default:
				break;
		};

		if(m_FromDrop)
		{
			GameServer()->m_World.DestroyEntity(this);
		}
		else
		{
			int RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
			if(RespawnTime >= 0)
				m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
		}
	}

	if(m_Type == POWERUP_MINOR_POTION || m_Type == POWERUP_GREATER_POTION)
	{
		if(Server()->Tick() > m_AnimationTimer)
		{
			int ID = -1;
			for(int i = 0; i < g_Config.m_SvMaxClients; i++)
			{
				if(GameServer()->m_apPlayers[i])
				{
					ID = i;
					break;
				}
			}
			if(ID == -1) return;
			GameServer()->CreateDeath(m_Pos, ID);
			GameServer()->CreateDeath(m_Pos, -1);
			float Sec = (m_Type == POWERUP_GREATER_POTION ? 0.3 : 0.5);
			m_AnimationTimer = Server()->Tick() + Sec*Server()->TickSpeed();
		}  
	}
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = RealPickup(m_Type);
	pP->m_Subtype = RealSubtype(m_Subtype);
}

void CPickup::CreateRandomFromBot(int lvl, vec2 Pos)
{
	m_FromDrop = true;
	m_Pos = Pos;
	
	int r = Server()->Tick()%100;
	
	if(lvl == 1)
	{
		if(r < 70)
			m_Type = POWERUP_HEALTH; //70%
		else if(r < 90)
			m_Type = POWERUP_MINOR_POTION; //20%
		else
		{ //10%
			if(r < 96)
			{
				m_Type = POWERUP_WEAPON;
				m_Subtype = WEAPON_SHOTGUN; //6%
			}
			else
			{
				m_Type = POWERUP_WEAPON;
				m_Subtype = WEAPON_GRENADE; //4%
			}
		}
	}
	else if(lvl == 2)
	{
		if(r < 35)
			m_Type = POWERUP_HEALTH; //35%
		else if(r < 60)
			m_Type = POWERUP_LIFE; //25%
		else if(r < 80)
			m_Type = POWERUP_MINOR_POTION; //20%
		else if(r < 95)
			m_Type = POWERUP_GREATER_POTION; //15%
		else
		{ //5%
			m_Type = POWERUP_WEAPON;
			m_Subtype = WEAPON_KAMIKAZE;
		}
	}
	else if(lvl == 3)
	{
		if(r < 25)
			m_Type = POWERUP_HEALTH; //25%
		else if(r < 30)
			m_Type = POWERUP_LIFE; //5%
		else if(r < 55)
			m_Type = POWERUP_MINOR_POTION; //25%
		else if(r < 80)
			m_Type = POWERUP_GREATER_POTION; //25%
		else
		{ //20%
			if(r < 92)
			{
				m_Type = POWERUP_WEAPON;
				m_Subtype = WEAPON_SHOTGUN; //12%
			}
			else
			{
				m_Type = POWERUP_WEAPON;
				m_Subtype = WEAPON_GRENADE; //8%
			}
		}
	}
	else if(lvl == 4)
	{
		m_Type = POWERUP_WEAPON;
		m_Subtype = WEAPON_FREEZER;
	}

	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && !GameServer()->m_apPlayers[id]->IsBot())
			Snap(id);
	}
}



void CPickup::CreateRandomFromTurret(int TurretType, vec2 Pos)
{
	m_FromDrop = true;
	m_Pos = Pos;

	int r = Server()->Tick()%100; //r < 75
	
	if(r < 70)
		m_Type = POWERUP_ARMOR; //70%
	else
	{
		if(TurretType == TURRET_TYPE_LASER)
		{
			m_Type = POWERUP_WEAPON;
			m_Subtype = WEAPON_RIFLE;
		}
		else if(TurretType == TURRET_TYPE_GUN)
		{
			m_Type = POWERUP_WEAPON;
			m_Subtype = WEAPON_GUN;
		}
	}
	
	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && !GameServer()->m_apPlayers[id]->IsBot())
			Snap(id);
	}
}

void CPickup::MakeBossShield()
{
	m_IsBossShield = true;
	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && !GameServer()->m_apPlayers[id]->IsBot())
			Snap(id);
	}
}

const char *CPickup::GetWeaponName(int wid)
{
	if(wid == WEAPON_HAMMER)
		return "HAMMER";
	else if(wid == WEAPON_GUN)
		return "GUN";
	else if(wid == WEAPON_SHOTGUN)
		return "SHOTGUN";
	else if(wid == WEAPON_GRENADE)
		return "GRENADE";
	else if(wid == WEAPON_RIFLE)
		return "LASER";
	else if(wid == WEAPON_NINJA)
		return "NINJA";
	else if(wid == WEAPON_KAMIKAZE)
		return "KAMIKAZE";
	else if(wid == WEAPON_FREEZER)
		return "FREEZER";
	return "?";
}

int CPickup::RealPickup(int Type)
{
	if(Type == POWERUP_LIFE)
		Type = POWERUP_HEALTH;
	else if(Type == POWERUP_MINOR_POTION || Type == POWERUP_GREATER_POTION)
		Type = POWERUP_HEALTH;
	return Type;
}

int CPickup::RealSubtype(int Type)
{
	if(Type == WEAPON_KAMIKAZE)
		Type = WEAPON_NINJA;
	else if(Type == WEAPON_FREEZER)
		Type = WEAPON_RIFLE;
	return Type;
}
