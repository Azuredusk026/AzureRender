#include "assets/TextureImportSettings.hpp"
#include <iostream>
using namespace azurerender;
int main(){try{
 const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
 auto normal=TextureImportSettings::parse({{"schemaVersion",1},{"usage","normal"},{"colorSpace","linear"},{"filter","linear"},{"wrapU","repeat"},{"wrapV","clamp"},{"anisotropy",16},{"mipPolicy","generate"}});
 require(normal.semantic()==TextureFilterSemantic::Normal,"Normal imports must renormalize mip levels");
 require(normal.sampler(4,8).maxAnisotropy==8,"Sampler must clamp anisotropy to device support");
 require(normal.sampler(1,1).maxLod==0,"Single-level textures must remain within their image");
 auto color=TextureImportSettings::parse({{"usage","albedo"}});require(color.semantic()==TextureFilterSemantic::Srgb,"Color defaults must use sRGB filtering");
 bool rejected=false;try{TextureImportSettings::parse({{"usage","normal"},{"colorSpace","srgb"}});}catch(const std::exception&){rejected=true;}
 require(rejected,"Normal maps must reject gamma color space");
 rejected=false;try{TextureImportSettings::parse({{"filter","unknown"}});}catch(const std::exception&){rejected=true;}require(rejected,"Invalid filters must be rejected");
 require(TextureImportSettings::parse(color.describe()).describe()==color.describe(),"Texture settings must roundtrip");
 std::vector<std::uint8_t> mask(4*4*4,255);
 const unsigned alpha[]={255,255,255,255,255,0,255,0,255,0,0,0,255,0,0,0};
 for(unsigned i=0;i<16;++i)mask[i*4+3]=static_cast<std::uint8_t>(alpha[i]);
 const auto mips=buildTextureMipChain(mask,4,4,TextureFilterSemantic::Linear,.5F);
 unsigned coverage=0;for(unsigned i=0;i<4;++i)coverage+=mips.pixels[64+i*4+3]>=128;
 if(coverage!=2){for(unsigned i=0;i<4;++i)std::cerr<<unsigned(mips.pixels[64+i*4+3])<<' ';std::cerr<<"coverage="<<coverage<<'\n';}
 require(coverage==2,"Linear alpha masks must preserve threshold coverage when minified");
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
