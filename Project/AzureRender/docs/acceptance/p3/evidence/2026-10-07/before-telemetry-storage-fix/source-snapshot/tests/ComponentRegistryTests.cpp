#include "runtime/ComponentRegistry.hpp"
#include "runtime/ComponentCodec.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
namespace {
struct ToolLabel { std::string text = "untitled"; float weight = 1; unsigned transientCount = 0; };
struct Counter { std::uint32_t value = 0; };
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
template<class F> void rejects(F&& action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "Invalid component accepted");
}
}
int main() {
    try {
        auto registry = makeRuntimeComponentRegistry();
        registry.registerComponent<ToolLabel>(reflection::reflectedType<ToolLabel>("tool.label", 1, {
            reflection::property<ToolLabel>("text", "Text", &ToolLabel::text, 0, 0),
            reflection::property<ToolLabel>("weight", "Weight", &ToolLabel::weight, 0, 100)}));
        auto counter = reflection::reflectedType<Counter>("tool.counter", 2, {
            reflection::property<Counter>("value", "Value", &Counter::value, 0, 100)});
        counter.properties[0].category = "Statistics";
        counter.properties[0].tooltip = "Read-only sample count";
        counter.properties[0].readOnly = true;
        counter.properties[0].toolVisible = false;
        registry.registerComponent<Counter>(std::move(counter));
        registry.addMigration("tool.counter", 1, [](auto data) { data["value"] = data.at("count"); data.erase("count"); return data; });
        ecs::World world;
        auto entity = world.createEntity();
        auto label = registry.defaults("tool.label"); label["data"]["text"] = "asset inspector";
        registry.install("tool.label", world, entity, label);
        require(world.tryGet<ToolLabel>(entity)->text == "asset inspector", "External component was not installed");
        require(registry.encode("tool.label", world, entity) == label, "External component roundtrip failed");
        world.tryGet<ToolLabel>(entity)->transientCount = 23;
        registry.install("tool.label", world, entity, label);
        require(world.tryGet<ToolLabel>(entity)->transientCount == 23, "Reflected update reset runtime-only component state");
        world.tryGet<ToolLabel>(entity)->weight = 17;
        auto partial = label; partial["data"].erase("weight");
        registry.install("tool.label", world, entity, partial);
        require(world.tryGet<ToolLabel>(entity)->weight == 17, "Partial update reset an omitted property");
        registry.install("tool.label", world, entity, label);
        registry.install("tool.counter", world, entity, {{"type", "tool.counter"}, {"version", 1}, {"data", {{"count", 7}}}});
        require(world.tryGet<Counter>(entity)->value == 7, "External migration failed");
        auto description = registry.describe("tool.counter");
        require(description.at("properties")[0].at("readOnly") == true, "Read-only metadata missing");
        require(description.at("properties")[0].at("toolVisible") == false, "Visibility metadata missing");
        rejects([&] { registry.validateWrite("tool.counter", "value", 8); });
        rejects([&] { registry.install("unknown", world, entity, label); });
        auto bad = label; bad["data"]["typo"] = 1;
        rejects([&] { registry.install("tool.label", world, entity, bad); });
        bad = label; bad["data"]["weight"] = std::numeric_limits<double>::infinity();
        rejects([&] { registry.install("tool.label", world, entity, bad); });
        require(world.tryGet<ToolLabel>(entity)->weight == 1, "Failed install mutated existing component");
        // Field validation must not apply cross-field rules against unrelated defaults.
        auto camera=registry.defaults("azure.third-person-camera");
        camera["data"]["distance"]=12;
        camera["data"]["maximumDistance"]=16;
        registry.install("azure.third-person-camera",world,entity,camera);
        registry.validateWrite("azure.third-person-camera","minimumDistance",8);
        camera["data"]["minimumDistance"]=8;
        registry.install("azure.third-person-camera",world,entity,camera);
        require(registry.encode("azure.third-person-camera",world,entity).at("data").at("minimumDistance")==8, "Valid camera edit rejected");
        registry.remove("tool.label", world, entity);
        require(!registry.contains("tool.label", world, entity), "Remove did not remove component");
        rejects([&] { registry.encode("tool.label", world, entity); });
        rejects([&] { registry.install("tool.label", world, 0, label); });
        runtimeComponentRegistry().registerComponent<ToolLabel>(reflection::reflectedType<ToolLabel>("tool.label", 1, {
            reflection::property<ToolLabel>("text", "Text", &ToolLabel::text, 0, 0),
            reflection::property<ToolLabel>("weight", "Weight", &ToolLabel::weight, 0, 100)}));
        validateComponents({{"tool.label", label}});
        installComponents(world, entity, {{"tool.label", label}});
        require(world.tryGet<ToolLabel>(entity)->text == "asset inspector", "Production codec bypassed registration");
        registry.seal();
        rejects([&] { registry.registerComponent<Counter>(reflection::reflectedType<Counter>("tool.late",1,{
            reflection::property<Counter>("value","Value",&Counter::value,0,100)})); });
        std::cout << "Registered components, metadata, migration and production codec passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
