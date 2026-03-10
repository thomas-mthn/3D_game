#ifndef WIN32_AUDIO_H
#define WIN32_AUDIO_H

#include "../langext.h"

structure(SoundSample){
	int left;
	int right;
};

void audioInit(void);
void audioDeviceBufferUpdate(void);

extern uint16 g_sound_buffer_ptr;
extern SoundSample g_sound_buffer[];

#endif