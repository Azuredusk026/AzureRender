#include "render/ReflectionProbeJson.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace azurerender;
int main() {
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
    auto rejects=[](const ReflectionProbe& value){try{value.validate();}catch(const std::invalid_argument&){return true;}return false;};
    p.halfExtents[0]=0; assert(rejects(p)); p.halfExtents[0]=2;
    p.intensity=-1; assert(rejects(p)); p.intensity=1;
    p.center[0]=std::numeric_limits<float>::quiet_NaN(); assert(rejects(p));
}
