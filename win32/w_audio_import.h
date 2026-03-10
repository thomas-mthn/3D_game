#ifndef AUDIO_IMPORT
#define AUDIO_IMPORT

#if __GNUC__
#define DLLIMPORT(RETURN) __attribute__((dllimport,stdcall)) RETURN
#else
#define DLLIMPORT(RETURN) _declspec(dllimport) RETURN _stdcall
#endif

#include "../langext.h"

typedef struct {
    unsigned Data1;
    uint16 Data2;
    uint16 Data3;
    uint8  Data4[8];
} GU_ID;

typedef struct {
	uint16 format_tag;
	uint16 channels;
	unsigned samples_per_sec;
	unsigned avg_bytes_per_sec;
	uint16 block_align;
	uint16 bits_per_sample;
	uint16 size;
} WaveFormatEx;

typedef struct {
	WaveFormatEx format;
	union{
		uint16 valid_bits_per_sample;
		uint16 samples_per_block;
		uint16 reserved;
	};
	unsigned channel_mask;
	GU_ID sub_format;
} WaveFormatExtensible;

#define WAVE_FORMAT_PCM 0x0001

typedef enum {
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_SHAREMODE_EXCLUSIVE
} AudClntShareMode;

typedef enum {
    eConsole,
    eMultimedia,
    eCommunications,
    ERole_enum_count,
} ERole;

typedef enum {
    eRender,
    eCapture,
    eAll,
    EDataFlow_enum_count,
} EDataFlow;

struct IAudioClient;
struct IMMDeviceEnumerator;
struct IAudioClient;
struct IUnkwown;

typedef struct IUnknownVtbl{
    unsigned (_stdcall *QueryInterface)(struct IUnknown* This,GU_ID* riid,void** object);
    unsigned (_stdcall *AddRef)(struct IUnknown* This);
    unsigned (_stdcall *Release)(struct IUnknown* This);
} IUnknownVtbl;

typedef struct IUnknown{
    struct IUnknownVtbl* vtable;
} IUnknown;

#ifdef __cplusplus
extern "C"{
#endif
DLLIMPORT(int) CoInitialize(void* pvReserved);
DLLIMPORT(int) CoCreateInstance(GU_ID* rclsid,IUnknown* outer,unsigned cls_contect,GU_ID* riid,void* ppv);
#ifdef __cplusplus
}
#endif

typedef struct IAudioRenderClientVtbl{
    int (_stdcall *QueryInterface)(struct IAudioRenderClient * This,GU_ID* riid,void **object);
    unsigned (_stdcall *AddRef)(struct IAudioRenderClient * This);
    unsigned (_stdcall *Release)(struct IAudioRenderClient * This);
    int (_stdcall *GetBuffer)(struct IAudioRenderClient * This,unsigned n_frames_requested,uint8** data);
    int (_stdcall *ReleaseBuffer)(struct IAudioRenderClient * This,unsigned n_frames_written,unsigned flags);
} IAudioRenderClientVtbl;

typedef struct IAudioRenderClient{
    struct IAudioRenderClientVtbl *vtable;
}IAudioRenderClient;

typedef struct{
    GU_ID fmtid;
    unsigned pid;
} PropertyKey;

typedef struct IPropertyStoreVtbl{
    int (_stdcall *QueryInterface)(struct IPropertyStore* This,GU_ID* riid,void **object);
    unsigned (_stdcall *AddRef)(struct IPropertyStore* This);
    unsigned (_stdcall *Release)(struct IPropertyStore* This);
    int (_stdcall *GetCount)(struct IPropertyStore* This,unsigned* props);
    int (_stdcall *GetAt)(struct IPropertyStore* This,unsigned prop,PropertyKey *pkey);
    int (_stdcall *GetValue)(struct IPropertyStore* This,PropertyKey* key,void *pv);
    int (_stdcall *SetValue)(struct IPropertyStore* This,PropertyKey* key,void* propvar);
    int (_stdcall *Commit)(struct IPropertyStore* This);
} IPropertyStoreVtbl;

typedef struct IPropertyStore{
    struct IPropertyStoreVtbl *vtable;
}IPropertyStore;

