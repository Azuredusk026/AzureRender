#include "assets/GeneratorRegistry.hpp"
#include "assets/GltfLoader.hpp"
#include "assets/generators/ProceduralContracts.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid procedural content must be rejected");}
class LocalCompiler final:public IGeometryCompiler {
    std::string compile(const std::string& kind,const GenerationRequest& r) override{
        require(kind=="geometry","Expected geometry compiler domain");return compileProceduralGeometry(proceduralRequest(r)).gltf;
    }
};
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Expected output directory");
    GenerationRequest request;request.outputReference="project:/generated/box.gltf";request.licenseSource="CC0-1.0";
    request.parameters={{"source",R"({"schemaVersion":1,"coordinates":"engine","root":{"op":"cube","size":[2,3,4]}})"}};
    auto generators=GeneratorRegistry::builtins();
    registerProceduralGenerators(generators,std::make_shared<LocalCompiler>());
    const auto result=generators.generate("azure.procedural",request);
    const auto output=std::filesystem::path(argv[1])/"box.gltf";std::filesystem::create_directories(output.parent_path());
    std::ofstream(output)<<result.bytes;
    const auto asset=loadGltfAsset(output.string());
    if(asset.indices.size()!=36 || asset.boundsMax!=std::array<float,3>{2,3,4})throw std::runtime_error("Generated cube geometry and bounds must load through the real asset importer");
    require(result.manifest.generatorVersion==1 && result.manifest.outputHashes.size()==1,"Generated assets retain versioned provenance");
    for(const auto* field:{"schemaVersion","sourceBytes","depth","triangles","timeoutMs"}){
        auto malformed=request;malformed.parameters["budget"]={{"schemaVersion",1}};
        malformed.parameters["budget"][field]=1.5;
        reject([&]{generators.generate("azure.procedural",malformed);});
        malformed.parameters["budget"][field]=-1;
        reject([&]{generators.generate("azure.procedural",malformed);});
        malformed.parameters["budget"][field]=18446744073709551615ULL;
        reject([&]{generators.generate("azure.procedural",malformed);});
    }
    auto source=Json::parse(request.parameters.at("source").get<std::string>());
    const auto compile=[&](Json document,Json values=Json::object()){
        ProceduralGeometryRequest r;r.source=document.dump();r.parameters=values;return compileProceduralGeometry(r);
    };
    auto combined=source;combined["root"]={{"op","difference"},{"children",Json::array({
        {{"op","cube"},{"size",{2,2,2}}},{{"op","cube"},{"size",{1,1,1}},{"translate",{.5,.5,.5}}}})}};
    require(std::abs(compile(combined).volume-7)<1e-6,"Subtraction must remove the closed interior volume");
    combined["root"]["op"]="intersection";
    require(std::abs(compile(combined).volume-1)<1e-6,"Intersection must retain only shared volume");
    combined["root"]["op"]="union";combined["root"]["children"][1]["translate"]={1,1,1};
    require(std::abs(compile(combined).volume-8)<1e-6,"Contained union must preserve a solid volume");
    auto modular=source;modular["parameters"]={{"width",{{"default",2},{"minimum",1},{"maximum",8}}}};
    modular["modules"]={{"wall",{{"op","cube"},{"size",Json::array({{{"parameter","width"}},3,1})}}}};
    modular["root"]={{"op","module"},{"name","wall"}};
    require(std::abs(compile(modular,{{"width",4}}).volume-12)<1e-6,"Module parameters alter actual geometry");
    require(compile(modular).gltf==compile(modular).gltf,"Repeated compilation with the same inputs is deterministic");
    reject([&]{compile(modular,{{"unknown",1}});});reject([&]{compile(modular,{{"width",0}});});
    auto randomized=source;randomized["root"]["size"][0]={{"random",{1,4}}};
    ProceduralGeometryRequest random;random.source=randomized.dump();random.seed=7;
    const auto seeded=compileProceduralGeometry(random);require(seeded.gltf==compileProceduralGeometry(random).gltf,"A fixed random seed reproduces bytes");
    random.seed=8;require(seeded.gltf!=compileProceduralGeometry(random).gltf,"Different seeds affect authored random values");
    auto transformed=source;transformed["root"]["translate"]={1,2,3};transformed["coordinates"]="scad";
    std::ofstream(output)<<compile(transformed).gltf;const auto converted=loadGltfAsset(output.string());
    require(converted.boundsMin==std::array<float,3>{1,3,-5} && converted.boundsMax==std::array<float,3>{3,7,-2},"SCAD basis converts translation and bounds consistently");
    auto mirrored=source;mirrored["root"]["scale"]={-1,1,1};require(std::abs(compile(mirrored).volume-24)<1e-6,"Mirrored solids preserve outward orientation");
    for(const char* primitive:{"sphere","cylinder"}){auto round=source;round["root"]={{"op",primitive},{"radius",1},{"segments",16}};if(std::string(primitive)=="cylinder")round["root"]["height"]=2;require(compile(round).triangles>12,"Round primitives produce a tessellated solid");}
    auto invalid=source;invalid["schemaVersion"]=2;reject([&]{compile(invalid);});
    invalid=source;invalid["root"]["op"]="eval";reject([&]{compile(invalid);});
    invalid=source;invalid["root"]["size"]={0,1,1};reject([&]{compile(invalid);});
    invalid=source;invalid["root"]["unexpected"]=true;reject([&]{compile(invalid);});
    invalid=source;invalid["root"]["radius"]=2;reject([&]{compile(invalid);});
    invalid=source;invalid["root"]["children"]=Json::array({source["root"]});reject([&]{compile(invalid);});
    invalid=source;invalid["root"]={{"op","sphere"},{"radius",1},{"height",2}};reject([&]{compile(invalid);});
    invalid=modular;invalid["modules"]["wall"]={{"op","module"},{"name","wall"}};reject([&]{compile(invalid);});
    invalid=source;invalid["root"]={{"op","mesh"},{"vertices",{{0,0,0},{1,0,0},{0,1,0}}},{"triangles",{{0,1,2}}}};reject([&]{compile(invalid);});
    invalid["root"]["vertices"][2]={2,0,0};reject([&]{compile(invalid);});
    ProceduralGeometryRequest bounded;bounded.source=source.dump();bounded.budget.triangles=11;reject([&]{compileProceduralGeometry(bounded);});
    bounded.budget.triangles=1000000;bounded.budget.sourceBytes=10;reject([&]{compileProceduralGeometry(bounded);});
    bounded.budget.sourceBytes=2*1024*1024;bounded.budget.depth=2;auto deep=source;deep["root"]={{"op","union"},{"children",Json::array({{{"op","union"},{"children",Json::array({source["root"]})}}})}};bounded.source=deep.dump();reject([&]{compileProceduralGeometry(bounded);});
    bounded.source=source.dump();bounded.budget.depth=64;bounded.check=[] {throw std::runtime_error("cancelled");};reject([&]{compileProceduralGeometry(bounded);});
    ProceduralGeometryRequest imports;auto imported=source;imported["imports"]={"assets:/library.json"};imported["root"]={{"op","module"},{"name","column"}};
    imports.source=imported.dump();imports.dependencies={{"assets:/library.json",R"({"schemaVersion":1,"modules":{"column":{"op":"cube","size":[1,1,5]}}})"}};
    require(std::abs(compileProceduralGeometry(imports).volume-5)<1e-6,"Declared dependency modules participate in generation");
    imports.dependencies[0].bytes=R"({"schemaVersion":1,"imports":["assets:/library.json"],"modules":{}})";reject([&]{compileProceduralGeometry(imports);});
    imported["imports"]={"assets:/../outside.json"};imports.source=imported.dump();reject([&]{compileProceduralGeometry(imports);});
    std::cout<<"Geometry, booleans, modules, seeds, coordinates, budgets, cancellation and provenance passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
