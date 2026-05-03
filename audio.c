#include "audio.h"
#include "octree.h"
#include "library.h"
#include "console.h"

#ifdef __linux__

static void* pcm_handle;

#endif

#if !defined(__wasm__) && !defined(__linux__)
#include "win32/w_audio_import.h"

static IAudioClient* g_audio_client;
static IAudioRenderClient* g_render_client;

static GU_ID IID_IAudioClient        = {0x1CB9AD4C,0xDBFA,0x4C32,{0xB1,0x78,0xC2,0xF5,0x68,0xA7,0x03,0xB2}};
static GU_ID IID_IMMDeviceEnumerator = {0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};
static GU_ID IID_IAudioRenderClient  = {0xF294ACFC,0x3146,0x4483,{0xA7,0xBF,0xAD,0xDC,0xA7,0xC2,0x60,0xE2}};
static GU_ID IID_MMDeviceEnumerator  = {0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};

#endif

#define AUDIO_SAMPLE_RATE 48000

typedef enum {
	AUDIO_CHANNEL_LEFT,
	AUDIO_CHANNEL_RIGHT,
} AudioChannel;

structure(SoundSample){
	int left;
	int right;
};

uint16 g_sound_buffer_ptr;
SoundSample g_sound_buffer[0x10000];

static void* lib;

typedef enum{
    SND_PCM_FORMAT_S8,
	SND_PCM_FORMAT_U8,
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_U16_LE,
	SND_PCM_FORMAT_U16_BE,
	SND_PCM_FORMAT_S24_LE,
	SND_PCM_FORMAT_S24_BE,
	SND_PCM_FORMAT_U24_LE,
	SND_PCM_FORMAT_U24_BE,
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_S32_BE,
	SND_PCM_FORMAT_U32_LE,
	SND_PCM_FORMAT_U32_BE,
	SND_PCM_FORMAT_FLOAT_LE,
	SND_PCM_FORMAT_FLOAT_BE,
	SND_PCM_FORMAT_FLOAT64_LE,
	SND_PCM_FORMAT_FLOAT64_BE,
	SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
	SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
} FormatPCM;

typedef enum {
	SND_PCM_ACCESS_MMAP_INTERLEAVED = 0,
	SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
	SND_PCM_ACCESS_MMAP_COMPLEX,
	SND_PCM_ACCESS_RW_INTERLEAVED,
	SND_PCM_ACCESS_RW_NONINTERLEAVED,
	SND_PCM_ACCESS_LAST = SND_PCM_ACCESS_RW_NONINTERLEAVED
} AccessPCM;

typedef enum _snd_pcm_stream {
	SND_PCM_STREAM_PLAYBACK = 0,
	SND_PCM_STREAM_CAPTURE,
	SND_PCM_STREAM_LAST = SND_PCM_STREAM_CAPTURE
} StreamPCM;


static int (*sndPcmOpen)(void** handle,char* name,StreamPCM stream,int mode);
static int (*sndPcmSetParams)(void* handle,FormatPCM format,AccessPCM access,unsigned channels,unsigned rate,int soft_resample,unsigned latency);
static int (*sndPcmWritei)(void* handle,void* buffer,unsigned long size);

