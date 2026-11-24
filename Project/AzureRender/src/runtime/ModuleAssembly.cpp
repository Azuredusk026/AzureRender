#include "runtime/ModuleAssembly.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>
namespace azurerender {
void ModuleAssembly::checkThread() const {
    if (owner_ != std::this_thread::get_id()) throw std::logic_error("Module assembly requires its owner thread");
}
void ModuleAssembly::add(ExtensionDescriptor descriptor, Start start, Stop stop) {
    checkThread();
    if (sealed_) throw std::logic_error("Module graph is sealed");
    if (descriptor.id.empty() || descriptor.apiVersion != 1 || !start || !stop)
        throw std::invalid_argument("Invalid module descriptor or lifecycle callbacks");
    for (const auto& entry : entries_)
        if (entry.descriptor.id == descriptor.id) throw std::invalid_argument("Duplicate module: " + descriptor.id);
    entries_.push_back({std::move(descriptor), std::move(start), std::move(stop)});
}
void ModuleAssembly::start(RuntimeLifecycle& runtime) {
    checkThread();
    if (sealed_) throw std::logic_error("Module assembly can start only once");
    std::map<std::string, std::size_t> indices;
    for (std::size_t i = 0; i < entries_.size(); ++i) indices.emplace(entries_[i].descriptor.id, i);
    std::vector<unsigned char> marks(entries_.size(), 0);
    std::vector<std::size_t> order;
    std::function<void(std::size_t)> visit = [&](std::size_t index) {
        if (marks[index] == 2) return;
        if (marks[index] == 1) throw std::invalid_argument("Module dependency cycle: " + entries_[index].descriptor.id);
        marks[index] = 1;
        for (const auto& dependency : entries_[index].descriptor.dependencies) {
            auto found = indices.find(dependency);
            if (found == indices.end()) throw std::invalid_argument("Missing module dependency: " + dependency);
            visit(found->second);
        }
        marks[index] = 2; order.push_back(index);
    };
    for (std::size_t i = 0; i < entries_.size(); ++i) visit(i);
    active_.reserve(order.size());
    sealed_ = true;
    try {
        for (auto index : order) {
            active_.push_back(index);
            try { entries_[index].start(runtime); }
            catch (...) {
                // Preserve the original startup exception even if diagnostics cannot allocate.
                try { diagnostics_.push_back("Module startup failed: " + entries_[index].descriptor.id); } catch (...) {}
                throw;
            }
        }
        running_ = true;
    } catch (...) { stop(); throw; }
}
void ModuleAssembly::stop() noexcept {
    while (!active_.empty()) {
        const auto index = active_.back(); active_.pop_back();
        try { entries_[index].stop(); }
        catch (...) {
            try { diagnostics_.push_back("Module shutdown failed: " + entries_[index].descriptor.id); } catch (...) {}
        }
    }
    running_ = false;
}
}
