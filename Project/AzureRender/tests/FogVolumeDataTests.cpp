#include "render/FogVolumeData.hpp"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <chrono>
#include <string>

using namespace azurerender;
namespace {
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void near(float a,float b,const char* message){require(std::abs(a-b)<2e-6F,message);}
template<class F> void reject(F function){bool failed=false;try{function();}catch(const std::invalid_argument&){failed=true;}require(failed,"Invalid fog volume must fail");}
}
int main(int argc,char** argv){try{
 FogVolumeData volume;volume.dimensions={2,2,2};
 for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
  for(auto value:{std::uint16_t(x?0x3c00:0),std::uint16_t(y?0x4000:0),std::uint16_t(z?0x4200:0),std::uint16_t(x?0x3800:0x3c00)})volume.rgba16f.push_back(value);
 const auto bytes=volume.encode();const auto restored=FogVolumeData::decode(bytes);
 require(restored.dimensions==volume.dimensions,"Volume dimensions survive serialization");
 require(restored.rgba16f==volume.rgba16f,"Half-float payload survives without quantization");
 const auto center=restored.sample({.5F,.5F,.5F});
 near(center.scattering[0],.5F,"Trilinear X center");near(center.scattering[1],1,"Trilinear Y center");near(center.scattering[2],1.5F,"Trilinear Z center");near(center.transmittance[0],.75F,"Trilinear optical transmittance");
 const auto corner=restored.sample({-1,2,3});near(corner.scattering[0],0,"Clamp-to-edge X");near(corner.scattering[1],2,"Clamp-to-edge Y");near(corner.scattering[2],3,"Clamp-to-edge Z");
 const auto texel=restored.sample({.75F,.25F,.25F});near(texel.scattering[0],1,"Half-texel normalized coordinates");near(texel.transmittance[1],.5F,"Scalar transmittance expands to all channels");
 auto bad=bytes;bad.pop_back();reject([&]{(void)FogVolumeData::decode(bad);});bad=bytes;bad.push_back(0);reject([&]{(void)FogVolumeData::decode(bad);});bad=bytes;bad[7]='2';reject([&]{(void)FogVolumeData::decode(bad);});
 for(unsigned component:{0U,1U,2U}){auto invalid=volume;invalid.dimensions[component]=0;reject([&]{invalid.validate();});invalid=volume;invalid.dimensions[component]=257;reject([&]{invalid.validate();});}
 auto invalid=volume;invalid.dimensions={256,256,256};reject([&]{invalid.validate();});invalid=volume;invalid.rgba16f.pop_back();reject([&]{invalid.validate();});
 for(std::uint16_t value:{std::uint16_t(0x7c00),std::uint16_t(0x7e00),std::uint16_t(0xbc00)}){invalid=volume;invalid.rgba16f[0]=value;reject([&]{invalid.validate();});}
 invalid=volume;invalid.rgba16f[3]=0x4000;reject([&]{invalid.validate();});
 reject([&]{(void)restored.sample({0,std::numeric_limits<float>::quiet_NaN(),0});});
 const auto path=std::filesystem::temp_directory_path()/("azure-fog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".azfog");
 volume.save(path);require(FogVolumeData::load(path).rgba16f==volume.rgba16f,"Save and reopen public volume");std::filesystem::remove(path);
 if(argc==2||argc==3){const auto source=FogVolumeData::load(argv[1]);require(source.dimensions==std::array<std::uint32_t,3>{117,90,128},"Captured source dimensions");for(auto uvw:{std::array<float,3>{0,0,0},std::array<float,3>{.2F,.7F,.4F},std::array<float,3>{1,1,1}})validateFogTransport(source.sample(uvw));if(argc==3)source.save(argv[2]);}
 std::cout<<"Fog volume checks: "<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
