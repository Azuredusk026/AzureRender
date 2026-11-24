#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#pragma comment(lib,"dbghelp.lib")
unsigned long long number(FILETIME t){return (static_cast<unsigned long long>(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
int main(int argc,char**argv){if(argc!=3)return 2;DWORD pid=std::stoul(argv[1]);HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);if(!p)return 3;
 SymSetOptions(SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS);
 if(!SymInitialize(p,argv[2],TRUE)){std::cerr<<"SymInitialize "<<GetLastError();return 4;}
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 e{sizeof(e)};DWORD busiest=0;unsigned long long maximum=0;
 if(Thread32First(snapshot,&e))do{if(e.th32OwnerProcessID!=pid)continue;HANDLE t=OpenThread(THREAD_QUERY_INFORMATION,FALSE,e.th32ThreadID);if(!t)continue;FILETIME c,x,k,u;if(GetThreadTimes(t,&c,&x,&k,&u)){auto n=number(k)+number(u);if(n>maximum){maximum=n;busiest=e.th32ThreadID;}}CloseHandle(t);}while(Thread32Next(snapshot,&e));CloseHandle(snapshot);
 HANDLE t=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,busiest);if(!t)return 5;std::cout<<"pid="<<pid<<" thread="<<busiest<<" cumulativeCpuSeconds="<<maximum/10000000.0<<'\n';
 if(SuspendThread(t)==DWORD(-1))return 6;
 struct Resume {HANDLE t;~Resume(){ResumeThread(t);CloseHandle(t);}} resume{t};
 CONTEXT c{};c.ContextFlags=CONTEXT_FULL;if(!GetThreadContext(t,&c))return 7;STACKFRAME64 f{};f.AddrPC.Offset=c.Rip;f.AddrFrame.Offset=c.Rbp;f.AddrStack.Offset=c.Rsp;f.AddrPC.Mode=f.AddrFrame.Mode=f.AddrStack.Mode=AddrModeFlat;
 for(int i=0;i<40&&f.AddrPC.Offset;++i){alignas(SYMBOL_INFO)char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto* s=reinterpret_cast<SYMBOL_INFO*>(bytes);s->SizeOfStruct=sizeof(SYMBOL_INFO);s->MaxNameLen=MAX_SYM_NAME;DWORD64 delta=0;IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof(module);SymGetModuleInfo64(p,f.AddrPC.Offset,&module);std::cout<<i<<" "<<module.ModuleName<<"!";if(SymFromAddr(p,f.AddrPC.Offset,&delta,s))std::cout<<s->Name<<"+"<<delta;else std::cout<<std::hex<<f.AddrPC.Offset<<std::dec;IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD offset=0;if(SymGetLineFromAddr64(p,f.AddrPC.Offset,&offset,&line))std::cout<<" "<<line.FileName<<":"<<line.LineNumber;std::cout<<'\n';if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,p,t,&f,&c,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;}
 // Resume the sampled thread before symbol cleanup.
 return 0;}
