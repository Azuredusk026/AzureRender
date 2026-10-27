#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace azurerender {
// Authoring order is priority order. Each volume captures local-axis HDR data;
// projection-disabled volumes sample the field's world-aligned directions.
struct ReflectionProbeVolume {
    enum class Shape { Sphere,Box };
    std::array<float,4> luminanceSH{0,0,0,1};
    std::array<float,3> halfExtents{1,1,1};
    std::array<std::array<float,4>,3> worldToLocal{{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    std::array<float,3> inverseBlendWidths{1,1,1};
    unsigned arrayLayer=0;
    float intensity=1,fade=1;
    Shape shape=Shape::Box;
    bool projection=true;
    void validate(unsigned layers) const {
        const auto finite=[](float f){if(!std::isfinite(f))throw std::invalid_argument("Nonfinite reflection volume parameter");};
        for(float f:luminanceSH)finite(f);
        for(auto row:worldToLocal)for(float f:row)finite(f);
        for(unsigned i=0;i<3;++i){finite(halfExtents[i]);finite(inverseBlendWidths[i]);
            if(halfExtents[i]<=0||inverseBlendWidths[i]<0)throw std::invalid_argument("Invalid reflection volume extent or blend width");
            for(unsigned j=0;j<3;++j){float dot=0;for(unsigned k=0;k<3;++k)dot+=worldToLocal[i][k]*worldToLocal[j][k];
                if(std::abs(dot-(i==j?1.F:0.F))>1e-4F)throw std::invalid_argument("Reflection volume transform must be orthogonal");}}
        finite(intensity);finite(fade);
        if(arrayLayer>=layers||intensity<0||fade<0||fade>1||(shape!=Shape::Box&&shape!=Shape::Sphere))
            throw std::invalid_argument("Invalid reflection volume layer, intensity, shape or fade");
        if(shape==Shape::Sphere&&(halfExtents[0]!=halfExtents[1]||halfExtents[0]!=halfExtents[2]))
            throw std::invalid_argument("Spherical reflection volume requires a uniform radius");
    }
    [[nodiscard]] std::array<float,3> local(const std::array<float,3>& value,bool point) const {
        std::array<float,3> out{};for(unsigned i=0;i<3;++i){const auto& row=worldToLocal[i];
            out[i]=std::fma(row[2],value[2],std::fma(row[1],value[1],row[0]*value[0]))+(point?row[3]:0.F);}
        return out;
    }
    [[nodiscard]] float coverage(const std::array<float,3>& point) const {
        float edge=1,dist2=0;
        for(unsigned i=0;i<3;++i){const auto a=std::abs(point[i]);if(a>halfExtents[i])return 0;
            edge=std::min(edge,(halfExtents[i]-a)*inverseBlendWidths[i]);dist2+=point[i]*point[i];}
        const auto sphere=(halfExtents[0]*halfExtents[0]-dist2)*inverseBlendWidths[0]*inverseBlendWidths[0];
        return std::clamp(shape==Shape::Box?edge:sphere,0.F,1.F)*fade;
    }
    [[nodiscard]] std::array<float,3> project(const std::array<float,3>& point,const std::array<float,3>& direction) const {
        if(!projection)return direction;
        const auto ray=local(direction,false);float distance=std::numeric_limits<float>::max();
        for(unsigned i=0;i<3;++i)if(ray[i]!=0)distance=std::min(distance,((ray[i]>0?halfExtents[i]:-halfExtents[i])-point[i])/ray[i]);
        std::array<float,3> out{};for(unsigned i=0;i<3;++i)out[i]=std::fma(ray[i],distance,point[i]);
        return out;
    }
};
struct ReflectionProbeContribution {
    unsigned layer;
    float weight,radianceScale;
    std::array<float,3> direction;
};
struct ReflectionProbeField {
    static constexpr unsigned maxProbes=32;
    std::vector<ReflectionProbeVolume> probes;
    std::array<float,4> defaultLuminanceSH{0,0,0,1};
    std::array<std::array<float,3>,3> worldToField{{{1,0,0},{0,1,0},{0,0,1}}};
    unsigned defaultLayer=0;
    float adaptation=0,intensity=1;
    void validate(unsigned layers) const {
        if(layers==0||layers>256||defaultLayer>=layers||probes.size()>maxProbes||!std::isfinite(adaptation)
            ||adaptation<0||adaptation>1||!std::isfinite(intensity)||intensity<0)
            throw std::invalid_argument("Invalid reflection probe field metadata");
        for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j){float dot=0;
            for(unsigned k=0;k<3;++k){if(!std::isfinite(worldToField[i][k]))throw std::invalid_argument("Nonfinite field mapping");dot+=worldToField[i][k]*worldToField[j][k];}
            if(std::abs(dot-(i==j?1.F:0.F))>1e-4F)throw std::invalid_argument("Field mapping must be orthogonal");}
        for(float f:defaultLuminanceSH)if(!std::isfinite(f))throw std::invalid_argument("Nonfinite fallback luminance");
        for(const auto& p:probes)p.validate(layers);
    }
    using Row=std::array<float,4>;
    [[nodiscard]] std::vector<Row> gpuRows(const Row& atlas,bool positiveZero) const {
        validate(256);std::vector<Row> rows{defaultLuminanceSH,
            {float(probes.size()),float(defaultLayer),adaptation,intensity},atlas,{positiveZero?1.F:0.F,0,0,0}};
        for(const auto& row:worldToField)rows.push_back({row[0],row[1],row[2],0});
        for(const auto& p:probes){rows.push_back(p.luminanceSH);
            rows.push_back({p.halfExtents[0],p.halfExtents[1],p.halfExtents[2],float(p.arrayLayer)});
            for(const auto& row:p.worldToLocal)rows.push_back(row);
            rows.push_back({p.inverseBlendWidths[0],p.inverseBlendWidths[1],p.inverseBlendWidths[2],p.intensity});
            rows.push_back({p.projection?1.F:0.F,p.shape==ReflectionProbeVolume::Shape::Box?1.F:0.F,0,p.fade});}
        return rows;
    }
    [[nodiscard]] float adapt(const std::array<float,4>& sh,const std::array<float,3>& normal,float target) const {
        const auto reference=std::max(1e-4F,std::fma(sh[2],normal[2],std::fma(sh[1],normal[1],sh[0]*normal[0]))+sh[3]);
        const auto ratio=(target+2*std::min(std::abs(target/reference),1.F))/(reference+2);
        return std::fma(ratio-1,adaptation,1.F);
    }
    [[nodiscard]] std::vector<ReflectionProbeContribution> query(const std::array<float,3>& enginePosition,
        const std::array<float,3>& engineDirection,const std::array<float,3>& engineNormal,float diffuseLuminance) const {
        validate(256);
        const auto map=[&](const std::array<float,3>& value){std::array<float,3> out{};
            for(unsigned i=0;i<3;++i){const auto& row=worldToField[i];out[i]=std::fma(row[2],value[2],std::fma(row[1],value[1],row[0]*value[0]));}return out;};
        const auto position=map(enginePosition),direction=map(engineDirection),normal=map(engineNormal);
        for(const auto& v:{position,direction,normal})for(float f:v)if(!std::isfinite(f))throw std::invalid_argument("Nonfinite reflection query");
        float magnitude=0,normalMagnitude=0;for(unsigned i=0;i<3;++i){magnitude+=direction[i]*direction[i];normalMagnitude+=normal[i]*normal[i];}
        if(!std::isfinite(magnitude)||magnitude<=0||!std::isfinite(normalMagnitude)||std::abs(normalMagnitude-1)>1e-3F
            ||!std::isfinite(diffuseLuminance)||diffuseLuminance<0)throw std::invalid_argument("Invalid reflection query direction, normal or luminance");
        std::vector<ReflectionProbeContribution> out;float remaining=1;
        for(const auto& p:probes){if(remaining<=.01F)break;const auto point=p.local(position,true);const auto weight=p.coverage(point);
            if(weight<=0)continue;
            out.push_back({p.arrayLayer,remaining*weight,intensity*p.intensity*adapt(p.luminanceSH,normal,diffuseLuminance),p.project(point,direction)});
            remaining*=1-weight;
        }
        if(remaining>.01F)out.push_back({defaultLayer,remaining,intensity*adapt(defaultLuminanceSH,normal,diffuseLuminance),direction});
        return out;
    }
};
}
