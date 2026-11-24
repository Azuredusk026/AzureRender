#pragma once
#include <cstdint>
namespace azurerender {
inline constexpr std::uint32_t kScriptInteropByteLimit=1024*1024;
using ScriptInvoke=std::int32_t (*)(std::uint64_t,const std::uint8_t*,std::uint32_t,
    std::uint8_t*,std::uint32_t,std::uint32_t*) noexcept;
struct ScriptHostApi {
    std::uint32_t version=1;
    std::uint32_t size=sizeof(ScriptHostApi);
    std::uint64_t session=0;
    ScriptInvoke invoke=nullptr;
};
}
