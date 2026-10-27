#include "render/ReflectionProbeFieldJson.hpp"
#include <iostream>
#include <functional>
using namespace azurerender;
void check(bool x,const char* message){if(!x)throw std::runtime_error(message);}
void rejects(const std::function<void()>& call){try{call();}catch(const std::exception&){return;}throw std::runtime_error("Invalid probe field accepted");}
int main()try{
    ReflectionProbeField field;ReflectionProbeVolume p;p.halfExtents={2,2,2};p.inverseBlendWidths={1,1,1};p.arrayLayer=1;
    field.probes={p};field.validate(2);
    auto q=field.query({0,0,0},{1,0,0},{0,1,0},1);
    check(q.size()==1&&q[0].weight==1&&q[0].layer==1,"Inner box consumes environment");
    q=field.query({1.5F,0,0},{1,0,0},{0,1,0},1);
    check(q.size()==2&&q[0].weight==.5F&&q[1].weight==.5F,"Boundary leaves fallback");
    check(q[0].direction==std::array<float,3>{2,0,0},"Projection uses local boundary");
    field.probes[0].fade=.25F;q=field.query({0,0,0},{0,0,1},{0,1,0},1);
    check(q.size()==2&&q[0].weight==.25F&&q[1].weight==.75F,"Fade multiplies coverage");
    field.probes.push_back(p);q=field.query({0,0,0},{0,0,1},{0,1,0},1);
    check(q.size()==2&&q[0].weight==.25F&&q[1].weight==.75F,"Order consumes remaining weight");
    field.probes={p};field.probes[0].shape=ReflectionProbeVolume::Shape::Sphere;
    check(field.query({1.5F,1.5F,0},{0,0,1},{0,1,0},1).size()==1,"Sphere outside falls back");
    field.probes[0].projection=false;q=field.query({0,0,0},{.2F,.3F,.4F},{0,1,0},1);
    check(q[0].direction==std::array<float,3>{.2F,.3F,.4F},"Unprojected direction remains world aligned");
    auto bad=field;bad.probes[0].arrayLayer=2;rejects([&]{bad.validate(2);});
    bad=field;bad.probes.resize(33);rejects([&]{bad.validate(2);});
    bad=field;bad.probes[0].halfExtents[0]=0;rejects([&]{bad.validate(2);});
    bad=field;bad.probes[0].worldToLocal[0][0]=2;rejects([&]{bad.validate(2);});
    rejects([&]{(void)field.query({0,0,0},{0,0,0},{0,1,0},1);});
    field.probes={p};field.adaptation=1;field.probes[0].luminanceSH={0,0,0,2};
    q=field.query({0,0,0},{0,0,1},{0,1,0},1);
    check(std::abs(q[0].radianceScale-.5F)<1e-6F,"Luminance adapts source environment");
    field.worldToField={{{-1,0,0},{0,1,0},{0,0,1}}};field.probes[0].projection=false;
    q=field.query({1,0,0},{1,0,0},{0,1,0},1);
    check(q[0].direction==std::array<float,3>{-1,0,0},"Explicit field coordinate mapping");
    nlohmann::json saved=field;auto restored=saved.get<ReflectionProbeField>();
    check(nlohmann::json(restored)==saved,"Complete field JSON roundtrip");
    for(const auto& key:{"schemaVersion","probes","defaultLayer","adaptation"}){
        auto badJson=saved;badJson.erase(key);rejects([&]{(void)badJson.get<ReflectionProbeField>();});}
    auto invalid=saved;invalid["schemaVersion"]=2;rejects([&]{(void)invalid.get<ReflectionProbeField>();});
    invalid=saved;invalid["probes"][0]["arrayLayer"]=-1;rejects([&]{(void)invalid.get<ReflectionProbeField>();});
    invalid=saved;invalid["probes"][0]["projection"]=1;rejects([&]{(void)invalid.get<ReflectionProbeField>();});
    invalid=saved;invalid["worldToField"][0].push_back(0);rejects([&]{(void)invalid.get<ReflectionProbeField>();});
    invalid=saved;invalid["probes"][0]["halfExtents"].push_back(1);rejects([&]{(void)invalid.get<ReflectionProbeField>();});
    std::cout<<"PASS: ordered volumes, blend, projection, luminance, fallback and bounds\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
