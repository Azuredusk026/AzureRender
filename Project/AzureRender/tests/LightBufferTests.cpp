#include "render/LightBuffer.hpp"
#include "render/LightShapeJson.hpp"
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <limits>

int main() { try {
    using Json=nlohmann::json;
    const Json blend={{"schemaVersion",3},{"composition","color-blend"},{"strength",.7F},
        {"facingWeight",.25},{"projectedDistance",true},{"order",8}};
    Json blendShape={{"type","spot"},{"falloff","range-power"},{"characterResponse",blend}};
    const auto blended=blendShape.get<azurerender::LightShape>();
    if(Json(blended)["characterResponse"]!=blend)throw std::runtime_error("Color blend must preserve its explicit version and fields");
    azurerender::RenderLight modifier;modifier.stableId=1;modifier.shape=blended;
    azurerender::RenderLight earlier=modifier;earlier.stableId=20;earlier.shape.characterResponse.order=2;
    azurerender::RenderLight physical;physical.stableId=30;
    azurerender::LightBuffer ordered;ordered.setLights({modifier,physical,earlier});
    if(ordered.lights()[0].stableId!=30||ordered.lights()[1].stableId!=20||ordered.lights()[2].stableId!=1)
        throw std::runtime_error("Ordered blends must follow additive lights and their authored order");
    const auto blendGpu=ordered.gpuData()[2].characterResponse;
    if(blendGpu!=std::array<float,4>{3,.7F,.25F,8})throw std::runtime_error("Blend mode must reuse the existing GPU response row");
    bool overflow=false;try{ordered.setLights({modifier,physical},1);}catch(const std::invalid_argument&){overflow=true;}
    if(!overflow)throw std::runtime_error("Truncating ordered color composition must be rejected");
    for(const auto& [field,bad]:std::vector<std::pair<std::string,Json>>{{"strength",1.00000001},{"strength",true},
        {"facingWeight",-.1},{"projectedDistance",1},{"order",65536},{"order",2.5},{"composition","add"},{"extra",0}}) {
        auto invalid=blendShape;invalid["characterResponse"][field]=bad;bool rejected=false;
        try{(void)invalid.get<azurerender::LightShape>();}catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid blend fields must be rejected: "+field);
    }
    const Json character={{"schemaVersion",1},{"diffuseFloor",.25},{"compression",.8}};
    const Json rim={{"schemaVersion",4},{"composition","screen-rim"},{"offsetScale",.4F},{"surfaceTint",.8F}};
    auto rimShape=Json{{"type","point"},{"characterResponse",rim}}.get<azurerender::LightShape>();
    if(Json(rimShape)["characterResponse"]!=rim)throw std::runtime_error("Screen rim fields must survive public serialization");
    azurerender::RenderLight special;special.shape=rimShape;azurerender::LightBuffer specialBuffer;specialBuffer.setLights({special});
    if(specialBuffer.gpuData()[0].characterResponse!=std::array<float,4>{4,.4F,.8F,0})
        throw std::runtime_error("Screen rim must preserve its response mode and parameters in 144 bytes");
    const Json specularOnly={{"schemaVersion",4},{"composition","specular-only"}};
    special.shape=Json{{"characterResponse",specularOnly}}.get<azurerender::LightShape>();specialBuffer.setLights({special});
    if(specialBuffer.gpuData()[0].characterResponse!=std::array<float,4>{5,0,0,0})
        throw std::runtime_error("Specular-only light must retain its distinct GPU consumer");
    for(const auto& [field,bad]:std::vector<std::pair<std::string,Json>>{{"offsetScale",-1},{"offsetScale",16.00000001},
        {"offsetScale",true},{"surfaceTint",1.00000001},{"surfaceTint",nullptr},{"composition","rim"},{"extra",0}}) {
        auto invalid=rim;invalid[field]=bad;bool rejected=false;
        try{(void)invalid.get<azurerender::LightShape::CharacterResponse>();}catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid screen rim parameters must be rejected: "+field);
    }
    Json localShape={{"type","point"},{"characterResponse",character}};
    auto local=localShape.get<azurerender::LightShape>();
    if(!Json(local).contains("characterResponse")
        ||Json(local)["characterResponse"]["diffuseFloor"].get<float>()!=.25F
        ||Json(local)["characterResponse"]["compression"].get<float>()!=.8F)
        throw std::runtime_error("Character local-light response must roundtrip through the public shape contract");
    azurerender::RenderLight localLight;localLight.shape=local;azurerender::LightBuffer localBuffer;localBuffer.setLights({localLight});
    const auto bytes=localBuffer.gpuData();
    if(sizeof(bytes[0])!=144)throw std::runtime_error("Character response requires one explicit GPU light row");
    const auto* packedResponse=reinterpret_cast<const float*>(&bytes[0])+32;
    if(packedResponse[0]!=1||packedResponse[1]!=.25F||packedResponse[2]!=.8F||packedResponse[3]!=0)
        throw std::runtime_error("GPU local response must preserve enabled, floor and compression");
    for(const auto& bad:std::vector<Json>{nullptr,Json::array(),Json{{"schemaVersion",1}},
        Json{{"schemaVersion",2},{"diffuseFloor",.25},{"compression",.8}},
        Json{{"schemaVersion",true},{"diffuseFloor",.25},{"compression",.8}},
        Json{{"schemaVersion",1},{"diffuseFloor",true},{"compression",.8}},
        Json{{"schemaVersion",1},{"diffuseFloor",-.1},{"compression",.8}},
        Json{{"schemaVersion",1},{"diffuseFloor",1.00000001},{"compression",.8}},
        Json{{"schemaVersion",1},{"diffuseFloor",.25},{"compression",1.1}},
        Json{{"schemaVersion",1},{"diffuseFloor",.25},{"compression",.8},{"unknown",1}}}) {
        auto invalid=localShape;invalid["characterResponse"]=bad;bool rejected=false;
        try{(void)invalid.get<azurerender::LightShape>();}catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid local response inputs must fail import");
    }
    auto directionalResponse=localShape;directionalResponse["type"]="directional";bool rejectedDirectional=false;
    try{(void)directionalResponse.get<azurerender::LightShape>();}catch(const std::exception&){rejectedDirectional=true;}
    if(!rejectedDirectional)throw std::runtime_error("Local character response requires a punctual light");
    auto lineShape=localShape;lineShape["falloff"]="smooth-inverse-square";
    lineShape["characterResponse"]["schemaVersion"]=2;lineShape["characterResponse"]["linearLength"]=1;
    auto line=lineShape.get<azurerender::LightShape>();
    if(Json(line)["characterResponse"]["linearLength"]!=1||Json(line)["characterResponse"]["schemaVersion"]!=2)
        throw std::runtime_error("Version two line response must roundtrip without changing the authored version");
    localLight.shape=line;localBuffer.setLights({localLight});const auto lineBytes=localBuffer.gpuData();
    if(sizeof(lineBytes[0])!=144||(reinterpret_cast<const float*>(&lineBytes[0])+32)[3]!=1)
        throw std::runtime_error("Line length must consume the existing GPU response row W");
    for(const auto& value:Json::array({-1,65537,65536.00001,"1",true,nullptr})) {
        auto bad=lineShape;bad["characterResponse"]["linearLength"]=value;bool rejected=false;
        try{(void)bad.get<azurerender::LightShape>();}catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Invalid line length must fail import");
    }
    for(const auto* field:{"type","falloff"}) {
        auto bad=lineShape;bad[field]=std::string(field)=="type"?"spot":"legacy";bool rejected=false;
        try{(void)bad.get<azurerender::LightShape>();}catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Line response requires a point shape and explicit distance model");
    }
    const Json receiver={{"schemaVersion",1},{"surfaces",true},{"characters",false}};
    Json authored=Json(azurerender::LightShape{});authored["receivers"]=receiver;
    auto parsed=authored.get<azurerender::LightShape>();
    if(Json(parsed).value("receivers",Json{})!=receiver)
        throw std::runtime_error("Light receiver selection must roundtrip through the production shape contract");
    for(unsigned bits=0;bits<4;++bits) {
        azurerender::RenderLight light;light.shape.receivers={bool(bits&1U),bool(bits&2U)};
        azurerender::LightBuffer fixture;fixture.setLights({light});
        if(fixture.gpuData()[0].boxParameters[3]!=static_cast<float>(bits))
            throw std::runtime_error("GPU receiver bits must match the public receiver contract");
    }
    for(const auto& bad:std::vector<Json>{nullptr,Json::array(),Json{{"schemaVersion",1}},
        Json{{"schemaVersion",2},{"surfaces",true},{"characters",false}},
        Json{{"schemaVersion",1},{"surfaces",1},{"characters",false}},
        Json{{"schemaVersion",1},{"surfaces",true},{"characters",false},{"unknown",1}}}) {
        auto invalid=authored;invalid["receivers"]=bad;bool rejected=false;
        try{(void)invalid.get<azurerender::LightShape>();}catch(const std::exception&){rejected=true;}
        if(!rejected)throw std::runtime_error("Malformed receiver configuration must fail import");
    }
    azurerender::LightBuffer buffer;
    azurerender::RenderLight responseLight;
    assert(responseLight.shape.specularScale==1.F);
    responseLight.shape.specularScale=.195F;buffer.setLights({responseLight});
    assert(buffer.gpuData()[0].boxParameters[2]==.195F);
    for(float value:{-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        responseLight.shape.specularScale=value;bool rejected=false;
        try{buffer.setLights({responseLight});}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);
    }
    azurerender::LightInfluenceBox box;
    box.enabled=true;box.falloffThreshold=.5F;
    assert(box.weight({0,0,0})==1.F);
    assert(box.weight({.75F,0,0})==1.F);
    assert(box.weight({.875F,0,0})==.25F);
    assert(box.weight({0,1,0})==0.F);
    assert(box.weight({0,0,-2})==0.F);
    box.worldToBox[0]={0,.5F,0,-.5F};box.worldToBox[1]={-1,0,0,0};
    box.validate();assert(box.weight({0,1,0})==1.F&&box.weight({2,1,0})==0.F);
    box.falloffThreshold=1;assert(box.weight({0,1,0})==1.F&&box.weight({1,1,0})==0.F);
    auto invalid=box;invalid.worldToBox[0]={0,0,0,0};bool rejectedBox=false;
    try{invalid.validate();}catch(const std::invalid_argument&){rejectedBox=true;}assert(rejectedBox);
    invalid=box;invalid.falloffThreshold=1.1F;rejectedBox=false;
    try{invalid.validate();}catch(const std::invalid_argument&){rejectedBox=true;}assert(rejectedBox);
    azurerender::RenderLight shaped;shaped.shape.influenceBox=box;buffer.setLights({shaped});
    assert(buffer.gpuData()[0].boxRows==box.worldToBox&&buffer.gpuData()[0].boxParameters[0]==1.F);
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
