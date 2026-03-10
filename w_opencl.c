#include "w_kernel.h"
#include "w_opencl.h"

int (stdcall *clGetPlatformIDs)(unsigned n_entries,void** platform,unsigned* n_platform_id);
int (stdcall *clGetDeviceIDs)(void* platform,uint64 device_type,unsigned num_entries,void** devices,unsigned* num_devices);
void* (stdcall *clCreateCommandQueueWithProperties)(void* context,void* device,const void** properties,int* errcode_ret);
void* (stdcall *clCreateProgramWithSource)(void* context,unsigned count,const char** strings,const size_t* lengths,int* errcode_ret);
void* (stdcall *clCreateContext)(
    const void** properties,
    unsigned n_devices,
    const void** devices,
    void (stdcall* pfn_notify)(const char* errinfo, const void* private_info,size_t cb, void* user_data),
    void* user_data,
    int* errcode_ret
);
int (stdcall *clBuildProgram)(
    void* program,
    unsigned num_devices,
    const void** device_list,
    const char* options,
    void (stdcall* pfn_notify)(void* program, void* user_data),
    void* user_data
);
void* (stdcall *clCreateKernel)(void* program,const char* kernel_name,int* errcode_ret);
int (stdcall *clEnqueueNDRangeKernel)(
    void* command_queue,
    void* kernel,
    unsigned work_dim,
    const size_t* global_work_offset,
    const size_t* global_work_size_t,
    const size_t* local_work_size_t,
    unsigned num_events_in_wait_list,
    const void** event_wait_list,
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
    const void** event_wait_list,
    void** event
);
int (stdcall *clEnqueueWriteBuffer)(
    void* command_queue,
    void* buffer,
    int blocking_write,
    size_t offset,
    size_t size_t,
    const void* ptr,
    unsigned num_events_in_wait_list,
    const void** event_wait_list,
    void** event
);
int (stdcall *clSetKernelArg)(
    void* kernel,
    unsigned arg_index,
    size_t arg_size_t,
    const void* arg_value
);
void* (stdcall *clCreateBuffer)(
    void* context,
    uint64 flags,
    size_t size_t,
    void* host_ptr,
    int* errcode_ret
);
int (stdcall *clFinish)(void* command_queue);

void initLibOpenCL(void){
    void* opencl_dll = LoadLibraryA("OpenCL.dll");

    clGetPlatformIDs                   = GetProcAddress(opencl_dll,"clGetPlatformIDs");
    clGetDeviceIDs                     = GetProcAddress(opencl_dll,"clGetDeviceIDs");
    clCreateCommandQueueWithProperties = GetProcAddress(opencl_dll,"clCreateCommandQueueWithProperties");
    clCreateProgramWithSource          = GetProcAddress(opencl_dll,"clCreateProgramWithSource");
    clCreateContext                    = GetProcAddress(opencl_dll,"clCreateContext");
    clBuildProgram                     = GetProcAddress(opencl_dll,"clBuildProgram");
    clCreateKernel                     = GetProcAddress(opencl_dll,"clCreateKernel");
    clEnqueueNDRangeKernel             = GetProcAddress(opencl_dll,"clEnqueueNDRangeKernel");
    clEnqueueReadBuffer                = GetProcAddress(opencl_dll,"clEnqueueReadBuffer");
    clEnqueueWriteBuffer               = GetProcAddress(opencl_dll,"clEnqueueWriteBuffer");
    clSetKernelArg                     = GetProcAddress(opencl_dll,"clSetKernelArg");
    clCreateBuffer                     = GetProcAddress(opencl_dll,"clCreateBuffer");
    clFinish                           = GetProcAddress(opencl_dll,"clFinish");
}
