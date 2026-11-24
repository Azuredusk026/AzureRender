#pragma once
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
namespace azurerender {
// Environment keys are ASCII; paths preserve the platform's native encoding.
inline std::optional<std::filesystem::path> environmentPath(const char* name) {
#ifdef _WIN32
    const std::string key(name);const std::wstring wide(key.begin(),key.end());
    if(const auto* value=_wgetenv(wide.c_str()))if(*value)return std::filesystem::path(value);
#else
    if(const auto* value=std::getenv(name))if(*value)return std::filesystem::u8path(value);
#endif
    return std::nullopt;
}
}
