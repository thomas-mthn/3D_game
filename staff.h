#ifndef STAFF_H
#define STAFF_H

#include "inventory.h"
#include "vec3.h"

structure(Voxel);

enumeration(SpellType){
	SPELL_BOLT,
	SPELL_ORB,
	SPELL_BOMB,
	SPELL_ADJ_DAMAGE,
	SPELL_ADJ_SPEED,
	SPELL_ADJ_DOUBLER,
	SPELL_ECOUNT,
};

structure(SpellStatic){
	bool adjective;
	int cost;
	int delay;
};

structure(Staff){
	Voxel* model;
	int reload;
	int delay;
	int capacity;
	int mana_generation;
	int mana_max;
	InventorySlot spell_array[0x10];
};

extern Staff g_equipped;
extern int g_mana;
extern bool g_equipped_staff;
extern int g_attack_animation;
extern int g_spell_index;
extern SpellStatic g_spell_static[];

void staffGenerate(Vec3 position);
void staffSkip(void);
void staffFire(void);

#endif