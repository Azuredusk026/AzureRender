#include "reflection/Registry.hpp"
#include "reflection/GeneratedRegistry.hpp"
#include "ecs/Components.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool value) { if (!value) throw std::runtime_error("Reflection contract failed"); }
int main() {
 try {
  auto registry = reflection::makeRuntimeRegistry();
  ecs::TransformComponent original;
  original.translation = {1, 2, 3};
  auto data = registry.encode("azure.transform", &original);
  check(data.at("type") == "azure.transform" && data.at("version") == 1);
  ecs::TransformComponent copy;
  registry.decode("azure.transform", &copy, data);
  check(copy.translation[0] == 1 && copy.translation[1] == 2 && copy.scale[2] == 1);
  const auto& type = registry.type("azure.transform");
  check(type.id != registry.type("azure.renderable").id && type.properties.size() == 3);
  check(type.properties[0].label == "Translation");
  registry.addMigration("azure.transform", 0, [](auto j) { j["translation"] = j.at("position"); j.erase("position"); return j; });
  registry.decode("azure.transform", &copy, {{"type","azure.transform"},{"version",0},{"data",{{"position",{4,5,6}}}}});
  check(copy.translation[0] == 4 && copy.translation[2] == 6);
  for (auto bad : {nlohmann::json{{"type","azure.transform"},{"version",2},{"data",{}}},
                  nlohmann::json{{"type","azure.transform"},{"version",1},{"data",{{"translation",{10,20,30}},{"scale",{-1,1,1}}}}},
                  nlohmann::json{{"type","azure.transform"},{"version",1},{"data",{{"typo",1}}}}}) {
   bool rejected=false;try { registry.decode("azure.transform",&copy,bad); } catch (const std::exception&) { rejected=true; }
   check(rejected && copy.translation[0] == 4);
  }
  bool duplicate=false;try { registry.addType(type); } catch (const std::exception&) { duplicate=true; }check(duplicate);
  std::cout << "Reflection roundtrip, metadata, migration and transactional validation passed\n";
 } catch (const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
}
