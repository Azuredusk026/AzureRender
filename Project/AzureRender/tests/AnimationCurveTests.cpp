#include "animation/AnimationCurve.hpp"
#include <iostream>
#include <limits>
void check(bool b){if(!b)throw std::runtime_error("Curve contract failed");}
int main(){try{
    AnimationCurve c{{0,2},{0,0,2, 0,2,0},1,CurveInterpolation::CubicSpline,false};
    check(std::abs(sampleCurve(c,1)[0]-1.5F)<1e-6F);
    check(sampleCurve(c,-1)[0]==0&&sampleCurve(c,3)[0]==2);
    c.interpolation=CurveInterpolation::Step;c.values={0,2};check(sampleCurve(c,1)[0]==0&&sampleCurve(c,2)[0]==2);
    c.interpolation=CurveInterpolation::Linear;check(sampleCurve(c,1)[0]==1);
    c.times={0,0};bool rejected=false;try{sampleCurve(c,0);}catch(const std::exception&){rejected=true;}check(rejected);
    c.times={0,2};c.values[0]=std::numeric_limits<float>::quiet_NaN();rejected=false;try{sampleCurve(c,0);}catch(const std::exception&){rejected=true;}check(rejected);
    AnimationCurve q{{0,2},{0,0,0,1,0,0,0,-1},4,CurveInterpolation::Linear,true};
    check(std::abs(sampleCurve(q,1)[3]-1)<1e-6F);
    q.interpolation=CurveInterpolation::CubicSpline;
    q.values={0,0,0,0, 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0};
    auto midpoint=sampleCurve(q,1);check(std::abs(midpoint[2]-.70710678F)<1e-6F&&std::abs(midpoint[3]-.70710678F)<1e-6F);

    for(const auto& curve : {AnimationCurve{{0,2},{1,3},1,CurveInterpolation::Step,false},
                            AnimationCurve{{0,2},{1,3},1,CurveInterpolation::Linear,false},q}){
        PreparedAnimationCurve prepared(curve);
        for(float t : {-1.F,0.F,.25F,1.F,2.F,3.F})check(prepared.sample(t)==sampleCurve(curve,t));
        bool invalidTime=false;try{prepared.sample(std::numeric_limits<float>::infinity());}catch(const std::invalid_argument&){invalidTime=true;}check(invalidTime);
    }
    AnimationCurve editable{{0,2},{1,3},1};PreparedAnimationCurve frozen(editable);
    editable.values[0]=std::numeric_limits<float>::quiet_NaN();
    check(frozen.sample(1)[0]==2);bool rawRejected=false;
    try{sampleCurve(editable,1);}catch(const std::invalid_argument&){rawRejected=true;}check(rawRejected);
    bool prepareRejected=false;try{PreparedAnimationCurve invalid(editable);}catch(const std::invalid_argument&){prepareRejected=true;}check(prepareRejected);
    std::cout<<"Curve interpolation, time scaling, quaternion and invalid inputs passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
