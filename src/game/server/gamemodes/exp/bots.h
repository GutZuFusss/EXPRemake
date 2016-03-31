#ifndef BOTS_H
#define BOTS_H

enum
{
	BOT_LEVELS=3,
	MAX_BOT_SPAWNS=256 //per level
};

struct CBotSpawn
{
	vec2 m_Pos;
	int m_Level;
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
