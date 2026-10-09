#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace azurerender::reflection {
using Json = nlohmann::json;
enum class Kind { Number, Boolean, String, Vector3 };
struct Property {
    std::string name, label;
    Kind kind;
    double minimum, maximum;
    std::function<Json(const void*)> read;
    std::function<void(void*, const Json&)> write;
    std::string category, tooltip;
    bool readOnly = false, toolVisible = true;
    std::function<void(const Json&)> validate;
    std::string reference, referenceDefault;
    std::vector<std::string> assetTypes;
    std::string unit;
    unsigned precision=3;
    bool batchEditable=true;
};
struct Type {
    std::string name;
    std::uint64_t id;
    unsigned version;
    std::vector<Property> properties;
    std::function<void(void*, const Json&)> assign;
};
inline std::uint64_t stableId(const std::string& name) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char c : name) { hash ^= c; hash *= 1099511628211ULL; }
    return hash;
}
template<class T, class F>
Property property(std::string name, std::string label, F T::* member, double minimum, double maximum) {
    constexpr bool vector = std::is_same_v<F, std::array<float, 3>>;
    constexpr Kind kind = vector ? Kind::Vector3 : std::is_same_v<F, bool> ? Kind::Boolean
        : std::is_same_v<F, std::string> ? Kind::String : Kind::Number;
    Property result{std::move(name), std::move(label), kind, minimum, maximum,
        [member](const void* object) { return Json(static_cast<const T*>(object)->*member); },
        [member, minimum, maximum](void* object, const Json& value) {
            const auto number = [&](const Json& v) {
                if (!v.is_number()) throw std::invalid_argument("Expected numeric property");
                const double n = v.get<double>();
                if (!std::isfinite(n) || n < minimum || n > maximum)
                    throw std::out_of_range("Property outside declared range");
            };
            if constexpr (vector) {
                if (!value.is_array() || value.size() != 3) throw std::invalid_argument("Expected vector3");
                for (const auto& v : value) number(v);
            } else if constexpr (std::is_same_v<F, bool>) {
                if (!value.is_boolean()) throw std::invalid_argument("Expected boolean property");
            } else if constexpr (std::is_same_v<F, std::string>) {
                if (!value.is_string()) throw std::invalid_argument("Expected string property");
            } else {
                number(value);
                if constexpr (std::is_integral_v<F>)
                    if (!value.is_number_integer()) throw std::invalid_argument("Expected integer property");
            }
            static_cast<T*>(object)->*member = value.get<F>();
        }, {}, {}, false, true, {}, {}, {}, {}, {}, 3, true};
    result.validate = [write = result.write](const Json& value) { T candidate{}; write(&candidate, value); };
    return result;
}
template<class T>
Type reflectedType(std::string name, unsigned version, std::vector<Property> properties) {
    auto assignProperties = properties;
    const auto id = stableId(name);
    return {std::move(name), id, version, std::move(properties),
        [fields = std::move(assignProperties)](void* object, const Json& data) {
            T candidate = *static_cast<T*>(object);
            for (const auto& field : fields)
                if (data.contains(field.name)) field.write(&candidate, data.at(field.name));
            *static_cast<T*>(object) = std::move(candidate);
        }};
}
inline Property withMetadata(Property value, std::string category, std::string tooltip, bool readOnly, bool toolVisible) {
    value.category=std::move(category); value.tooltip=std::move(tooltip);
    value.readOnly=readOnly; value.toolVisible=toolVisible; return value;
}
inline Property withEditor(Property value,std::string unit,unsigned precision,bool batchEditable) {
    if(precision>9)throw std::invalid_argument("Editor precision exceeds its budget");
    value.unit=std::move(unit);value.precision=precision;value.batchEditable=batchEditable;return value;
}
inline Property withReference(Property value,std::string kind,std::vector<std::string> assetTypes={},std::string defaultPolicy={}) {
    if(value.kind!=Kind::String || (kind!="node"&&kind!="asset"))throw std::invalid_argument("References require a string property and a declared kind");
    if(defaultPolicy!=""&&defaultPolicy!="selected-node")throw std::invalid_argument("Unknown reference default policy");
    if(kind!="node"&&!defaultPolicy.empty())throw std::invalid_argument("Selected-node defaults require a node reference");
    if(kind=="node"&&!assetTypes.empty())throw std::invalid_argument("Node references cannot declare asset types");
    value.reference=std::move(kind);value.assetTypes=std::move(assetTypes);value.referenceDefault=std::move(defaultPolicy);return value;
}
class Registry {
public:
    void addType(Type value);
    const Type& type(const std::string& name) const;
    const std::map<std::string, Type>& types() const noexcept { return types_; }
    Json encode(const std::string& name, const void* object) const;
    void decode(const std::string& name, void* object, const Json& envelope) const;
    void addMigration(const std::string& name, unsigned version, std::function<Json(Json)> migration);
private:
    std::map<std::string, Type> types_;
    std::map<std::pair<std::string, unsigned>, std::function<Json(Json)>> migrations_;
};
Registry makeRuntimeRegistry();
} // namespace azurerender::reflection
