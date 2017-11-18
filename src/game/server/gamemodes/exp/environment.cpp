#include <engine/shared/config.h>

#include <game/collision.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>
#include <game/server/entities/pickup.h>

#include "exp.h"

void CGameControllerEXP::TickEnvironment() {
	TickPlayerRelatedEnvironment();
	TickPlayerUnrelatedEnvironment();
}

void CGameControllerEXP::TickPlayerRelatedEnvironment() {
	for each (CPlayer* player in GameServer()->m_apPlayers) {
		if (player) {
			TickTeleport(player);
			TickWeaponStrip(player);
			TickZones(player);
		}
	}
}

void CGameControllerEXP::TickPlayerUnrelatedEnvironment() {
	TickMines();
	TickTraps();
	TickTurrets();
}

void CGameControllerEXP::TickTeleport(CPlayer* player) {
	CCharacter* character = player->GetCharacter();
	if (character) {
		vec2 charPos = character->GetPos();
		vec2 Dest = GameServer()->Collision()->Teleport(charPos.x, charPos.y);
		if (Dest.x >= 0 && Dest.y >= 0) {
			character->m_Core.m_Pos = Dest;
			character->m_Core.m_Vel = vec2(0, 0);
		}
	}
}

void CGameControllerEXP::TickWeaponStrip(CPlayer* player) {
	if (player->IsBot()) return;

	CCharacter* character = player->GetCharacter();
	if (character) {
		vec2 pos = character->GetPos();
		bool isOverlapping = GameServer()->Collision()->GetCollisionAt(pos.x, pos.y) & CCollision::COFLAG_WEAPONSTRIP;
		if (isOverlapping) {
			player->RemovePermaWeapons();
		}
	}
}

void CGameControllerEXP::TickZones(CPlayer* player) {
	CCharacter* character = player->GetCharacter();
	if (character) {
		vec2 charPos = character->GetPos();
		int collision = GameServer()->Collision()->GetCollisionAt(charPos.x, charPos.y);
		if (collision & CCollision::COLFLAG_HEALING) {
			TickHealingZone(character, player);
		}
		else if (collision & CCollision::COLFLAG_POISON) {
			TickPoisonZone(character, player);
		}
	}
}

void CGameControllerEXP::TickHealingZone(CCharacter* character, CPlayer* player) {
	bool isTicking = Server()->Tick() > player->m_GameExp.m_RegenTimer;
	if (isTicking) {
		if (character->m_Health < 10) {
			character->m_Health++;
		}
		else if (character->m_Armor < 10) {
			character->m_Armor++;
		}
		player->m_GameExp.m_RegenTimer = Server()->Tick() + (GameServer()->Tuning()->m_RegenTimer * Server()->TickSpeed());
	}
}

void CGameControllerEXP::TickPoisonZone(CCharacter* character, CPlayer* player) {
	bool isTicking = Server()->Tick() > player->m_GameExp.m_PoisonTimer;
	if (isTicking) {
		character->TakeDamage(vec2(0, 0), 1, -1, WEAPON_WORLD);
		player->m_GameExp.m_PoisonTimer = Server()->Tick() + (GameServer()->Tuning()->m_PoisonTimer * Server()->TickSpeed());
	}
}

