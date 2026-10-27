#include "assets/UnlitTextureLayers.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace azurerender {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid(const std::string& field) {throw std::invalid_argument("Invalid unlit texture layers: "+field);}
void fields(const Json& value,std::initializer_list<const char*> names,const char* field,
    std::initializer_list<const char*> optional={}) {
    if(!value.is_object()||value.size()<names.size()||value.size()>names.size()+optional.size())invalid(field);
    for(const char* name:names)if(!value.contains(name))invalid(std::string(field)+"."+name);
    for(const auto& entry:value.items()) {
        bool known=false;for(const char* name:names)known|=entry.key()==name;
        for(const char* name:optional)known|=entry.key()==name;
        if(!known)invalid(std::string(field)+"."+entry.key());
    }
}
float scalar(const Json& value,const char* name,float minimum=-std::numeric_limits<float>::max(),float maximum=std::numeric_limits<float>::max()) {
    if(!value.is_number())invalid(name);
    const double number=value.get<double>();
    if(!std::isfinite(number)||number<minimum||number>maximum)invalid(name);
    return static_cast<float>(number);
}
template<std::size_t N> std::array<float,N> vector(const Json& value,const char* name) {
    if(!value.is_array()||value.size()!=N)invalid(name);
    std::array<float,N> result{};
    for(std::size_t i=0;i<N;++i)result[i]=scalar(value[i],name);
    return result;
}
bool integer(const Json& value) {return value.is_number_integer();}
float saturate(float value) {return std::clamp(value,0.F,1.F);}
void finite(float value,const char* name) {if(!std::isfinite(value))invalid(name);}
std::array<float,4> sample(std::array<float,4> rgba,UnlitTextureLayer::SampleMode mode) {
    for(float v:rgba)finite(v,"sample");
    return mode==UnlitTextureLayer::SampleMode::RedAsAlpha?std::array<float,4>{1,1,1,rgba[0]}:rgba;
}
}
UnlitTextureLayers parseUnlitTextureLayers(const Json& source,std::size_t textureCount) {
    fields(source,{"schemaVersion","lighting","layers","tint","blendTint","clock","softIntersection","highlightExpansion","postExposureWeight","globalColorWeight"},"surface",{"viewDepthFade"});
    if(!integer(source.at("schemaVersion"))||source.at("schemaVersion")!=2||source.at("lighting")!="unlit")invalid("schemaVersion or lighting");
    const auto& layers=source.at("layers");
    if(!layers.is_array()||layers.empty()||layers.size()>2)invalid("layers");
    UnlitTextureLayers result;result.layerCount=static_cast<unsigned>(layers.size());
    for(unsigned i=0;i<result.layerCount;++i) {
        const auto& input=layers[i];auto& layer=result.layers[i];
        fields(input,{"role","texture","sampleMode","uv"},"layer");
        if(i==0) {if(input.at("role")!="base")invalid("first layer role");}
        else if(input.at("role")=="mask")layer.role=UnlitTextureLayer::Role::Mask;
        else if(input.at("role")=="blend")layer.role=UnlitTextureLayer::Role::Blend;
        else invalid("second layer role");
        const auto& texture=input.at("texture");
        if(!integer(texture)||texture.get<double>()<0||texture.get<double>()>=static_cast<double>(textureCount)
            ||texture.get<double>()>std::numeric_limits<int>::max())invalid("layer.texture");
        layer.texture=texture.get<int>();
        if(input.at("sampleMode")=="red-as-alpha")layer.sampleMode=UnlitTextureLayer::SampleMode::RedAsAlpha;
        else if(input.at("sampleMode")!="rgba")invalid("layer.sampleMode");
        const auto& uv=input.at("uv");fields(uv,{"set","scaleOffset","rotation","speed"},"layer.uv");
        if(!integer(uv.at("set"))||uv.at("set")!=0)invalid("layer.uv.set");
        layer.scaleOffset=vector<4>(uv.at("scaleOffset"),"layer.uv.scaleOffset");
        layer.rotation=vector<4>(uv.at("rotation"),"layer.uv.rotation");
        layer.speed=vector<2>(uv.at("speed"),"layer.uv.speed");
    }
    result.tint=vector<4>(source.at("tint"),"tint");result.blendTint=vector<4>(source.at("blendTint"),"blendTint");
    const auto& clock=source.at("clock");fields(clock,{"rate","offsetSeconds"},"clock");
    result.clockRate=scalar(clock.at("rate"),"clock.rate");result.clockOffsetSeconds=scalar(clock.at("offsetSeconds"),"clock.offsetSeconds");
    const auto& soft=source.at("softIntersection");fields(soft,{"distance","bias"},"softIntersection");
    result.softDistance=scalar(soft.at("distance"),"softIntersection.distance");
    if(result.softDistance<=0)invalid("softIntersection.distance");
    result.softBias=scalar(soft.at("bias"),"softIntersection.bias");
    const auto& highlight=source.at("highlightExpansion");fields(highlight,{"threshold","gain"},"highlightExpansion");
    result.highlightThreshold=scalar(highlight.at("threshold"),"highlightExpansion.threshold",0);
    result.highlightGain=scalar(highlight.at("gain"),"highlightExpansion.gain",0);
    result.postExposureWeight=scalar(source.at("postExposureWeight"),"postExposureWeight",0,1);
    result.globalColorWeight=scalar(source.at("globalColorWeight"),"globalColorWeight",0,1);
    if(source.contains("viewDepthFade")) {
        const auto& fade=source.at("viewDepthFade");fields(fade,{"near","far"},"viewDepthFade");
        const auto nearRange=vector<2>(fade.at("near"),"viewDepthFade.near");
        const auto farRange=vector<2>(fade.at("far"),"viewDepthFade.far");
        result.viewDepthFadePresent=true;result.viewDepthFade={nearRange[0],nearRange[1],farRange[0],farRange[1]};
        (void)evaluateUnlitViewDepthFade(result,0);
    }
    result.present=true;return result;
}
double evaluateUnlitMaterialTime(const UnlitTextureLayers& profile,double presentationSeconds) {
    const double tau=profile.clockRate*presentationSeconds+profile.clockOffsetSeconds;
    if(!std::isfinite(presentationSeconds)||!std::isfinite(tau))invalid("presentation time");
    return tau;
}
std::array<float,2> evaluateUnlitLayerUv(const UnlitTextureLayer& layer,std::array<float,2> uv,double materialSeconds) {
    if(!std::isfinite(materialSeconds))invalid("material time");
    for(float v:uv)finite(v,"uv");
    const double x=uv[0]+layer.speed[0]*materialSeconds-.5,y=uv[1]+layer.speed[1]*materialSeconds-.5;
    const double u=(x*layer.rotation[0]+y*layer.rotation[2]+.5)*layer.scaleOffset[0]+layer.scaleOffset[2];
    const double v=(x*layer.rotation[1]+y*layer.rotation[3]+.5)*layer.scaleOffset[1]+layer.scaleOffset[3];
    if(!std::isfinite(u)||!std::isfinite(v)||std::abs(u)>std::numeric_limits<float>::max()||std::abs(v)>std::numeric_limits<float>::max())invalid("uv result");
    return {static_cast<float>(u),static_cast<float>(v)};
}
float evaluateSoftIntersection(float sceneViewZ,float effectViewZ,float distance,float bias) {
    for(float v:{sceneViewZ,effectViewZ,distance,bias})finite(v,"soft intersection");
    if(distance<=0)invalid("soft intersection distance");
    return saturate((std::abs(sceneViewZ)-std::abs(effectViewZ)+bias)/distance);
}
void validateUnlitEffectFrame(const UnlitEffectFrame& frame) {
    finite(frame.exposureGain,"frame exposure");finite(frame.saturation,"frame saturation");
    if(frame.exposureGain<0||frame.saturation<0)invalid("frame gain domain");
    for(float gain:frame.colorGain)finite(gain,"frame color");
}
float evaluateUnlitViewDepthFade(const UnlitTextureLayers& profile,float effectViewZ) {
    finite(effectViewZ,"view depth");if(!profile.viewDepthFadePresent)return 1;
    const auto& range=profile.viewDepthFade;for(float value:range)finite(value,"viewDepthFade range");
    if(range[0]<0||range[2]<0||range[1]<=range[0]||range[3]<=range[2])invalid("viewDepthFade range");
    const float depth=std::abs(effectViewZ);
    return saturate((depth-range[0])/(range[1]-range[0]))
        *saturate((range[3]-depth)/(range[3]-range[2]));
}
std::array<float,4> evaluateUnlitTextureLayers(const UnlitTextureLayers& profile,
    std::array<float,4> baseSample,std::array<float,4> secondSample,
    float softIntersection,float nodeOpacity,const UnlitEffectFrame& frame,std::optional<float> effectViewZ) {
    if(!profile.present||profile.layerCount<1||profile.layerCount>2)invalid("unbound profile");
    validateUnlitEffectFrame(frame);
    for(float value:{softIntersection,nodeOpacity})finite(value,"frame");
    if(softIntersection<0||softIntersection>1||nodeOpacity<0||nodeOpacity>1)invalid("frame domain");
    if(profile.viewDepthFadePresent&&!effectViewZ)invalid("missing view depth");
    if(effectViewZ)softIntersection*=evaluateUnlitViewDepthFade(profile,*effectViewZ);
    auto result=sample(baseSample,profile.layers[0].sampleMode);
    for(unsigned i=0;i<4;++i)result[i]*=profile.tint[i];
    if(profile.layerCount==2) {
        const auto second=sample(secondSample,profile.layers[1].sampleMode);
        if(profile.layers[1].role==UnlitTextureLayer::Role::Mask)for(unsigned i=0;i<4;++i)result[i]*=second[i];
        else if(profile.layers[1].role==UnlitTextureLayer::Role::Blend) {
            const float weight=saturate((result[3]+second[3])*profile.blendTint[3]);
            for(unsigned i=0;i<3;++i)result[i]+=second[i]*profile.blendTint[i]*weight;
        } else invalid("second layer role");
    }
    for(unsigned i=0;i<3;++i) {
        result[i]+=std::max(result[i]-profile.highlightThreshold,0.F)*profile.highlightGain;
        result[i]=std::clamp(result[i]*(1+(frame.exposureGain-1)*profile.postExposureWeight),0.F,1000.F);
    }
    result[3]=saturate(result[3]*softIntersection)*nodeOpacity;
    const float luminance=result[0]*.212673F+result[1]*.715152F+result[2]*.072175F;
    for(unsigned i=0;i<3;++i) {
        const float mapped=(luminance+frame.saturation*(result[i]-luminance))*frame.colorGain[i];
        result[i]+=(mapped-result[i])*profile.globalColorWeight;
        finite(result[i],"color result");
    }
    finite(result[3],"alpha result");return result;
}
}
