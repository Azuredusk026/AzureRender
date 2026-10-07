#include "ProceduralContracts.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
namespace azurerender {
ProceduralGeometryRequest proceduralRequest(const GenerationRequest& input){
    const auto& p=input.parameters;
    if(!p.is_object())throw std::invalid_argument("Procedural generation requires parameter object");
    for(const auto& field:p.items())if(field.key()!="source"&&field.key()!="sourceReference"&&field.key()!="values"&&field.key()!="seed"&&field.key()!="budget"&&field.key()!="scale")
        throw std::invalid_argument("Unknown procedural generation parameter: "+field.key());
    ProceduralGeometryRequest result;result.parameters=p.value("values",nlohmann::json::object());result.check=input.check;
    const auto seed=p.value("seed",nlohmann::json(0));
    if(!seed.is_number_integer()||(!seed.is_number_unsigned()&&seed.get<std::int64_t>()<0))throw std::invalid_argument("Procedural seed requires an unsigned integer");
    result.seed=seed.get<std::uint64_t>();
    if(p.contains("source")){
        if(p.contains("sourceReference")||!input.inputs.empty())throw std::invalid_argument("Procedural inline source has ambiguous input sources");
        result.source=p.at("source").get<std::string>();
    }else{
        const auto ref=p.at("sourceReference").get<std::string>();
        const auto found=std::find_if(input.inputs.begin(),input.inputs.end(),[&](const auto& value){return value.reference==ref;});
        if(found==input.inputs.end()||input.inputs.size()!=1)throw std::invalid_argument("Procedural source reference requires one declared input");
        result.source=found->bytes;
    }
    result.dependencies=input.dependencies;
    if(p.contains("budget")){
        const auto& b=p.at("budget");if(!b.is_object())throw std::invalid_argument("Procedural budget requires object");
        for(const auto& f:b.items())if(f.key()!="schemaVersion"&&f.key()!="sourceBytes"&&f.key()!="depth"&&f.key()!="triangles"&&f.key()!="timeoutMs")throw std::invalid_argument("Unknown procedural budget field");
        for(const auto& f:b.items()){
            const auto& value=f.value();
            if(!value.is_number_integer()||(!value.is_number_unsigned()&&value.get<std::int64_t>()<=0))throw std::invalid_argument("Procedural budgets require positive integers");
            const std::uint64_t maximum=f.key()=="schemaVersion"?1:f.key()=="sourceBytes"?2*1024*1024:f.key()=="depth"?64:f.key()=="triangles"?1000000:60000;
            if(value.get<std::uint64_t>()==0||value.get<std::uint64_t>()>maximum)throw std::invalid_argument("Procedural budget exceeds its supported limit");
        }
        result.budget.schemaVersion=b.at("schemaVersion").get<unsigned>();
        result.budget.sourceBytes=b.value("sourceBytes",result.budget.sourceBytes);result.budget.depth=b.value("depth",result.budget.depth);
        result.budget.triangles=b.value("triangles",result.budget.triangles);result.budget.timeoutMs=b.value("timeoutMs",result.budget.timeoutMs);
    }
    return result;
}
void registerProceduralGenerators(GeneratorRegistry& registry,std::shared_ptr<IGeometryCompiler> compiler){
    if(!compiler)throw std::invalid_argument("Procedural registration requires a compiler service");
    for(const auto* kind:{"geometry","rig","rig-graph"}){
        const auto id=std::string(kind)=="geometry"?"azure.procedural":std::string(kind)=="rig"?"azure.rig-text":"azure.rig-animation-graph";
        registry.add(id,1,[compiler,kind](const GenerationRequest& r){
            const bool graph=std::string(kind)=="rig-graph";
            const auto extension=std::filesystem::path(r.outputReference).extension();
            if((graph&&extension!=".json")||(!graph&&extension!=".gltf"))throw std::invalid_argument("Procedural compiler output requires its registered asset format");
            return compiler->compile(kind,r);
        });
    }
}
}
