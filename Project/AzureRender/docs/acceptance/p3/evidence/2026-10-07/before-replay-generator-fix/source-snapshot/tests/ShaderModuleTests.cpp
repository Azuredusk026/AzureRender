#include "render/ShaderSharedTypes.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    try {
        using azurerender::shader::BloomParameters;
        if(argc!=3) throw std::runtime_error("Two independently compiled module descriptions are required");
        const BloomParameters defaults;
        if(defaults.threshold!=1.2F || defaults.extractBright!=0) throw std::runtime_error("Shared host defaults differ");
        for(int index=1;index<argc;++index) {
            std::ifstream file(argv[index]);nlohmann::json data;file>>data;
            if(data.at("compilerVersion")!="2026.8" || data.at("validatedTypes")!=nlohmann::json::array({"BloomParameters"}))
                throw std::runtime_error("Compiler provenance or shared type validation differs");
            const auto& reflected=data.at("reflection");
            if(reflected.at("entryPoints").at(0).at("threadGroupSize")!=nlohmann::json::array({8,8,1}))
                throw std::runtime_error("Compute dispatch contract differs");
            bool found=false;
            for(const auto& parameter:reflected.at("parameters")) {
                if(parameter.at("name")=="parameters") {
                    const auto& type=parameter.at("type");
                    if(type.at("elementVarLayout").at("binding").at("size")!=sizeof(BloomParameters))
                        throw std::runtime_error("Reflected push-constant span differs from host");
                    const auto& fields=type.at("elementType").at("fields");
                    if(fields.at(0).at("binding").at("offset")!=offsetof(BloomParameters,threshold)
                        || fields.at(1).at("binding").at("offset")!=offsetof(BloomParameters,extractBright))
                        throw std::runtime_error("Reflected member offsets differ from host");
                    found=true;
                }
                if(parameter.at("name")=="sourceImage"
                    && (parameter.at("binding").at("index")!=0 || !parameter.at("type").at("combined").get<bool>()))
                    throw std::runtime_error("Combined source texture contract differs");
                if(parameter.at("name")=="destinationImage"
                    && (parameter.at("binding").at("index")!=1 || parameter.at("format")!="rgba16f"))
                    throw std::runtime_error("Destination texture contract differs");
            }
            if(!found) throw std::runtime_error("Shared parameters missing from module");
        }
        std::cout<<"Shared host offsets, constant span and two compiled module layouts passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
