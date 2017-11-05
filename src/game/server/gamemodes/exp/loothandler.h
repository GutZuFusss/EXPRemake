#ifndef LOOTHANDLER_H
#define LOOTHANDLER_H

#include "../../../../base/vmath.h"
#include "../../gamecontext.h"
#include "../../entities/pickup.h"
#include <map>
#include <tuple>


class CLootHandler {

public:
	static void HandleLoot(CGameWorld *pGameWorld, vec2 Pos, int BotType);
	static void DropWeapon(CGameWorld *pGameWorld, vec2 Pos, int WeaponType);
	static void DropItem(CGameWorld *pGameWorld, vec2 Pos, int ItemType);

private:
	enum {
		LOOT_HEALTH,
		LOOT_ARMOR,
		LOOT_HEALTHPOTION,
		LOOT_GUN,
		LOOT_GRENADE,
		LOOT_SHOTGUN,
		LOOT_LASER,
		LOOT_NINJA,
		NUM_LOOT
	};

	//chance that something drops at all in %
	static const int OverallDropChance = 100;//35;
	static const int DefaultLootId = 0;

	static void DropRandomLoot(CGameWorld *pGameWorld, vec2 Pos, int BotType);
	static int GetRandomLoot(std::map<int, float> Droprates);

	/* the loot enum is made up from two groups (health, shields, weapons) and the weapon_subtypes.
	we have to make sure we return the corresponding numbers. This is a hardcoded lookup table.
	Result contains pickup_type and subtype*/
	const static std::pair<int, int> LootToPickup(const int LootId);
	static void DropLoot(CGameWorld *pGameWorld, vec2 Pos, std::pair<int, int> Type);
	


	
};



#endif