#include "reflection/Registry.hpp"
#include "reflection/GeneratedRegistry.hpp"
#include "ecs/Components.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
struct FloatRangeFixture {
 float scalar=.1F;
 std::array<float,3> vector{.0001F,.1F,.01F};
};
void check(bool value) { if (!value) throw std::runtime_error("Reflection contract failed"); }
int main() {
 try {
  auto registry = reflection::makeRuntimeRegistry();
  registry.addType(reflection::reflectedType<FloatRangeFixture>("test.float-range",1,{
   reflection::property<FloatRangeFixture>("scalar","Scalar",&FloatRangeFixture::scalar,.0001,.1),
   reflection::property<FloatRangeFixture>("vector","Vector",&FloatRangeFixture::vector,.0001,.1)}));
  FloatRangeFixture bounded,boundedCopy;
  registry.decode("test.float-range",&boundedCopy,registry.encode("test.float-range",&bounded));
  check(boundedCopy.scalar==bounded.scalar&&boundedCopy.vector==bounded.vector);
  for(float value:{std::nextafter(.0001F,0.F),std::nextafter(.1F,1.F)}){
   for(const auto& field:{"scalar","vector"}){
    auto invalid=registry.encode("test.float-range",&bounded);
    invalid["data"][field]=std::string(field)=="scalar"?nlohmann::json(value):nlohmann::json::array({value,.01F,.01F});
    bool rejected=false;try{registry.decode("test.float-range",&boundedCopy,invalid);}catch(const std::exception&){rejected=true;}
    check(rejected&&boundedCopy.scalar==bounded.scalar&&boundedCopy.vector==bounded.vector);
   }
  }
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
                  nlohmann::json{{"type","azure.transform"},{"version",1},{"data",{{"translation",{10,20,30}},{"scale",{0,1,1}}}}},
                  nlohmann::json{{"type","azure.transform"},{"version",1},{"data",{{"typo",1}}}}}) {
   bool rejected=false;try { registry.decode("azure.transform",&copy,bad); } catch (const std::exception&) { rejected=true; }
   check(rejected && copy.translation[0] == 4);
  }
  registry.decode("azure.transform", &copy, {{"type","azure.transform"},{"version",1},
      {"data",{{"scale",{-1,2,3}}}}});
  check(copy.scale[0]==-1 && copy.scale[1]==2);
  bool duplicate=false;try { registry.addType(type); } catch (const std::exception&) { duplicate=true; }check(duplicate);
  std::cout << "Reflection roundtrip, metadata, migration and transactional validation passed\n";
 } catch (const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
}
