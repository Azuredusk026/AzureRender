#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace azurerender {
enum class ShaderReloadState { Idle, Compiling, Ready, Error, Cancelled };
struct ShaderProgramDescriptor {
    std::filesystem::path source, output;
    std::vector<std::string> defines;
};
struct ShaderReloadOptions {
    std::filesystem::path sourceRoot, binaryRoot, scratchRoot, compiler;
    std::vector<ShaderProgramDescriptor> programs;
    std::uint32_t pollIntervalMs=500, timeoutMs=60000;
    std::size_t sourceBytes=16*1024*1024;
};
struct ShaderReloadCandidate {
    std::uint64_t generation=0;
    std::filesystem::path directory;
    std::string fingerprint;
};
struct ShaderReloadStatus {
    ShaderReloadState state=ShaderReloadState::Idle;
    std::uint64_t activeGeneration=0;
    std::string diagnostic;
};
class IShaderHotReloader {
public:
    virtual ~IShaderHotReloader()=default;
    virtual void requestRebuild()=0;
    virtual void poll()=0;
    virtual ShaderReloadStatus status() const=0;
    virtual std::optional<ShaderReloadCandidate> candidate() const=0;
    // Owner calls this only after candidate creation and GPU retirement.
    virtual void finishCandidate(bool accepted,std::string diagnostic)=0;
    virtual void cancel()=0;
};
}
