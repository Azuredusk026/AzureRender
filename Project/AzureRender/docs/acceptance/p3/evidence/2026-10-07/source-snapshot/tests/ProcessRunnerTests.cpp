#include "platform/ProcessRunner.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
using namespace azurerender;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int execute(int argc,char** argv){try{
    if(argc>1){
        const std::string mode=argv[1];
        if(mode=="sleep"){std::this_thread::sleep_for(std::chrono::seconds(20));return 0;}
        if(mode=="flood"){for(int i=0;i<10000;++i)std::cout<<std::string(1024,'x')<<std::flush;return 0;}
        if(mode=="echo"){for(int i=2;i<argc;++i)std::cout<<argv[i]<<'\n';return 0;}
        return 7;
    }
    std::atomic<bool> cancel{false};ProcessRequest request;request.executable=std::filesystem::absolute(argv[0]);
    request.arguments={"echo","with spaces","quote\"value","C:\\path with spaces\\","$(literal) & `literal`","中文参数"};
    const auto quoted=runProcess(request,cancel);
    require(quoted.passed,"Direct process invocation succeeds");
    auto output=quoted.output;output.erase(std::remove(output.begin(),output.end(),'\r'),output.end());
    require(output=="with spaces\nquote\"value\nC:\\path with spaces\\\n$(literal) & `literal`\n中文参数\n","Arguments retain quotes, Unicode and shell metacharacters");
    request.arguments={"sleep"};request.timeoutMs=100;
    const auto start=std::chrono::steady_clock::now();const auto timeout=runProcess(request,cancel);
    require(!timeout.passed && timeout.timedOut,"Deadline terminates a real child process");
    require(std::chrono::steady_clock::now()-start<std::chrono::seconds(5),"Deadline finishes within a bounded interval");
    request.timeoutMs=10000;std::thread trigger([&]{std::this_thread::sleep_for(std::chrono::milliseconds(100));cancel=true;});
    const auto cancelled=runProcess(request,cancel);trigger.join();cancel=false;
    require(cancelled.cancelled && !cancelled.passed,"Cancellation terminates a running child");
    request.arguments={"flood"};request.outputBytes=1024;const auto flood=runProcess(request,cancel);
    require(!flood.passed && flood.output.size()<=1024 && flood.diagnostic.find("budget")!=std::string::npos,"Flooding output remains bounded");
    request.arguments={"fail"};require(runProcess(request,cancel).exitCode==7,"Child exit code remains observable");
    request.executable="missing-process.exe";require(!runProcess(request,cancel).diagnostic.empty(),"Missing executable is diagnosed");
    std::cout<<"Controlled process arguments, deadlines, cancellation and output budgets passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    std::vector<std::string> storage;storage.reserve(argc);std::vector<char*> arguments;
    for(int i=0;i<argc;++i){
        const int size=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);
        std::string value(size,'\0');WideCharToMultiByte(CP_UTF8,0,argv[i],-1,value.data(),size,nullptr,nullptr);
        value.pop_back();storage.push_back(std::move(value));
    }
    for(auto& value:storage)arguments.push_back(value.data());return execute(argc,arguments.data());
}
#else
int main(int argc,char** argv){return execute(argc,argv);}
#endif
