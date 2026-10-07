#pragma once
#include "ecs/World.hpp"
#include "reflection/Registry.hpp"
#include <memory>
#include <utility>
namespace azurerender {
using Json = nlohmann::json;
struct ComponentBinding {
    std::string type;
    std::function<void(ecs::World&, ecs::Entity, const Json&)> install;
    std::function<Json(const ecs::World&, ecs::Entity)> encode;
    std::function<void(ecs::World&, ecs::Entity)> remove;
    std::function<bool(const ecs::World&, ecs::Entity)> contains;
    std::function<Json()> defaults;
    std::function<void(const Json&)> validate;
};
class ComponentRegistry final {
public:
    explicit ComponentRegistry(reflection::Registry metadata = {})
        : metadata_(std::make_shared<reflection::Registry>(std::move(metadata))) {}
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;
    ComponentRegistry(ComponentRegistry&&) = default;
    ComponentRegistry& operator=(ComponentRegistry&&) = default;
    void seal() noexcept { sealed_=true; }
    void add(ComponentBinding binding);
    template<class T> void bind(const std::string& name) {
        metadata_->type(name);
        auto metadata = metadata_;
        add({name,
            [metadata, name](auto& world, auto entity, const Json& envelope) {
                const auto* current = world.template tryGet<T>(entity);
                T candidate = current ? *current : T{};
                metadata->decode(name, &candidate, envelope);
                world.addComponent(entity, std::move(candidate));
            },
            [metadata, name](const auto& world, auto entity) {
                auto* value = world.template tryGet<T>(entity);
                if (!value) throw std::invalid_argument("Entity lacks component: " + name);
                return metadata->encode(name, value);
            },
            [](auto& world, auto entity) { world.template removeComponent<T>(entity); },
            [](const auto& world, auto entity) { return world.template has<T>(entity); },
            [metadata, name] { T value{}; return metadata->encode(name, &value); },
            [metadata, name](const Json& envelope) { T value{}; metadata->decode(name, &value, envelope); }});
    }
    template<class T> void registerComponent(reflection::Type type) {
        if(sealed_)throw std::logic_error("Component registration is sealed");
        const auto name = type.name;
        if (bindings_.count(name)) throw std::invalid_argument("Duplicate component binding: " + name);
        metadata_->addType(std::move(type)); bind<T>(name);
    }
    void addMigration(const std::string& name, unsigned version, std::function<Json(Json)> migration) {
        if(sealed_)throw std::logic_error("Component registration is sealed");
        metadata_->addMigration(name, version, std::move(migration));
    }
    const reflection::Registry& metadata() const noexcept { return *metadata_; }
    void install(const std::string& type, ecs::World& world, ecs::Entity entity, const Json& envelope) const;
    Json encode(const std::string& type, const ecs::World& world, ecs::Entity entity) const;
    void remove(const std::string& type, ecs::World& world, ecs::Entity entity) const;
    bool contains(const std::string& type, const ecs::World& world, ecs::Entity entity) const;
    Json defaults(const std::string& type) const;
    void validate(const std::string& type, const Json& envelope) const;
    void validateWrite(const std::string& type, const std::string& field, const Json& value, bool toolAccess = true) const;
    Json describe(const std::string& type) const;
private:
    const ComponentBinding& binding(const std::string& type) const;
    std::shared_ptr<reflection::Registry> metadata_;
    std::map<std::string, ComponentBinding> bindings_;
    bool sealed_=false;
};
ComponentRegistry makeRuntimeComponentRegistry();
ComponentRegistry& runtimeComponentRegistry();
}
