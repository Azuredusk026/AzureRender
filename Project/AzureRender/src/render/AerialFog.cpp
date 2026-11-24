#include "render/AerialFog.hpp"
#include "render/FogMath.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace azurerender {
namespace {
void finite(float v){if(!std::isfinite(v))throw std::invalid_argument("Aerial fog value must be finite");}
void nonnegative(float v){finite(v);if(v<0)throw std::invalid_argument("Aerial fog density and radiance must be nonnegative");}
std::array<float,3> normalized(std::array<float,3> v){
    for(auto x:v)finite(x);
    const double n=std::hypot(double(v[0]),double(v[1]),double(v[2]));
    if(n==0)throw std::invalid_argument("Aerial fog sun direction must be nonzero");
    for(auto& x:v)x=static_cast<float>(x/n);return v;
}
}
void ExponentialAerialFog::validate() const {
    nonnegative(density);nonnegative(heightFalloff);finite(referenceHeight);nonnegative(startDistance);
    for(const auto* values:{&extinction,&rayleighColor,&mieColor,&ambientColor})for(auto v:*values)nonnegative(v);
    (void)normalized(sunDirection);finite(anisotropy);
    if(std::abs(anisotropy)>=1)throw std::invalid_argument("Aerial anisotropy must lie strictly between -1 and 1");
    nonnegative(radianceScale);finite(phaseDenominatorFloor);
    if(phaseDenominatorFloor<=0)throw std::invalid_argument("Aerial phase denominator floor must be positive");
    if(minimumHeightDelta){finite(*minimumHeightDelta);if(std::abs(*minimumHeightDelta)>80)throw std::invalid_argument("Aerial integral clamp must lie in [-80,80]");}
}
PackedAerialFog ExponentialAerialFog::pack() const {
    validate();const auto direction=normalized(sunDirection);PackedAerialFog p;
    p.rows={{{extinction[0],extinction[1],extinction[2],density},
        {rayleighColor[0],rayleighColor[1],rayleighColor[2],heightFalloff},
        {mieColor[0],mieColor[1],mieColor[2],referenceHeight},
        {ambientColor[0],ambientColor[1],ambientColor[2],startDistance},
        {direction[0],direction[1],direction[2],anisotropy},
        {radianceScale,phaseDenominatorFloor,minimumHeightDelta.value_or(0),minimumHeightDelta?1.F:0.F}}};
    return p;
}
FogTransport evaluateAerialFog(const ExponentialAerialFog& p,std::array<float,3> start,std::array<float,3> end){
    p.validate();for(auto v:start)finite(v);for(auto v:end)finite(v);
    std::array<double,3> delta{};for(unsigned i=0;i<3;++i)delta[i]=double(end[i])-start[i];
    const double length=std::hypot(delta[0],delta[1],delta[2]);
    if(p.density==0||length<=p.startDistance)return {};
    const auto sun=normalized(p.sunDirection);double cosine=0;
    for(unsigned i=0;i<3;++i)cosine+=delta[i]/length*sun[i];cosine=std::clamp(cosine,-1.0,1.0);
    const double pi=std::acos(-1.0),g=p.anisotropy;
    const double rayleigh=3/(16*pi)*(1+cosine*cosine);
    const double denominator=4*pi*std::pow(std::max(1+g*g-2*g*cosine,0.0),1.5);
    const double mie=(1-g*g)/std::max(denominator,double(p.phaseDenominatorFloor));
    double average=detail::fogLogHeightAverage(p.heightFalloff,p.referenceHeight,start[1],end[1]);
    if(p.minimumHeightDelta){
        const double exponent=-std::max(-double(p.heightFalloff)*delta[1],double(*p.minimumHeightDelta));
        average=-double(p.heightFalloff)*(double(end[1])-p.referenceHeight)+detail::fogLogExprel(exponent);
    }
    const double column=std::log(double(p.density))+std::log(length-p.startDistance)+average;
    FogTransport result;
    for(unsigned i=0;i<3;++i){
        const double t=p.extinction[i]==0?1:detail::fogTransmittanceFromLogDepth(column+std::log(double(p.extinction[i])));
        const double source=std::clamp(double(p.rayleighColor[i])*rayleigh+double(p.mieColor[i])*mie+p.ambientColor[i],0.0,1.0)*p.radianceScale;
        result.transmittance[i]=static_cast<float>(t);result.scattering[i]=static_cast<float>(source*(1-t));
    }
    return result;
}
}
