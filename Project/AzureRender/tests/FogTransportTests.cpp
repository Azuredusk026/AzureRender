#include "render/FogTransport.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace azurerender;
namespace {
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void near(float a,float b,const char* message){require(std::abs(a-b)<2e-6F,message);}
template<class F> void reject(F function){bool failed=false;try{function();}catch(const std::invalid_argument&){failed=true;}require(failed,"Invalid fog input must fail");}
}
int main(){try{
 const FogTransport front{{.1F,.2F,.3F},{.5F,.25F,.75F}},back{{.4F,.8F,.2F},{.2F,.4F,.8F}};
 const auto combined=composeFogTransport(front,back);
 near(combined.scattering[0],.3F,"Front attenuates back scattering");
 near(combined.scattering[1],.4F,"Spectral scattering uses independent channels");
 near(combined.transmittance[2],.6F,"Segment transmittances multiply");
 require(composeFogTransport({},front).scattering==front.scattering,"Identity front segment");
 require(composeFogTransport(front,{}).transmittance==front.transmittance,"Identity back segment");
 const auto alpha=applyFogTransport(front,{2,4,8});const auto additive=applyFogTransport(front,{2,4,8},0);
 near(alpha[0],1.1F,"Alpha surface includes in-scattered radiance");
 near(additive[0],1,"Additive emission retains attenuation independently");
 near(alpha[2],6.3F,"HDR color remains linear");
 ExponentialHeightFog medium;medium.density=.1F;medium.scatteringColor={.5F,1,2};
 const auto horizontal=evaluateHeightFog(medium,{0,2,0},{10,2,0});const float transmission=std::exp(-1.F);
 near(horizontal.transmittance[0],transmission,"Homogeneous optical depth");
 near(horizontal.scattering[2],2*(1-transmission),"Homogeneous source radiance");
 medium.heightFalloff=.25F;medium.referenceHeight=2;
 const auto up=evaluateHeightFog(medium,{0,2,0},{0,6,0});const auto down=evaluateHeightFog(medium,{0,6,0},{0,2,0});
 near(up.transmittance[0],std::exp(-.4F*(1-std::exp(-1.F))),"Height integral has an independent analytic solution");
 near(up.transmittance[0],down.transmittance[0],"Density integral is invariant under segment reversal");
 const auto first=evaluateHeightFog(medium,{0,2,0},{0,4,0}),second=evaluateHeightFog(medium,{0,4,0},{0,6,0});
 const auto split=composeFogTransport(first,second);near(split.transmittance[0],up.transmittance[0],"Split ray preserves optical depth");
 near(split.scattering[1],up.scattering[1],"Split ray preserves in-scattering");
 medium.density=0;require(evaluateHeightFog(medium,{0,-1e30F,0},{0,1e30F,0}).transmittance==std::array<float,3>{1,1,1},"Empty medium is exact for extreme geometry");
 medium.density=1;medium.heightFalloff=1;
 require(evaluateHeightFog(medium,{0,-1e30F,0},{0,-1e30F,10}).transmittance[0]==0,"Dense extreme height has finite zero transmittance");
 require(evaluateHeightFog(medium,{0,1e30F,0},{0,1e30F,10}).transmittance[0]==1,"Sparse extreme height has finite unit transmittance");
 require(evaluateHeightFog(medium,{1,2,3},{1,2,3}).scattering==std::array<float,3>{},"Zero-length segment is exact");
 for(float invalid:{-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
  auto bad=medium;bad.density=invalid;reject([&]{(void)evaluateHeightFog(bad,{0,0,0},{1,0,0});});
  FogTransport badTransport=front;badTransport.transmittance[0]=invalid;reject([&]{validateFogTransport(badTransport);});
 }
 auto bad=front;bad.transmittance[0]=1.1F;reject([&]{validateFogTransport(bad);});
 bad=front;bad.scattering[1]=-1;reject([&]{validateFogTransport(bad);});
 reject([&]{(void)applyFogTransport(front,{1,2,3},1.1F);});
 reject([&]{(void)evaluateHeightFog(medium,{0,0,std::numeric_limits<float>::infinity()},{1,0,0});});
 std::cout<<"Fog transport checks: "<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
