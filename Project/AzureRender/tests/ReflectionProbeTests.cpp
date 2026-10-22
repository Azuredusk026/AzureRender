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
}
