#include "render/FogViewProjection.hpp"
#include "support/FogViewProjectionFixture.hpp"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace azurerender;
namespace {
unsigned checks=0;
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
void near(float a,float b,const char* text){check(std::abs(a-b)<2e-6F,text);}
template<class F> void reject(F function){bool failed=false;try{function();}catch(const std::invalid_argument&){failed=true;}check(failed,"Invalid view fog projection must fail");}
}
int main(int argc,char** argv)try{
 FogViewProjection p;p.extent={100,50};p.depthEncoding={1,1,.5F};
 auto value=p.coordinates({10,20},3);near(value[0],.105F,"Upper-left pixel center U");near(value[1],.41F,"Upper-left pixel center V");near(value[2],1,"Logarithmic view-depth encoding");near(value[3],1,"Positive depth has complete near weight");
 p.pixelOrigin=FogViewProjection::PixelOrigin::LowerLeft;value=p.coordinates({10,20},-3);near(value[1],.59F,"Lower-left texture origin");near(value[2],1,"Signed view depth has equal optical coordinates");
 p.minimumViewDepth=2;p.nearFadeWidth=2;near(p.coordinates({10,20},3)[3],.5F,"Near-plane interpolation");near(p.coordinates({10,20},1)[3],0,"Before near-plane cutoff");
 const auto packed=p.pack();check(packed.rows[0]==std::array<float,4>{.01F,.02F,0,.5F},"Packed viewport, jitter and offset");check(packed.rows[2]==std::array<float,4>{2,100,50,8},"Packed dimensions and origin flags");
 auto bad=p;bad.extent[0]=0;reject([&]{bad.validate();});bad=p;bad.extent[1]=16385;reject([&]{bad.validate();});bad=p;bad.framePhase=8;reject([&]{bad.validate();});bad=p;bad.depthEncoding[1]=0;reject([&]{bad.validate();});bad=p;bad.jitterRadius=-1;reject([&]{bad.validate();});bad=p;bad.nearFadeWidth=0;reject([&]{bad.validate();});
 reject([&]{(void)p.coordinates({100,20},1);});reject([&]{(void)p.coordinates({1,2},std::numeric_limits<float>::infinity());});
 if(argc==2){const auto fixture=test::readFogProjectionFixture(std::filesystem::u8path(argv[1]));for(const auto& s:fixture.samples){const auto actual=fixture.profile.coordinates(s.pixel,s.viewDepth);for(unsigned i=0;i<4;++i)near(actual[i],s.projected[i],"Captured shader coordinates and near weight");}}
 std::cout<<"Fog view projection checks: "<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
