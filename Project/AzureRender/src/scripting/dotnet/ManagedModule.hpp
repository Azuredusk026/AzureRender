#pragma once
#include "scripting/ScriptInterop.hpp"
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
namespace azurerender {
// Trusted compiled extensions. Module mappings persist for process lifetime;
// closeSession and collectible CoreCLR contexts own script object lifetimes.
class ManagedModule final {
public:
    ManagedModule(std::string backend,std::filesystem::path module,std::filesystem::path runtimeConfig);
    ~ManagedModule();
    nlohmann::json call(const ScriptHostApi&,const nlohmann::json&);
    double startupMilliseconds() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
