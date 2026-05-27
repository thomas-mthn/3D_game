#ifndef STAFF_H
#define STAFF_H

#include "vec3.h"
#include "string.h"

structure(Voxel);
structure(Staff);

#define SPELL_LIST \
    X(BOLT) X(ORB) X(BOMB) X(ADJ_DAMAGE) X(ADJ_SPEED) \
    X(ADJ_DOUBLER)

typedef enum{
#define X(name) SPELL_##name,
    SPELL_LIST
#undef X
    SPELL_ECOUNT
} SpellType;

structure(SpellStatic){
	bool adjective;
	real cost;
	real delay;
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

structure(Staff){
	Voxel* model;
	real reload;
	real delay;
	int capacity;
	real mana_generation;
	real mana_max;
    int recoil;
	InventorySlot spell_array[0x10];
};

extern Staff g_equipped;
extern real g_mana;
extern bool g_equipped_staff;
extern int g_spell_index;
extern SpellStatic g_spell_static[];
extern String g_spell_names[];
extern unsigned g_shoot_timestamp;
extern unsigned g_delay_timestamp;

void staffGenerate(Vec3 position);
void staffSkip(void);
void staffFire(void);

#endif
