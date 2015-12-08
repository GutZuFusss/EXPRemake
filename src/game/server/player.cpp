/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <math.h>

#include "entities/character.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"
#include <game/server/gamemodes/exp/bots.h>
#include <game/server/gamemodes/exp/environment.h>

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, bool Dummy)
{
	m_pGameServer = pGameServer;
	m_RespawnTick = Server()->Tick();
	m_DieTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	m_pCharacter = 0;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->GetStartTeam();
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_InactivityTickCounter = 0;
	m_Dummy = Dummy;
	m_IsReadyToPlay = !GameServer()->m_pController->IsPlayerReadyMode();
	m_RespawnDisabled = GameServer()->m_pController->GetStartRespawnState();
	m_DeadSpecMode = false;
	m_Spawning = 0;
}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Tick()
{
	if(!IsBot() && !Server()->ClientIngame(m_ClientID))
		return;

	Server()->SetClientScore(m_ClientID, m_Score);

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(m_pCharacter && !m_pCharacter->IsAlive())
	{
		delete m_pCharacter;
		m_pCharacter = 0;
	}

	if(!GameServer()->m_pController->IsGamePaused())
	{
		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x-m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y-m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick() && !m_DeadSpecMode)
			Respawn();

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
				m_ViewPos = m_pCharacter->GetPos();
		}
		else if(m_Spawning && m_RespawnTick <= Server()->Tick())
			TryRespawn();

		if(!m_DeadSpecMode && m_LastActionTick != Server()->Tick())
			++m_InactivityTickCounter;
	}
	else
	{
		++m_RespawnTick;
		++m_DieTick;
		++m_ScoreStartTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
 	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators and dead players
	if((m_Team == TEAM_SPECTATORS || m_DeadSpecMode) && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID])
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->m_ViewPos;
}

void CPlayer::Snap(int SnappingClient)
{
	if(!IsBot() && !Server()->ClientIngame(m_ClientID))
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_PlayerFlags = m_PlayerFlags&PLAYERFLAG_CHATTING;
	if(Server()->IsAuthed(m_ClientID))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_ADMIN;
	if(!GameServer()->m_pController->IsPlayerReadyMode() || m_IsReadyToPlay)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_READY;
	if(m_RespawnDisabled && (!GetCharacter() || !GetCharacter()->IsAlive()))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_DEAD;
	if(SnappingClient != -1 && (m_Team == TEAM_SPECTATORS || m_DeadSpecMode) && SnappingClient == m_SpectatorID)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_WATCHING;
	pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	pPlayerInfo->m_Score = m_Score;

	if(m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_DeadSpecMode))
	{
		CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;

		pSpectatorInfo->m_SpectatorID = m_SpectatorID;
		pSpectatorInfo->m_X = m_ViewPos.x;
		pSpectatorInfo->m_Y = m_ViewPos.y;
	}

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_ClientInfo *pClientInfo = static_cast<CNetObj_De_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_CLIENTINFO, m_ClientID, sizeof(CNetObj_De_ClientInfo)));
		if(!pClientInfo)
			return;

		pClientInfo->m_Local = 0;
		pClientInfo->m_Team = m_Team;
		StrToInts(pClientInfo->m_aName, 4, Server()->ClientName(m_ClientID));
		StrToInts(pClientInfo->m_aClan, 3, Server()->ClientClan(m_ClientID));
		pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);

		for(int p = 0; p < 6; p++)
		{
			StrToInts(pClientInfo->m_aaSkinPartNames[p], 6, m_TeeInfos.m_aaSkinPartNames[p]);
			pClientInfo->m_aUseCustomColors[p] = m_TeeInfos.m_aUseCustomColors[p];
			pClientInfo->m_aSkinPartColors[p] = m_TeeInfos.m_aSkinPartColors[p];
		}
	}
}

