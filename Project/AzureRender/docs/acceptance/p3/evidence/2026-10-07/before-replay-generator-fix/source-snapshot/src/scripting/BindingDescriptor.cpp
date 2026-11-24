#include "scripting/GeneratedBindings.hpp"
#include <cmath>
#include <stdexcept>
#include <string_view>
namespace azurerender {
namespace {
void value(const nlohmann::json& input,unsigned depth=0) {
    if(depth>16)throw std::invalid_argument("Script value nesting exceeds limit");
    if(input.is_number()) {
        if(!std::isfinite(input.get<double>()))throw std::invalid_argument("Script number must be finite");
    } else if(input.is_array()) {
        if(input.size()>1024)throw std::invalid_argument("Script array exceeds limit");
        for(const auto& entry:input)value(entry,depth+1);
    } else if(!input.is_boolean() && !input.is_string())
        throw std::invalid_argument("Script values require a scalar or dense array");
}
}
const BindingDescriptor& scriptBinding(const std::string& name) {
    for(const auto& binding:kScriptBindings)if(name==binding.name)return binding;
    throw std::invalid_argument("Unknown script binding: "+name);
}
void validateScriptArguments(const BindingDescriptor& binding,const nlohmann::json& args) {
    if(!args.is_array() || args.size()!=binding.arity)throw std::invalid_argument("Script binding arity mismatch");
    if(args.dump().size()>1024*1024)throw std::invalid_argument("Script arguments exceed one MiB");
    for(std::size_t i=0;i<args.size();++i) {
        const std::string_view type=binding.parameterTypes[i];const auto& arg=args[i];
        if((type=="string" && !arg.is_string()) || (type=="boolean" && !arg.is_boolean()) ||
           (type=="number" && !arg.is_number()) || (type=="array" && !arg.is_array()))
            throw std::invalid_argument("Script binding parameter type mismatch");
        value(arg);
    }
}
}
