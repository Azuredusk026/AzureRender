#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <nlohmann/json.hpp>
namespace azurerender {
struct BindingDescriptor {
    const char* name;
    const char* result;
    bool mutating;
    std::array<const char*,3> parameterTypes;
    std::size_t arity;
};
const BindingDescriptor& scriptBinding(const std::string& name);
void validateScriptArguments(const BindingDescriptor&,const nlohmann::json&);
}
