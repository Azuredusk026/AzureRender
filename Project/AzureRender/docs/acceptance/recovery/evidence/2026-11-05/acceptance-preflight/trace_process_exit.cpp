#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <string>
#include <vector>
#pragma comment(lib,"dbghelp.lib")
void stack(HANDLE process,DWORD id) {
 HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,id);
 if(!thread)return;
 CONTEXT context{};context.ContextFlags=CONTEXT_ALL;
 if(GetThreadContext(thread,&context)) {
  std::cout<<"thread="<<id<<" rcx="<<context.Rcx<<" rip="<<std::hex<<context.Rip<<std::dec<<'\n';
  SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_LOAD_LINES|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS);
  if(SymInitialize(process,nullptr,TRUE)) {
   STACKFRAME64 frame{};frame.AddrPC={context.Rip,0,AddrModeFlat};frame.AddrFrame={context.Rbp,0,AddrModeFlat};frame.AddrStack={context.Rsp,0,AddrModeFlat};
   for(int i=0;i<30;++i){
    if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr)||!frame.AddrPC.Offset)break;
    char bytes[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto symbol=reinterpret_cast<SYMBOL_INFO*>(bytes);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;DWORD64 offset=0;
    IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof(module);SymGetModuleInfo64(process,frame.AddrPC.Offset,&module);
    std::cout<<std::hex<<frame.AddrPC.Offset<<" "<<module.ModuleName<<"+"<<(frame.AddrPC.Offset-module.BaseOfImage);
    if(SymFromAddr(process,frame.AddrPC.Offset,&offset,symbol))std::cout<<" "<<symbol->Name<<"+"<<offset;
    IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD displacement=0;if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&displacement,&line))std::cout<<" "<<line.FileName<<":"<<std::dec<<line.LineNumber;
    std::cout<<std::dec<<'\n';
   }
   SymCleanup(process);
  }
 }
 CloseHandle(thread);std::cout.flush();
}
int wmain(int argc,wchar_t** argv){
 if(argc<3)return 2;
 SECURITY_ATTRIBUTES attributes{sizeof(attributes),nullptr,TRUE};HANDLE log=CreateFileW(argv[1],GENERIC_WRITE,FILE_SHARE_READ,&attributes,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(log==INVALID_HANDLE_VALUE)return 3;
 std::wstring command;for(int i=2;i<argc;++i){if(i>2)command+=L" ";command+=L"\""+std::wstring(argv[i])+L"\"";}std::vector<wchar_t> args(command.begin(),command.end());args.push_back(0);
 STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;startup.hStdOutput=log;startup.hStdError=log;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION child{};
 if(!CreateProcessW(nullptr,args.data(),nullptr,nullptr,TRUE,DEBUG_ONLY_THIS_PROCESS,nullptr,nullptr,&startup,&child)){std::cout<<"CreateProcess failed "<<GetLastError()<<'\n';CloseHandle(log);return 4;}
 void* target=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlExitUserProcess"));BYTE original=0;bool armed=false;ULONGLONG start=GetTickCount64();DWORD result=5;
 for(;;){
  DEBUG_EVENT event{};if(!WaitForDebugEvent(&event,1000)){if(GetTickCount64()-start>60000){TerminateProcess(child.hProcess,87);}continue;}
  DWORD disposition=DBG_CONTINUE;
  if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){
   if(event.u.CreateProcessInfo.hFile)CloseHandle(event.u.CreateProcessInfo.hFile);
   SIZE_T bytes=0;DWORD old=0;
   if(ReadProcessMemory(child.hProcess,target,&original,1,&bytes)&&bytes==1&&VirtualProtectEx(child.hProcess,target,1,PAGE_EXECUTE_READWRITE,&old)){
    BYTE breakpoint=0xcc;armed=WriteProcessMemory(child.hProcess,target,&breakpoint,1,&bytes)!=FALSE;FlushInstructionCache(child.hProcess,target,1);DWORD ignored=0;VirtualProtectEx(child.hProcess,target,1,old,&ignored);
   }
   std::cout<<"pid="<<child.dwProcessId<<" exitBreakpoint="<<armed<<'\n';
  }else if(event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT){if(event.u.LoadDll.hFile)CloseHandle(event.u.LoadDll.hFile);
  }else if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
   const auto& exception=event.u.Exception.ExceptionRecord;
   if(armed&&exception.ExceptionCode==EXCEPTION_BREAKPOINT&&exception.ExceptionAddress==target){
    std::cout<<"RtlExitUserProcess breakpoint\n";stack(child.hProcess,event.dwThreadId);
    HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,event.dwThreadId);CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;GetThreadContext(thread,&context);context.Rip=reinterpret_cast<DWORD64>(target);SetThreadContext(thread,&context);CloseHandle(thread);
    SIZE_T written=0;DWORD old=0;VirtualProtectEx(child.hProcess,target,1,PAGE_EXECUTE_READWRITE,&old);WriteProcessMemory(child.hProcess,target,&original,1,&written);FlushInstructionCache(child.hProcess,target,1);DWORD ignored=0;VirtualProtectEx(child.hProcess,target,1,old,&ignored);armed=false;
   }else if(exception.ExceptionCode!=EXCEPTION_BREAKPOINT){disposition=DBG_EXCEPTION_NOT_HANDLED;if(!event.u.Exception.dwFirstChance){std::cout<<"Unhandled exception "<<std::hex<<exception.ExceptionCode<<std::dec<<'\n';stack(child.hProcess,event.dwThreadId);}}
  }else if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT){result=event.u.ExitProcess.dwExitCode;std::cout<<"Exit event "<<result<<'\n';ContinueDebugEvent(event.dwProcessId,event.dwThreadId,DBG_CONTINUE);break;}
  ContinueDebugEvent(event.dwProcessId,event.dwThreadId,disposition);
 }
 CloseHandle(child.hThread);CloseHandle(child.hProcess);CloseHandle(log);return static_cast<int>(result);
}
