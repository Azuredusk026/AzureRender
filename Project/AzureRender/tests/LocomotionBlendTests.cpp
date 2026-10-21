#include "animation/LocomotionBlend.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
LocomotionBlendProfile fixture(){return locomotionBlendProfileFromJson({
    {"schemaVersion",1},{"smoothingSeconds",0},{"maximumCyclesPerSecond",6},
    {"samples",{{{"clip",0},{"speed",0},{"duration",2}},
                {{"clip",1},{"speed",1},{"duration",1}},
                {{"clip",2},{"speed",3},{"duration",.5}}}}});}
int main(){try{
    auto p=fixture();LocomotionBlendState state;
    const auto middle=evaluateLocomotion(p,2,.1,state);
    check(middle.samples.size()==2,"Intermediate speed must blend two gait samples");
    check(middle.samples[0].clip==1&&std::abs(middle.samples[0].weight-.5F)<1e-6F,"Walk weight");
    check(middle.samples[1].clip==2&&std::abs(middle.samples[1].weight-.5F)<1e-6F,"Run weight");
    check(std::abs(middle.phase-.16)<1e-8,"Phase follows speed divided by blended stride length");
    check(std::abs(middle.samples[0].time/middle.samples[0].duration-middle.samples[1].time/middle.samples[1].duration)<1e-8,"Gaits share normalized phase");
    for(float speed:{0.F,.1F,.5F,1.F,2.F,3.F,10.F}){
        const auto result=evaluateLocomotion(p,speed,.01,state);double sum=0;
        for(const auto& s:result.samples){check(s.weight>=0,"Nonnegative weight");sum+=s.weight;}
        check(std::abs(sum-1)<1e-5,"Weights sum to one across the speed domain");
    }
    p.smoothingSeconds=.2;LocomotionBlendState smooth;
    evaluateLocomotion(p,0,0,smooth);evaluateLocomotion(p,3,.02,smooth);
    check(smooth.weights[0]>0&&smooth.weights[2]>0,"Start transition retains both endpoints");
    const auto before=smooth;const auto paused=evaluateLocomotion(p,0,0,smooth);
    check(smooth.phase==before.phase&&smooth.weights==before.weights&&smooth.idleTime==before.idleTime,"Zero delta freezes time and weights");
    const auto reverse=evaluateLocomotion(p,0,.02,smooth);
    check(reverse.samples.front().weight>before.weights.front(),"Stopping interrupts the current transition immediately");
    LocomotionBlendState other;evaluateLocomotion(p,1,.1,other);
    check(other.weights!=smooth.weights,"Entities own independent weights");
    for(double bad:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        auto unchanged=smooth;bool rejected=false;
        try{evaluateLocomotion(p,bad,.1,smooth);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&smooth.weights==unchanged.weights&&smooth.phase==unchanged.phase,"Invalid input rejects before mutation");
        rejected=false;try{evaluateLocomotion(p,1,bad,smooth);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Invalid time rejects");
    }
    LocomotionBlendState reset;evaluateLocomotion(p,0,.1,reset);evaluateLocomotion(p,3,0,reset,true);
    check(reset.weights.back()==1&&reset.phase==0&&reset.idleTime==0,"Explicit reset clears blend history");
    const auto longStep=evaluateLocomotion(p,3,std::numeric_limits<double>::max(),reset);
    check(std::isfinite(longStep.phase)&&std::isfinite(reset.idleTime),"Finite long duration keeps cycle clocks bounded");
    for(int hz:{30,60,144}){auto q=fixture();LocomotionBlendState stable;
        for(int i=0;i<hz;++i)evaluateLocomotion(q,2,1./hz,stable);
        check(std::abs(stable.phase-.6)<1e-8,"Constant-speed phase is independent of sampling rate");}
    for(int kind=0;kind<5;++kind){auto invalid=fixture();
        if(kind==0)invalid.samples[1].speed=0;
        if(kind==1)invalid.samples[1].clip=invalid.samples[0].clip;
        if(kind==2)invalid.samples[1].duration=0;
        if(kind==3)invalid.samples[1].phaseOffset=std::numeric_limits<double>::infinity();
        if(kind==4)invalid.maximumCyclesPerSecond=0;
        bool rejected=false;try{invalid.validate();}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Invalid profile rejects");
    }
    auto badDuration=fixture();badDuration.samples[1].duration=0;
    bool located=false;try{badDuration.validate();}catch(const std::invalid_argument& error){
        located=std::string(error.what()).find("locomotion.samples[1].duration")!=std::string::npos;}
    check(located,"Profile diagnostics must locate the invalid sample duration");
    const nlohmann::json missingSamples={{"schemaVersion",1}};
    located=false;try{(void)locomotionBlendProfileFromJson(missingSamples);}catch(const std::exception& error){
        located=std::string(error.what()).find("locomotion.samples")!=std::string::npos;}
    check(located,"Missing profile field diagnostics must include the full path");
    std::cout<<"Locomotion blend contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
