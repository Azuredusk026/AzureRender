#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
namespace {
std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\"";size_t slashes=0;
    for(auto c:value) {if(c==L'\\'){++slashes;continue;}result.append(c==L'\"'?slashes*2+1:slashes,L'\\');result+=c;slashes=0;}
    result.append(slashes*2,L'\\');return result+L"\"";
}
std::filesystem::path inside(const std::filesystem::path& root,const std::string& relative) {
    const auto path=std::filesystem::u8path(relative);if(path.empty()||path.is_absolute())throw std::runtime_error("Invalid package path");
    for(const auto& part:path)if(part=="..")throw std::runtime_error("Package path escapes its directory");
    const auto result=std::filesystem::weakly_canonical(root/path);
    const auto check=result.lexically_relative(root);if(check.empty()||*check.begin()=="..")throw std::runtime_error("Package path escapes its directory");
    if(!std::filesystem::exists(result))throw std::runtime_error("Missing game dependency: "+relative);return result;
}
std::filesystem::path defaultSaveRoot() {
    const auto required=GetEnvironmentVariableW(L"AZURERENDER_SAVE_ROOT",nullptr,0);
    if(required) {
        std::wstring value(required,L'\0');
        const auto count=GetEnvironmentVariableW(L"AZURERENDER_SAVE_ROOT",value.data(),required);
        if(!count || count>=required)throw std::runtime_error("Game save root environment changed during startup");
        value.resize(count);const std::filesystem::path path(value);
        if(!path.is_absolute())throw std::runtime_error("AZURERENDER_SAVE_ROOT requires an absolute directory");
        return path;
    }
    struct Apartment {
        HRESULT status=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
        ~Apartment(){if(SUCCEEDED(status))CoUninitialize();}
    } apartment;
    if(FAILED(apartment.status)&&apartment.status!=RPC_E_CHANGED_MODE)
        throw std::runtime_error("Cannot initialize game save folder lookup");
    struct Folder { PWSTR value=nullptr; ~Folder(){CoTaskMemFree(value);} } folder;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&folder.value)) || !folder.value)
        throw std::runtime_error("Cannot locate the local application data folder");
    return std::filesystem::path(folder.value)/"AzureRender"/"GameSaves";
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    bool showError=true,explicitSaveRoot=false;int count=0;auto values=CommandLineToArgvW(GetCommandLineW(),&count);
    try {
        if(!values)throw std::runtime_error("Cannot read game arguments");
        for(int i=1;i<count;++i){
            if(std::wstring(values[i])==L"--no-error-dialog")showError=false;
            if(std::wstring(values[i])==L"--save-root")explicitSaveRoot=true;
        }
        std::wstring buffer(32768,L'\0');const auto length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));if(!length)throw std::runtime_error("Cannot locate game executable");buffer.resize(length);
        const auto root=std::filesystem::weakly_canonical(std::filesystem::path(buffer).parent_path());
        const auto descriptor=root/"game-launch.json";if(!std::filesystem::is_regular_file(descriptor)||std::filesystem::file_size(descriptor)>65536)throw std::runtime_error("Missing or invalid game-launch.json");
        nlohmann::json config;{std::ifstream input(descriptor);input>>config;}if(config.at("schemaVersion")!=1)throw std::runtime_error("Unsupported game launch version");
        const auto player=inside(root,config.at("player"));const auto project=inside(root,config.at("project"));const auto resources=inside(root,config.at("resources"));
        std::wstring command=quote(player.wstring())+L" --project "+quote(project.wstring())+L" --resource-root "+quote(resources.wstring());
        if(!explicitSaveRoot)command+=L" --save-root "+quote(defaultSaveRoot().wstring());
        if(config.contains("width")){command+=L" --width "+std::to_wstring(config.at("width").get<int>())+L" --height "+std::to_wstring(config.at("height").get<int>());}
        for(int i=1;i<count;++i)if(std::wstring(values[i])!=L"--no-error-dialog")command+=L" "+quote(values[i]);
        std::filesystem::create_directories(root/"captures");
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        const auto log=CreateFileW((root/"captures/game-startup.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&attributes,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(log==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open game startup log");
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=startup.hStdError=log;
        startup.hStdInput=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,0,nullptr);
        PROCESS_INFORMATION process{};
        const bool started=CreateProcessW(player.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process)!=0;
        const auto error=GetLastError();CloseHandle(log);if(startup.hStdInput!=INVALID_HANDLE_VALUE)CloseHandle(startup.hStdInput);
        if(!started)throw std::runtime_error("Cannot start packaged runtime; Windows error "+std::to_string(error));
        CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
        if(code!=0&&showError)MessageBoxW(nullptr,L"The game runtime failed. See captures/game-startup.log for details.",L"AzureRender game startup",MB_OK|MB_ICONERROR);
        LocalFree(values);return static_cast<int>(code);
    }catch(const std::exception& error) {
        if(values)LocalFree(values);
        if(showError){const auto text=std::filesystem::u8path(error.what()).wstring();MessageBoxW(nullptr,text.c_str(),L"AzureRender game startup",MB_OK|MB_ICONERROR);}
        return 1;
    }
}