void CPlayer::OnDisconnect()
{
	KillCharacter();

	if(m_Team != TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				if(GameServer()->m_apPlayers[i]->m_DeadSpecMode)
					GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
				else
					GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
			}
		}
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(GameServer()->m_World.m_Paused)
	{
		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
 		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
		m_pCharacter->OnDirectInput(NewInput);

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		Respawn();

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
		m_InactivityTickCounter = 0;
	}
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);
		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn()
{
	if(m_RespawnDisabled)
	{
		// enable spectate mode for dead players
		m_DeadSpecMode = true;
		m_IsReadyToPlay = true;
		UpdateDeadSpecMode();
		return;
	}

	m_DeadSpecMode = false;

	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

bool CPlayer::SetSpectatorID(int SpectatorID)
{
	if(m_SpectatorID == SpectatorID || m_ClientID == SpectatorID)
		return false;

	if(m_Team == TEAM_SPECTATORS)
	{
		// check for freeview or if wanted player is playing
		if(SpectatorID == SPEC_FREEVIEW || (GameServer()->m_apPlayers[SpectatorID] && GameServer()->m_apPlayers[SpectatorID]->GetTeam() != TEAM_SPECTATORS))
		{
			m_SpectatorID = SpectatorID;
			return true;
		}
	}
	else if(m_DeadSpecMode)
	{
		// check if wanted player can be followed
		if(GameServer()->m_apPlayers[SpectatorID] && DeadCanFollow(GameServer()->m_apPlayers[SpectatorID]))
		{
			m_SpectatorID = SpectatorID;
			return true;
		}
	}

	return false;
}

bool CPlayer::DeadCanFollow(CPlayer *pPlayer) const
{
	// check if wanted player is in the same team and alive
	return (!pPlayer->m_RespawnDisabled || (pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())) && pPlayer->GetTeam() == m_Team;
}

void CPlayer::UpdateDeadSpecMode()
{
	// check if actual spectator id is valid
	if(m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID] && DeadCanFollow(GameServer()->m_apPlayers[m_SpectatorID]))
		return;

	// find player to follow
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && DeadCanFollow(GameServer()->m_apPlayers[i]))
		{
			m_SpectatorID = i;
			return;
		}
	}

	// no one available to follow -> turn spectator mode off
	m_DeadSpecMode = false;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	KillCharacter();

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;
	m_DeadSpecMode = false;
	
	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	
	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				if(GameServer()->m_apPlayers[i]->m_DeadSpecMode)
					GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
				else
					GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
			}
		}
	}
}

void CPlayer::TryRespawn()
{
	if(m_GameExp.m_LastFlag == 0)
	{
		vec2 SpawnPos = vec2(0.0f, 0.0f);
		if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
			return;

		m_GameExp.m_EnterTick = Server()->Tick();
		LoadGame(SpawnPos, m_GameExp.m_LastFlag, m_Score, 0, m_GameExp.m_ArmorMax, m_GameExp.m_Weapons, m_GameExp.m_Items, m_GameExp.m_BossHitter, m_GameExp.m_BossKiller);
	}
	else if(m_Team != -1)
	{
		LoadGame(((CGameControllerEXP*)GameServer()->m_pController)->m_aFlagsCP[m_GameExp.m_LastFlag-1]->GetPos(), m_GameExp.m_LastFlag,
			m_Score, m_GameExp.m_Time, m_GameExp.m_ArmorMax, m_GameExp.m_Weapons, m_GameExp.m_Items, m_GameExp.m_BossHitter, m_GameExp.m_BossKiller);
	}
}

void CPlayer::MakeBot(CBotSpawn *pSpawn)
{
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_Team = 1;
	m_pCharacter->Spawn(GameServer()->m_apPlayers[m_ClientID], pSpawn->m_Pos);
	GameServer()->CreatePlayerSpawn(pSpawn->m_Pos);
	m_BotLevel = pSpawn->m_Level;
	m_pBotSpawn = pSpawn;
	
	m_pCharacter->m_Health = MaxHealth();
	m_pCharacter->m_Armor = MaxArmor();
	
	if(m_BotLevel == 1)
	{
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY], "bear", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING], "bear", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION], "hair", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES]));
	}
	else if(m_BotLevel == 2)
	{
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES]));

		m_pCharacter->m_aWeapons[WEAPON_HAMMER].m_Got = false;
		m_pCharacter->m_aWeapons[WEAPON_GUN].m_Got = false;
		m_pCharacter->m_aWeapons[WEAPON_KAMIKAZE].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_KAMIKAZE].m_Ammo = -1;
		m_pCharacter->m_QueuedWeapon = WEAPON_KAMIKAZE;
		m_pCharacter->m_ActiveWeapon = WEAPON_KAMIKAZE;
	}
	else if(m_BotLevel == 3)
	{
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES]));

		m_pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Ammo = 10;
	}
	else if(m_BotLevel == 4)
	{
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_BODY]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_MARKING]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_DECORATION]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_HANDS]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_FEET]));
		str_copy(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES], "standard", sizeof(m_TeeInfos.m_aaSkinPartNames[SKINPART_EYES]));

		m_pCharacter->m_aWeapons[WEAPON_GUN].m_Got = false;
		m_pCharacter->m_aWeapons[WEAPON_FREEZER].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_FREEZER].m_Ammo = -1;
	}
}

