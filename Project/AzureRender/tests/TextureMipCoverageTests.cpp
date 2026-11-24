#include "assets/TextureMipChain.hpp"
#include <iostream>
using namespace azurerender;
double sampledCoverage(const std::vector<std::uint8_t>& pixels,std::size_t offset,unsigned width,unsigned height){
    double covered=0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        auto alpha=[&](unsigned sx,unsigned sy){return pixels[offset+(std::size_t(sy%height)*width+sx%width)*4+3]/255.;};
        double a=alpha(x,y),b=alpha(x+1,y),c=alpha(x,y+1),d=alpha(x+1,y+1);
        for(unsigned sy=0;sy<32;++sy)for(unsigned sx=0;sx<32;++sx){double u=(sx+.5)/32,v=(sy+.5)/32;
            covered+=(a*(1-u)+b*u)*(1-v)+(c*(1-u)+d*u)*v>=.5;}
    }
    return covered/(double(width)*height*1024);
}
int main(){
 bool failed=false;
 for(unsigned shape=0;shape<2;++shape){
    std::vector<std::uint8_t> pixels(512*512*4,255);double target=0;
    for(unsigned y=0;y<512;++y)for(unsigned x=0;x<512;++x){
        double distance=std::hypot(double(x)-255.5,double(y)-255.5);
        bool covered=shape==0?std::fmod(x+y*.19,32.)<9:distance>=153&&distance<169;
        auto i=(y*512+x)*4;pixels[i]=std::uint8_t(x);pixels[i+1]=std::uint8_t(y);pixels[i+2]=std::uint8_t(x+y);
        pixels[i+3]=covered?255:0;target+=covered;
    }
    target/=512*512;
    auto chain=buildTextureMipChain(pixels,512,512,shape==0?TextureFilterSemantic::Linear:TextureFilterSemantic::Srgb,.5F,true);
    auto point=buildTextureMipChain(pixels,512,512,shape==0?TextureFilterSemantic::Linear:TextureFilterSemantic::Srgb,.5F);
    // Frozen texel-sampling alpha chains from cefc31f, including small tails.
    constexpr std::uint64_t pointHashes[]{5740879283081582922ULL,10050683099083470810ULL};
    std::uint64_t pointHash=14695981039346656037ULL;
    for(std::size_t i=3;i<point.pixels.size();i+=4)pointHash=(pointHash^point.pixels[i])*1099511628211ULL;
    if(pointHash!=pointHashes[shape])throw std::runtime_error("Texel-sampling alpha mip bytes changed");
    auto unmasked=buildTextureMipChain(pixels,512,512,shape==0?TextureFilterSemantic::Linear:TextureFilterSemantic::Srgb);
    for(std::size_t i=0;i<chain.pixels.size();++i)if(i%4!=3&&chain.pixels[i]!=unmasked.pixels[i])throw std::runtime_error("Alpha coverage changed colour mips");
    if(!std::equal(pixels.begin(),pixels.end(),chain.pixels.begin()))throw std::runtime_error("Authored level changed");
    std::size_t offset=0;unsigned width=512,height=512;
    for(unsigned level=0;level<=4;++level){
        if(level>=2){double coverage=sampledCoverage(chain.pixels,offset,width,height);double error=std::abs(coverage/target-1);
            std::cout<<"shape="<<shape<<" level="<<level<<" coverage="<<coverage<<" target="<<target<<" relativeError="<<error<<'\n';failed|=error>.02;}
        offset+=std::size_t(width)*height*4;width/=2;height/=2;
    }
 }
 if(failed){std::cerr<<"Bilinearly sampled mip coverage loses resolvable cutout area\n";return 1;}
}
