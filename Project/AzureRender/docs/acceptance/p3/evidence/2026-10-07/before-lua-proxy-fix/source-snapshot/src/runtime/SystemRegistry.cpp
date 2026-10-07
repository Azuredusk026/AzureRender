#include "runtime/SystemRegistry.hpp"
#include <stdexcept>
namespace azurerender {
void SystemRegistry::add(const std::string& id, Factory factory) {
    if(id.empty() || !factory) throw std::invalid_argument("Invalid runtime system registration");
    if(!factories_.emplace(id,std::move(factory)).second) throw std::invalid_argument("Duplicate runtime system: " + id);
}
std::unique_ptr<IRuntimeSystem> SystemRegistry::create(const std::string& id,const Json& config) const {
    const auto found=factories_.find(id);
    if(found==factories_.end()) throw std::invalid_argument("Unknown runtime system: " + id);
    if(!config.is_object()) throw std::invalid_argument("System configuration must be an object: " + id);
    auto result=found->second(config);
    if(!result) throw std::invalid_argument("System factory returned null: " + id);
    return result;
}
std::vector<std::string> SystemRegistry::ids() const {
    std::vector<std::string> result; for(const auto& item:factories_)result.push_back(item.first);return result;
}
}
