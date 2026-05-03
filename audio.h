#ifndef AUDIO_H
#define AUDIO_H

#include "vec3.h"

typedef enum{
	AUDIO_FOOTSTEP,
	AUDIO_JUMP,
	AUDIO_LAND,
	AUDIO_CHEST_OPEN,
	AUDIO_ITEM_GAINED,
	AUDIO_SHOOT,
	AUDIO_PUNCH,
	AUDIO_PUNCH_HIT,
	AUDIO_EXPLOSION,
} AudioType;

void audioInit(void);
void audioDeviceBufferUpdate(void);
void audioPlay(Vec3 position,AudioType type);

#endif
