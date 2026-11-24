#include "runtime/ModuleAssembly.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace azurerender;
namespace {
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
template<class F> void rejects(F&& action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "Invalid module graph accepted");
}
}
int main() {
    try {
        RuntimeLifecycle runtime;
        std::vector<std::string> calls;
        ModuleAssembly assembly;
        assembly.add({"tool", 1, {}, {"middle"}}, [&](auto&) { calls.push_back("tool+"); }, [&] { calls.push_back("tool-"); });
        assembly.add({"core", 1, {}, {}}, [&](auto&) { calls.push_back("core+"); }, [&] { calls.push_back("core-"); });
        assembly.add({"middle", 1, {}, {"core"}}, [&](auto&) { calls.push_back("middle+"); }, [&] { calls.push_back("middle-"); });
        assembly.start(runtime);
        rejects([&] { assembly.start(runtime); });
        rejects([&] { assembly.add({"late", 1, {}, {}}, [](auto&) {}, [] {}); });
        assembly.stop(); assembly.stop();
        require(calls == std::vector<std::string>{"core+", "middle+", "tool+", "tool-", "middle-", "core-"}, "Dependency lifecycle order changed");
        ModuleAssembly failed;
        calls.clear();
        failed.add({"core", 1, {}, {}}, [&](auto&) { calls.push_back("core+"); }, [&] { calls.push_back("core-"); });
        failed.add({"broken", 1, {}, {"core"}}, [](auto&) { throw std::runtime_error("start failed"); }, [&] { calls.push_back("broken-"); });
        rejects([&] { failed.start(runtime); });
        require(calls == std::vector<std::string>{"core+", "broken-", "core-"}, "Partial start did not roll back");
        require(failed.diagnostics() == std::vector<std::string>{"Module startup failed: broken"}, "Startup diagnostic does not identify the failed module");
        ModuleAssembly cycle;
        cycle.add({"a", 1, {}, {"b"}}, [](auto&) {}, [] {});
        cycle.add({"b", 1, {}, {"a"}}, [](auto&) {}, [] {});
        rejects([&] { cycle.start(runtime); });
        ModuleAssembly missing;
        missing.add({"a", 1, {}, {"missing"}}, [](auto&) {}, [] {});
        rejects([&] { missing.start(runtime); });
        rejects([&] { missing.add({"a", 1, {}, {}}, [](auto&) {}, [] {}); });
        rejects([&] { missing.add({"bad", 2, {}, {}}, [](auto&) {}, [] {}); });
        rejects([&] { missing.add({"", 1, {}, {}}, [](auto&) {}, [] {}); });
        ModuleAssembly closing;
        bool coreClosed = false;
        closing.add({"core", 1, {}, {}}, [](auto&) {}, [&] { coreClosed = true; });
        closing.add({"broken-close", 1, {}, {"core"}}, [](auto&) {}, [] { throw std::runtime_error("close failed"); });
        closing.start(runtime); closing.stop();
        require(coreClosed && closing.diagnostics().size() == 1, "Close failure prevented remaining shutdown");
        std::cout << "Module lifecycle, graph rejection and rollback passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
