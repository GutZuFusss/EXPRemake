#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

enum CTurretTypes
{
	TURRET_TYPE_LASER=0,
	TURRET_TYPE_GUN
};

enum CDoorTypes
{
	DOOR_TYPE_VERTICAL=0,
	DOOR_TYPE_HORIZONTAL
};

struct CExplorerEntity
{
	bool m_Used;
	vec2 m_Pos;
	CExplorerEntity() { m_Used = false; }
};

struct CTurret : CExplorerEntity
{
	int m_Type;
	bool m_Dead;
	int m_Life;
	float m_Timer;
	float m_TimerRespawn;
	int m_DamageTaken;
	float m_DamageTakenTick;
	bool m_Frozen;
	float m_FrozenTimer;
};

struct CMine : CExplorerEntity
{
	bool m_Dead;
	float m_TimerRespawn;
};

struct CTrap : CExplorerEntity
{
	float m_Timer;
};

struct CDoor : CExplorerEntity
{
	int m_Type;
	class CLaserDoor * m_Laser;
	bool m_CreateLaser;
};

enum
{
	MAX_TURRETS=256,
	MAX_MINES=256,
	MAX_TRAPS=256,
	MAX_DOORS=128
};

#endif
