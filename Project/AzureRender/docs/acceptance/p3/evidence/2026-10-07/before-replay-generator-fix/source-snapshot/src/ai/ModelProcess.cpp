#include "ModelClient.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace azurerender {
namespace {
using Clock=std::chrono::steady_clock;
std::shared_future<ModelResponse> failure(std::string message){std::promise<ModelResponse> result;result.set_value({false,{},std::move(message),{}});return result.get_future().share();}
#ifdef _WIN32
std::wstring quote(const std::wstring& value){
    std::wstring result=L"\"";std::size_t slashes=0;
    for(const auto character:value){if(character==L'\\'){++slashes;continue;}result.append(character==L'"'?slashes*2+1:slashes,L'\\');result+=character;slashes=0;}
    result.append(slashes*2,L'\\');return result+L'"';
}
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    void close(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=nullptr;}
};
class Process {
public:
    explicit Process(const ModelProcessOptions& options,Clock::time_point deadline,const std::atomic<bool>& cancelled){
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        Handle inputRead,outputWrite,error;
        if(!CreatePipe(&inputRead.value,&input_.value,&security,0)||!CreatePipe(&output_.value,&outputWrite.value,&security,0))
            throw std::runtime_error("Model pipe creation failed");
        if(!SetHandleInformation(input_.value,HANDLE_FLAG_INHERIT,0)||!SetHandleInformation(output_.value,HANDLE_FLAG_INHERIT,0))
            throw std::runtime_error("Model pipe ownership failed");
        error.value=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        if(error.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Model diagnostic handle failed");
        STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput=inputRead.value;startup.StartupInfo.hStdOutput=outputWrite.value;startup.StartupInfo.hStdError=error.value;
        SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<unsigned char> attributes(bytes);
        startup.lpAttributeList=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&bytes))throw std::runtime_error("Model process attributes failed");
        struct AttributeGuard{PPROC_THREAD_ATTRIBUTE_LIST value;~AttributeGuard(){DeleteProcThreadAttributeList(value);}} guard{startup.lpAttributeList};
        HANDLE inherited[]={inputRead.value,outputWrite.value,error.value};
        if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr))
            throw std::runtime_error("Model inherited handles failed");
        std::wstring command=quote(options.executable.wstring());
        for(const auto& argument:options.arguments)command+=L" "+quote(std::filesystem::u8path(argument).wstring());
        PROCESS_INFORMATION child{};
        const auto directory=options.workingDirectory.wstring();
        if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,
            directory.empty()?nullptr:directory.c_str(),&startup.StartupInfo,&child))throw std::runtime_error("Optional model process unavailable");
        process_.value=child.hProcess;CloseHandle(child.hThread);
        monitor_=std::thread([this,deadline,&cancelled]{
            while(!finished_){
                if(cancelled.load()||Clock::now()>=deadline){TerminateProcess(process_.value,1);return;}
                if(WaitForSingleObject(process_.value,5)==WAIT_OBJECT_0)return;
            }
        });
    }
    ~Process(){finished_=true;if(monitor_.joinable())monitor_.join();input_.close();if(process_.value&&WaitForSingleObject(process_.value,100)!=WAIT_OBJECT_0){TerminateProcess(process_.value,1);WaitForSingleObject(process_.value,5000);}}
    void write(const nlohmann::json& frame){
        const auto bytes=frame.dump()+"\n";if(bytes.size()>8*1024*1024)throw std::invalid_argument("Model output frame exceeds budget");
        // The bridge reads continuously; one bounded request fits its pipe consumer.
        std::size_t offset=0;
        while(offset<bytes.size()){
            DWORD written=0;const auto count=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,65536));
            if(!WriteFile(input_.value,bytes.data()+offset,count,&written,nullptr)||written==0)throw std::runtime_error("Model input pipe closed");
            offset+=written;
        }
    }
    std::string line(Clock::time_point deadline,const std::atomic<bool>& cancelled){
        while(true){
            if(cancelled.load())throw std::runtime_error("Cancelled: Model request cancelled");
            if(Clock::now()>=deadline)throw std::runtime_error("Timeout: Model request deadline exceeded");
            const auto newline=buffer_.find('\n');
            if(newline!=std::string::npos){auto result=buffer_.substr(0,newline);buffer_.erase(0,newline+1);return result;}
            DWORD available=0;
            if(!PeekNamedPipe(output_.value,nullptr,0,nullptr,&available,nullptr))throw std::runtime_error("Model output pipe closed");
            if(available){
                char bytes[65536];DWORD read=0;
                if(!ReadFile(output_.value,bytes,std::min<DWORD>(available,sizeof(bytes)),&read,nullptr)||!read)throw std::runtime_error("Model output pipe closed");
                buffer_.append(bytes,read);
                if(buffer_.size()>8*1024*1024)throw std::runtime_error("Model input frame exceeds budget");
            }else{
                if(WaitForSingleObject(process_.value,0)==WAIT_OBJECT_0)throw std::runtime_error("Model process exited before its response");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }
private:
    Handle process_,input_,output_;
    std::string buffer_;
    std::atomic<bool> finished_{false};
    std::thread monitor_;
};
#endif
}
struct ModelProcess::State {
    ModelProcessOptions options;
    std::mutex mutex;
    std::atomic<bool> busy{false},cancelled{false};
    std::string runId;
    std::thread worker;
};
ModelProcess::ModelProcess(ModelProcessOptions options):state_(std::make_unique<State>()){state_->options=std::move(options);}
ModelProcess::~ModelProcess(){state_->cancelled=true;if(state_->worker.joinable())state_->worker.join();}
std::shared_future<ModelResponse> ModelProcess::request(const ModelRequest& request){
    try{validateModelRequest(request);}catch(const std::exception& error){return failure(error.what());}
    std::lock_guard<std::mutex> guard(state_->mutex);
    if(state_->busy)return failure("Model transport already owns a request");
    if(state_->worker.joinable())state_->worker.join();
    auto promise=std::make_shared<std::promise<ModelResponse>>();auto future=promise->get_future().share();
    state_->busy=true;state_->cancelled=false;state_->runId=request.runId;
    state_->worker=std::thread([this,request,promise]{
        ModelResponse response;
        const auto deadline=Clock::now()+std::chrono::milliseconds(request.timeoutMs);
        try{
#ifdef _WIN32
            Process process(state_->options,deadline,state_->cancelled);
            process.write({{"jsonrpc","2.0"},{"id",1},{"method","initialize"},{"params",{{"protocolVersion",1}}}});
            validateModelHandshake(nlohmann::json::parse(process.line(deadline,state_->cancelled)),1);
            process.write({{"jsonrpc","2.0"},{"id",2},{"method","llm.chat"},{"params",{{"runId",request.runId},{"prompt",request.prompt},
                {"schema",request.schema},{"history",request.history},{"profile",request.profile},{"timeoutMs",request.timeoutMs}}}});
            ModelFrameDecoder decoder(2,request.runId);
            try{
                while(true){if(auto result=decoder.accept(process.line(deadline,state_->cancelled))){response=std::move(*result);break;}}
            }catch(...){
                if(state_->cancelled)try{process.write({{"jsonrpc","2.0"},{"id",3},{"method","run.cancel"},{"params",{{"runId",request.runId}}}});}catch(const std::exception&){}
                throw;
            }
            if(!state_->cancelled)process.write({{"jsonrpc","2.0"},{"id",4},{"method","shutdown"},{"params",nlohmann::json::object()}});
#else
            response.diagnostic="Optional model process requires Windows";
#endif
        }catch(const std::exception& error){response.passed=false;response.diagnostic=state_->cancelled?"Cancelled: Model request cancelled":Clock::now()>=deadline?"Timeout: Model request deadline exceeded":error.what();}
        state_->busy=false;promise->set_value(std::move(response));
    });
    return future;
}
void ModelProcess::cancel(const std::string& runId){std::lock_guard<std::mutex> guard(state_->mutex);if(state_->busy&&state_->runId==runId)state_->cancelled=true;}
}
