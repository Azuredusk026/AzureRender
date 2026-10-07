#include "editor/commands/EditRegistry.hpp"
#include <cmath>
namespace azurerender {
void EditRegistry::add(EditDescriptor descriptor,Handler handler) {
    if(descriptor.id.empty()||descriptor.version!=1||!handler||!descriptor.parameters.is_object())
        throw std::invalid_argument("Invalid edit registration");
    const auto id=descriptor.id;
    if(!entries_.emplace(id,Entry{std::move(descriptor),std::move(handler)}).second)
        throw std::invalid_argument("Duplicate edit operation: "+id);
}
const EditRegistry::Entry* EditRegistry::find(const std::string& id) const {
    const auto entry=entries_.find(id);return entry==entries_.end()?nullptr:&entry->second;
}
nlohmann::json EditRegistry::describe() const {
    auto result=nlohmann::json::array();
    for(const auto& item:entries_) { const auto& d=item.second.descriptor;
        result.push_back({{"id",d.id},{"version",d.version},{"parameters",d.parameters},
            {"modifiesDocument",d.modifiesDocument},{"transactional",d.transactional},{"requiresIdle",d.requiresIdle}});
    }
    return result;
}
void EditRegistry::validate(const nlohmann::json& value,const nlohmann::json& schema) {
    if(schema.empty())return;
    const auto type=schema.value("type",std::string());
    if((type=="object"&&!value.is_object())||(type=="array"&&!value.is_array())||
       (type=="string"&&!value.is_string())||(type=="boolean"&&!value.is_boolean())||
       (type=="number"&&(!value.is_number()||!std::isfinite(value.get<double>())))||
       (type=="integer"&&!value.is_number_integer()))throw EditRejection("Parameter type mismatch: "+type);
    if(value.is_number()) {
        const auto number=value.get<double>();
        if(!std::isfinite(number)
            ||(schema.contains("minimum")&&number<schema.at("minimum").get<double>())
            ||(schema.contains("maximum")&&number>schema.at("maximum").get<double>()))
            throw EditRejection("Parameter number is outside its declared range");
    }
    if(value.is_object()) {
        const auto fields=schema.value("properties",nlohmann::json::object());
        for(const auto& name:schema.value("required",nlohmann::json::array()))
            if(!value.contains(name.get<std::string>()))throw EditRejection("Missing parameter: "+name.get<std::string>());
        for(const auto& field:value.items()) {
            if(!fields.contains(field.key()))throw EditRejection("Unknown parameter: "+field.key());
            validate(field.value(),fields.at(field.key()));
        }
    }
    if(value.is_array()) {
        if(value.size()<schema.value("minItems",std::size_t(0))||value.size()>schema.value("maxItems",std::size_t(1024)))
            throw EditRejection("Invalid parameter array size");
        if(schema.contains("items"))for(const auto& item:value)validate(item,schema.at("items"));
    }
    if(value.is_string()&&value.get_ref<const std::string&>().size()>schema.value("maxLength",std::size_t(4096)))
        throw EditRejection("Parameter text exceeds its budget");
}
}
