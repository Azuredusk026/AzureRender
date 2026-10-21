#include "render/LightBuffer.hpp"
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <limits>

int main() { try {
    azurerender::LightBuffer buffer;
    buffer.setLights({{9}, {2}, {5}});
    assert(buffer.lights().size() == 3);
    assert(buffer.lights()[0].stableId == 2);
    assert(buffer.lights()[2].stableId == 9);
    assert(buffer.gpuData().size() == 3);
    assert(buffer.gpuData()[0].positionRadius[3] == buffer.lights()[0].radius);

    std::vector<azurerender::RenderLight> many;
    for (std::uint64_t i = 0; i < 5; ++i) many.push_back({i});
    buffer.setLights(std::move(many), 3);
    assert(buffer.lights().size() == 3);
    assert(buffer.lights()[2].stableId == 2);

    azurerender::RenderLight spot;
    spot.shape.type = azurerender::LightKind::Spot;
    spot.shape.direction = {0, 0, -1};
    spot.shape.innerCone = 0.25F;
    spot.shape.outerCone = 0.5F;
    if (spot.shape.angularWeight({0, 0, -1}) != 1.F
        || spot.shape.angularWeight({1, 0, 0}) != 0.F)
        throw std::runtime_error("Spot cone must reject light outside the cone");
    const float cosine = (std::cos(.25F) + std::cos(.5F)) * .5F;
    if (std::abs(spot.shape.angularWeight({std::sqrt(1-cosine*cosine),0,-cosine})-.25F)>1e-5F)
        throw std::runtime_error("Spot penumbra must use squared cosine interpolation");
    buffer.setLights({spot});
    const auto packed = buffer.gpuData()[0];
    if (packed.directionOuter[2] != -1.F || std::abs(packed.directionOuter[3]-std::cos(.5F))>1e-6F)
        throw std::runtime_error("GPU must receive the emitter direction and cone");
    spot.shape.direction = {0, 0, 0};
    bool rejected = false;
    try { buffer.setLights({spot}); } catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Invalid spot direction must be rejected");
    spot.shape.direction = {0,0,-1};
    spot.shape.innerCone = spot.shape.outerCone;
    spot.shape.validate();
    if(spot.shape.angularWeight({0,0,-1})!=1.F || spot.shape.angularWeight({1,0,0})!=0.F)
        throw std::runtime_error("Equal inner and outer cones must produce a hard edge");
    spot.shape.falloff = azurerender::LightFalloff::RangePower;
    spot.shape.falloffExponent = 3.5F;
    buffer.setLights({spot});
    if(buffer.gpuData()[0].coneType[2]!=2.F || buffer.gpuData()[0].coneType[3]!=3.5F)
        throw std::runtime_error("GPU must receive the selected distance model and exponent");
    for(float invalid : {0.F,-1.F,65.F,std::numeric_limits<float>::infinity()}) {
        spot.shape.falloffExponent=invalid;rejected=false;
        try {buffer.setLights({spot});} catch(const std::invalid_argument&) {rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid light falloff exponent must be rejected");
    }
    azurerender::RenderLight shadowSun;
    shadowSun.shape.type=azurerender::LightKind::Directional;
    shadowSun.rasterShadowCaster=true;buffer.setLights({shadowSun});
    if(buffer.gpuData()[0].directionOuter[3]!=1.F)
        throw std::runtime_error("Primary directional must retain its raster shadow binding");
    shadowSun.rasterShadowCaster=false;buffer.setLights({shadowSun});
    if(buffer.gpuData()[0].directionOuter[3]!=0.F)
        throw std::runtime_error("Secondary directionals must not sample another emitter's shadow map");
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; } }