//todo refactor
void CGameControllerEXP::TickMines() {
	for (int m = 0; m < m_CurMine; m++)
	{
		if (!m_aMines[m].m_Used)
			continue;

		if (m_aMines[m].m_Dead)
		{
			if (Server()->Tick() > m_aMines[m].m_TimerRespawn + GameServer()->Tuning()->m_RespawnTimer*Server()->TickSpeed())
				BuildMine(m);
		}
		else
		{
			//create tic-tic
			CCharacter *pClosest = GameServer()->m_World.ClosestCharacter(m_aMines[m].m_Pos, 400, NULL, true);
			if (pClosest)
			{
				int Mod = (int)(distance(pClosest->GetPos(), m_aMines[m].m_Pos) / 8);
				if (Mod == 0 || Server()->Tick() % Mod == 0)
					GameServer()->CreateSound(m_aMines[m].m_Pos, SOUND_HOOK_ATTACH_GROUND);
			}

			//emote close players
			CCharacter *apCloseChars[MAX_CLIENTS];
			int n = GameServer()->m_World.FindEntities(m_aMines[m].m_Pos, 300.0f, (CEntity**)apCloseChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < n; i++)
			{
				if (!apCloseChars[i] || apCloseChars[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aMines[m].m_Pos, apCloseChars[i]->GetPos(), NULL, NULL, false))
					continue;
				apCloseChars[i]->m_EmoteType = EMOTE_SURPRISE;
				apCloseChars[i]->m_EmoteStop = Server()->Tick() + 1 * Server()->TickSpeed();
				if (Server()->Tick() > apCloseChars[i]->GetPlayer()->m_LastEmote + Server()->TickSpeed() * 2)
				{
					GameServer()->SendEmoticon(apCloseChars[i]->GetPlayer()->GetCID(), EMOTICON_EXCLAMATION); //!
					apCloseChars[i]->GetPlayer()->m_LastEmote = Server()->Tick();
				}
			}

			CCharacter *apCloseCharacters[MAX_CLIENTS];
			int num = GameServer()->m_World.FindEntities(m_aMines[m].m_Pos, 16.0f, (CEntity**)apCloseCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < num; i++)
			{
				if (!apCloseCharacters[i] || apCloseCharacters[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aMines[m].m_Pos, apCloseCharacters[i]->GetPos(), NULL, NULL, false))
					continue;
				DestroyMine(m);
				break;
			}
		}
	}
}