int CPlayer::MaxHealth()
{
	if(m_BotLevel == 1)
		return 10;
	else if(m_BotLevel == 2)
		return 8;
	else if(m_BotLevel == 3)
		return 10;
	else if(m_BotLevel == 4)
		return 200;
	return 10;
}

int CPlayer::MaxArmor()
{
	if(m_BotLevel == 1)
		return 0;
	else if(m_BotLevel == 2)
		return 0;
	else if(m_BotLevel == 3)
		return 5;
	else if(m_BotLevel == 4)
		return 0;
	return 10;
}

const char *CPlayer::GetMonsterName()
{
	int Life = m_pCharacter->m_Health + m_pCharacter->m_Armor;
	int MaxLife = MaxHealth() + MaxArmor();
	float coef = (float)Life / (float)MaxLife;
	
	if(coef > 0.8f)
		return "|-----|";
	else if(coef > 0.6f)
		return "|----|";
	else if(coef > 0.4f)
		return "|---|";
	else if(coef > 0.2f)
		return "|--|";
	else
		return "|-|";
}

void CPlayer::LoadGame(vec2 SpawnPos, int Flag, int Kills, int Time, int Armor, int w, CItems Items, bool BHitter, bool BKiller)
{
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(GameServer()->m_apPlayers[m_ClientID], SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos);
	m_GameExp.m_ArmorMax = Armor;
	m_pCharacter->m_Armor = Armor;
	m_GameExp.m_LastFlag = Flag;
	m_GameExp.m_Kills = Kills;
	m_Score = Kills;
	m_GameExp.m_Time = Time;
	if(w & (int)pow(2, WEAPON_GUN))
	{
		m_pCharacter->m_aWeapons[WEAPON_GUN].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_GUN].m_Ammo = g_pData->m_Weapons.m_aId[WEAPON_GUN].m_Maxammo;
	}
	if(w & (int)pow(2, WEAPON_SHOTGUN))
	{
		m_pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Ammo = g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Maxammo;
	}
	if(w & (int)pow(2, WEAPON_GRENADE))
	{
		m_pCharacter->m_aWeapons[WEAPON_GRENADE].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_GRENADE].m_Ammo = g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Maxammo;
	}
	if(w & (int)pow(2, WEAPON_LASER))
	{
		m_pCharacter->m_aWeapons[WEAPON_LASER].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_LASER].m_Ammo = g_pData->m_Weapons.m_aId[WEAPON_LASER].m_Maxammo;
	}
	if(w & (int)pow(2, WEAPON_KAMIKAZE))
	{
		m_pCharacter->m_aWeapons[WEAPON_KAMIKAZE].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_KAMIKAZE].m_Ammo = -1;
	}
	if(w & (int)pow(2, WEAPON_FREEZER))
	{
		m_pCharacter->m_aWeapons[WEAPON_FREEZER].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_FREEZER].m_Ammo = -1;
	}
	m_GameExp.m_Weapons = w;
	m_GameExp.m_Items = Items;
	m_GameExp.m_BossHitter = BHitter;
	m_GameExp.m_BossKiller = BKiller;
}

void CPlayer::GetWeapon(int WID)
{
	if(m_GameExp.m_Weapons & (int)pow(2, WID))
		return;
	
	if(WID == WEAPON_LASER && m_GameExp.m_Weapons & (int)pow(2, WEAPON_FREEZER))
		return;
	if(WID == WEAPON_FREEZER)
	{
		m_GameExp.m_Weapons &= ~(int)pow(2, WEAPON_LASER);
		if(m_pCharacter)
			m_pCharacter->m_aWeapons[WEAPON_LASER].m_Got = false;
	}
	
	m_GameExp.m_Weapons |= (int)pow(2, WID);
	
	if(m_pCharacter)
	{
		m_pCharacter->m_aWeapons[WID].m_Got = true;
		
		if(WID == WEAPON_HAMMER || WID == WEAPON_KAMIKAZE || WID == WEAPON_FREEZER)
			m_pCharacter->m_aWeapons[WID].m_Ammo = -1;
		else
			m_pCharacter->m_aWeapons[WID].m_Ammo = g_pData->m_Weapons.m_aId[WID].m_Maxammo;
	}
}
