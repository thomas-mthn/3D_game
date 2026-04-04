#include "audio.h"
#include "octree.h"

#if !defined(__wasm__) && !defined(__linux__)
#include "win32/w_audio.h"
#endif

typedef enum {
	AUDIO_CHANNEL_LEFT,
	AUDIO_CHANNEL_RIGHT,
} AudioChannel;

static void audioWriteBuffer(int index,int value,AudioChannel channel){
#if !defined(__wasm__) && !defined(__linux__)
	if(channel == AUDIO_CHANNEL_LEFT)
		g_sound_buffer[(uint16)(g_sound_buffer_ptr + index)].left += value;
	else
		g_sound_buffer[(uint16)(g_sound_buffer_ptr + index)].right += value;
#endif
}

void audioPlay(Vec3 position,AudioType type){
	switch(type){
		case AUDIO_FOOTSTEP:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR(tSin(j * FIXED_ONE / wave_size) * 64,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR(tSin(j * FIXED_ONE / wave_size) * 64,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
		case AUDIO_JUMP:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
		case AUDIO_LAND:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
		case AUDIO_CHEST_OPEN:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
		case AUDIO_ITEM_GAINED:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
		case AUDIO_SHOOT:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
		case AUDIO_EXPLOSION:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR((tRnd() % FIXED_ONE - FIXED_ONE / 2) * 32,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
		default:{
			int wave_size = tRnd() % 0x80 + 0x100;
			for(int j = 0;j < 0x1000;j++){
				audioWriteBuffer(j,fixedMulR(tSin(j * FIXED_ONE / wave_size) * 64,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_LEFT);
				audioWriteBuffer(j,fixedMulR(tSin(j * FIXED_ONE / wave_size) * 64,FIXED_ONE * (0x800 - tAbs(j - 0x800)) / 0x800),AUDIO_CHANNEL_RIGHT);
			}
		} break;
	}
}
