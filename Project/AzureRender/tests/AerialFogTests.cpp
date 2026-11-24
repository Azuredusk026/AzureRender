#include "render/AerialFog.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
namespace {
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void near(float a,double b){const float reference=static_cast<float>(b);require(std::abs(a-reference)<2e-6F,"Aerial transport differs from rounded float analytic reference");}
template<class F>void reject(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}require(caught,"Invalid aerial input accepted");}
}
int main()try{
 ExponentialAerialFog p;
 require(evaluateAerialFog(p,{0,0,0},{0,0,10}).transmittance==std::array<float,3>{1,1,1},"Empty atmosphere is identity");
 p.density=.2F;p.extinction={1,2,.5F};p.ambientColor={.3F,.4F,.5F};
 auto v=evaluateAerialFog(p,{0,0,0},{0,0,5});
 for(unsigned i=0;i<3;++i){near(v.transmittance[i],std::exp(-double(p.extinction[i])));near(v.scattering[i],p.ambientColor[i]*(1-v.transmittance[i]));}
 p.startDistance=2;v=evaluateAerialFog(p,{0,0,0},{0,0,5});near(v.transmittance[0],std::exp(-.6));
 require(evaluateAerialFog(p,{0,0,0},{0,0,1}).transmittance[0]==1,"Start distance bounds optical length");
 p.startDistance=0;p.heightFalloff=.3F;p.referenceHeight=2;
 const auto up=evaluateAerialFog(p,{0,2,0},{0,6,0}),down=evaluateAerialFog(p,{0,6,0},{0,2,0});
 near(up.transmittance[0],std::exp(-.2*4*(1-std::exp(-1.2))/1.2));
 near(up.transmittance[0],down.transmittance[0]);
 p.heightFalloff=0;p.minimumHeightDelta=2.F;
 v=evaluateAerialFog(p,{0,0,0},{0,0,5});near(v.transmittance[0],std::exp(-(1-std::exp(-2.0))/2));
 p.minimumHeightDelta.reset();p.ambientColor={};p.rayleighColor={1,0,0};p.mieColor={0,1,0};
 v=evaluateAerialFog(p,{0,0,0},{0,0,5});const double pi=std::acos(-1.0);
 near(v.scattering[0],3/(16*pi)*(1-v.transmittance[0]));near(v.scattering[1],1/(4*pi)*(1-v.transmittance[1]));
 p.rayleighColor={};p.mieColor={};p.ambientColor={2,.5F,0};p.radianceScale=255;
 v=evaluateAerialFog(p,{0,0,0},{0,0,5});near(v.scattering[0],255*(1-std::exp(-double(p.density)*5)));
 const auto packed=p.pack();static_assert(sizeof(PackedAerialFog)==96&&alignof(PackedAerialFog)==16);
 require(PackedAerialFog::layoutVersion==1&&packed.rows[5][3]==0,"Aerial layout and optional integral clamp");
 p.minimumHeightDelta=.01F;require(p.pack().rows[5][3]==1,"Packed clamp is explicit");
 p.sunDirection={0,10,0};near(p.pack().rows[4][1],1);
 p.density=1;p.heightFalloff=1;p.referenceHeight=0;p.minimumHeightDelta.reset();
 require(evaluateAerialFog(p,{0,-1e30F,0},{0,-1e30F,10}).transmittance[0]==0,"Extreme dense atmosphere remains finite");
 require(evaluateAerialFog(p,{0,1e30F,0},{0,1e30F,10}).transmittance[0]==1,"Extreme sparse atmosphere remains finite");
 p.density=.25F;p.heightFalloff=2;
 near(evaluateAerialFog(p,{0,1e30F,0},{0,0,0}).transmittance[0],std::exp(-.125));
 near(evaluateAerialFog(p,{0,0,0},{0,1e30F,0}).transmittance[0],std::exp(-.125));
 p={};p.density=-1;reject([&]{p.pack();});p={};p.anisotropy=1;reject([&]{p.pack();});
 p={};p.anisotropy=-1;reject([&]{p.pack();});p={};p.sunDirection={};reject([&]{p.pack();});
 p={};p.phaseDenominatorFloor=0;reject([&]{p.pack();});p={};p.extinction[2]=-1;reject([&]{p.pack();});
 p={};p.minimumHeightDelta=std::numeric_limits<float>::quiet_NaN();reject([&]{p.pack();});
 p={};p.minimumHeightDelta=81.F;reject([&]{p.pack();});
 p={};p.radianceScale=std::numeric_limits<float>::infinity();reject([&]{p.pack();});
 p={};reject([&]{evaluateAerialFog(p,{0,0,0},{0,std::numeric_limits<float>::infinity(),0});});
 std::cout<<checks<<" aerial fog checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