typedef struct IMMDeviceVtbl {
    int (_stdcall *QueryInterface)(struct IMMDevice* This,GU_ID* riid,void **object);
    unsigned (_stdcall *AddRef)(struct IMMDevice* This);
    unsigned (_stdcall *Release)(struct IMMDevice* This);
    int (_stdcall *Activate)(
        struct IMMDevice* This,
        GU_ID* iid,
        unsigned dwClsCtx,
        void* pActivationParams,
        void** ppInterface
    );
    int (_stdcall *OpenPropertyStore)(struct IMMDevice* This,unsigned stgmAccess,IPropertyStore** ppProperties);
    int (_stdcall *GetId)(struct IMMDevice* This,uint16* ppstrId);
    int (_stdcall *GetState)(struct IMMDevice* This,unsigned* pdwState);
} IMMDeviceVtbl;

typedef struct IMMDevice{
    struct IMMDeviceVtbl *vtable;
} IMMDevice;

typedef struct IMMNotificationClientVtbl{
    int (_stdcall *QueryInterface)(struct IMMNotificationClient* This,GU_ID* riid,void **object);
    unsigned (_stdcall *AddRef)(struct IMMNotificationClient* This);
    unsigned (_stdcall *Release)(struct IMMNotificationClient* This);
    int (_stdcall *OnDeviceStateChanged)(struct IMMNotificationClient* This,uint16* device_id,unsigned nw_state);
    int (_stdcall *OnDeviceAdded)(struct IMMNotificationClient* This,uint16* device_id);
    int (_stdcall *OnDeviceRemoved)(struct IMMNotificationClient* This,uint16* device_id);
    int (_stdcall *OnDefaultDeviceChanged)(
        struct IMMNotificationClient * This,
        EDataFlow flow,
        ERole role,
        uint16* pwstrDefaultDeviceId
    );
    int (_stdcall *OnPropertyValueChanged)(struct IMMNotificationClient* This,uint16* device_id,PropertyKey key);
} IMMNotificationClientVtbl;

typedef struct IMMNotificationClient{
    struct IMMNotificationClientVtbl *vtable;
} IMMNotificationClient;

typedef struct IMMDeviceCollectionVtbl{
    int (_stdcall *QueryInterface)(struct IMMDeviceCollection* This,GU_ID* riid,void** object);
    unsigned (_stdcall *AddRef)(struct IMMDeviceCollection* This);
    unsigned (_stdcall *Release)(struct IMMDeviceCollection* This);
    int (_stdcall *GetCount)(struct IMMDeviceCollection* This,unsigned *devices);
    int (_stdcall *Item)(struct IMMDeviceCollection* This,unsigned n_device,IMMDevice** device);
} IMMDeviceCollectionVtbl;

typedef struct IMMDeviceCollection{
    struct IMMDeviceCollectionVtbl *vtable;
}IMMDeviceCollection;

typedef struct IMMDeviceEnumeratorVtbl{
    int ( _stdcall *QueryInterface )(struct IMMDeviceEnumerator* This,GU_ID* riid,void **object);
    unsigned (_stdcall *AddRef)(struct IMMDeviceEnumerator* This);
    unsigned (_stdcall *Release)(struct IMMDeviceEnumerator* This);
    int (_stdcall *EnumAudioEndpoints)(
        struct IMMDeviceEnumerator * This,
        EDataFlow dataFlow,
        unsigned dwStateMask,
        IMMDeviceCollection **ppDevices
    );
    int (_stdcall *GetDefaultAudioEndpoint)(
        struct IMMDeviceEnumerator* This,
        EDataFlow data_flow,
        ERole role,
        IMMDevice **endpoint
    );
    int (_stdcall *GetDevice)(struct IMMDeviceEnumerator* This,uint16* id,IMMDevice** device);
    int (_stdcall *RegisterEndpointNotificationCallback)(struct IMMDeviceEnumerator* This,IMMNotificationClient* client);
    int (_stdcall *UnregisterEndpointNotificationCallback)(struct IMMDeviceEnumerator* This,IMMNotificationClient* client
    );
} IMMDeviceEnumeratorVtbl;

