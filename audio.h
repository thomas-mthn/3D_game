#ifndef AUDIO_H
#define AUDIO_H

#include "vec3.h"

enumeration(AudioType){
	AUDIO_FOOTSTEP,
	AUDIO_JUMP,
	AUDIO_LAND,
	AUDIO_CHEST_OPEN,
	AUDIO_ITEM_GAINED,
	AUDIO_SHOOT,
	AUDIO_PUNCH,
	AUDIO_PUNCH_HIT,
	AUDIO_EXPLOSION,
};

void audioPlay(Vec3 position,AudioType type);

#endif