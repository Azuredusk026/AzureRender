#pragma once
#include "assets/GeneratorRegistry.hpp"
#include <memory>
namespace azurerender {
struct ProceduralBudget {
    unsigned schemaVersion=1;
    std::size_t sourceBytes=2*1024*1024,depth=64,triangles=1000000;
    std::uint32_t timeoutMs=60000;
};
struct ProceduralGeometryRequest {
    std::string source;
    nlohmann::json parameters=nlohmann::json::object();
    std::vector<GenerationInput> dependencies;
    std::uint64_t seed=0;
    ProceduralBudget budget;
    std::function<void()> check;
};
struct ProceduralGeometryResult {
    std::string gltf;
    std::size_t triangles=0;
    double volume=0;
};
class IGeometryCompiler {
public:
    virtual ~IGeometryCompiler()=default;
    virtual std::string compile(const std::string& kind,const GenerationRequest&)=0;
};
ProceduralGeometryRequest proceduralRequest(const GenerationRequest&);
ProceduralGeometryResult compileProceduralGeometry(const ProceduralGeometryRequest&);
void registerProceduralGenerators(GeneratorRegistry&,std::shared_ptr<IGeometryCompiler>);
}