void audioInit(void){
#ifdef __linux__
    lib = libraryLoad("libasound.so");

    struct{
        char* name;
        funcptr_t* fn_ptr;
    } functions[] =  {
        {.name = "snd_pcm_open",.fn_ptr = (funcptr_t*)&sndPcmOpen},
        {.name = "snd_pcm_set_params",.fn_ptr = (funcptr_t*)&sndPcmSetParams},
        {.name = "snd_pcm_writei",.fn_ptr = (funcptr_t*)&sndPcmWritei},
    };
    for(int i = countof(functions);i--;){
        *functions[i].fn_ptr = libraryFunctionLoad(lib,functions[i].name);
        if(!*functions[i].fn_ptr){
            print((String)STRING_LITERAL("function not found in audio library: "));
            printNL(stringMake(functions[i].name));
            goto unload;
        }
    }
    
    if(sndPcmOpen(&pcm_handle,"hw:2,0",SND_PCM_STREAM_PLAYBACK,0)){
        print((String)STRING_LITERAL("failed to open audio device"));
        goto unload;
    }
    
    sndPcmSetParams(
        pcm_handle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        2,
        AUDIO_SAMPLE_RATE,
        1,
        10000
    );

    return;
    
 unload:
    libraryUnload(lib);
    lib = 0;
#elif defined(_MSC_VER)
    IMMDeviceEnumerator* device_enumerator = 0;
    IMMDevice* audio_device = 0;
    int hnsRequestedDuration = 0x10000;

    CoInitialize(0);

    CoCreateInstance(&IID_MMDeviceEnumerator,0,CLSCTX_ALL,&IID_IMMDeviceEnumerator,(void**)&device_enumerator);
    if(!device_enumerator)
        return;
    device_enumerator->vtable->GetDefaultAudioEndpoint(device_enumerator,eRender,eConsole,&audio_device);
    audio_device->vtable->Activate((void*)audio_device,&IID_IAudioClient,CLSCTX_ALL,0,(void**)&g_audio_client);

    static WaveFormatEx wave_formatn = {
        .format_tag = WAVE_FORMAT_PCM,
        .channels = 2,
        .samples_per_sec = AUDIO_SAMPLE_RATE,
        .bits_per_sample = 16,
        .block_align = 2 * 2,
        .avg_bytes_per_sec = AUDIO_SAMPLE_RATE * 2 * 2,
        .size = 0
    };

    g_audio_client->vtable->Initialize(g_audio_client,AUDCLNT_SHAREMODE_SHARED,0,hnsRequestedDuration,0,(WaveFormatEx*)&wave_formatn,0);
    g_audio_client->vtable->GetService(g_audio_client,&IID_IAudioRenderClient,(void**)&g_render_client);
    g_audio_client->vtable->Start(g_audio_client);
#endif
}

void audioDeviceBufferUpdate(void){
    if(!lib)
        return;
#ifdef __linux__
    int frames = 512;
    int16_t data[512 * 2];

    for(int i = 0; i < frames; i++){
        data[i*2+0] = tClamp(g_sound_buffer[(uint16_t)(g_sound_buffer_ptr + i)].left >> 8,SHRT_MIN,SHRT_MAX);
        data[i*2+1] = tClamp(g_sound_buffer[(uint16_t)(g_sound_buffer_ptr + i)].right >> 8,SHRT_MIN,SHRT_MAX);
    }
    sndPcmWritei(pcm_handle,data,frames);
#elif defined (_MSC_VER)
    unsigned buffer_size;
    unsigned padding;
    struct{
        int16 left;
        int16 right;
    }* data;
    g_audio_client->vtable->GetBufferSize(g_audio_client,&buffer_size);
    g_audio_client->vtable->GetCurrentPadding(g_audio_client,&padding);
    int frames = buffer_size - padding;
    g_render_client->vtable->GetBuffer((void*)g_render_client,frames,(uint8**)&data);
    for(int i = 0;i < frames;i++){
        data[i].left = tClamp(g_sound_buffer[(uint16)(g_sound_buffer_ptr + i)].left >> 8,SHRT_MIN,SHRT_MAX);
        data[i].right = tClamp(g_sound_buffer[(uint16)(g_sound_buffer_ptr + i)].right >> 8,SHRT_MIN,SHRT_MAX);
    }
    g_render_client->vtable->ReleaseBuffer((void*)g_render_client,frames,0);
#endif
    for(int i = 0;i < frames;i++)
        g_sound_buffer[(uint16_t)(g_sound_buffer_ptr + i)] = (SoundSample){0,0};

    g_sound_buffer_ptr += frames;
}

static void audioWriteBuffer(int index,int value,AudioChannel channel){
	if(channel == AUDIO_CHANNEL_LEFT)
		g_sound_buffer[(uint16)(g_sound_buffer_ptr + index)].left += value;
	else
		g_sound_buffer[(uint16)(g_sound_buffer_ptr + index)].right += value;
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
