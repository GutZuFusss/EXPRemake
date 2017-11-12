/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <string.h> //strncmp
#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/entities/pickup.h>

#include "exp.h"

CGameControllerEXP::CGameControllerEXP(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pGameType = "EXP";
	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS;

	m_CurFlag = 0;
	m_CurTurret = 0;
	m_CurDoor = 0;
	m_CurMine = 0;
	m_CurTrap = 0;

	for(int i = 0; i < NUM_BOTTYPES; i++)
		m_aNumBotSpawns[i] = 0;
	
	for(int i = 0; i < MAX_CHECKPOINTS; i++)
		m_aFlagsCP[i] = NULL;
	m_FlagEnd = NULL;
	
	m_Boss.m_Exist = false;
	for(int i = 0; i < 3; i++)
		m_Boss.m_apShieldIcons[i] = NULL;

	// force config
	g_Config.m_SvMaxClients = 6;
	g_Config.m_SvScorelimit = 1;
	g_Config.m_SvTeamdamage = 0;
}

void CGameControllerEXP::Tick()
{
	IGameController::Tick();

	TickBots();
	TickEnvironment();

	for(int fi = 0; fi < m_CurFlag+1; fi++)
	{
		CFlag *f = NULL;
		if(fi == m_CurFlag)
			f = m_FlagEnd;
		else
			f = m_aFlagsCP[fi];
		if(!f) // if there isn't flag end
			continue;
		
		CCharacter *apCloseCharacters[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(f->GetPos(), CFlag::ms_PhysSize, (CEntity**)apCloseCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			if(!apCloseCharacters[i]->IsAlive() || apCloseCharacters[i]->GetPlayer()->IsBot() || apCloseCharacters[i]->GetPlayer()->GetTeam() == -1)
				continue;
			int id = apCloseCharacters[i]->GetPlayer()->GetCID();
			
			if(fi == m_CurFlag)
			{
				// END
				if(!m_Boss.m_Exist || GameServer()->m_apPlayers[id]->m_GameExp.m_BossKiller)
					StopClient(id);
			}
			else
			{
				// REGEN
				if(Server()->Tick() > apCloseCharacters[i]->GetPlayer()->m_GameExp.m_RegenTimer + GameServer()->Tuning()->m_RegenTimer*Server()->TickSpeed())
				{
					if(apCloseCharacters[i]->m_Health < 10) //regen health
					{
						apCloseCharacters[i]->m_Health++;
						apCloseCharacters[i]->GetPlayer()->m_GameExp.m_RegenTimer = Server()->Tick();
					}
					else // regen ammo
					{
						int WID = apCloseCharacters[i]->m_ActiveWeapon;
						if(apCloseCharacters[i]->m_aWeapons[WID].m_Ammo != -1)
						{
							int MaxAmmo = g_pData->m_Weapons.m_aId[WID].m_Maxammo;
							if(apCloseCharacters[i]->m_aWeapons[WID].m_Ammo < MaxAmmo)
							{
								apCloseCharacters[i]->m_aWeapons[WID].m_Ammo++;
								apCloseCharacters[i]->GetPlayer()->m_GameExp.m_RegenTimer = Server()->Tick();
							}
						}
					}
				}

				// SAVE
				if(apCloseCharacters[i]->GetPlayer()->m_GameExp.m_LastFlag != fi+1)
				{
					apCloseCharacters[i]->GetPlayer()->m_GameExp.m_LastFlag = fi+1;
					GameServer()->SendChatTarget(apCloseCharacters[i]->GetPlayer()->GetCID(), "Checkpoint reached.");
					//apCloseCharacters[i]->GetPlayer()->m_GameExp.m_SaveTimer = Server()->Tick() + 5.0f*Server()->TickSpeed();
				}
			}
		}
	}
}

bool CGameControllerEXP::OnEntity(int Index, vec2 Pos)
{
	if (IGameController::OnEntity(Index, Pos)) {
		return true;
	}

	switch (Index) {
	case ENTITY_SPAWN_BOT_HAMMER:
		OnBotEntity(BOTTYPE_HAMMER, Pos);
		break;
	case ENTITY_SPAWN_BOT_GUN:
		OnBotEntity(BOTTYPE_GUN, Pos);
		break;
	case ENTITY_SPAWN_BOT_KAMIKAZE:
		OnBotEntity(BOTTYPE_KAMIKAZE, Pos);
		break;
	case ENTITY_SPAWN_BOT_SHOTGUN:
		OnBotEntity(BOTTYPE_SHOTGUN, Pos);
		break;
	case ENTITY_SPAWN_BOT_ENDBOSS:
		if (m_Boss.m_Exist) {
			dbg_msg("exp", "there can't be 2 boss entities on one map");
			break;
		}
		OnBotEntity(BOTTYPE_ENDBOSS, Pos);
		dbg_msg("exp", "boss added");
		break;
	default:
		break;
	}

	
	if(Index == ENTITY_TURRET_LASER)
	{
		if(m_CurTurret < MAX_TURRETS)
		{
			dbg_msg("exp", "laser turret added (%d)", m_CurTurret);
			m_aTurrets[m_CurTurret].m_Used = true;
			m_aTurrets[m_CurTurret].m_Pos = Pos;
			m_aTurrets[m_CurTurret].m_Type = TURRET_TYPE_LASER;
			BuildTurret(m_CurTurret++);
		}
		else
			dbg_msg("exp", "can't create laser turret: too many turrets");
		return true;
	}
	else if(Index == ENTITY_TURRET_GUN)
	{
		if(m_CurTurret < MAX_TURRETS)
		{
			dbg_msg("exp", "gun turret added (%d)", m_CurTurret);
			m_aTurrets[m_CurTurret].m_Used = true;
			m_aTurrets[m_CurTurret].m_Pos = Pos;
			m_aTurrets[m_CurTurret].m_Type = TURRET_TYPE_GUN;
			BuildTurret(m_CurTurret++);
		}
		else
			dbg_msg("exp", "can't create gun turret: too many turrets");
		return true;
	}
	else if(Index == ENTITY_MINE)
	{
		if(m_CurMine < MAX_MINES)
		{
			dbg_msg("exp", "mine added (%d)", m_CurMine);
			m_aMines[m_CurMine].m_Used = true;
			m_aMines[m_CurMine].m_Pos = vec2(Pos.x, Pos.y+14);
			BuildMine(m_CurMine++);
		}
		else
			dbg_msg("exp", "can't create mine: too many mines");
		return true;
	}
	else if(Index == ENTITY_TRAP)
	{
		if(m_CurTrap < MAX_TRAPS)
		{
			dbg_msg("exp", "trap added (%d)", m_CurTrap);
			m_aTraps[m_CurTrap].m_Used = true;
			m_aTraps[m_CurTrap].m_Pos = vec2(Pos.x, Pos.y-14);
			BuildTrap(m_CurTrap++);
		}
		else
			dbg_msg("exp", "can't create trap: too many traps");
		return true;
	}
	else if(Index == ENTITY_DOOR_VERTICAL)
	{
		if(m_CurDoor < MAX_DOORS)
		{
			dbg_msg("exp", "vertical door added (%d)", m_CurDoor);
			m_aDoors[m_CurDoor].m_Used = true;
			m_aDoors[m_CurDoor].m_Pos = vec2(Pos.x, Pos.y-16);
			m_aDoors[m_CurDoor].m_Type = DOOR_TYPE_VERTICAL;
			BuildDoor(m_CurDoor++);
		}
		else
			dbg_msg("exp", "can't create vertical door: too many doors");
		return true;
	}
	else if(Index == ENTITY_DOOR_HORIZONTAL)
	{
		if(m_CurDoor < MAX_DOORS)
		{
			dbg_msg("exp", "horizontal door added (%d)", m_CurDoor);
			m_aDoors[m_CurDoor].m_Used = true;
			m_aDoors[m_CurDoor].m_Pos = vec2(Pos.x-16, Pos.y);
			m_aDoors[m_CurDoor].m_Type = DOOR_TYPE_HORIZONTAL;
			BuildDoor(m_CurDoor++);
		}
		else
			dbg_msg("exp", "can't create horizontal door: too many doors");
		return true;
	}
	else if(Index == ENTITY_FLAGSTAND_RED)
	{
		if(m_CurFlag < MAX_CHECKPOINTS)
		{
			dbg_msg("exp", "checkpoint added (%d)", m_CurFlag);
			CFlag *f = new CFlag(&GameServer()->m_World, 0, Pos);
			f->m_Pos = Pos;
			m_aFlagsCP[m_CurFlag++] = f;
			g_Config.m_SvScorelimit++;
		}
		else
			dbg_msg("exp", "can't create checkpoint: too many checkpoints");
		return true;
	}
	else if(Index == ENTITY_FLAGSTAND_BLUE)
	{
		dbg_msg("exp", "blue flag added");
		CFlag *f = new CFlag(&GameServer()->m_World, 1, Pos);
		f->m_Pos = Pos;
		m_FlagEnd = f;
		return true;
	}

	return false;
}

bool CGameControllerEXP::OnBotEntity(int BotType, vec2 pos) {
	dbg_msg("exp", "bot spawn level %d added (%d)", BotType, m_aNumBotSpawns[BotType]);
	m_aaBotSpawns[BotType][m_aNumBotSpawns[BotType]].m_Pos = pos;
	m_aaBotSpawns[BotType][m_aNumBotSpawns[BotType]].m_BotType = BotType;
	m_aaBotSpawns[BotType][m_aNumBotSpawns[BotType]].m_Spawned = false;
	m_aaBotSpawns[BotType][m_aNumBotSpawns[BotType]].m_RespawnTimer = Server()->Tick() - (GameServer()->Tuning()->m_RespawnTimer - 2)*Server()->TickSpeed();
	m_aNumBotSpawns[BotType]++;

	return false;
}

bool CGameControllerEXP::CheckCommand(int ClientID, int Team, const char *aMsg)
{
	if(!strncmp(aMsg, "/info", 5) || !strncmp(aMsg, "!info", 5) || !strncmp(aMsg, "/help", 5) || !strncmp(aMsg, "/about", 6))
	{
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		GameServer()->SendChatTarget(ClientID, "EXPlorer v2.0");
		GameServer()->SendChatTarget(ClientID, "Based on the EXPlorer mod by <xush'> and <Choupom>");
		GameServer()->SendChatTarget(ClientID, "Aim: explore, fight and eventually capture the blue flag");
		GameServer()->SendChatTarget(ClientID, "Kill monsters to earn /items");
		GameServer()->SendChatTarget(ClientID, "Say /cmdlist for the command list");
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		return true;
	}
	else if(!strncmp(aMsg, "/top5", 5))
	{
		GameServer()->Top5(g_Config.m_SvMap, ClientID);
		return true;
	}
	else if(!strncmp(aMsg, "/rank", 5))
	{
		GameServer()->Rank(g_Config.m_SvMap, Server()->ClientName(ClientID), ClientID);
		return true;
	}
	else if(!strncmp(aMsg, "/cmdlist", 8) || !strncmp(aMsg, "/cmd", 4))
	{
		GameServer()->SendChatTarget(ClientID, " ");
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		GameServer()->SendChatTarget(ClientID, "COMMAND LIST");
		GameServer()->SendChatTarget(ClientID, "");
		GameServer()->SendChatTarget(ClientID, "'/info': Get info about the modification.");
		GameServer()->SendChatTarget(ClientID, "'/top5': View the top 5 players.");
		GameServer()->SendChatTarget(ClientID, "'/items': Get info about the items.");
		GameServer()->SendChatTarget(ClientID, "'/new': Restart the game.");
		GameServer()->SendChatTarget(ClientID, "'/bind': Learn how to bind a key to use an item.");
		GameServer()->SendChatTarget(ClientID, "'/game': Shows your weapons and kills.");
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		return true;
	}
	else if(!strncmp(aMsg, "/items", 6))
	{
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		GameServer()->SendChatTarget(ClientID, "ITEMS");
		GameServer()->SendChatTarget(ClientID, " ");
		GameServer()->SendChatTarget(ClientID, "Check out '/bind' to learn how to bind items.");
		GameServer()->SendChatTarget(ClientID, "Weapons: You keep it when you have it.");
		GameServer()->SendChatTarget(ClientID, "Potion: Use it to restore health.");
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		return true;
	}
	else if(!strncmp(aMsg, "/game", 5))
	{
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		GameServer()->SendChatTarget(ClientID, "GAME");
		GameServer()->SendChatTarget(ClientID, " ");
		
		char aBuf[256] = {0};
		if(GameServer()->m_apPlayers[ClientID]->m_GameExp.m_Weapons == 0)
			str_format(aBuf, sizeof(aBuf), "Weapons: none", aBuf);
		for(int i = 1; i < NUM_WEAPONS+2; i++)
		{
			if(GameServer()->m_apPlayers[ClientID]->m_GameExp.m_Weapons & (int)pow(2, i))
			{
				if(aBuf[0] == 0) str_format(aBuf, sizeof(aBuf), "Weapons: %s", GetWeaponName(i));
				else str_format(aBuf, sizeof(aBuf), "%s, %s", aBuf, GetWeaponName(i));
			}
		}
		GameServer()->SendChatTarget(ClientID, aBuf);
				
		str_format(aBuf, sizeof(aBuf), "Kills: %d", GameServer()->m_apPlayers[ClientID]->m_Score);
		GameServer()->SendChatTarget(ClientID, aBuf);
		
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		return true;
	}
	
	else if(!strncmp(aMsg, "/bind", 5))
	{
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		GameServer()->SendChatTarget(ClientID, "BIND DOCUMENTATION");
		GameServer()->SendChatTarget(ClientID, "");
		GameServer()->SendChatTarget(ClientID, "1) Open the Local Console (F1).");
		GameServer()->SendChatTarget(ClientID, "2) Type \"bind <key> say <item>\"");
		GameServer()->SendChatTarget(ClientID, "Replace <key> by the key you want to press.");
		GameServer()->SendChatTarget(ClientID, "Replace <item> by the item: potion.");
		GameServer()->SendChatTarget(ClientID, "Example: \"bind l say potion\"");
		GameServer()->SendChatTarget(ClientID, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		return true;
	}
	
	else if(!strncmp(aMsg, "/new", 4))
	{
		RestartClient(ClientID);
		return true;
	}
	
	else if(!strncmp(aMsg, "/potion", 7) || !strncmp(aMsg, "potion", 7))
	{
		Use(ClientID, "Potion");
		return true;
	}
	
	else if(!strncmp(aMsg, "/", 1))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Unknown command: '%s'", aMsg);
		GameServer()->SendChatTarget(ClientID, aBuf);
		return true;
	}
	else
		return false;
}

void CGameControllerEXP::StartClient(int ID)
{
	GameServer()->m_apPlayers[ID]->KillCharacter(WEAPON_GAME);
	CItems Items; Items.m_Potions = 0;
	GameServer()->m_apPlayers[ID]->LoadNewGame(m_aaSpawnPoints[1][0]);
	GameServer()->m_apPlayers[ID]->m_GameExp.m_EnterTick = Server()->Tick();
	//SendItems(ID);
}

void CGameControllerEXP::StopClient(int ID)
{
	UpdateGame(ID);
	int min = GameServer()->m_apPlayers[ID]->m_GameExp.m_Time / 60;
	int sec = GameServer()->m_apPlayers[ID]->m_GameExp.m_Time % 60;
	char buf[512];
	str_format(buf, sizeof(buf), "'%s' finished in %d minutes %d seconds with %d kills. Good Game!", Server()->ClientName(ID), min, sec, GameServer()->m_apPlayers[ID]->m_GameExp.m_Kills);
	GameServer()->SendChatTarget(-1, buf);

	GameServer()->SaveRank(g_Config.m_SvMap, Server()->ClientName(ID), GameServer()->m_apPlayers[ID]->m_GameExp.m_Time, GameServer()->m_apPlayers[ID]->m_GameExp.m_Kills);
	
	bool GotFreezer = false;
	if(GameServer()->m_apPlayers[ID]->m_GameExp.m_Weapons & (int)pow((int)2, (int)WEAPON_FREEZER))
		GotFreezer = true;
	
	RestartClient(ID);
	
	if(GotFreezer)
		GameServer()->m_apPlayers[ID]->GiveWeaponPermanently(WEAPON_FREEZER, -1);
}

void CGameControllerEXP::RestartClient(int ID)
{
	StartClient(ID);
	GameServer()->SendChatTarget(ID, "Game restarted.");
}

void CGameControllerEXP::UpdateGame(int ID)
{
	int DiffTick = Server()->Tick() - GameServer()->m_apPlayers[ID]->m_GameExp.m_EnterTick;
	GameServer()->m_apPlayers[ID]->m_GameExp.m_Time += (int) (DiffTick / Server()->TickSpeed());
	GameServer()->m_apPlayers[ID]->m_GameExp.m_EnterTick = Server()->Tick();
	GameServer()->m_apPlayers[ID]->m_GameExp.m_Kills = GameServer()->m_apPlayers[ID]->m_Score;
}

const char *CGameControllerEXP::GetWeaponName(int WID)
{
	if(WID == WEAPON_HAMMER)
		return "Hammer";
	else if(WID == WEAPON_GUN)
		return "Gun";
	else if(WID == WEAPON_SHOTGUN)
		return "Shotgun";
	else if(WID == WEAPON_GRENADE)
		return "Grenade";
	else if(WID == WEAPON_RIFLE)
		return "Rifle";
	else if(WID == WEAPON_NINJA)
		return "Ninja";
	else if(WID == WEAPON_KAMIKAZE)
		return "Kamikaze";
	else if(WID == WEAPON_FREEZER)
		return "Freezer";
	return "?";
}

bool CGameControllerEXP::Use(int ClientID, const char *aCommand)
{
	CPlayer *p = GameServer()->m_apPlayers[ClientID];
	
	if(str_find_nocase(aCommand, "Potion"))
	{
		if(p->GetCharacter())
		{
			if(p->m_GameExp.m_Items.m_Potions > 0)
			{
				if(p->GetCharacter()->m_Health < 10)
				{
					p->m_GameExp.m_Items.m_Potions--;
					p->GetCharacter()->m_Health = 10;
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "<Potion> used. You have %d <Potions> left.", p->m_GameExp.m_Items.m_Potions);
					GameServer()->SendChatTarget(ClientID, aBuf);
				}
				else
					GameServer()->SendChatTarget(ClientID, "You don't need to use that now!");
			}
			else
				GameServer()->SendChatTarget(ClientID, "You haven't got a <Potion>!");
		}
		return true;
	}
	
	return false;
}

void CGameContext::ConTeleport(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int CID2 = clamp(pResult->GetInteger(1), 0, (int)MAX_CLIENTS - 1);

	if (pSelf->m_apPlayers[CID1] && pSelf->m_apPlayers[CID2]) {
		CCharacter* pChr = pSelf->GetPlayerChar(CID1);
		if (pChr) {
			pChr->Teleport(pSelf->m_apPlayers[CID2]->m_ViewPos);
		}
	}
}

void CGameContext::ConTeleflag(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	CGameControllerEXP* expGC = (CGameControllerEXP*) pSelf->m_pController;
	int CID1 = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int FlagID = clamp(pResult->GetInteger(1), 0, (int)MAX_CHECKPOINTS - 1);

	CPlayer* player = pSelf->m_apPlayers[CID1];
	CFlag* flag = expGC->m_aFlagsCP[FlagID];

	if (player && flag) {
		CCharacter* pChr = pSelf->GetPlayerChar(CID1);
		if (pChr) {
			player->m_GameExp.m_LastFlag = FlagID;
			pChr->Teleport(flag->m_Pos);
		}
	}
}

