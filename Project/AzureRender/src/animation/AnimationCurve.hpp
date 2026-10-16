#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

enum class CurveInterpolation { Step, Linear, CubicSpline };
struct AnimationCurve {
    std::vector<float> times, values;
    std::size_t width=0;
    CurveInterpolation interpolation=CurveInterpolation::Linear;
    bool rotation=false;
};
inline void validateCurve(const AnimationCurve& curve) {
    const auto stride=curve.interpolation==CurveInterpolation::CubicSpline?3U:1U;
    if(curve.times.empty()||!curve.width||curve.width>64||curve.values.size()!=curve.times.size()*curve.width*stride)
        throw std::invalid_argument("Animation curve output width or key count mismatch");
    if(curve.rotation&&curve.width!=4)throw std::invalid_argument("Quaternion curve requires four components");
    for(std::size_t i=0;i<curve.times.size();++i)
        if(!std::isfinite(curve.times[i])||curve.times[i]<0||(i&&curve.times[i]<=curve.times[i-1]))
            throw std::invalid_argument("Animation times must be finite, nonnegative and strictly increasing");
    for(float value:curve.values)if(!std::isfinite(value))throw std::invalid_argument("Animation output must be finite");
    if(curve.rotation)for(std::size_t key=0;key<curve.times.size();++key){
        float length=0;const auto offset=(key*stride+(stride==3?1:0))*4;
        for(unsigned i=0;i<4;++i)length+=curve.values[offset+i]*curve.values[offset+i];
        if(length<1e-16F)throw std::invalid_argument("Quaternion key must have nonzero length");
    }
}
inline std::vector<float> sampleCurve(const AnimationCurve& curve,float time) {
    validateCurve(curve);
    if(!std::isfinite(time))throw std::invalid_argument("Animation sample time must be finite");
    const std::size_t stride=curve.interpolation==CurveInterpolation::CubicSpline?3:1;
    auto upper=std::upper_bound(curve.times.begin(),curve.times.end(),time);
    std::size_t end=static_cast<std::size_t>(upper-curve.times.begin());
    end=std::min(end,curve.times.size()-1);const auto start=end?end-1:0;
    const float dt=curve.times[end]-curve.times[start];
    const float t=dt>0?std::clamp((time-curve.times[start])/dt,0.0F,1.0F):0;
    const auto key=[&](std::size_t k,std::size_t component,std::size_t part){return curve.values[(k*stride+part)*curve.width+component];};
    std::vector<float> out(curve.width);
    const auto valuePart=stride==3?1U:0U;
    if(time<=curve.times.front()||time>=curve.times.back()||curve.interpolation==CurveInterpolation::Step){
        const auto k=time<=curve.times.front()?0:time>=curve.times.back()?curve.times.size()-1:start;
        for(std::size_t i=0;i<out.size();++i)out[i]=key(k,i,valuePart);
    }else if(stride==3){
        const float t2=t*t,t3=t2*t;
        for(std::size_t i=0;i<out.size();++i)out[i]=(2*t3-3*t2+1)*key(start,i,1)+(t3-2*t2+t)*dt*key(start,i,2)
            +(-2*t3+3*t2)*key(end,i,1)+(t3-t2)*dt*key(end,i,0);
    }else{
        float a=1-t,b=t;
        if(curve.rotation){
            float dot=0,la=0,lb=0;for(unsigned i=0;i<4;++i){dot+=key(start,i,0)*key(end,i,0);la+=key(start,i,0)*key(start,i,0);lb+=key(end,i,0)*key(end,i,0);}
            la=std::sqrt(la);lb=std::sqrt(lb);dot/=la*lb;
            const float sign=dot<0?-1.0F:1.0F;dot=std::clamp(std::abs(dot),0.0F,1.0F);
            if(dot<.9995F){const float angle=std::acos(dot),den=std::sin(angle);a=std::sin((1-t)*angle)/den;b=std::sin(t*angle)/den;}
            a/=la;b=b*sign/lb;
        }
        for(std::size_t i=0;i<out.size();++i)out[i]=a*key(start,i,0)+b*key(end,i,0);
    }
    if(curve.rotation){float length=0;for(float v:out)length+=v*v;if(length<1e-16F)throw std::invalid_argument("Interpolated quaternion has zero length");for(float& v:out)v/=std::sqrt(length);}
    return out;
}