typedef struct IMMDeviceEnumerator{
    struct IMMDeviceEnumeratorVtbl *vtable;
}IMMDeviceEnumerator;

typedef struct IAudioClientVtbl
{
    int (_stdcall *QueryInterface)( struct IAudioClient* This,GU_ID* riid,void** object);
    unsigned (_stdcall *AddRef)(struct IAudioClient* This);
    unsigned (_stdcall *Release)(struct IAudioClient* This);
    int (_stdcall *Initialize)(
        struct IAudioClient* This,
        AudClntShareMode share_mode,
        unsigned stream_flags,
        int64 buffer_duration,
        int64 periodicity,
        WaveFormatEx* format,
        GU_ID* audio_session_guid
    );
    int (_stdcall *GetBufferSize )(struct IAudioClient* This,unsigned* n_buffer_frames);
    int (_stdcall *GetStreamLatency)(struct IAudioClient* This,int64* latency);
    int (_stdcall *GetCurrentPadding)(struct IAudioClient* This,unsigned* n_padding_frames);
    int (_stdcall *IsFormatSupported)(
        struct IAudioClient* This,
        AudClntShareMode share_mode,
        WaveFormatEx* format,
        WaveFormatEx** closest_match
    );
    int (_stdcall *GetMixFormat)(struct IAudioClient* This,WaveFormatEx** device_format);
    int (_stdcall *GetDevicePeriod)(struct IAudioClient* This,int64* default_device_period,int64* minimum_device_period);
    int (_stdcall *Start)(struct IAudioClient* This);
    int (_stdcall *Stop)(struct IAudioClient* This);
    int (_stdcall *Reset)( struct IAudioClient* This);
    int (_stdcall *SetEventHandle)( struct IAudioClient* This,void* event_handle);
    int (_stdcall *GetService)(struct IAudioClient* This,GU_ID* riid,void** ppv);
} IAudioClientVtbl;

typedef struct IAudioClient{
    struct IAudioClientVtbl *vtable;
} IAudioClient;

typedef enum{
    CLSCTX_INPROC_SERVER	= 0x1,
    CLSCTX_INPROC_HANDLER	= 0x2,
    CLSCTX_LOCAL_SERVER	= 0x4,
    CLSCTX_INPROC_SERVER16	= 0x8,
    CLSCTX_REMOTE_SERVER	= 0x10,
    CLSCTX_INPROC_HANDLER16	= 0x20,
    CLSCTX_RESERVED1	= 0x40,
    CLSCTX_RESERVED2	= 0x80,
    CLSCTX_RESERVED3	= 0x100,
    CLSCTX_RESERVED4	= 0x200,
    CLSCTX_NO_CODE_DOWNLOAD	= 0x400,
    CLSCTX_RESERVED5	= 0x800,
    CLSCTX_NO_CUSTOM_MARSHAL	= 0x1000,
    CLSCTX_ENABLE_CODE_DOWNLOAD	= 0x2000,
    CLSCTX_NO_FAILURE_LOG	= 0x4000,
    CLSCTX_DISABLE_AAA	= 0x8000,
    CLSCTX_ENABLE_AAA	= 0x10000,
    CLSCTX_FROM_DEFAULT_CONTEXT	= 0x20000,
    CLSCTX_ACTIVATE_X86_SERVER	= 0x40000,
    CLSCTX_ACTIVATE_32_BIT_SERVER	= CLSCTX_ACTIVATE_X86_SERVER,
    CLSCTX_ACTIVATE_64_BIT_SERVER	= 0x80000,
    CLSCTX_ENABLE_CLOAKING	= 0x100000,
    CLSCTX_APPCONTAINER	= 0x400000,
    CLSCTX_ACTIVATE_AAA_AS_IU	= 0x800000,
    CLSCTX_RESERVED6	= 0x1000000,
    CLSCTX_ACTIVATE_ARM32_SERVER	= 0x2000000,
    CLSCTX_PS_DLL	= 0x80000000
} CLSCTX;

#define CLSCTX_ALL (CLSCTX_INPROC_SERVER| CLSCTX_INPROC_HANDLER| CLSCTX_LOCAL_SERVER| CLSCTX_REMOTE_SERVER)

#endif