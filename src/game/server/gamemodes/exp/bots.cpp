#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include <game/server/entities/pickup.h>
#include "exp.h"
#include "bots.h"

void CGameControllerEXP::TickBots()
{
	// REMOVE DEAD BOTS
	for(int b = g_Config.m_SvMaxClients; b < MAX_CLIENTS; b++)
	{
		if(GameServer()->m_apPlayers[b] && GameServer()->m_apPlayers[b]->m_MustRemoveBot)
			RemoveBot(b, true);
	}

	// CHECK FOR NOBODY
	for(int b = g_Config.m_SvMaxClients; b < MAX_CLIENTS; b++)
	{
		CPlayer *p = GameServer()->m_apPlayers[b];
		if(!p || !p->GetCharacter())
			continue;
		if(p->m_BotLevel == 4)
			continue;
		
		bool Nobody = true;
		for(int i = 0; i < g_Config.m_SvMaxClients; i++)
		{
			if(GameServer()->m_apPlayers[i] && !p->GetCharacter()->NetworkClipped(i))
			{
				Nobody = false;
				break;
			}
		}
		
		if(Nobody)
		{
			if(p->m_NobodyTimer == 0)
			{
				p->m_NobodyTimer = Server()->Tick() + 10.0f*Server()->TickSpeed();
			}
			else
			{
				if(Server()->Tick() > p->m_NobodyTimer)
					RemoveBot(b, false);
			}
		}
		else
			p->m_NobodyTimer = 0;
	}
	
	
	// CHECK FOR BOT SPAWNS
	for(int l = 0; l < BOT_LEVELS; l++)
	{
		for(int s = 0; s < m_aNumBotSpawns[l]; s++)
		{
			if(BotCanSpawn() == -1)
				break;
			
			CBotSpawn *pSpawn = &m_aaBotSpawns[l][s];
			if(pSpawn->m_Spawned)
				continue;
			if(Server()->Tick() < pSpawn->m_RespawnTimer + GameServer()->Tuning()->m_RespawnTimer*Server()->TickSpeed())
				continue;
			
			for(int p = 0; p < g_Config.m_SvMaxClients; p++)
			{
				if(!GameServer()->m_apPlayers[p] || !GameServer()->m_apPlayers[p]->GetCharacter())
					continue;
				
				if(distance(GameServer()->m_apPlayers[p]->GetCharacter()->GetPos(), pSpawn->m_Pos) < 700.0f)
				{
					BotSpawn(pSpawn);
					break;
				}
			}
		}
	}

	// TICK BOSS
	if(m_Boss.m_Exist)
	{
		if(m_Boss.m_Spawn.m_Spawned)
		{
			if(distance(GameServer()->m_apPlayers[m_Boss.m_ClientID]->GetCharacter()->GetPos(), m_Boss.m_Spawn.m_Pos) > GameServer()->Tuning()->m_BossDistancelimit)
			{
				GameServer()->m_apPlayers[m_Boss.m_ClientID]->GetCharacter()->Teleport(m_Boss.m_Spawn.m_Pos);
			}
			
			if(m_Boss.m_ShieldActive)
			{
				/*if(Server()->Tick() > m_Boss.m_RegenTimer)
				{
					if(GameServer()->m_apPlayers[m_Boss.m_ClientID]->GetCharacter()->m_Health < GameServer()->m_apPlayers[m_Boss.m_ClientID]->MaxHealth())
					{
						GameServer()->m_apPlayers[m_Boss.m_ClientID]->GetCharacter()->m_Health++;
						m_Boss.m_RegenTimer = Server()->Tick() + 1.0f*Server()->TickSpeed();
					}
				}*/
				
				vec2 BossPos = GameServer()->m_apPlayers[m_Boss.m_ClientID]->GetCharacter()->GetPos();
				vec2 BossVel = GameServer()->m_apPlayers[m_Boss.m_ClientID]->GetCharacter()->m_Core.m_Vel;
				
				if(m_Boss.m_apShieldIcons[0])
					m_Boss.m_apShieldIcons[0]->m_Pos = (BossPos-BossVel*1)*0.1f + m_Boss.m_apShieldIcons[0]->m_Pos*0.9f;
				if(m_Boss.m_apShieldIcons[1])
					m_Boss.m_apShieldIcons[1]->m_Pos = (BossPos-BossVel*4)*0.1f + m_Boss.m_apShieldIcons[1]->m_Pos*0.9f;
				if(m_Boss.m_apShieldIcons[2])
					m_Boss.m_apShieldIcons[2]->m_Pos = (BossPos-BossVel*7)*0.1f + m_Boss.m_apShieldIcons[2]->m_Pos*0.9f;
			}
			else
			{
				if(Server()->Tick() > m_Boss.m_ShieldTimer)
				{
					m_Boss.m_ShieldActive = true;
					m_Boss.m_ShieldHealth = 18;
					for(int s = 0; s < 3; s++)
					{
						m_Boss.m_apShieldIcons[s] = new CPickup(&GameServer()->m_World, 1, 0);//, GameServer()->m_apPlayers[m_Boss.m_ClientID]->GetCharacter()->GetPos());
						m_Boss.m_apShieldIcons[s]->MakeBossShield();
					}
				}
			}
		}
		else if(m_Boss.m_Spawn.m_RespawnTimer + GameServer()->Tuning()->m_RespawnTimer*2*Server()->TickSpeed())
		{
			for(int p = 0; p < g_Config.m_SvMaxClients; p++)
			{
				if(!GameServer()->m_apPlayers[p] || !GameServer()->m_apPlayers[p]->GetCharacter())
					continue;
				if(GameServer()->m_apPlayers[p]->m_GameExp.m_BossKiller) // he has already killed the boss
					continue;
				
				if(distance(GameServer()->m_apPlayers[p]->GetCharacter()->GetPos(), m_Boss.m_Spawn.m_Pos) < 700.0f)
				{
					BotSpawn(&m_Boss.m_Spawn);
					break;
				}
			}
		}
	}
}

