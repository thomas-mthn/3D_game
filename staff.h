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
	int cost;
	int delay;
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
	int reload;
	int delay;
	int capacity;
	int mana_generation;
	int mana_max;
    int recoil;
	InventorySlot spell_array[0x10];
};

extern Staff g_equipped;
extern int g_mana;
extern bool g_equipped_staff;
extern int g_attack_animation;
extern int g_spell_index;
extern SpellStatic g_spell_static[];
extern String g_spell_names[];

void staffGenerate(Vec3 position);
void staffSkip(void);
void staffFire(void);

#endif
