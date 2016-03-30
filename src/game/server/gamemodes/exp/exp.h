/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_EXP_H
#define GAME_SERVER_GAMEMODES_EXP_H
#include <game/server/gamecontroller.h>
#include <game/server/entities/flag.h>
#include <engine/console.h>

#include "environment.h"
#include "bots.h"

enum
{
	ITEM_LIFE=0,
	ITEM_MINOR_POTION,
	ITEM_GREATER_POTION,
	NUM_ITEMS
};


struct CItems
{
	int m_Lives;
	int m_MinorPotions;
	int m_GreaterPotions;
};

enum
{
	MAX_CHECKPOINTS=32,
	FLAG_VISIBLE_RADIUS=850
};

class CGameControllerEXP : public IGameController
{
public:
	CGameControllerEXP(class CGameContext *pGameServer);
	virtual void Tick();
	virtual bool OnEntity(int Index, vec2 Pos);
	bool CheckCommand(int ClientID, int Team, const char *pMsg);

	int m_CurTurret;
	int m_CurFlag;
	int m_CurMine;
	int m_CurTrap;
	int m_CurDoor;
	CTurret m_aTurrets[MAX_TURRETS];
	CMine m_aMines[MAX_MINES];
	CTrap m_aTraps[MAX_TRAPS];
	CDoor m_aDoors[MAX_DOORS];

	void BuildTurret(int t);
	void HitTurret(int t, int Owner, int Dmg);
	void DestroyTurret(int t, int Killer);
	void FreezeTurret(int t);
	void BuildMine(int m);
	void DestroyMine(int m);
	void BuildTrap(int t);
	void BuildDoor(int d);

	void TickEnvironment();
	void TickBots();

	// bots functions
	int BotCanSpawn();
	void BotSpawn(CBotSpawn *pSpawn);
	void RemoveBot(int ID, bool Killed);

	// bots variables
	CBotSpawn m_aaBotSpawns[BOT_LEVELS][MAX_BOT_SPAWNS];
	int m_aNumBotSpawns[BOT_LEVELS];
	CBoss m_Boss;

	//flags
	CFlag *m_aFlagsCP[MAX_CHECKPOINTS];
	CFlag *m_FlagEnd;

	void UpdateGame(int ID);
	void StartClient(int ID);
	void StopClient(int ID);
	void RestartClient(int ID);

	const char *GetWeaponName(int WID);

	bool Use(int ClientID, const char *aCommand);

	void RegisterExpCommands();
	static void ConTeleflag(IConsole::IResult *pResult, void *pUserData);
	static void ConTeleport(IConsole::IResult *pResult, void *pUserData);
};
#endif
