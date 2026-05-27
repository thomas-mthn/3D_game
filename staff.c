#include "staff.h"
#include "octree.h"
#include "memory.h"
#include "main.h"
#include "entity.h"

#include "platform/audio.h"

String g_spell_names[] = {
#define X(name) STRING_LITERAL(#name),
    SPELL_LIST
#undef X
};

unsigned g_shoot_timestamp;
unsigned g_delay_timestamp;

real g_mana;
bool g_equipped_staff;
Staff g_equipped;

SpellStatic g_spell_static[] = {
	[SPELL_ADJ_DAMAGE] = {
		.adjective = true
	},
	[SPELL_ADJ_DOUBLER] = {
		.adjective = true
	},
	[SPELL_ADJ_SPEED] = {
		.adjective = true
	},
	[SPELL_BOLT] = {
		.cost = REAL_UNIT * 0x10,
		.delay = 0x80,
	},
	[SPELL_ORB] = {
		.cost = REAL_UNIT * 0x40,
		.delay = 0x80,
	},
	[SPELL_BOMB] = {
		.cost = REAL_UNIT * 0x100,
		.delay = 0x80,
	},
};

void staffGenerate(Vec3 position){
	Voxel* staff_model = tMallocZero(sizeof *staff_model);
	int depth = 3;
	real size = 1 << depth;

	for(int i = 0;i < size;i++){
		voxelSet(staff_model,(Vec3i){size / 2,size / 2,i},depth,VOXEL_BLOCK);
		voxelSet(staff_model,(Vec3i){size / 2,size / 2 + 1,i},depth,VOXEL_BLOCK);
		voxelSet(staff_model,(Vec3i){size / 2 + 1,size / 2,i},depth,VOXEL_BLOCK);
		voxelSet(staff_model,(Vec3i){size / 2 + 1,size / 2 + 1,i},depth,VOXEL_BLOCK);
	}

	size = 1 << depth + 1; 

	voxelSet(staff_model,(Vec3i){size / 2,size / 2,size - 2},depth + 1,VOXEL_BLOCK_BLUE);
	voxelSet(staff_model,(Vec3i){size / 2,size / 2 + 1,size - 2},depth + 1,VOXEL_BLOCK_BLUE);
	voxelSet(staff_model,(Vec3i){size / 2 + 1,size / 2,size - 2},depth + 1,VOXEL_BLOCK_BLUE);
	voxelSet(staff_model,(Vec3i){size / 2 + 1,size / 2 + 1,size - 2},depth + 1,VOXEL_BLOCK_BLUE);

	voxelSet(staff_model,(Vec3i){size / 2,size / 2,size - 1},depth + 1,VOXEL_BLOCK_BLUE);
	voxelSet(staff_model,(Vec3i){size / 2,size / 2 + 1,size - 1},depth + 1,VOXEL_BLOCK_BLUE);
	voxelSet(staff_model,(Vec3i){size / 2 + 1,size / 2,size - 1},depth + 1,VOXEL_BLOCK_BLUE);
	voxelSet(staff_model,(Vec3i){size / 2 + 1,size / 2 + 1,size - 1},depth + 1,VOXEL_BLOCK_BLUE);

	Entity* staff = entityCreate(position,ENTITY_STAFF);
	staff->velocity.x += realRandom(FIXED_ONE / 8) - FIXED_ONE / 16;
	staff->velocity.y += realRandom(FIXED_ONE / 8) - FIXED_ONE / 16;
	staff->velocity.z +=  realRandom(FIXED_ONE / 8)+ FIXED_ONE / 8;
	staff->color_emit = (Vec3){realRandom(FIXED_ONE),realRandom(FIXED_ONE),realRandom(FIXED_ONE)};

	staff->staff = (Staff){
		.capacity = tRnd() % 4 + 1,
		.mana_generation = realRandom(REAL_UNIT),
		.mana_max = realRandom(REAL_UNIT * 0x400) + REAL_UNIT * 0x40,
		.delay = realRandom(FIXED_ONE / 256) + FIXED_ONE / 1024,
		.reload = realRandom(FIXED_ONE) + FIXED_ONE / 2,
        .recoil = realRandom(FIXED_ONE),
		.model = staff_model,
	};
	staff->staff.spell_array[0].spell_type = tRnd() % SPELL_ECOUNT;
	staff->staff.spell_array[0].spell_type = SPELL_ADJ_DOUBLER;
	staff->staff.spell_array[0].type = INVENTORY_SPELL;
	for(int i = 1;i < staff->staff.capacity;i++){
		staff->staff.spell_array[i].type = INVENTORY_SPELL;
		staff->staff.spell_array[i].spell_type = tRnd() % SPELL_ECOUNT;
	}
}

