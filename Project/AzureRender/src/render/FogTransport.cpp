#include "render/FogTransport.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace azurerender {
namespace {
void finite(float value){if(!std::isfinite(value))throw std::invalid_argument("Fog value must be finite");}
void nonnegative(float value){finite(value);if(value<0)throw std::invalid_argument("Fog radiance and density must be nonnegative");}
float checked(double value){if(!std::isfinite(value)||value>std::numeric_limits<float>::max())throw std::invalid_argument("Fog result exceeds the finite float range");return static_cast<float>(value);}
// log((exp(x)-1)/x), including horizontal rays and extreme height ranges.
double logExprel(double x){
    if(std::abs(x)<1e-5)return std::log1p(x*.5+x*x/6+x*x*x/24);
    if(x>50)return x-std::log(x)+std::log1p(-std::exp(-x));
    if(x< -50)return std::log1p(-std::exp(x))-std::log(-x);
    return std::log(std::expm1(x)/x);
}
}
void validateFogTransport(const FogTransport& fog) {
    for(float value:fog.scattering)nonnegative(value);
    for(float value:fog.transmittance){finite(value);if(value<0||value>1)throw std::invalid_argument("Fog transmittance must be in [0,1]");}
}
FogTransport composeFogTransport(const FogTransport& front,const FogTransport& back) {
    validateFogTransport(front);validateFogTransport(back);FogTransport result;
    for(unsigned i=0;i<3;++i){
        result.transmittance[i]=front.transmittance[i]*back.transmittance[i];
        result.scattering[i]=checked(double(front.scattering[i])+double(front.transmittance[i])*back.scattering[i]);
    }
    return result;
}
std::array<float,3> applyFogTransport(const FogTransport& fog,std::array<float,3> color,float weight) {
    validateFogTransport(fog);finite(weight);if(weight<0||weight>1)throw std::invalid_argument("Fog scattering weight must be in [0,1]");
    for(unsigned i=0;i<3;++i){nonnegative(color[i]);color[i]=checked(double(color[i])*fog.transmittance[i]+double(weight)*fog.scattering[i]);}
    return color;
}
FogTransport evaluateHeightFog(const ExponentialHeightFog& fog,std::array<float,3> start,std::array<float,3> end) {
    nonnegative(fog.density);nonnegative(fog.heightFalloff);finite(fog.referenceHeight);
    for(float value:fog.scatteringColor)nonnegative(value);
    for(float value:start)finite(value);for(float value:end)finite(value);
    const double length=std::hypot(double(end[0])-start[0],double(end[1])-start[1],double(end[2])-start[2]);
    if(fog.density==0||length==0)return {};
    const double height=-double(fog.heightFalloff)*(double(start[1])-fog.referenceHeight);
    const double delta=-double(fog.heightFalloff)*(double(end[1])-start[1]);
    const double logDepth=std::log(double(fog.density))+std::log(length)+height+logExprel(delta);
    const double transmission=logDepth>std::log(std::numeric_limits<double>::max())?0:std::exp(-std::exp(logDepth));
    FogTransport result;
    for(unsigned i=0;i<3;++i){result.transmittance[i]=static_cast<float>(transmission);result.scattering[i]=static_cast<float>(fog.scatteringColor[i]*(1-transmission));}
    return result;
}
}
