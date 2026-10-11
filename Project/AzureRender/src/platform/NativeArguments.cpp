#include "platform/NativeArguments.hpp"
#include <stdexcept>
#include <memory>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif
namespace azurerender {
std::vector<std::string> nativeArguments(int argc,char** argv) {
    std::vector<std::string> result;
#ifdef _WIN32
    static_cast<void>(argc);static_cast<void>(argv);
    int count=0;auto* raw=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!raw)throw std::runtime_error("Cannot read native process arguments");
    const auto release=[](wchar_t** value){LocalFree(value);};
    std::unique_ptr<wchar_t*,decltype(release)> arguments(raw,release);
    for(int i=0;i<count;++i) {
        const auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,raw[i],-1,nullptr,0,nullptr,nullptr);
        if(!size)throw std::runtime_error("Cannot encode process argument as UTF-8");
        std::string text(static_cast<std::size_t>(size),'\0');
        if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,raw[i],-1,text.data(),size,nullptr,nullptr))throw std::runtime_error("Cannot encode process argument as UTF-8");
        text.pop_back();result.push_back(std::move(text));
    }
#else
    for(int i=0;i<argc;++i)result.emplace_back(argv[i]);
#endif
    return result;
}
}
