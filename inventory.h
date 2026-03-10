#ifndef INVENTORY_H
#define INVENTORY_H

#include "langext.h"

enumeration(SpellType);
structure(Staff);

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