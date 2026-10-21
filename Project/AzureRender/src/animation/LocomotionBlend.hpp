#pragma once
#include "animation/AnimationClipSample.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>
namespace azurerender {
struct LocomotionSpeedSample {
    std::uint32_t clip=0;
    double speed=0,duration=1,phaseOffset=0;
};
struct LocomotionBlendProfile {
    std::vector<LocomotionSpeedSample> samples;
    double smoothingSeconds=.15,maximumCyclesPerSecond=6;
    void validate() const {
        if(samples.size()<2||samples.size()>8)
            throw std::invalid_argument("locomotion.samples requires two to eight points");
        if(samples.front().speed!=0)
            throw std::invalid_argument("locomotion.samples[0].speed must be zero");
        if(!std::isfinite(smoothingSeconds)||smoothingSeconds<0||smoothingSeconds>10)
            throw std::invalid_argument("locomotion.smoothingSeconds must be finite and in [0,10]");
        if(!std::isfinite(maximumCyclesPerSecond)||maximumCyclesPerSecond<=0||maximumCyclesPerSecond>60)
            throw std::invalid_argument("locomotion.maximumCyclesPerSecond must be finite and in (0,60]");
        std::set<std::uint32_t> clips;double previous=-1;
        for(std::size_t i=0;i<samples.size();++i){
            const auto& sample=samples[i];const auto path="locomotion.samples["+std::to_string(i)+"].";
            if(!clips.insert(sample.clip).second)throw std::invalid_argument(path+"clip must be unique");
            if(!std::isfinite(sample.speed)||sample.speed<=previous||sample.speed>100)
                throw std::invalid_argument(path+"speed must be finite, strictly increasing and at most 100");
            if(!std::isfinite(sample.duration)||sample.duration<.01||sample.duration>600)
                throw std::invalid_argument(path+"duration must be finite and in [0.01,600]");
            if(!std::isfinite(sample.phaseOffset)||sample.phaseOffset<0||sample.phaseOffset>=1)
                throw std::invalid_argument(path+"phaseOffset must be finite and in [0,1)");
            previous=sample.speed;
        }
    }
};
inline LocomotionBlendProfile locomotionBlendProfileFromJson(const nlohmann::json& source){
    if(!source.is_object())throw std::invalid_argument("locomotion must be an object");
    if(!source.contains("schemaVersion")||!source["schemaVersion"].is_number_integer()||source["schemaVersion"]!=1)
        throw std::invalid_argument("locomotion.schemaVersion must be one");
    const auto number=[](const nlohmann::json& object,const char* key,const std::string& path,
                          bool optional=false,double fallback=0.){
        if(optional&&!object.contains(key))return fallback;
        if(!object.contains(key)||!object[key].is_number())throw std::invalid_argument(path+key+" must be a number");
        return object[key].get<double>();
    };
    LocomotionBlendProfile result;
    result.smoothingSeconds=number(source,"smoothingSeconds","locomotion.",true,.15);
    result.maximumCyclesPerSecond=number(source,"maximumCyclesPerSecond","locomotion.",true,6.);
    if(!source.contains("samples")||!source["samples"].is_array())
        throw std::invalid_argument("locomotion.samples must be an array");
    for(const auto& row:source["samples"]){
        const auto path="locomotion.samples["+std::to_string(result.samples.size())+"].";
        if(!row.is_object()||!row.contains("clip"))throw std::invalid_argument(path+"clip is required");
        const auto& clip=row["clip"];
        if(!clip.is_number_integer()||clip.get<std::int64_t>()<0||clip.get<std::uint64_t>()>UINT32_MAX)
            throw std::invalid_argument(path+"clip must be an unsigned 32-bit integer");
        result.samples.push_back({clip.get<std::uint32_t>(),number(row,"speed",path),
            number(row,"duration",path),number(row,"phaseOffset",path,true,0.)});
    }
    result.validate();return result;
}
struct LocomotionBlendState {
    std::vector<float> weights;
    double phase=0,idleTime=0;
};
struct LocomotionBlendResult {
    std::vector<AnimationClipSample> samples;
    double phase=0;
};
inline LocomotionBlendResult evaluateLocomotion(const LocomotionBlendProfile& profile,double speed,
        double delta,LocomotionBlendState& state,bool reset=false){
    profile.validate();
    if(!std::isfinite(speed)||speed<0||!std::isfinite(delta)||delta<0)
        throw std::invalid_argument("Locomotion speed and delta must be finite and nonnegative");
    std::vector<float> target(profile.samples.size(),0);
    const auto upper=std::upper_bound(profile.samples.begin(),profile.samples.end(),speed,
        [](double value,const auto& point){return value<point.speed;});
    if(upper==profile.samples.end())target.back()=1;
    else {
        const auto index=static_cast<std::size_t>(upper-profile.samples.begin());
        const auto& low=profile.samples[index-1];
        const float fraction=static_cast<float>((speed-low.speed)/(upper->speed-low.speed));
        target[index]=fraction;target[index-1]=1-fraction;
    }
    if(reset||state.weights.size()!=target.size()){state={};state.weights=target;}
    const double alpha=profile.smoothingSeconds==0?(delta>0?1.:0.):1-std::exp(-delta/profile.smoothingSeconds);
    double total=0,stride=0;
    for(std::size_t i=0;i<target.size();++i){
        state.weights[i]=static_cast<float>(state.weights[i]+alpha*(target[i]-state.weights[i]));
        total+=state.weights[i];
    }
    for(std::size_t i=0;i<target.size();++i){
        if(delta>0)state.weights[i]=static_cast<float>(state.weights[i]/total);
        stride+=state.weights[i]*profile.samples[i].speed*profile.samples[i].duration;
    }
    const double cycles=stride>1e-8?std::min(speed/stride,profile.maximumCyclesPerSecond):0;
    if(delta>0){
        if(cycles>0)state.phase=std::fmod(state.phase+std::fmod(delta,1/cycles)*cycles,1.);
        state.idleTime=std::fmod(state.idleTime+std::fmod(delta,profile.samples.front().duration),
                                 profile.samples.front().duration);
    }
    LocomotionBlendResult result;result.phase=state.phase;
    for(std::size_t i=0;i<target.size();++i){
        const auto& point=profile.samples[i];if(state.weights[i]<=0)continue;
        const double time=i==0?state.idleTime:std::fmod(state.phase+point.phaseOffset,1.)*point.duration;
        result.samples.push_back({point.clip,time,state.weights[i],true,point.duration});
    }
    return result;
}
}