int g_spell_index;

void staffSkip(void){
	InventorySlot* spell_slot;
	int i = 0;
	struct{
		bool is_adjective;
		int index;
		int amount;
	} adj_table[] = {
		[SPELL_ADJ_DAMAGE] = {true,0},	
		[SPELL_ADJ_DOUBLER] = {true,1},	
		[SPELL_ADJ_SPEED] = {true,2},	
	};
	int cost = 0;
	for(;;){
		do
			spell_slot = g_equipped.spell_array + (g_spell_index + i) % g_equipped.capacity;
		while(i++ < 0x10 && spell_slot->type != INVENTORY_SPELL);

		cost += g_spell_static[spell_slot->spell_type].cost;

		if(spell_slot->spell_type < countof(adj_table) && adj_table[spell_slot->spell_type].is_adjective){
			continue;
		}
		break;
	}
		
	g_spell_index += i;

	if(spell_slot->type != INVENTORY_SPELL)
		return;

	g_mana -= cost / 4;
}

void staffFire(void){
	EntityType spell_entity[] = {
		[SPELL_BOLT] = ENTITY_BOLT,
		[SPELL_BOMB] = ENTITY_BOMB,
		[SPELL_ORB]  = ENTITY_ORB,
	};

	InventorySlot* spell_slot;
	int i = 0;
	
	struct{
		bool is_adjective;
		int index;
		int amount;
	} adj_table[] = {
		[SPELL_ADJ_DAMAGE] = {true,0},	
		[SPELL_ADJ_DOUBLER] = {true,1},	
		[SPELL_ADJ_SPEED] = {true,2},	
	};

	real cost = 0;
    real delay = 0;

	for(;;){
		do
			spell_slot = g_equipped.spell_array + (g_spell_index + i) % g_equipped.capacity;
		while(i++ < 0x10 && spell_slot->type != INVENTORY_SPELL);

        SpellStatic* spell_s = g_spell_static + spell_slot->spell_type;
        
		cost += spell_s->cost;
        delay += spell_s->delay;

		if(spell_slot->spell_type < countof(adj_table) && adj_table[spell_slot->spell_type].is_adjective){
			adj_table[spell_slot->spell_type].amount += 1;
			continue;
		}
		break;
	}
		
	g_spell_index += i;

	if(spell_slot->type != INVENTORY_SPELL || g_mana < cost || g_time.time < g_delay_timestamp)
		return;

    g_shoot_timestamp = g_time.time;
    g_delay_timestamp = g_time.time + realToInt(delay * 0x1000);

	g_mana -= cost;

	Entity* spell = entityCreate(g_surface.position,spell_entity[spell_slot->spell_type]);
	Vec3 direction = getLookDirection(g_surface.angle);
	direction = vec3Normalize(direction);
	spell->adj_speed  = adj_table[SPELL_ADJ_SPEED].amount;
	spell->adj_damage = adj_table[SPELL_ADJ_DAMAGE].amount;
	spell->velocity = direction;
	spell->velocity = vec3MulS(spell->velocity,FIXED_ONE * (adj_table[SPELL_ADJ_SPEED].amount + 1));
    spell->velocity = vec3MulS(spell->velocity,FIXED_ONE * 4);
    spell->parent = g_player.entity;

	audioPlay(g_surface.position,AUDIO_SHOOT);
}
