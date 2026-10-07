#pragma once
#include "runtime/IRuntimeSystem.hpp"
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
namespace azurerender {
class SystemRegistry {
public:
    using Json = nlohmann::json;
    using Factory = std::function<std::unique_ptr<IRuntimeSystem>(const Json&)>;
    void add(const std::string& id, Factory factory);
    std::unique_ptr<IRuntimeSystem> create(const std::string& id, const Json& config) const;
    std::vector<std::string> ids() const;
private:
    std::map<std::string, Factory> factories_;
};
}
