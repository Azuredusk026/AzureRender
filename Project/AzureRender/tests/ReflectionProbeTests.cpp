#ifdef NDEBUG
#error Reflection probe contract checks require active assertions.
#endif
#include "render/ReflectionProbeJson.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
using namespace azurerender;
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    ReflectionProbe p;
    assert(p.weight({0,0,0}) == 0);
    p.texture = "room.azenv";
    p.halfExtents = {2,2,2}; p.blendDistance = 1;
    assert(p.weight({0,0,0}) == 1);
    assert(std::abs(p.weight({1.5F,0,0})-.5F)<1e-6F);
    assert(p.weight({2,0,0}) == 0);
    assert(p.weight({3,0,0}) == 0);
    p.capturePosition = {0,0,0};
    const auto projected=p.project({1,0,0},{0,0,1});
    assert(std::abs(projected[0]-1)<1e-6F && std::abs(projected[2]-2)<1e-6F);
    p.boxProjection=false;
    assert((p.project({1,0,0},{0,0,1})==std::array<float,3>{0,0,1}));
    p.blendDistance=0;
    assert(p.weight({1.99F,0,0})==1);
    const auto payload=nlohmann::json(p);
    const auto saved=payload.get<ReflectionProbe>();
    assert(saved.texture==p.texture && saved.halfExtents==p.halfExtents && !saved.boxProjection);
    auto invalid=payload;invalid["version"]=999;
    bool rejectedVersion=false;
    try{(void)invalid.get<ReflectionProbe>();}catch(const std::invalid_argument&){rejectedVersion=true;}
    assert(rejectedVersion);
    // A separately authored projection origin survives serialization and
    // controls the reflected ray even when the capture position changes.
    const nlohmann::json explicitOrigin={{"version",2},{"texture","room.azenv"},
        {"center",{3,4,5}},{"halfExtents",{2,2,2}},
        {"capturePosition",{1,2,3}},{"projectionOrigin",{3,4,5}}};
    ReflectionProbe originProbe;
    try{originProbe=explicitOrigin.get<ReflectionProbe>();}
    catch(const std::exception& error){
        std::fprintf(stderr,"Version 2 projection origin was rejected: %s\n",error.what());
        return 1;
    }
    assert((originProbe.project({3,4,5},{1,0,0})==std::array<float,3>{2,0,0}));
    originProbe.capturePosition={7,8,9};
    assert((originProbe.project({3,4,5},{1,0,0})==std::array<float,3>{2,0,0}));
    const auto originPayload=nlohmann::json(originProbe);
    assert(originPayload.at("version")==2);
    assert(originPayload.at("projectionOrigin")==nlohmann::json({3,4,5}));
    const auto originReloaded=nlohmann::json::parse(originPayload.dump()).get<ReflectionProbe>();
    assert((originReloaded.project({3,4,5},{1,0,0})==std::array<float,3>{2,0,0}));
    auto legacy=explicitOrigin;
    legacy["version"]=1;legacy.erase("projectionOrigin");
    auto legacyProbe=legacy.get<ReflectionProbe>();
    assert((legacyProbe.project({3,4,5},{1,0,0})==std::array<float,3>{4,2,2}));
    legacyProbe.capturePosition={2,3,4};
    assert((legacyProbe.project({3,4,5},{1,0,0})==std::array<float,3>{3,1,1}));
    assert(!nlohmann::json(legacyProbe).contains("projectionOrigin"));
    legacy["version"]=2;
    assert((legacy.get<ReflectionProbe>().project({3,4,5},{1,0,0})==std::array<float,3>{4,2,2}));
    auto zeroOrigin=explicitOrigin;zeroOrigin["projectionOrigin"]={0,0,0};
    assert((zeroOrigin.get<ReflectionProbe>().project({3,4,5},{1,0,0})==std::array<float,3>{5,4,5}));
    auto unprojected=explicitOrigin;unprojected["boxProjection"]=false;
    assert((unprojected.get<ReflectionProbe>().project({3,4,5},{1,0,0})==std::array<float,3>{1,0,0}));
    const auto rejectsJson=[&](const nlohmann::json& value){
        auto target=originProbe;
        try{from_json(value,target);}catch(const std::exception&){
            assert(nlohmann::json(target)==originPayload);
            return true;
        }
        return false;
    };
    for(const nlohmann::json& origin:{nlohmann::json(nullptr),nlohmann::json(true),nlohmann::json("center"),
        nlohmann::json::object(),nlohmann::json::array(),nlohmann::json({1,2}),nlohmann::json({1,2,3,4}),
        nlohmann::json({1,nullptr,3}),nlohmann::json({1,true,3}),nlohmann::json({1,"2",3}),
        nlohmann::json({1,std::numeric_limits<double>::infinity(),3}),
        nlohmann::json({1,std::numeric_limits<double>::quiet_NaN(),3}),nlohmann::json({1,1e100,3})}){
        auto bad=explicitOrigin;bad["projectionOrigin"]=origin;assert(rejectsJson(bad));
    }
    for(const nlohmann::json& version:{nlohmann::json(nullptr),nlohmann::json(true),nlohmann::json("2"),
        nlohmann::json(0),nlohmann::json(-1),nlohmann::json(3),nlohmann::json(1.0),nlohmann::json(2.0),
        nlohmann::json(4294967298ULL)}){
        auto bad=explicitOrigin;bad["version"]=version;assert(rejectsJson(bad));
    }
    auto missingVersion=explicitOrigin;missingVersion.erase("version");assert(rejectsJson(missingVersion));
    auto incompatible=explicitOrigin;incompatible["version"]=1;assert(rejectsJson(incompatible));
    assert(rejectsJson(nullptr));assert(rejectsJson(nlohmann::json::array()));
    const nlohmann::json logarithmic={{"version",3},{"texture","room.azenv"},
        {"lodMapping",{{"schemaVersion",1},{"kind","logarithmic"},{"scale",1.2},{"bias",5},{"minimumRoughness",.001}}}};
    const auto mapped=logarithmic.get<ReflectionProbe>();
    for(const auto& sample:std::array<std::array<float,2>,5>{{{1,5},{.5F,3.8F},{.25F,2.6F},{.125F,1.4F},{.0625F,.2F}}})
        assert(std::abs(mapped.lod(sample[0],8)-sample[1])<2e-6F);
    // Explicit D3D11 diagnostic readback from the captured active GBuffer.
    for(const auto& sample:std::array<std::array<float,2>,7>{{
        {.08699902147054672F,.7725715637207031F},{.12316715717315674F,1.374427318572998F},
        {.1006842628121376F,1.0254921913146973F},{.09677419066429138F,.9569196701049805F},
        {.11925708502531052F,1.3185763359069824F},{.1163245365023613F,1.2754731178283691F},
        {.11925708502531052F,1.3185763359069824F}}})
        assert(std::abs(mapped.lod(sample[0],8)-sample[1])<2e-6F);
    assert(mapped.lod(0,8)==0&&mapped.lod(1,4)==3&&mapped.lod(1,1)==0);
    assert(p.lod(.5F,8)==3.5F);
    const auto mappedPayload=nlohmann::json(mapped);
    assert(mappedPayload.at("version")==3&&mappedPayload.at("lodMapping").at("kind")=="logarithmic");
    for(const auto* field:{"scale","bias","minimumRoughness"})
        assert(mappedPayload.at("lodMapping").at(field).get<float>()==logarithmic.at("lodMapping").at(field).get<float>());
    assert((mapped.lodMapping->shaderParameters()==std::array<float,3>{1.2F,5,.001F}));
    assert(std::abs(nlohmann::json::parse(mappedPayload.dump()).get<ReflectionProbe>().lod(.5F,8)-3.8F)<2e-6F);
    auto explicitLinear=logarithmic;explicitLinear["lodMapping"]={{"schemaVersion",1},{"kind","linear"}};
    assert(explicitLinear.get<ReflectionProbe>().lod(.5F,8)==3.5F);
    auto logarithmicZeroSlope=logarithmic;logarithmicZeroSlope["lodMapping"]["scale"]=0;
    assert(logarithmicZeroSlope.get<ReflectionProbe>().lod(0,8)==5);
    auto mappingRejects=[&](const nlohmann::json& candidate){
        auto target=mapped;try{from_json(candidate,target);}catch(const std::exception&){assert(nlohmann::json(target)==mappedPayload);return true;}return false;};
    for(const auto& field:std::array<const char*,3>{"scale","bias","minimumRoughness"}){
        for(const auto& value:{nlohmann::json(nullptr),nlohmann::json(true),nlohmann::json("1"),
            nlohmann::json(std::numeric_limits<double>::quiet_NaN()),nlohmann::json(1e100)}){
            auto bad=logarithmic;bad["lodMapping"][field]=value;assert(mappingRejects(bad));
        }
        auto missing=logarithmic;missing["lodMapping"].erase(field);assert(mappingRejects(missing));
    }
    for(const auto& fieldValue:std::array<std::pair<const char*,float>,6>{{{"scale",-1.F},{"scale",33.F},{"bias",-33.F},{"bias",33.F},{"minimumRoughness",0.F},{"minimumRoughness",1.1F}}}){
        auto bad=logarithmic;bad["lodMapping"][fieldValue.first]=fieldValue.second;assert(mappingRejects(bad));
    }
    for(const nlohmann::json& value:{nlohmann::json(nullptr),nlohmann::json(true),nlohmann::json::array(),nlohmann::json::object()}){
        auto bad=logarithmic;bad["lodMapping"]=value;assert(mappingRejects(bad));
    }
    for(int oldVersion:{1,2}){auto bad=logarithmic;bad["version"]=oldVersion;assert(mappingRejects(bad));}
    for(const nlohmann::json& value:{nlohmann::json(true),nlohmann::json(1.0),nlohmann::json(2)}){
        auto bad=logarithmic;bad["lodMapping"]["schemaVersion"]=value;assert(mappingRejects(bad));
    }
    auto unknownMapping=logarithmic;unknownMapping["lodMapping"]["extra"]=0;assert(mappingRejects(unknownMapping));
    unknownMapping=logarithmic;unknownMapping["lodMapping"]["kind"]="unknown";assert(mappingRejects(unknownMapping));
    auto linearExtra=explicitLinear;linearExtra["lodMapping"]["scale"]=1;assert(mappingRejects(linearExtra));
    for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        bool failed=false;try{(void)mapped.lod(bad,8);}catch(const std::invalid_argument&){failed=true;}assert(failed);
    }
    bool invalidMips=false;try{(void)mapped.lod(.5F,0);}catch(const std::invalid_argument&){invalidMips=true;}assert(invalidMips);
    auto rejects=[](const ReflectionProbe& value){try{value.validate();}catch(const std::invalid_argument&){return true;}return false;};
    for(const float value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto nonfiniteOrigin=originProbe;(*nonfiniteOrigin.projectionOrigin)[1]=value;
        assert(rejects(nonfiniteOrigin));
        bool rejectedSave=false;
        try{(void)nlohmann::json(nonfiniteOrigin);}catch(const std::invalid_argument&){rejectedSave=true;}
        assert(rejectedSave);
    }
    p.halfExtents[0]=0; assert(rejects(p)); p.halfExtents[0]=2;
    p.intensity=-1; assert(rejects(p)); p.intensity=1;
    p.center[0]=std::numeric_limits<float>::quiet_NaN(); assert(rejects(p));
    ReflectionProbe fieldProbe;fieldProbe.texture="room.azoa";fieldProbe.field=ReflectionProbeField{};
    nlohmann::json fieldJson=fieldProbe;
    assert(fieldJson.at("version")==4&&fieldJson.contains("field"));
    const auto fieldRoundtrip=fieldJson.get<ReflectionProbe>();
    assert(fieldRoundtrip.field&&nlohmann::json(fieldRoundtrip)==fieldJson);
    auto conflict=fieldJson;conflict["center"]={1,2,3};bool conflictRejected=false;
    try{(void)conflict.get<ReflectionProbe>();}catch(const std::exception&){conflictRejected=true;}
    assert(conflictRejected);

}
