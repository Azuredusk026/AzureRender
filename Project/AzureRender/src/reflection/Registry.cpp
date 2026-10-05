#include "reflection/Registry.hpp"
#include "reflection/GeneratedRegistry.hpp"
#include <set>
#include <cmath>
namespace azurerender::reflection {
void Registry::addType(Type value) {
    if (value.name.empty() || value.version == 0 || !value.assign) throw std::invalid_argument("Invalid reflected type");
    for (const auto& entry : types_)
        if (entry.second.id == value.id) throw std::invalid_argument("Duplicate reflected type id: " + value.name);
    std::set<std::string> fields;
    for (const auto& field : value.properties)
        if (!fields.insert(field.name).second || field.minimum > field.maximum)
            throw std::invalid_argument("Invalid reflected field: " + field.name);
    types_.emplace(value.name, std::move(value));
}
const Type& Registry::type(const std::string& name) const {
    const auto found = types_.find(name);
    if (found == types_.end()) throw std::invalid_argument("Unknown reflected type: " + name);
    return found->second;
}
Json Registry::encode(const std::string& name, const void* object) const {
    if (!object) throw std::invalid_argument("Null reflected object");
    const auto& t = type(name);
    Json data = Json::object();
    for (const auto& field : t.properties) data[field.name] = field.read(object);
    return {{"type", name}, {"version", t.version}, {"data", data}};
}
void Registry::decode(const std::string& name, void* object, const Json& envelope) const {
    if (!object) throw std::invalid_argument("Null reflected object");
    const auto& t = type(name);
    if (envelope.at("type") != name || !envelope.at("version").is_number_unsigned()
        && !envelope.at("version").is_number_integer()) throw std::invalid_argument("Invalid component envelope");
    const auto numericVersion = envelope.at("version").get<std::int64_t>();
    if (numericVersion < 0 || numericVersion > t.version) throw std::invalid_argument("Unsupported component version");
    auto version = static_cast<unsigned>(numericVersion);
    Json data = envelope.at("data");
    if (!data.is_object()) throw std::invalid_argument("Component data must be an object");
    while (version < t.version) {
        const auto migration = migrations_.find({name, version});
        if (migration == migrations_.end()) throw std::invalid_argument("Missing migration: " + name);
        data = migration->second(std::move(data));
        if (!data.is_object()) throw std::invalid_argument("Migration must return object");
        ++version;
    }
    for (const auto& entry : data.items()) {
        bool found = false;
        for (const auto& field : t.properties) found |= field.name == entry.key();
        if (!found) throw std::invalid_argument("Unknown component field: " + entry.key());
    }
    if (name == "azure.transform" && data.contains("scale")) {
        const auto scale = data.at("scale").get<std::array<float,3>>();
        for (const auto value : scale)
            if (!std::isfinite(value) || std::abs(value)<.001F)
                throw std::invalid_argument("Transform scale must have absolute magnitude at least 0.001");
    }
    if (name == "azure.third-person-camera") {
        auto candidate = encode(name, object).at("data");
        candidate.update(data);
        const auto numeric = [&](const char* field) {
            const auto& value = candidate.at(field);
            if (!value.is_number()) throw std::invalid_argument("Expected numeric camera property");
            return value.get<double>();
        };
        const auto distance = numeric("distance");
        if (numeric("minimumDistance") > distance || distance > numeric("maximumDistance")
            || numeric("minimumPitch") > numeric("maximumPitch"))
            throw std::invalid_argument("Incompatible third-person camera ranges");
    }
    t.assign(object, data);
}
void Registry::addMigration(const std::string& name, unsigned version, std::function<Json(Json)> migration) {
    if (version >= type(name).version || !migration || !migrations_.emplace(std::make_pair(name, version), std::move(migration)).second)
        throw std::invalid_argument("Invalid or duplicate migration");
}
Registry makeRuntimeRegistry() {
    Registry registry; registerGeneratedTypes(registry);
    registry.addMigration("azure.character", 1, [](Json data) { return data; });
    registry.addMigration("azure.character", 2, [](Json data) { data["sprintMultiplier"]=2.5F;return data; });
    registry.addMigration("azure.animator", 1, [](Json data) { return data; });
    return registry;
}
} // namespace azurerender::reflection
