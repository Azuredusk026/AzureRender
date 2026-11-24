/* Diagnostic only. Hooks are installed explicitly in the injector's own child. */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "MinHook.h"
typedef LONG (NTAPI *AllocFn)(HANDLE,PVOID*,ULONG_PTR,PSIZE_T,ULONG,ULONG);
typedef LONG (NTAPI *AllocExFn)(HANDLE,PVOID*,PSIZE_T,ULONG,ULONG,PVOID,ULONG);
typedef LONG (NTAPI *FreeFn)(HANDLE,PVOID*,PSIZE_T,ULONG);
typedef struct Event {
    uint64_t qpc,thread,api,flags,protection,status,process,inputBase,inputSize,
             outputBase,outputSize,frameCount,published,frames[16];
} Event;
typedef struct Header {char magic[8];uint64_t version,capacity,count,dropped,reentrant,invalid,frequency;} Header;
static AllocFn realAlloc;
static AllocExFn realAllocEx;
static FreeFn realFree;
static Event *events;
static const LONG64 capacity=262144;
static volatile LONG64 count,dropped,reentrant,invalid;
static volatile LONG active;
/* NT calls can precede a thread's static TLS array initialization. */
static DWORD recordingSlot=TLS_OUT_OF_INDEXES;
static wchar_t output[MAX_PATH*4];
static LARGE_INTEGER frequency;
static uint64_t readPointer(PVOID *value) {
    __try {return value?(uint64_t)(ULONG_PTR)*value:0;}
    __except(EXCEPTION_EXECUTE_HANDLER) {InterlockedIncrement64(&invalid);return 0;}
}
static uint64_t readSize(PSIZE_T value) {
    __try {return value?(uint64_t)*value:0;}
    __except(EXCEPTION_EXECUTE_HANDLER) {InterlockedIncrement64(&invalid);return 0;}
}
static void record(uint64_t api,HANDLE process,PVOID *base,PSIZE_T size,
                   uint64_t inputBase,uint64_t inputSize,ULONG flags,ULONG protection,LONG status) {
    LONG64 slot;Event *entry;LARGE_INTEGER qpc;void *frames[16];USHORT n,i;DWORD lastError;
    if(!active)return;
    lastError=GetLastError();
    if(TlsGetValue(recordingSlot)){InterlockedIncrement64(&reentrant);SetLastError(lastError);return;}
    TlsSetValue(recordingSlot,(void*)1);
    slot=InterlockedIncrement64(&count)-1;
    if(slot>=capacity){InterlockedIncrement64(&dropped);TlsSetValue(recordingSlot,NULL);SetLastError(lastError);return;}
    entry=events+slot;
    QueryPerformanceCounter(&qpc);
    entry->qpc=(uint64_t)qpc.QuadPart;entry->thread=GetCurrentThreadId();
    entry->api=api;entry->flags=flags;entry->protection=protection;
    entry->status=(uint64_t)(ULONG)(status);entry->process=(uint64_t)(ULONG_PTR)process;
    entry->inputBase=inputBase;entry->inputSize=inputSize;
    entry->outputBase=readPointer(base);entry->outputSize=readSize(size);
    n=CaptureStackBackTrace(1,16,frames,NULL);entry->frameCount=n;
    for(i=0;i<n;++i)entry->frames[i]=(uint64_t)(ULONG_PTR)frames[i];
    InterlockedExchange64((volatile LONG64*)&entry->published,1);
    TlsSetValue(recordingSlot,NULL);SetLastError(lastError);
}
static LONG NTAPI allocate(HANDLE process,PVOID *base,ULONG_PTR bits,PSIZE_T size,ULONG flags,ULONG protection) {
    uint64_t inBase=readPointer(base),inSize=readSize(size);
    LONG status=realAlloc(process,base,bits,size,flags,protection);
    record(1,process,base,size,inBase,inSize,flags,protection,status);return status;
}
static LONG NTAPI allocateEx(HANDLE process,PVOID *base,PSIZE_T size,ULONG flags,ULONG protection,PVOID params,ULONG n) {
    uint64_t inBase=readPointer(base),inSize=readSize(size);
    LONG status=realAllocEx(process,base,size,flags,protection,params,n);
    record(2,process,base,size,inBase,inSize,flags,protection,status);return status;
}
static LONG NTAPI releaseMemory(HANDLE process,PVOID *base,PSIZE_T size,ULONG flags) {
    uint64_t inBase=readPointer(base),inSize=readSize(size);
    LONG status=realFree(process,base,size,flags);
    record(3,process,base,size,inBase,inSize,flags,0,status);return status;
}
__declspec(dllexport) DWORD WINAPI AzureTraceStart(void *parameter) {
    HMODULE nt=GetModuleHandleW(L"ntdll.dll");void *warmup[16];
    if(active||!parameter)return 10;
    if(wcslen((wchar_t*)parameter)>=sizeof(output)/sizeof(output[0]))return 11;
    wcscpy_s(output,sizeof(output)/sizeof(output[0]),(wchar_t*)parameter);
    events=VirtualAlloc(NULL,(SIZE_T)capacity*sizeof(Event),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!events)return 12;
    memset(events,0,(SIZE_T)capacity*sizeof(Event));QueryPerformanceFrequency(&frequency);
    CaptureStackBackTrace(0,16,warmup,NULL);
    recordingSlot=TlsAlloc();
    if(recordingSlot>=64)return 13;
    if(MH_Initialize()!=MH_OK)return 20;
    if(MH_CreateHook(GetProcAddress(nt,"NtAllocateVirtualMemory"),allocate,(LPVOID*)&realAlloc)!=MH_OK)return 21;
    if(MH_CreateHook(GetProcAddress(nt,"NtAllocateVirtualMemoryEx"),allocateEx,(LPVOID*)&realAllocEx)!=MH_OK)return 22;
    if(MH_CreateHook(GetProcAddress(nt,"NtFreeVirtualMemory"),releaseMemory,(LPVOID*)&realFree)!=MH_OK)return 23;
    if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK)return 24;
    InterlockedExchange(&active,1);return 0;
}
static void dump(void) {
    Header header;HANDLE file;DWORD wrote;LONG64 recorded=count;SIZE_T bytes;
    InterlockedExchange(&active,0);
    if(!events||!output[0])return;
    memcpy(header.magic,"AZNT001",8);header.version=1;header.capacity=(uint64_t)capacity;
    header.count=(uint64_t)recorded;header.dropped=(uint64_t)dropped;
    header.reentrant=(uint64_t)reentrant;header.invalid=(uint64_t)invalid;
    header.frequency=(uint64_t)frequency.QuadPart;
    file=CreateFileW(output,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return;
    if(!WriteFile(file,&header,sizeof(header),&wrote,NULL)||wrote!=sizeof(header)){CloseHandle(file);return;}
    bytes=(SIZE_T)(recorded<capacity?recorded:capacity)*sizeof(Event);
    WriteFile(file,events,(DWORD)bytes,&wrote,NULL);FlushFileBuffers(file);CloseHandle(file);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)module;
    if(reason==DLL_PROCESS_DETACH&&reserved&&active)dump();
    return TRUE;
}
