#include "scripting/dotnet/ManagedModule.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <chrono>
#include <cstdlib>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace azurerender {
namespace {
using Entry=std::int32_t (__cdecl *)(const ScriptHostApi*,const std::uint8_t*,std::uint32_t,std::uint8_t*,std::uint32_t,std::uint32_t*);
std::mutex modulesMutex;
std::map<std::wstring,Entry> loaded;
HMODULE library(const std::filesystem::path& path) {
    const auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module)throw std::runtime_error("Cannot load managed module: "+path.u8string()+" (Win32 "+std::to_string(GetLastError())+")");
    return module;
}
template<class T>T symbol(HMODULE module,const char* name) {
    const auto proc=GetProcAddress(module,name);if(!proc)throw std::runtime_error(std::string("Missing managed export: ")+name);
    return reinterpret_cast<T>(proc);
}
std::filesystem::path hostfxr() {
    std::filesystem::path root=L"C:/Program Files/dotnet";
    wchar_t* configured=nullptr;std::size_t length=0;
    if(_wdupenv_s(&configured,&length,L"DOTNET_ROOT")==0 && configured){root=configured;std::free(configured);}
    const auto fxr=root/"host/fxr";
    std::filesystem::path best;
    if(std::filesystem::is_directory(fxr))for(const auto& item:std::filesystem::directory_iterator(fxr))
        if(item.is_directory() && std::filesystem::is_regular_file(item.path()/"hostfxr.dll") && item.path().filename()>best.filename())best=item.path();
    if(best.empty())throw std::runtime_error("CoreCLR requires an installed .NET 9 runtime or DOTNET_ROOT");
    return best/"hostfxr.dll";
}
Entry load(const std::string& backend,const std::filesystem::path& module,const std::filesystem::path& config) {
    const auto key=std::filesystem::canonical(module).wstring();
    std::lock_guard<std::mutex> lock(modulesMutex);const auto found=loaded.find(key);if(found!=loaded.end())return found->second;
    Entry entry=nullptr;
    if(backend=="nativeaot")entry=symbol<Entry>(library(module),"AzureScriptEntry");
    else if(backend=="coreclr") {
        const auto host=library(hostfxr());
        using Initialize=std::int32_t (__cdecl *)(const wchar_t*,const void*,void**);
        using Delegate=std::int32_t (__cdecl *)(void*,int,void**);
        using Close=std::int32_t (__cdecl *)(void*);
        using Load=std::int32_t (__stdcall *)(const wchar_t*,const wchar_t*,const wchar_t*,const void*,void*,void**);
        auto initialize=symbol<Initialize>(host,"hostfxr_initialize_for_runtime_config");
        auto get=symbol<Delegate>(host,"hostfxr_get_runtime_delegate");auto close=symbol<Close>(host,"hostfxr_close");
        void* context=nullptr;
        if(initialize(config.c_str(),nullptr,&context)<0 || !context)throw std::runtime_error("CoreCLR runtime configuration rejected");
        void* delegate=nullptr;const auto status=get(context,5,&delegate);close(context);
        if(status<0 || !delegate)throw std::runtime_error("CoreCLR entry loader unavailable");
        void* pointer=nullptr;
        const auto loader=reinterpret_cast<Load>(delegate);
        if(loader(module.c_str(),L"Azure.Engine.Entry, Azure.Engine",L"Invoke",reinterpret_cast<void*>(static_cast<std::intptr_t>(-1)),nullptr,&pointer)<0 || !pointer)
            throw std::runtime_error("CoreCLR managed entry rejected");
        entry=reinterpret_cast<Entry>(pointer);
    } else throw std::invalid_argument("Unknown managed backend");
    loaded.emplace(key,entry);return entry;
}
}
struct ManagedModule::Impl {
    Entry entry;
    double startup=0;
    std::vector<std::uint8_t> response=std::vector<std::uint8_t>(kScriptInteropByteLimit);
    std::thread::id owner=std::this_thread::get_id();
};
ManagedModule::ManagedModule(std::string backend,std::filesystem::path module,std::filesystem::path config):impl_(std::make_unique<Impl>()) {
    const auto start=std::chrono::steady_clock::now();impl_->entry=load(backend,std::filesystem::absolute(module),std::filesystem::absolute(config));
    impl_->startup=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
ManagedModule::~ManagedModule()=default;
double ManagedModule::startupMilliseconds() const noexcept{return impl_->startup;}
nlohmann::json ManagedModule::call(const ScriptHostApi& api,const nlohmann::json& request) {
    if(impl_->owner!=std::this_thread::get_id())throw std::logic_error("Managed module owner thread required");
    const auto bytes=request.dump();if(bytes.empty() || bytes.size()>kScriptInteropByteLimit)throw std::length_error("Managed request byte budget exceeded");
    std::uint32_t written=0;
    const auto status=impl_->entry(&api,reinterpret_cast<const std::uint8_t*>(bytes.data()),static_cast<std::uint32_t>(bytes.size()),
        impl_->response.data(),static_cast<std::uint32_t>(impl_->response.size()),&written);
    if(written==0 || written>impl_->response.size())throw std::runtime_error("Managed response ownership violation");
    const auto response=nlohmann::json::parse(impl_->response.begin(),impl_->response.begin()+written);
    if(status!=0 || !response.at("ok").get<bool>())throw std::runtime_error("Managed script: "+response.value("error",std::string("Unknown error")));
    return response.at("value");
}
}
