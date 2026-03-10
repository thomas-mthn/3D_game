#include "w_audio_import.h"
#include "w_audio.h"

#include "../tmath.h"

//I hate this code

static IAudioClient* g_audio_client;
static IAudioRenderClient* g_render_client;

static GU_ID IID_IAudioClient        = {0x1CB9AD4C,0xDBFA,0x4C32,{0xB1,0x78,0xC2,0xF5,0x68,0xA7,0x03,0xB2}};
static GU_ID IID_IMMDeviceEnumerator = {0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};
static GU_ID IID_IAudioRenderClient  = {0xF294ACFC,0x3146,0x4483,{0xA7,0xBF,0xAD,0xDC,0xA7,0xC2,0x60,0xE2}};
static GU_ID IID_MMDeviceEnumerator  = {0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};

#define AUDIO_SAMPLE_RATE 48000

void audioInit(void){
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
}

uint16 g_sound_buffer_ptr;
SoundSample g_sound_buffer[0x10000];

void audioDeviceBufferUpdate(void){
    return;
    if(!g_audio_client)
        return;
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
    for(int i = 0;i < frames;i++)
        g_sound_buffer[(uint16)(g_sound_buffer_ptr + i)] = (SoundSample){0,0};
    g_sound_buffer_ptr += frames;
}