#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <iostream>
#include <cstdlib>
#pragma comment(lib,"dbghelp.lib")
int main(int argc,char**argv){
 if(argc!=2)return 2;DWORD pid=std::strtoul(argv[1],nullptr,10);
 HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
 if(!process){std::cerr<<"OpenProcess error "<<GetLastError()<<'\n';return 3;}
 SymSetOptions(SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_DEFERRED_LOADS);
 SymInitialize(process,nullptr,TRUE);
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
 THREADENTRY32 entry{};entry.dwSize=sizeof(entry);
 if(Thread32First(snapshot,&entry))do{
  if(entry.th32OwnerProcessID!=pid)continue;
  HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
  if(!thread)continue;
  DWORD previous=SuspendThread(thread);if(previous==DWORD(-1)){CloseHandle(thread);continue;}
  CONTEXT context{};context.ContextFlags=CONTEXT_FULL;
  std::cout<<"Thread "<<entry.th32ThreadID<<" priorSuspend="<<previous<<'\n';
  if(GetThreadContext(thread,&context)){
   STACKFRAME64 frame{};frame.AddrPC.Offset=context.Rip;frame.AddrFrame.Offset=context.Rbp;frame.AddrStack.Offset=context.Rsp;
   frame.AddrPC.Mode=frame.AddrFrame.Mode=frame.AddrStack.Mode=AddrModeFlat;
   for(int i=0;i<40&&StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr);++i){
    char storage[sizeof(SYMBOL_INFO)+1024]{};auto*symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;DWORD64 displacement=0;
    IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof(module);SymGetModuleInfo64(process,frame.AddrPC.Offset,&module);
    std::cout<<"  "<<std::hex<<frame.AddrPC.Offset<<" "<<module.ModuleName<<"+"<<(frame.AddrPC.Offset-module.BaseOfImage);
    if(SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol))std::cout<<" "<<symbol->Name<<"+"<<displacement;
    std::cout<<std::dec<<'\n';
   }
  }
  ResumeThread(thread);CloseHandle(thread);std::cout.flush();
 }while(Thread32Next(snapshot,&entry));
 CloseHandle(snapshot);SymCleanup(process);CloseHandle(process);
}
