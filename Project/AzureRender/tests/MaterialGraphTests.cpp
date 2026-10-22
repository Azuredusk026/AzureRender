#include "assets/MaterialGraph.hpp"
#include <iostream>
using namespace azurerender;
int main(){try{
    auto check=[](bool value,const char* text){if(!value)throw std::runtime_error(text);};
    auto rejects=[&](const nlohmann::json& value){bool failed=false;try{MaterialGraph::parse(value).compile();}catch(const std::exception&){failed=true;}check(failed,"Invalid graphs must be rejected");};
    for(const auto* type:{"azure.pbr","azure.stylized"}){
        auto material=MaterialData::defaults(type);material.parameters["roughness"]=.25;
        auto graph=MaterialGraph::fromMaterial(material);
        check(graph.compile().parameters==material.parameters,"Graph must compile the actual registered material fields");
        auto encoded=graph.encode();encoded["nodes"][0]["position"]={120,80};
        check(MaterialGraph::parse(encoded).encode()==encoded,"Layout and stable node IDs must survive reloading");
        graph.setField("roughness",.4);
        check(graph.compile().parameters["roughness"]==.4,"Field edits must update graph output");
        material=graph.compile();material.authoringGraph=graph.encode();
        check(MaterialData::parse(material.encode()).authoringGraph==graph.encode(),"Material save must retain the authoring graph");
        auto bad=encoded;bad["schemaVersion"]=99;rejects(bad);
        bad=encoded;bad["nodes"].push_back(bad["nodes"][0]);rejects(bad);
        bad=encoded;bad["nodes"][0]["position"]={"broken",0};rejects(bad);
        bad=encoded;bad["links"][0]["source"]="missing";rejects(bad);
        bad=encoded;bad["links"].push_back(bad["links"][0]);rejects(bad);
        bad=encoded;bad["links"][0]["slot"]="roughness";rejects(bad);
        bad=material.encode();bad["parameters"]["roughness"]=.8;
        bool stale=false;try{MaterialData::parse(bad);}catch(const std::exception&){stale=true;}
        check(stale,"Graph and runtime values must agree");
    }
    const auto patch=MaterialData::parse({{"schemaVersion",2},{"mode","parameters"},{"typeId","azure.pbr"},
        {"parameters",{{"emissiveStrength",2.0}}},{"textures",nlohmann::json::object()}});
    auto patchGraph=MaterialGraph::fromMaterial(patch);
    check(patchGraph.compile().encode()==patch.encode(),"Override graphs must preserve mode and sparse fields");
    patchGraph.setField("roughness",.5);
    check(patchGraph.compile().parameters.size()==2,"Adding an override must not activate unrelated defaults");
    auto removed=patchGraph.encode();removed["links"].erase(removed["links"].begin());
    check(MaterialGraph::parse(removed).compile().parameters.size()==1,"Disconnected override inputs must inherit the source");
    std::cout<<"Material graph compilation, layouts, links and persistence passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
