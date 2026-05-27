#include "opencl.h"
#include "memory.h"
#include "console.h"

#include "platform/library.h"
#include "platform/storage.h"

#define CL_PROGRAM_BUILD_LOG 0x1183

#define CL_PLATFORM_PROFILE     0x0900
#define CL_PLATFORM_VERSION     0x0901
#define CL_PLATFORM_NAME        0x0902
#define CL_PLATFORM_VENDOR      0x0903
#define CL_PLATFORM_EXTENSIONS  0x0904

#define CL_DEVICE_NAME   0x102B

int (stdcall *clGetPlatformIDs)(unsigned n_entries,void** platform,unsigned* n_platform_id);
int (stdcall *clGetDeviceIDs)(void* platform,uint64 device_type,unsigned num_entries,void** devices,unsigned* num_devices);
void* (stdcall *clCreateCommandQueueWithProperties)(void* context,void* device,void** properties,int* errcode_ret);
void* (stdcall *clCreateProgramWithSource)(void* context,unsigned count,char** strings,size_t* lengths,int* errcode_ret);
void* (stdcall *clCreateContext)(
    void** properties,
    unsigned n_devices,
    void** devices,
    void (stdcall* pfn_notify)(char* errinfo,void* private_info,size_t cb, void* user_data),
    void* user_data,
    int* errcode_ret
);
int (stdcall *clBuildProgram)(
    void* program,
    unsigned num_devices,
    void** device_list,
    char* options,
    void (stdcall* pfn_notify)(void* program, void* user_data),
    void* user_data
);
void* (stdcall *clCreateKernel)(void* program,char* kernel_name,int* errcode_ret);
int (stdcall *clEnqueueNDRangeKernel)(
    void* command_queue,
    void* kernel,
    unsigned work_dim,
    size_t* global_work_offset,
    size_t* global_work_size_t,
    size_t* local_work_size_t,
    unsigned num_events_in_wait_list,
    void** event_wait_list,
    void** event
);
int (stdcall *clEnqueueReadBuffer)(
    void* command_queue,
    void* buffer,
    int blocking_read,
    size_t offset,
    size_t size_t,
    void* ptr,
    unsigned num_events_in_wait_list,
    void** event_wait_list,
    void** event
);
int (stdcall *clEnqueueWriteBuffer)(
    void* command_queue,
    void* buffer,
    int blocking_write,
    size_t offset,
    size_t size_t,
    void* ptr,
    unsigned num_events_in_wait_list,
    void** event_wait_list,
    void** event
);
int (stdcall *clSetKernelArg)(
    void* kernel,
    unsigned arg_index,
    size_t arg_size_t,
    void* arg_value
);
void* (stdcall *clCreateBuffer)(
    void* context,
    uint64 flags,
    size_t size_t,
    void* host_ptr,
    int* errcode_ret
);
int (stdcall *clFinish)(void* command_queue);
int (stdcall *clReleaseMemObject)(void* memobj);
int (stdcall *clGetProgramBuildInfo)(
    void* program,
    void* device,
    int param_name,
    size_t param_value_size,
    void* param_value,
    size_t* param_value_size_ret
);
int (stdcall *clGetPlatformInfo)(
    void* platform,
    int param_name,
    size_t param_value_size,
    void* param_value,
    size_t* param_value_size_ret
);
int (stdcall *clGetDeviceInfo)(
    void* device,
    int param_name,
    size_t param_value_size,
    void* param_value,
    size_t* param_value_size_ret
);

void* g_cl_context;
static void* cl_inference;
static void* cl_inference_bf;
static void* g_cl_command_queue;

static void* cl_raytrace;

void* g_opencl_lib;

