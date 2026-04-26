#ifndef INVENTORY_H
#define INVENTORY_H

#include "langext.h"

structure(Staff);

enumeration(SpellType){
	SPELL_BOLT,
	SPELL_ORB,
	SPELL_BOMB,
	SPELL_ADJ_DAMAGE,
	SPELL_ADJ_SPEED,
	SPELL_ADJ_DOUBLER,
	SPELL_ECOUNT,
};

structure(InventorySlot){
	enum{
		INVENTORY_SPELL = 1,
		INVENTORY_STAFF,
	} type;
	union{
		SpellType spell_type;
		Staff* staff;
	};
};

#endif