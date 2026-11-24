#include <iostream>
#include "render/IrradianceClipmap.hpp"
#include "render/IrradianceResource.hpp"
#include "support/IrradianceClipmapFixture.hpp"
#include <limits>
#include <functional>
#include <iomanip>
using namespace azurerender;
namespace {
void check(bool p,const char* text){if(!p)throw std::runtime_error(text);}
void close(float a,float b){check(std::abs(a-b)<1e-5F,"Clipmap value differs from independent scalar reference");}
void reject(const std::function<void()>& f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid clipmap must be rejected");}
auto fixture(){return test::clipmapFixture();}
}
int main(int argc,char** argv)try{
    if(argc==3&&std::string(argv[1])=="--query"){
        const auto field=IrradianceClipmap::load(std::filesystem::u8path(argv[2]));
        std::array<float,3> p{};std::cout<<std::setprecision(9);
        while(std::cin>>p[0]>>p[1]>>p[2]){const auto q=field.query(p);
            for(const auto& color:q.coefficients)for(float f:color)std::cout<<f<<' ';std::cout<<'\n';}
        return 0;
    }
    auto v=fixture();auto bytes=v.encode();
    check(bytes.size()==IrradianceClipmap::headerBytes+8*16*3,"Self-contained exact length");
    check(IrradianceClipmap::decode(bytes).encode()==bytes,"Lossless flags, transforms, defaults and all RGBA lanes");
    auto q=v.query({0,0,0});check(q.weight==1,"Clipmap includes its explicit default environment");
    close(q.coefficients[0][0],2);close(q.coefficients[0][1],2.F/255);close(q.coefficients[0][2],-2);
    close(q.coefficients[1][0],-4);close(q.coefficients[2][2],8);
    close(q.coefficients[0][3],1);close(v.query({30,0,0}).coefficients[0][3],1.5F);
    close(v.query({120,0,0}).coefficients[0][3],3);close(v.query({200,0,0}).coefficients[0][3],4);
    close(v.query({480,0,0}).coefficients[0][3],2.025F);
    check(v.query({500,0,0}).coefficients==v.defaultCoefficients,"Outside uses authored default SH");
    v.enabled=false;check(v.query({0,0,0}).coefficients==v.defaultCoefficients,"Disabled stack uses default SH");v.enabled=true;
    v.worldToField[0][0]=-1;close(v.query({-30,0,0}).coefficients[0][3],1.5F);
    close(v.query({0,0,0}).coefficients[0][0],-2);v.worldToField[0][0]=1;
    for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)v.amplitude[0][(z*2+y)*2+1]=test::clipmapAmplitude(1);
    close(v.query({.25F,0,0}).coefficients[0][3],1.25F);
    close(v.query({-.25F,0,0}).coefficients[0][3],1.25F);
    close(v.query({0,1.5F,0}).coefficients[0][0],2);
    for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)v.directional[0][((z*6+y)*2+x)*4+3]=255;
    close(v.query({0,0,0}).coefficients[0][1],2.F/255+.05F);
    close(v.query({0,0,0}).coefficients[0][3],1.05F);
    for(const auto mutation:{0U,7U,20U,216U}){auto b=bytes;b[mutation]=std::byte{255};reject([&]{(void)IrradianceClipmap::decode(b);});}
    auto b=bytes;b.pop_back();reject([&]{(void)IrradianceClipmap::decode(b);});
    b=bytes;b.push_back(std::byte{0});reject([&]{(void)IrradianceClipmap::decode(b);});
    auto invalid=fixture();invalid.dimensions={0,2,2};reject([&]{invalid.validate();});
    invalid=fixture();invalid.dimensions={0xffffffffU,0xffffffffU,2};reject([&]{invalid.validate();});
    invalid=fixture();invalid.layers[0].centerSpacing[3]=0;reject([&]{invalid.validate();});
    invalid=fixture();invalid.layers[0].fade[2]=0;reject([&]{invalid.validate();});
    invalid=fixture();invalid.worldToField[0][0]=2;reject([&]{invalid.validate();});
    invalid=fixture();invalid.defaultCoefficients[0][0]=std::numeric_limits<float>::infinity();reject([&]{invalid.validate();});
    invalid=fixture();invalid.amplitude[0][0]=31U<<6;reject([&]{invalid.validate();});
    invalid=fixture();invalid.directional[0].pop_back();reject([&]{invalid.validate();});
    reject([&]{(void)v.query({std::numeric_limits<float>::quiet_NaN(),0,0});});
    if(argc==2){const auto path=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(path.parent_path());
        std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());f.close();
        check(IrradianceClipmap::load(path).encode()==bytes,"File save/reopen");
        check(readIrradianceResourceKind(path)==IrradianceResourceKind::Clipmap,"Typed clipmap header");
        const auto renamed=path.parent_path()/"renamed.bin";
        std::filesystem::copy_file(path,renamed,std::filesystem::copy_options::overwrite_existing);
        check(readIrradianceResourceKind(renamed)==IrradianceResourceKind::Clipmap,"Kind is independent of file extension");
        IrradianceVolume grid;grid.cells.resize(1);const auto old=grid.encode();
        const auto gridPath=path.parent_path()/"grid.bin";
        {std::ofstream out(gridPath,std::ios::binary);out.write(reinterpret_cast<const char*>(old.data()),old.size());}
        check(readIrradianceResourceKind(gridPath)==IrradianceResourceKind::Grid,"Existing grid header");
        {std::ofstream out(renamed,std::ios::binary);out<<"AZIC0002";}
        reject([&]{(void)readIrradianceResourceKind(renamed);});
        {std::ofstream out(renamed,std::ios::binary);out<<"AZI";}
        reject([&]{(void)readIrradianceResourceKind(renamed);});
        reject([&]{(void)readIrradianceResourceKind(path.parent_path()/"missing-field.bin");});
    }
    std::cout<<"PASS: clipmap asset, interpolation, independent colors, transforms, transitions and invalid input\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
