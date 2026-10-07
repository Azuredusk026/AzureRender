#include "assets/generators/ProceduralContracts.hpp"
#include "assets/generators/RigTextContracts.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::invalid_argument("Usage: AzureGeometryCompiler request.json output.gltf");
    const auto source=std::filesystem::u8path(argv[1]),output=std::filesystem::u8path(argv[2]);
    if(std::filesystem::file_size(source)>16*1024*1024)throw std::invalid_argument("Compiler protocol input exceeds budget");
    std::ifstream file(source,std::ios::binary);nlohmann::json envelope;file>>envelope;
    if(envelope.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported compiler protocol version");
    GenerationRequest request;request.parameters=envelope.at("parameters");
    for(const auto& input:envelope.at("inputs"))request.inputs.push_back({input.at("reference").get<std::string>(),input.at("bytes").get<std::string>()});
    for(const auto& input:envelope.at("dependencies"))request.dependencies.push_back({input.at("reference").get<std::string>(),input.at("bytes").get<std::string>()});
    const auto typed=proceduralRequest(request);const auto kind=envelope.at("kind").get<std::string>();std::string bytes;
    if(kind=="geometry")bytes=compileProceduralGeometry(typed).gltf;
    else if(kind=="rig"||kind=="rig-graph"){
        const auto result=generateRigText({typed,request.parameters.value("scale",1.)});bytes=kind=="rig"?result.gltf:result.animationGraph.dump();
    }else throw std::invalid_argument("Unknown compiler domain");
    std::ofstream target(output,std::ios::binary);target.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));target.close();
    if(!target)throw std::runtime_error("Cannot persist compiled candidate");
    std::cout<<"Candidate compiled with Manifold 3.5.2\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