//todo refactor
void CGameControllerEXP::TickTraps() {	
	for (int t = 0; t < m_CurTrap; t++) {
		CTrap *trap = &m_aTraps[t];
		bool isOnCooldown = Server()->Tick() < trap->m_Timer;
		if (!trap->m_Used || isOnCooldown) {
			continue;
		}

		float maxRange = 600.0f;
		vec2 trapPos = trap->m_Pos;
		CCharacter* closestChars[MAX_CLIENTS];
		int amountPlayersCloseBy = GameServer()->m_World.FindEntities(trapPos, maxRange, (CEntity**)closestChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

		for (int i = 0; i < amountPlayersCloseBy; i++) {
			CCharacter* character = closestChars[i];
			vec2 charPos = character->GetPos();

			bool isBot = character->GetPlayer()->IsBot();
			bool hasNoLineOfSight = GameServer()->Collision()->IntersectLine(trapPos, charPos, NULL, NULL, false);

			if (isBot || hasNoLineOfSight) {
				continue;
			}
			
			//todo built traps that shoot up, left, right?
			bool characterBelowTrap = charPos.y > trapPos.y;
			bool characterInHorizontalRange = (charPos.x > trapPos.x - 64) && (charPos.x < trapPos.x + 64);

			if (characterBelowTrap && characterInHorizontalRange) {
				vec2 dir = vec2(0, 1);
				int lifeSpan = (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime);
				int dmg = 5;
				bool isExplosive = true;
				new CProjectile(&GameServer()->m_World,	WEAPON_GRENADE,	-1, trapPos, dir, lifeSpan,	dmg, isExplosive, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);
				trap->m_Timer = Server()->Tick() + g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Firedelay * Server()->TickSpeed() / 1000.0f;
				GameServer()->CreateSound(trapPos, SOUND_GRENADE_FIRE);
				break;
			}
		}
	}
}


//todo refactor
void CGameControllerEXP::TickTurrets() {
	TickLaserTurrets();
	TickGunTurrets();

	for (int t = 0; t < m_CurTurret; t++)
	{
		if (!m_aTurrets[t].m_Used)
			continue;

		if (m_aTurrets[t].m_Dead)
		{
			if (Server()->Tick() > m_aTurrets[t].m_TimerRespawn + GameServer()->Tuning()->m_RespawnTimer*Server()->TickSpeed())
				BuildTurret(t);
			continue;
		}

		if (m_aTurrets[t].m_Frozen)
		{
			if (Server()->Tick() > m_aTurrets[t].m_FrozenTimer)
				m_aTurrets[t].m_Frozen = false;
		}

		// LASER TURRETS
		if (m_aTurrets[t].m_Type == TURRET_TYPE_LASER)
		{
			new CLaser(&GameServer()->m_World, m_aTurrets[t].m_Pos, m_aTurrets[t].m_Pos, 0.0f, -1, true, false);

			if (Server()->Tick() < m_aTurrets[t].m_Timer || m_aTurrets[t].m_Frozen)
				continue;

			CCharacter *apCloseChars[MAX_CLIENTS];
			int num = GameServer()->m_World.FindEntities(m_aTurrets[t].m_Pos, GameServer()->Tuning()->m_TurretLaserRadius, (CEntity**)apCloseChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < num; i++)
			{
				if (!apCloseChars[i] || apCloseChars[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aTurrets[t].m_Pos, apCloseChars[i]->GetPos(), NULL, NULL, false))
					continue;

				vec2 Direction = normalize(apCloseChars[i]->GetPos() - m_aTurrets[t].m_Pos);
				new CLaser(&GameServer()->m_World, m_aTurrets[t].m_Pos, Direction, GameServer()->Tuning()->m_TurretLaserRadius, -1, true, false);
				m_aTurrets[t].m_Timer = Server()->Tick() + GameServer()->Tuning()->m_TurretReload*Server()->TickSpeed();
				GameServer()->CreateSound(m_aTurrets[t].m_Pos, SOUND_RIFLE_FIRE);

				break;
			}
		}

		// GUN TURRETS
		else if (m_aTurrets[t].m_Type == TURRET_TYPE_GUN)
		{
			if (Server()->Tick() < m_aTurrets[t].m_Timer || m_aTurrets[t].m_Frozen)
				continue;

			CCharacter *apCloseChars[MAX_CLIENTS];
			int num = GameServer()->m_World.FindEntities(m_aTurrets[t].m_Pos, 1000, (CEntity**)apCloseChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < num; i++)
			{
				if (!apCloseChars[i] || apCloseChars[i]->GetPlayer()->IsBot() || GameServer()->Collision()->IntersectLine(m_aTurrets[t].m_Pos, apCloseChars[i]->GetPos(), NULL, NULL, false))
					continue;

				vec2 Direction = normalize(apCloseChars[i]->GetPos() - m_aTurrets[t].m_Pos);
				new CProjectile(&GameServer()->m_World, WEAPON_GUN, -1, m_aTurrets[t].m_Pos, Direction, (int)(Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime), 1, 0, 0, -1, WEAPON_GUN);
				m_aTurrets[t].m_Timer = Server()->Tick() + GameServer()->Tuning()->m_TurretReload*Server()->TickSpeed();
				GameServer()->CreateSound(m_aTurrets[t].m_Pos, SOUND_GUN_FIRE);

				break;
			}
		}
	}
}


void CGameControllerEXP::TickLaserTurrets() {

}

void CGameControllerEXP::TickGunTurrets() {

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
	str_format(aBuf, sizeof(aBuf), "Turret [Health: %i]", m_aTurrets[t].m_Life);
	GameServer()->SendBroadcast(aBuf, Owner);
}

void CGameControllerEXP::DestroyTurret(int t, int Killer)
{
	m_aTurrets[t].m_Dead = true;
	m_aTurrets[t].m_Life = 0;
	m_aTurrets[t].m_Timer = 0.0f;
	m_aTurrets[t].m_TimerRespawn = Server()->Tick();
	
	GameServer()->m_apPlayers[Killer]->m_Score++;
	
	if(Server()->Tick()%100 < 75)
	{
		CPickup *p = new CPickup(&GameServer()->m_World, 0, 0);//, m_aTurrets[t].m_Pos);
		p->CreateRandomFromTurret(m_aTurrets[t].m_Type, m_aTurrets[t].m_Pos);
	}
	
	GameServer()->CreateDeath(m_aTurrets[t].m_Pos, g_Config.m_SvMaxClients);
}

void CGameControllerEXP::FreezeTurret(int t)
{
	m_aTurrets[t].m_Frozen = true;
	m_aTurrets[t].m_FrozenTimer = Server()->Tick() + 5.0f*Server()->TickSpeed();
}

void CGameControllerEXP::BuildMine(int m)
{
	m_aMines[m].m_Dead = false;
	m_aMines[m].m_TimerRespawn = 0.0f;
}

void CGameControllerEXP::DestroyMine(int m)
{
	m_aMines[m].m_Dead = true;
	m_aMines[m].m_TimerRespawn = Server()->Tick();
	
	GameServer()->CreateExplosion(m_aMines[m].m_Pos, -1, WEAPON_WORLD, false);
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
