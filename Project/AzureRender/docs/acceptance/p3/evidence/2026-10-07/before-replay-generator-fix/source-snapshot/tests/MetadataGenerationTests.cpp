#define registerGeneratedTypes registerToolTypes
#include "fixtures/GeneratedToolRegistry.hpp"
#undef registerGeneratedTypes
#include <iostream>
#include <stdexcept>
int main() {
    try {
        azurerender::reflection::Registry registry;
        azurerender::reflection::registerToolTypes(registry);
        const auto& field=registry.type("tool.metric").properties.at(0);
        if(!field.readOnly || field.toolVisible || field.category!="Statistics" || field.tooltip!="Collected sample count")
            throw std::runtime_error("Generated field permissions or descriptions lost");
        azurerender::test::Metric value;
        registry.decode("tool.metric",&value,{{"type","tool.metric"},{"version",1},{"data",{{"samples",7}}}});
        if(value.samples!=7)throw std::runtime_error("Read-only field could not be loaded");
        std::cout<<"Generated metadata and content loading passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
