#include "devtools/GeometryProcessCompiler.hpp"
#include <filesystem>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Controlled compilation must reject cancelled, expired or invalid requests");}
int main(int argc,char** argv){try{
    require(argc==3,"Expected compiler and scratch paths");const auto scratch=std::filesystem::path(argv[2]);
    auto compiler=std::make_shared<GeometryProcessCompiler>(std::filesystem::u8path(argv[1]),scratch);
    auto registry=GeneratorRegistry::builtins();registerProceduralGenerators(registry,compiler);
    GenerationRequest r;r.outputReference="assets:/box.gltf";r.licenseSource="CC0-1.0";
    r.parameters={{"source",R"({"schemaVersion":1,"root":{"op":"cube","size":[1,2,3]}})"}};
    const auto valid=registry.generate("azure.procedural",r);
    require(nlohmann::json::parse(valid.bytes).at("asset").at("version")=="2.0","Compiler subprocess produces standard glTF");
    require(std::filesystem::is_empty(scratch),"Successful compilation retires its candidate directory");
    r.parameters["budget"]={{"schemaVersion",1},{"timeoutMs",1}};reject([&]{registry.generate("azure.procedural",r);});r.parameters.erase("budget");
    int checks=0;r.check=[&]{if(++checks>=2)throw std::runtime_error("cancelled");};reject([&]{registry.generate("azure.procedural",r);});r.check={};
    require(std::filesystem::is_empty(scratch),"Timeout and cancellation retire candidate directories");
    auto slow=nlohmann::json::parse(r.parameters.at("source").get<std::string>());
    slow["root"]={{"op","union"},{"children",nlohmann::json::array()}};
    for(int i=0;i<100;++i)slow["root"]["children"].push_back({{"op","sphere"},{"radius",1},{"segments",128},{"translate",{i*.2,0,0}}});
    const auto originalSource=r.parameters["source"];r.parameters["source"]=slow.dump();
    const auto started=std::chrono::steady_clock::now();bool inFlight=false;
    r.check=[&]{if(std::chrono::steady_clock::now()-started>std::chrono::milliseconds(100)){
        for(const auto& candidate:std::filesystem::directory_iterator(scratch))if(std::filesystem::exists(candidate.path()/"request.json")&&!std::filesystem::exists(candidate.path()/"candidate.asset"))inFlight=true;
        throw std::runtime_error("cancelled in-flight compilation");
    }};
    reject([&]{registry.generate("azure.procedural",r);});r.check={};r.parameters["source"]=originalSource;
    require(inFlight&&std::filesystem::is_empty(scratch),"In-flight process cancellation must retire the active candidate");
    r.parameters["source"]="{broken";reject([&]{registry.generate("azure.procedural",r);});
    r.parameters["source"]=R"({"schemaVersion":1,"root":{"op":"cube","size":[1,2,3]}})";
    require(registry.generate("azure.procedural",r).bytes==valid.bytes,"Failed subprocesses permit deterministic recovery");
    auto missing=std::make_shared<GeometryProcessCompiler>(scratch/"missing.exe",scratch);reject([&]{missing->compile("geometry",r);});
    require(std::filesystem::is_empty(scratch),"Unavailable compilers preserve scratch ownership");
    std::cout<<"Real compiler process, deadlines, cancellation, cleanup and recovery passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
