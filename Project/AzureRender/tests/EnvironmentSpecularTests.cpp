#include "render/EnvironmentSpecular.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include <fstream>
#ifndef AZ_ENVIRONMENT_SPECULAR_NO_TEXT
#include "runtime/SceneDocument.hpp"
#include <filesystem>
#include <sstream>
#endif
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool result,const char* message){if(!result)throw std::runtime_error(message);}
}
int main(int argc,char** argv) try {
    RenderSettings settings;
    require(settings.environmentSpecularModel==EnvironmentSpecularModel::LegacySchlick,"legacy must be the default");
    const auto encoded=encodeLevelRenderSettings(settings);
    require(encoded.at("environmentSpecularModel")=="legacy-schlick","legacy encoding");
    decodeLevelRenderSettings(settings,{{"environmentSpecularModel","ggx-multiscatter-fit"}});
    require(settings.environmentSpecularModel==EnvironmentSpecularModel::GgxMultiscatterFit,"explicit fit selection");
    auto copy=RenderSettings{};decodeLevelRenderSettings(copy,encodeLevelRenderSettings(settings));
    require(copy.environmentSpecularModel==settings.environmentSpecularModel,"fit round trip");
    for(const nlohmann::json invalid:{nlohmann::json("unknown"),nlohmann::json(1),nlohmann::json(nullptr)}){
        bool rejected=false;try{decodeLevelRenderSettings(settings,{{"environmentSpecularModel",invalid}});}catch(const std::exception&){rejected=true;}
        require(rejected,"invalid model must be rejected");
        require(settings.environmentSpecularModel==EnvironmentSpecularModel::GgxMultiscatterFit,"decode failure must retain target");
    }
    auto legacy=migrateRenderSettings(settings,14);
    require(legacy.environmentSpecularModel==EnvironmentSpecularModel::LegacySchlick,"version 14 migration");
    require(migrateRenderSettings(settings,15).environmentSpecularModel==EnvironmentSpecularModel::GgxMultiscatterFit,"version 15 migration");
    settings.environmentSpecularModel=static_cast<EnvironmentSpecularModel>(2);
    bool invalid=false;try{validateRenderSettings(settings);}catch(const std::invalid_argument&){invalid=true;}
    require(invalid,"invalid enum");
    const auto fit=ggxMultiscatterEnvironment({.04F,.04F,.04F},.6F,.1F);
    require(std::abs(fit[0]-.10981373793139523)<2e-7,"independent DXBC sample");
    const auto white=ggxMultiscatterEnvironment({1.F,1.F,1.F},1.F,0.F);
    require(std::abs(white[0]-1)<2e-6,"unit reflectance energy identity");
    bool domain=false;try{(void)ggxMultiscatterEnvironment({.04F,.04F,.04F},-.1F,.5F);}catch(const std::invalid_argument&){domain=true;}
    require(domain,"invalid input domain");
    unsigned cases=0;double maximum=0;
    if(argc>1){
        std::ifstream input(argv[1]);nlohmann::json oracle;input>>oracle;
        for(const auto& row:oracle.at("cases")){
            const auto actual=ggxMultiscatterEnvironment(row.at("f0").get<std::array<float,3>>(),
                row.at("roughness").get<float>(),row.at("ndotv").get<float>());
            for(unsigned channel=0;channel<3;++channel){
                const double error=std::abs(actual[channel]-row.at("expected").at(channel).get<double>());
                require(std::isfinite(actual[channel])&&error<2e-6,"independent instruction-grid mismatch");
                maximum=std::max(maximum,error);
            }
            ++cases;
        }
        require(cases==1764,"complete static reference grid");
    }
#ifndef AZ_ENVIRONMENT_SPECULAR_NO_TEXT
    require(argc>2,"expected private artifact directory");
    const std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
    const auto path=output/"fit.azscene";
    SceneDocument document;document.sceneId="environment-specular-contract";
    document.renderSettings.environmentSpecularModel=EnvironmentSpecularModel::GgxMultiscatterFit;
    document.save(path);
    require(SceneDocument::load(path).renderSettings.environmentSpecularModel==EnvironmentSpecularModel::GgxMultiscatterFit,"text model round trip");
    std::ifstream saved(path);std::stringstream bytes;bytes<<saved.rdbuf();auto text=bytes.str();saved.close();
    const auto currentVersion=std::string("renderSettingsVersion ")+std::to_string(RenderSettings::kSchemaVersion);
    const auto version=text.find(currentVersion);require(version!=std::string::npos,"text schema version");
    const auto reflection=text.find("screenSpaceReflection ");
    if(reflection!=std::string::npos)text.erase(reflection,text.find('\n',reflection)+1-reflection);
    text.replace(version,currentVersion.size(),"renderSettingsVersion 15");
    std::ofstream version15(output/"version15.azscene");version15<<text;version15.close();
    require(SceneDocument::load(output/"version15.azscene").renderSettings.environmentSpecularModel==EnvironmentSpecularModel::GgxMultiscatterFit,"version 15 text preserves model");
    text.replace(version,std::string("renderSettingsVersion 15").size(),"renderSettingsVersion 14");
    const auto model=text.find("environmentSpecularModel ");require(model!=std::string::npos,"text model present");
    text.erase(model,text.find('\n',model)+1-model);
    std::ofstream old(output/"legacy.azscene");old<<text;old.close();
    require(SceneDocument::load(output/"legacy.azscene").renderSettings.environmentSpecularModel==EnvironmentSpecularModel::LegacySchlick,"old text migration");
#endif
    std::cout<<"Instruction grid: "<<cases<<"; maximum absolute error: "<<maximum<<'\n';
    std::cout<<"Environment specular CPU contract passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
