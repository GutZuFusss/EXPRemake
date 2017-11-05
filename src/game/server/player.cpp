/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
//#include <generated/protocol.h>
#include <math.h>
#include <new>
#include <engine/shared/config.h>
#include "player.h"

#include <game/server/gamemodes/exp/bots.h>
#include <game/server/gamemodes/exp/environment.h>

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_RespawnTick = Server()->Tick();
	m_DieTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	m_pCharacter = 0;
	m_ClientID = ClientID;
	m_Team = GameServer()->m_pController->ClampTeam(Team);
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();

	m_ClosestFlag = 999999;
}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!IsBot() && !Server()->ClientIngame(m_ClientID))
		return;

	Server()->SetClientScore(m_ClientID, m_Score);

	// do latency stuff
	if(!IsBot())
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
	else
	{
		m_Latency.m_Avg = 420;
		m_Latency.m_Max = 420;
		m_Latency.m_Min = 420;
	}

	if(!GameServer()->m_World.m_Paused)
	{
		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x-m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y-m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				m_ViewPos = m_pCharacter->m_Pos;
			}
			else
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && m_RespawnTick <= Server()->Tick())
			TryRespawn();
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

	// update view pos for spectators
	if(m_Team == TEAM_SPECTATORS && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID])
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->m_ViewPos;
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS-g_Config.m_DbgDummies)
#endif
	if(!IsBot() && !Server()->ClientIngame(m_ClientID))
		return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, m_ClientID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, IsBot() ? GetMonsterName() : Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);
	pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	pPlayerInfo->m_Local = 0;
	pPlayerInfo->m_ClientID = m_ClientID;
	pPlayerInfo->m_Score = m_Score;
	pPlayerInfo->m_Team = m_Team;

	if(m_ClientID == SnappingClient)
		pPlayerInfo->m_Local = 1;

	if(m_ClientID == SnappingClient && m_Team == TEAM_SPECTATORS)
	{
		CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;

		pSpectatorInfo->m_SpectatorID = m_SpectatorID;
		pSpectatorInfo->m_X = m_ViewPos.x;
		pSpectatorInfo->m_Y = m_ViewPos.y;
	}
}

void CPlayer::OnDisconnect(const char *pReason)
{
	KillCharacter();

	if(Server()->ClientIngame(m_ClientID))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(m_ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(m_ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
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
		m_Spawning = true;

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
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
	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	// clamp the team
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	char aBuf[512];
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(m_ClientID), GameServer()->m_pController->GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	KillCharacter();

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;
	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
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
		LoadNewGame(SpawnPos);
	}
	else if(m_Team != -1)
	{
		LoadGame(((CGameControllerEXP*)GameServer()->m_pController)->m_aFlagsCP[m_GameExp.m_LastFlag-1]->GetPos(), m_GameExp.m_Time);

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
		str_copy(m_TeeInfos.m_SkinName, "brownbear", sizeof(m_TeeInfos.m_SkinName));
	}
	else if(m_BotLevel == 2)
	{
		str_copy(m_TeeInfos.m_SkinName, "x_ninja", sizeof(m_TeeInfos.m_SkinName));

		m_pCharacter->m_aWeapons[WEAPON_HAMMER].m_Got = false;
		m_pCharacter->m_aWeapons[WEAPON_GUN].m_Got = false;
		m_pCharacter->m_aWeapons[WEAPON_KAMIKAZE].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_KAMIKAZE].m_Ammo = -1;
		m_pCharacter->m_QueuedWeapon = WEAPON_KAMIKAZE;
		m_pCharacter->m_ActiveWeapon = WEAPON_KAMIKAZE;
	}
	else if(m_BotLevel == 3)
	{
		str_copy(m_TeeInfos.m_SkinName, "twinbop", sizeof(m_TeeInfos.m_SkinName));

		m_pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Got = true;
		m_pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Ammo = 10;
	}
	else if(m_BotLevel == 4)
	{
		str_copy(m_TeeInfos.m_SkinName, "bluekitty", sizeof(m_TeeInfos.m_SkinName));

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
	if(!m_pCharacter)
		return "R.I.P.";

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

void CPlayer::LoadNewGame(vec2 SpawnPos)
{
	LoadGame(SpawnPos, 0);
}

void CPlayer::LoadGame(vec2 SpawnPos, int Time)
{
	m_GameExp.m_Time = Time;
	m_Spawning = false;
	GameServer()->CreatePlayerSpawn(SpawnPos);
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(GameServer()->m_apPlayers[m_ClientID], SpawnPos);
	m_pCharacter->m_Armor = m_GameExp.m_ArmorMax;
	
	for (int i = 0; i < NUM_WEAPONS+2; i += 1) {
		bool gotWeapon = m_GameExp.m_PermaWeapons[i].m_Got;
		int ammo = m_GameExp.m_PermaWeapons[i].m_StartAmmo;
		m_pCharacter->m_aWeapons[i].m_Got = gotWeapon;
		m_pCharacter->m_aWeapons[i].m_Ammo = ammo;
	}
}

bool CPlayer::GiveWeaponPermanently(int Weapon, int PermaStartAmmo) {
	if (m_GameExp.m_PermaWeapons[Weapon].m_Got == false) {
		m_GameExp.m_PermaWeapons[Weapon].m_Got = true;
		m_GameExp.m_PermaWeapons[Weapon].m_StartAmmo = PermaStartAmmo;
		return true;
	}
	return false;
}

bool CPlayer::HasWeaponPermanently (int Weapon) {
	return m_GameExp.m_PermaWeapons[Weapon].m_Got;
}