int CGameControllerEXP::BotCanSpawn()
{
	for(int p = g_Config.m_SvMaxClients; p < MAX_CLIENTS; p++)
		if(!GameServer()->m_apPlayers[p]) return p;
	return -1;
}

void CGameControllerEXP::BotSpawn(CBotSpawn *pSpawn)
{
	CEntity *pEnts[1] = {0};
	int NumEnts = GameServer()->m_World.FindEntities(pSpawn->m_Pos, 28.0f, pEnts, 1, CGameWorld::ENTTYPE_CHARACTER);
	if(NumEnts != 0)
		return;

	int BID = BotCanSpawn();
	if(BID == -1) return;
	
	GameServer()->OnClientConnected(BID, true);
	GameServer()->m_apPlayers[BID]->MakeBot(pSpawn);
	//GameServer()->m_apPlayers[BID]->SetTeam(0);
	pSpawn->m_Spawned = true;
	
	if(pSpawn->m_Level == 4)
	{
		m_Boss.m_ClientID = BID;
		m_Boss.m_ShieldActive = false;
		m_Boss.m_ShieldTimer = Server()->Tick() - 1;
	}
}

void CGameControllerEXP::RemoveBot(int ID, bool Killed)
{
	GameServer()->m_apPlayers[ID]->m_pBotSpawn->m_Spawned = false;
	if(Killed)
		GameServer()->m_apPlayers[ID]->m_pBotSpawn->m_RespawnTimer = Server()->Tick();
	
	if(Killed)
	{
		CPickup *pPickup = new CPickup(&GameServer()->m_World, 0, 0);//, GameServer()->m_apPlayers[ID]->m_LastPos);
		pPickup->CreateRandomFromBot(GameServer()->m_apPlayers[ID]->m_BotLevel, GameServer()->m_apPlayers[ID]->m_LastPos);
	}
	
	delete GameServer()->m_apPlayers[ID]->m_pCharacter;
	GameServer()->m_apPlayers[ID]->m_pCharacter = NULL;
	//GameServer()->m_apPlayers[ID]->character = NULL;
	delete GameServer()->m_apPlayers[ID];
	GameServer()->m_apPlayers[ID] = NULL;
}
