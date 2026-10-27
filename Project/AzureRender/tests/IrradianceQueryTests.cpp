#include "render/IrradianceQuery.hpp"
#include "render/IrradianceVolumeJson.hpp"
#include "support/IrradianceQueryFixture.hpp"
#include <iostream>
#include <limits>
#include <functional>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void reject(const std::function<void()>& operation){bool failed=false;try{operation();}catch(const std::invalid_argument&){failed=true;}require(failed,"Invalid query input accepted");}
int main()try{
    float maximum=0;
    for(const auto& row:test::nativeIrradianceQueries){
        IrradianceQueryParameters parameters{row[3],row[7],row[10]};
        auto result=parameters.queryPosition({row[0],row[1],row[2]},{row[4],row[5],row[6]},{row[8],row[9]});
        require(irradianceQuerySignedNoise({row[8],row[9]},row[10])==row[15],"Native signed noise differs");
        for(unsigned axis=0;axis<3;++axis)maximum=std::max(maximum,std::abs(result[axis]-row[12+axis]));
    }
    require(maximum<2e-6F,"Native receiver position differs");
    for(const auto& row:test::nativeIrradianceQueries){
        IrradianceQueryParameters query{row[3],row[7],row[10]};
        query.pixelOrigin=IrradianceQueryParameters::PixelOrigin::BottomLeft;
        query.jitterSpace=IrradianceQueryParameters::JitterSpace::Field;
        const IrradianceQueryFrame frame{1440,{-1,0,0,0,1,0,0,0,1}};
        const auto result=query.queryPosition({-row[0],row[1],row[2]},
            {-row[4],row[5],row[6]},{row[8],1439-row[9]},frame);
        const std::array<float,3> expected{-row[12],row[13],row[14]};
        for(unsigned axis=0;axis<3;++axis)require(std::abs(result[axis]-expected[axis])<2e-6F,
            "Mirrored world and presented viewport query differs from native hardware");
    }
    IrradianceQueryParameters parameters;
    const std::array<float,3> surface{2,-3,4},normal{0,1,0};
    require(parameters.queryPosition(surface,normal,{0,0})==surface,"Default query changes surface position");
    parameters.normalOffset=.25F;
    require(parameters.queryPosition(surface,normal,{0,0})==std::array<float,3>{2,-2.75F,4},"Normal offset axis differs");
    require(irradianceQuerySignedNoise({0,0},0)==-1,"Zero pixel calibration differs");
    require(irradianceQuerySignedNoise({1323.75F,1078.5F},49.875F)==test::nativeIrradianceQueries[0][15],"Pixel floor or phase truncation differs");
    require(irradianceQuerySignedNoise({20.5F,30.5F},-1.75F)==irradianceQuerySignedNoise({20,30},-1),"Negative phase must truncate toward zero");
    parameters.pixelOrigin=IrradianceQueryParameters::PixelOrigin::BottomLeft;
    parameters.jitterSpace=IrradianceQueryParameters::JitterSpace::Field;
    require(parameters.enabledFlags(true)==7&&parameters.enabledFlags(false)==0,"Query flags differ");
    require(parameters.noisePixel({3.75F,4.5F},10)==std::array<float,2>{3,5},"Bottom origin pixel floor differs");
    reject([&]{(void)parameters.noisePixel({0,0});});
    reject([&]{(void)parameters.noisePixel({0,10},10);});
    reject([&]{(void)parameters.noisePixel({-1,0},10);});
    IrradianceQueryFrame invalidFrame{10,{2,0,0,0,1,0,0,0,1}};
    reject([&]{(void)parameters.queryPosition(surface,normal,{0,0},invalidFrame);});
    parameters.pixelOrigin=IrradianceQueryParameters::PixelOrigin::TopLeft;
    parameters.jitterSpace=IrradianceQueryParameters::JitterSpace::World;
    parameters.normalOffset=-1;reject([&]{parameters.validate();});parameters.normalOffset=0;
    parameters.jitterAmplitude=-1;reject([&]{parameters.validate();});parameters.jitterAmplitude=0;
    parameters.phase=std::numeric_limits<float>::infinity();reject([&]{parameters.validate();});parameters.phase=0;
    reject([&]{(void)parameters.queryPosition(surface,{0,2,0},{0,0});});
    reject([&]{(void)parameters.queryPosition({0,std::numeric_limits<float>::quiet_NaN(),0},normal,{0,0});});
    reject([&]{(void)irradianceQuerySignedNoise({-1,0},0);});
    reject([&]{(void)irradianceQuerySignedNoise({std::numeric_limits<float>::infinity(),0},0);});
    reject([&]{(void)irradianceQuerySignedNoise({0,0},16777218.F);});
    nlohmann::json legacy={{"version",1},{"asset","field.bin"},{"enabled",true},{"intensity",.5}};
    auto binding=legacy.get<IrradianceVolumeBinding>();
    nlohmann::json requested=legacy;requested["version"]=2;
    requested["sampling"]={{"normalOffset",.25},{"jitterAmplitude",.2},{"phase",49.875}};
    binding=requested.get<IrradianceVolumeBinding>();
    auto saved=nlohmann::json(binding);
    require(saved.at("version")==2,"Sampling binding version differs");
    for(const auto* name:{"normalOffset","jitterAmplitude","phase"})
        require(saved.at("sampling").at(name).get<float>()==requested.at("sampling").at(name).get<float>(),"Sampling parameters lost during roundtrip");
    require(nlohmann::json(legacy.get<IrradianceVolumeBinding>()).at("sampling").at("normalOffset")==0,"Legacy query must default to the surface");
    auto invalid=requested;invalid["version"]=3;reject([&]{(void)invalid.get<IrradianceVolumeBinding>();});
    auto oriented=requested;oriented["version"]=3;
    oriented["sampling"]["pixelOrigin"]="bottom-left";
    oriented["sampling"]["jitterSpace"]="field";
    const auto orientedBinding=oriented.get<IrradianceVolumeBinding>();
    const auto orientedSaved=nlohmann::json(orientedBinding);
    require(orientedSaved.at("version")==3&&orientedSaved.at("sampling").at("pixelOrigin")=="bottom-left"
        &&orientedSaved.at("sampling").at("jitterSpace")=="field","Query frame policy lost during roundtrip");
    auto oldVersion=oriented;oldVersion["version"]=2;reject([&]{(void)oldVersion.get<IrradianceVolumeBinding>();});
    for(const auto* key:{"pixelOrigin","jitterSpace"}){
        auto unknown=oriented;unknown["sampling"][key]="invalid";reject([&]{(void)unknown.get<IrradianceVolumeBinding>();});
        auto wrongType=oriented;wrongType["sampling"][key]=false;reject([&]{(void)wrongType.get<IrradianceVolumeBinding>();});
    }
    invalid=requested;invalid["sampling"]["normalOffset"]=-1;reject([&]{(void)invalid.get<IrradianceVolumeBinding>();});
    invalid=requested;invalid["sampling"]["jitterAmplitude"]=-1;reject([&]{(void)invalid.get<IrradianceVolumeBinding>();});
    invalid=requested;invalid["sampling"]["phase"]=16777218;reject([&]{(void)invalid.get<IrradianceVolumeBinding>();});
    invalid=requested;invalid["version"]=1;reject([&]{(void)invalid.get<IrradianceVolumeBinding>();});
    std::cout<<"Receiver query: native noise exact, maximum position error="<<maximum<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
