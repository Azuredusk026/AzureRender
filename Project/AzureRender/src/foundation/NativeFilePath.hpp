#pragma once

#include <filesystem>

namespace azurerender {

inline std::filesystem::path nativeFilePath(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto value = std::filesystem::absolute(path).lexically_normal().native();
    if (value.rfind(L"\\\\?\\", 0) == 0) return value;
    if (value.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + value.substr(2);
    return L"\\\\?\\" + value;
#else
    return path;
#endif
}

}
