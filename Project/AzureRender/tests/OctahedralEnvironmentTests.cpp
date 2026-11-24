#include "support/OctahedralEnvironmentFixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
#include <chrono>
#include <limits>
#include <string>
using namespace azurerender;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void reject(const std::function<void()>& call){try{call();}catch(const std::exception&){return;}throw std::runtime_error("Invalid octahedral input accepted");}
int main()try{
    auto image=test::octahedralFixture();image.validate();
    check(image.texels.size()==42,"All layers and levels are retained");
    const std::size_t offsets[3][2]={{0,16},{32,36},{40,41}};
    for(unsigned mip=0;mip<3;++mip)for(unsigned layer=0;layer<2;++layer)
        check(image.subresourceOffset(mip,layer)==offsets[mip][layer],"Mip-major array offsets");
    reject([&]{(void)image.subresourceOffset(3,0);});reject([&]{(void)image.subresourceOffset(0,2);});
    auto bytes=image.encode();check(bytes.size()==216&&std::memcmp(bytes.data(),"AZOA0001",8)==0,"Versioned exact payload");
    check(bytes[8]==std::byte{4}&&bytes[12]==std::byte{4}&&bytes[16]==std::byte{2}&&bytes[20]==std::byte{3},"Little-endian dimensions");
    auto decoded=OctahedralEnvironment::decode(bytes);check(decoded.encode()==bytes,"Complete binary roundtrip");
    check(decoded.texels==image.texels,"Packed HDR words preserve every bit");
    const auto temporary=std::filesystem::temp_directory_path()/("azure-oct-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".azoa");
    {std::ofstream out(temporary,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    const auto loaded=OctahedralEnvironment::load(temporary);std::filesystem::remove(temporary);
    check(loaded.encode()==bytes,"Saved asset load");
    for(unsigned index:{0U,7U,40U,44U}){auto invalid=bytes;invalid[index]=std::byte{127};reject([&]{(void)OctahedralEnvironment::decode(invalid);});}
    auto truncated=bytes;truncated.pop_back();reject([&]{(void)OctahedralEnvironment::decode(truncated);});
    auto trailing=bytes;trailing.push_back(std::byte{});reject([&]{(void)OctahedralEnvironment::decode(trailing);});
    for(unsigned index:{8U,12U,16U,20U}){auto invalid=bytes;for(unsigned n=0;n<4;++n)invalid[index+n]=std::byte{255};reject([&]{(void)OctahedralEnvironment::decode(invalid);});}
    for(auto value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        auto bad=image;bad.uvScaleOffset[0]=value;reject([&]{bad.validate();});
        reject([&]{(void)image.directionUv({value,0,1});});
    }
    for(const auto& region:std::array<std::array<float,4>,4>{{{0,1,0,0},{1.1F,1,0,0},{1,1,-.1F,0},{.9F,1,.2F,0}}}){
        auto bad=image;bad.uvScaleOffset=region;reject([&]{bad.validate();});
    }
    auto mismatch=image;mismatch.texels.pop_back();reject([&]{mismatch.validate();});
    auto nonsquare=image;nonsquare.height=3;reject([&]{nonsquare.validate();});
    auto overBudget=image;overBudget.width=overBudget.height=4096;overBudget.layers=256;overBudget.mipLevels=1;
    reject([&]{overBudget.validate();});
    auto invalidFold=image;invalidFold.foldZeroSign=static_cast<OctahedralEnvironment::FoldZeroSign>(2);reject([&]{invalidFold.validate();});
    for(auto packed:{0x7c0U,0x7c0U<<11,0x3e0U<<22}){auto bad=image;bad.texels[0]=packed;reject([&]{bad.validate();});}
    auto subnormal=image;subnormal.texels[0]=1U|(1U<<11)|(1U<<22);subnormal.validate();
    reject([&]{(void)image.directionUv({0,0,0});});
    const std::array<std::array<float,3>,6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
    const std::array<std::array<float,2>,6> expected{{{1,.5F},{0,.5F},{.5F,1},{.5F,0},{.5F,.5F},{1,1}}};
    for(unsigned i=0;i<6;++i)check(image.directionUv(directions[i])==expected[i],"Canonical axis projection");
    image.foldZeroSign=OctahedralEnvironment::FoldZeroSign::Mathematical;
    check(image.directionUv({0,0,-1})==std::array<float,2>{.5F,.5F},"Source mathematical-zero fold is explicit");
    image.uvScaleOffset={8.F/9,8.F/9,1.F/18,1.F/18};
    const auto uv=image.directionUv({1,0,0});check(std::abs(uv[0]-17.F/18)<1e-6F&&uv[1]==.5F,"Authored padding transform");
    check(OctahedralEnvironment::decode(image.encode()).foldZeroSign==OctahedralEnvironment::FoldZeroSign::Mathematical,"Fold convention survives storage");
    std::cout<<"PASS: octahedral asset budgets, finite HDR, subresources, padding and explicit zero-sign convention\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