void openclInit(void){
#ifdef _MSC_VER
    g_opencl_lib  = libraryLoad("OpenCL.dll");
#elif defined(__linux__)
    g_opencl_lib = libraryLoad("libOpenCL.so");
#endif
    if(!g_opencl_lib){
        print((String)STRING_LITERAL("opencl library not found\n"));
        return;
    }
    struct{
        char* name;
        funcptr_t* fn_ptr;
    } functions[] =  {
        {.name = "clGetPlatformIDs",.fn_ptr = (funcptr_t*)&clGetPlatformIDs},
        {.name = "clGetDeviceIDs",.fn_ptr = (funcptr_t*)&clGetDeviceIDs},
        {.name = "clCreateCommandQueueWithProperties",.fn_ptr = (funcptr_t*)&clCreateCommandQueueWithProperties},
        {.name = "clCreateProgramWithSource",.fn_ptr = (funcptr_t*)&clCreateProgramWithSource},
        {.name = "clCreateContext",.fn_ptr = (funcptr_t*)&clCreateContext},
        {.name = "clCreateKernel",.fn_ptr = (funcptr_t*)&clCreateKernel},
        {.name = "clEnqueueNDRangeKernel",.fn_ptr = (funcptr_t*)&clEnqueueNDRangeKernel},
        {.name = "clEnqueueReadBuffer",.fn_ptr = (funcptr_t*)&clEnqueueReadBuffer},
        {.name = "clEnqueueWriteBuffer",.fn_ptr = (funcptr_t*)&clEnqueueWriteBuffer},
        {.name = "clSetKernelArg",.fn_ptr = (funcptr_t*)&clSetKernelArg},
        {.name = "clCreateBuffer",.fn_ptr = (funcptr_t*)&clCreateBuffer},
        {.name = "clFinish",.fn_ptr = (funcptr_t*)&clFinish},
        {.name = "clReleaseMemObject",.fn_ptr = (funcptr_t*)&clReleaseMemObject},
        {.name = "clGetProgramBuildInfo",.fn_ptr = (funcptr_t*)&clGetProgramBuildInfo},
        {.name = "clGetPlatformInfo",.fn_ptr = (funcptr_t*)&clGetPlatformInfo},
        {.name = "clGetDeviceInfo",.fn_ptr = (funcptr_t*)&clGetDeviceInfo},
        {.name = "clBuildProgram",.fn_ptr = (funcptr_t*)&clBuildProgram},
    };
    for(int i = countof(functions);i--;){
        *functions[i].fn_ptr = libraryFunctionLoad(g_opencl_lib,functions[i].name);
        if(!*functions[i].fn_ptr){
            print((String)STRING_LITERAL("function not found in opencl library: "));
            printNL(stringMake(functions[i].name));
            goto unload;
        }
    }

    FileContent file = storageFileRead(&g_arena_frame,"opencl/markov_texture.cl");

    void* platforms[20];
    void* devices[20];
    unsigned n_platform = 0;
    unsigned n_device = 0;

    int platform_error = clGetPlatformIDs(20,platforms,&n_platform);
    if(platform_error){
        print((String)STRING_LITERAL("opencl platform ids fetch failed\n"));
        print((String)STRING_LITERAL("error code: "));
        printNumberNL((int)platform_error);
        goto unload;
    }
    clGetDeviceIDs(platforms[0],CL_DEVICE_TYPE_ALL,1,devices,&n_device);

    int context_error;
    
    g_cl_context = clCreateContext(0,1,(void**)&devices[0],0,0,&context_error);

    if(!g_cl_context){
        print((String)STRING_LITERAL("opencl context creation failed\n"));
        print((String)STRING_LITERAL("error code: "));
        printNumberNL((int)context_error);
        PRINT_VAR(n_device);
        PRINT_VAR(n_platform);
        goto unload;
    }
    
    g_cl_command_queue = clCreateCommandQueueWithProperties(g_cl_context,devices[0],0,0);
#if 0
    String source_file = stringConcat(&g_arena_frame,(String){.data = file.content,.size = file.size},(String)STRING_LITERAL("\0"));
    
    void* g_cl_program = clCreateProgramWithSource(g_cl_context,1,(char**)&source_file.data,0,0);
    int build_error = clBuildProgram(g_cl_program,0,0,"-Werror",0,0);

    cl_inference    = clCreateKernel(g_cl_program,"inference",0);
    cl_inference_bf = clCreateKernel(g_cl_program,"inferenceBruteForce",0);

    if(build_error){
        size_t log_size = 0;
        clGetProgramBuildInfo(g_cl_program,devices[0],CL_PROGRAM_BUILD_LOG,0,NULL,&log_size);
        char *log = virtualAllocate(log_size + 1);
        clGetProgramBuildInfo(g_cl_program,devices[0],CL_PROGRAM_BUILD_LOG,log_size,log,NULL);
        log[log_size] = '\0';
        print((String)STRING_LITERAL("opencl kernel build failed\n"));
        debugPrint(log);
        goto unload;
    }
#endif
    char name[128];

    clGetDeviceInfo(devices[0],CL_DEVICE_NAME,sizeof name,name,0);

    file = storageFileRead(&g_arena_frame,"opencl/ray_tree.cl");
    String source_file = stringConcat(&g_arena_frame,(String){.data = file.content,.size = file.size},(String)STRING_LITERAL("\0"));
    void* g_cl_program = clCreateProgramWithSource(g_cl_context,1,(char**)&source_file.data,0,0);
    int build_error = clBuildProgram(g_cl_program,0,0,"-Werror",0,0);
    
    if(build_error){
        size_t log_size = 0;
        clGetProgramBuildInfo(g_cl_program,devices[0],CL_PROGRAM_BUILD_LOG,0,NULL,&log_size);
        char *log = virtualAllocate(log_size + 1);
        clGetProgramBuildInfo(g_cl_program,devices[0],CL_PROGRAM_BUILD_LOG,log_size,log,NULL);
        log[log_size] = '\0';
        print((String)STRING_LITERAL("opencl kernel build failed\n"));
        debugPrint(log);
        print((String)STRING_LITERAL("error code: "));
        printNumberNL((int)g_cl_program);
        PRINT_VAR((int)g_cl_context);
        goto unload;
    }
    cl_raytrace = clCreateKernel(g_cl_program,"inference",0);
    return;
    
 unload:
    libraryUnload(g_opencl_lib);
    g_opencl_lib = 0;
}

