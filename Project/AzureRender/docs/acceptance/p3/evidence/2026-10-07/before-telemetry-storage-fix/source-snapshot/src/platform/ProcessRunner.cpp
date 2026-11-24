#include "ProcessRunner.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace azurerender {
namespace {
#ifdef _WIN32
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::wstring quote(const std::wstring& text) {
    std::wstring result=L"\""; std::size_t slashes=0;
    for (const auto c:text) {
        if(c==L'\\'){++slashes;continue;}
        result.append(c==L'"'?2*slashes+1:slashes,L'\\'); result+=c; slashes=0;
    }
    result.append(2*slashes,L'\\'); return result+L'"';
}
#endif
}
ProcessResult runProcess(const ProcessRequest& request,const std::atomic<bool>& cancelled) {
    ProcessResult result;
    try {
        if(request.timeoutMs==0 || request.timeoutMs>300000 || request.outputBytes==0 || request.outputBytes>8*1024*1024)
            throw std::invalid_argument("Invalid process budget");
        if(request.arguments.size()>256)throw std::invalid_argument("Too many process arguments");
        std::size_t argumentBytes=0;
        for(const auto& argument:request.arguments){
            argumentBytes+=argument.size();
            if(argument.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid process argument");
        }
        if(argumentBytes>24000)throw std::invalid_argument("Process arguments exceed budget");
        if(cancelled){result.cancelled=true;return result;}
#ifdef _WIN32
        if(!std::filesystem::is_regular_file(request.executable))throw std::runtime_error("Process executable unavailable");
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        Handle read,write,input,job,process,thread;
        if(!CreatePipe(&read.value,&write.value,&security,0) || !SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0))
            throw std::runtime_error("Process output pipe creation failed");
        input.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        job.value=CreateJobObjectW(nullptr,nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(input.value==INVALID_HANDLE_VALUE || !job.value || !SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
            throw std::runtime_error("Process job creation failed");
        STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput=input.value;startup.StartupInfo.hStdOutput=write.value;startup.StartupInfo.hStdError=write.value;
        SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<unsigned char> storage(bytes);
        startup.lpAttributeList=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&bytes))throw std::runtime_error("Process attributes unavailable");
        struct Guard{PPROC_THREAD_ATTRIBUTE_LIST p;~Guard(){DeleteProcThreadAttributeList(p);}} guard{startup.lpAttributeList};
        HANDLE inherited[]={input.value,write.value};
        if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr))
            throw std::runtime_error("Process handle selection failed");
        const auto executable=std::filesystem::absolute(request.executable).wstring();
        std::wstring command=quote(executable);
        for(const auto& argument:request.arguments)command+=L" "+quote(std::filesystem::u8path(argument).wstring());
        PROCESS_INFORMATION info{};const auto directory=request.workingDirectory.wstring();
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,
            CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,
            directory.empty()?nullptr:directory.c_str(),&startup.StartupInfo,&info))throw std::runtime_error("Process creation failed");
        process.value=info.hProcess;thread.value=info.hThread;
        if(!AssignProcessToJobObject(job.value,process.value)){
            TerminateProcess(process.value,1);WaitForSingleObject(process.value,5000);
            throw std::runtime_error("Process job ownership failed");
        }
        if(ResumeThread(thread.value)==static_cast<DWORD>(-1))throw std::runtime_error("Process start failed");
        CloseHandle(write.value);write.value=nullptr;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(request.timeoutMs);
        while(true){
            DWORD available=0;
            while(PeekNamedPipe(read.value,nullptr,0,nullptr,&available,nullptr) && available){
                char buffer[4096];DWORD count=0;
                if(!ReadFile(read.value,buffer,std::min<DWORD>(available,sizeof(buffer)),&count,nullptr) || !count)break;
                if(result.output.size()+count>request.outputBytes){
                    TerminateJobObject(job.value,1);WaitForSingleObject(process.value,5000);
                    throw std::runtime_error("Process output exceeds budget");
                }
                result.output.append(buffer,count);
                if(cancelled || std::chrono::steady_clock::now()>=deadline)break;
            }
            if(cancelled || std::chrono::steady_clock::now()>=deadline){
                result.cancelled=cancelled;result.timedOut=!result.cancelled;
                TerminateJobObject(job.value,1);WaitForSingleObject(process.value,5000);break;
            }
            if(WaitForSingleObject(process.value,5)==WAIT_OBJECT_0){
                // A final loop drains bytes written just before process exit.
                if(PeekNamedPipe(read.value,nullptr,0,nullptr,&available,nullptr) && available)continue;
                DWORD code=1;GetExitCodeProcess(process.value,&code);result.exitCode=code;
                result.passed=code==0;break;
            }
        }
        if(!result.passed)result.diagnostic=result.cancelled?"Process cancelled":result.timedOut?"Process deadline exceeded":"Process failed";
#else
        result.diagnostic="Controlled process execution is unavailable on this platform";
#endif
    }catch(const std::exception& error){result.diagnostic=error.what();}
    return result;
}
}
