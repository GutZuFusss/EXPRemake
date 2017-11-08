#ifndef BOTS_H
#define BOTS_H


const int MAX_BOT_SPAWNS = 256;

enum BOTTYPES
{
	BOTTYPE_HAMMER,
	BOTTYPE_PISTOL,
	BOTTYPE_NINJA,
	BOTTYPE_KAMIKAZE,
	BOTTYPE_SHOTGUN,
	BOTTYPE_GRENADE,
	BOTTYPE_LASER,
	BOTTYPE_THOR,
	BOTTYPE_FLAGBEARER,
	BOTTYPE_ENDBOSS,
	NUM_BOTTYPES
};

struct CBotSpawn
{
	vec2 m_Pos;
	int m_BotType;
	bool m_Spawned;
	int m_RespawnTimer;
};

struct CBoss
{
	bool m_Exist;
	CBotSpawn m_Spawn;
	int m_ClientID;
	
	bool m_ShieldActive;
	int m_ShieldHealth;
	float m_ShieldTimer;
	float m_RegenTimer;
	
	float m_FreezerTimer;
	
	class CPickup *m_apShieldIcons[3];
};
#endif
