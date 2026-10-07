#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#endif
#include "PathSelection.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace azurerender {
PathSelectionResult choosePath(const PathSelectionRequest& request) {
    PathSelectionResult result;
#ifdef _WIN32
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE){result.diagnostic="Cannot initialize Windows path selection";return result;}
    IFileOpenDialog* dialog=nullptr;auto status=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog));
    if(SUCCEEDED(status)){
        DWORD flags=0;dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|(request.directory?FOS_PICKFOLDERS:FOS_FILEMUSTEXIST));
        const auto title=std::filesystem::u8path(request.title).wstring();dialog->SetTitle(title.c_str());
        if(!request.initialDirectory.empty()){
            IShellItem* folder=nullptr;if(SUCCEEDED(SHCreateItemFromParsingName(request.initialDirectory.c_str(),nullptr,IID_PPV_ARGS(&folder)))){dialog->SetFolder(folder);folder->Release();}
        }
        std::wstring extensions;for(const auto& extension:request.extensions){if(!extensions.empty())extensions+=L";";extensions+=L"*"+std::filesystem::u8path(extension).wstring();}
        const COMDLG_FILTERSPEC filter{L"Supported files",extensions.c_str()};if(!request.directory&&!extensions.empty())dialog->SetFileTypes(1,&filter);
        status=dialog->Show(static_cast<HWND>(request.owner));
        if(status==HRESULT_FROM_WIN32(ERROR_CANCELLED))result.cancelled=true;
        else if(SUCCEEDED(status)){
            IShellItem* item=nullptr;status=dialog->GetResult(&item);
            if(SUCCEEDED(status)){PWSTR path=nullptr;status=item->GetDisplayName(SIGDN_FILESYSPATH,&path);if(SUCCEEDED(status)){result.path=path;CoTaskMemFree(path);}item->Release();}
        }
        dialog->Release();
    }
    if(SUCCEEDED(initialized))CoUninitialize();
    if(FAILED(status)&&!result.cancelled)result.diagnostic="Windows path selection failed: "+std::to_string(static_cast<unsigned long>(status));
#else
    (void)request;result.diagnostic="Native path selection requires the Windows editor";
#endif
    return result;
}
void PathHistory::remember(const std::string& purpose,const std::filesystem::path& path,bool directory){
    if(purpose.empty()||path.empty())throw std::invalid_argument("Path history requires a purpose and a path");
    const auto folder=directory?path:path.parent_path();if(!std::filesystem::is_directory(folder))throw std::invalid_argument("Selected directory does not exist");
    const auto value=std::filesystem::weakly_canonical(folder).u8string();auto& history=directories_[purpose];history.erase(std::remove(history.begin(),history.end(),value),history.end());history.insert(history.begin(),value);if(history.size()>10)history.resize(10);
}
std::vector<std::string> PathHistory::directories(const std::string& purpose) const{const auto found=directories_.find(purpose);return found==directories_.end()?std::vector<std::string>{}:found->second;}
void PathHistory::load(const std::filesystem::path& file){
    if(!std::filesystem::is_regular_file(file))return;std::ifstream input(file);nlohmann::json document;input>>document;
    if(document.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported path history version");
    auto candidate=document.at("directories").get<std::map<std::string,std::vector<std::string>>>();
    for(auto& [purpose,history]:candidate)if(history.size()>10)history.resize(10);directories_=std::move(candidate);
}
void PathHistory::save(const std::filesystem::path& file) const{
    std::filesystem::create_directories(file.parent_path());std::ofstream output(file);output<<nlohmann::json{{"schemaVersion",1},{"directories",directories_}}.dump(2);output.flush();if(!output)throw std::runtime_error("Cannot save path history");
}
}