#include "texture_markov.h"

static void* cl_indices;
static void* cl_vram_buffers;
static void* cl_graph_buffers[0x10];

void markovInferenceInitOpenCL(Texture texture,Graph* graph,int image_size){
    for(int i = 0;i < 0x10;i++){
        if(!graph[i].n_nodes)
            continue;
        cl_graph_buffers[i] = clCreateBuffer(g_cl_context,CL_MEM_READ_WRITE,graph[i].n_nodes * sizeof(GraphNode),0,0);
        clEnqueueWriteBuffer(g_cl_command_queue,cl_graph_buffers[i],true,0,graph[i].n_nodes * sizeof(GraphNode),graph[i].entry,0,0,0);
    }

    cl_vram_buffers = clCreateBuffer(g_cl_context,CL_MEM_READ_WRITE,image_size * image_size * sizeof(int) * 2,0,0);
    cl_indices = clCreateBuffer(g_cl_context,CL_MEM_WRITE_ONLY,image_size * image_size * sizeof(int),0,0);
}

void markovInferenceBruteforceOpenCL(Texture texture,Graph* graph,int* indices,int mipmap,int image_size,int n_node){
    if(!graph[mipmap].n_nodes)
        return;
    
    clSetKernelArg(cl_inference_bf,0,sizeof(void*),cl_graph_buffers + mipmap);
    clSetKernelArg(cl_inference_bf,1,sizeof(void*),&cl_vram_buffers);
    clSetKernelArg(cl_inference_bf,2,sizeof(void*),&cl_indices);
    clSetKernelArg(cl_inference_bf,3,sizeof(int),&mipmap);
    clSetKernelArg(cl_inference_bf,4,sizeof(int),&image_size);
    clSetKernelArg(cl_inference_bf,5,sizeof(int),&n_node);

    clEnqueueWriteBuffer(g_cl_command_queue,cl_vram_buffers,true,0,image_size * image_size * sizeof(int) * 2,texture.pixel_data,0,0,0);
    clEnqueueWriteBuffer(g_cl_command_queue,cl_indices,true,0,image_size * image_size * sizeof(int),indices,0,0,0);
    size_t n_output = (image_size >> mipmap) * (image_size >> mipmap);
    for(int i = 0;i < 2;i++)
        clEnqueueNDRangeKernel(g_cl_command_queue,cl_inference_bf,1,0,&n_output,0,0,0,0);
    clEnqueueReadBuffer(g_cl_command_queue,cl_vram_buffers,true,0,image_size * image_size * sizeof(int) * 2,texture.pixel_data,0,0,0);
}

void markovInferenceOpenCL(Texture texture,Graph* graph,int* indices,int mipmap,int image_size,int quality,int n_node){
    if(!graph[mipmap].n_nodes)
        return;
    
    clSetKernelArg(cl_inference,0,sizeof(void*),cl_graph_buffers + mipmap);
    clSetKernelArg(cl_inference,1,sizeof(void*),&cl_vram_buffers);
    clSetKernelArg(cl_inference,2,sizeof(void*),&cl_indices);
    clSetKernelArg(cl_inference,3,sizeof(int),&mipmap);
    clSetKernelArg(cl_inference,4,sizeof(int),&quality);
    clSetKernelArg(cl_inference,5,sizeof(int),&image_size);

    clEnqueueWriteBuffer(g_cl_command_queue,cl_vram_buffers,true,0,image_size * image_size * sizeof(int) * 2,texture.pixel_data,0,0,0);
    clEnqueueWriteBuffer(g_cl_command_queue,cl_indices,true,0,image_size * image_size * sizeof(int),indices,0,0,0);
    size_t n_output = (image_size >> mipmap) * (image_size >> mipmap);
    for(int i = 0;i < 2;i++)
        clEnqueueNDRangeKernel(g_cl_command_queue,cl_inference,1,0,&n_output,0,0,0,0);
    clEnqueueReadBuffer(g_cl_command_queue,cl_vram_buffers,true,0,image_size * image_size * sizeof(int) * 2,texture.pixel_data,0,0,0);
}

void markovInferenceDeInitOpenCL(void){
    for(int i = 0;i < 0x10;i++){
        if(!cl_graph_buffers[i])
            continue;
        clReleaseMemObject(cl_graph_buffers[i]);
        cl_graph_buffers[i] = 0;
    }
    clReleaseMemObject(cl_vram_buffers);
    clReleaseMemObject(cl_indices);

    cl_vram_buffers = 0;
    cl_indices = 0;
}

static void* cl_voxel_gpu;

void uploadVoxelGPU(void* data,int n){
    clSetKernelArg(cl_raytrace,0,sizeof(void*),&cl_voxel_gpu);
    clEnqueueWriteBuffer(g_cl_command_queue,cl_voxel_gpu,true,0,n * sizeof(VoxelGPU),data,0,0,0);
}
