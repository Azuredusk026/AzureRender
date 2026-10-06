#pragma once
#include "render/IShaderHotReloader.hpp"
#include <atomic>
#include <map>
namespace azurerender {
struct ShaderSourceSnapshot {
    std::map<std::filesystem::path,std::string> files;
    std::string fingerprint;
};
struct ShaderCompileResult {
    bool passed=false,cancelled=false;
    ShaderReloadCandidate candidate;
    std::string diagnostic;
};
ShaderSourceSnapshot snapshotShaders(const ShaderReloadOptions&);
ShaderCompileResult compileShaderCandidate(const ShaderReloadOptions&,const ShaderSourceSnapshot&,
    std::uint64_t generation,const std::atomic<bool>& cancelled);
}
