#pragma once
#include "extensions/ExtensionDescriptor.hpp"
#include <functional>
#include <string>
#include <thread>
#include <vector>
namespace azurerender {
class RuntimeLifecycle;
class ModuleAssembly final {
public:
    using Start = std::function<void(RuntimeLifecycle&)>;
    using Stop = std::function<void()>;
    ModuleAssembly() = default;
    ~ModuleAssembly() { stop(); }
    ModuleAssembly(const ModuleAssembly&) = delete;
    ModuleAssembly& operator=(const ModuleAssembly&) = delete;
    void add(ExtensionDescriptor descriptor, Start start, Stop stop);
    void start(RuntimeLifecycle& runtime);
    void stop() noexcept;
    bool running() const noexcept { return running_; }
    const std::vector<std::string>& diagnostics() const noexcept { return diagnostics_; }
private:
    struct Entry { ExtensionDescriptor descriptor; Start start; Stop stop; };
    void checkThread() const;
    std::vector<Entry> entries_;
    std::vector<std::size_t> active_;
    std::vector<std::string> diagnostics_;
    std::thread::id owner_ = std::this_thread::get_id();
    bool sealed_ = false, running_ = false;
};
}
