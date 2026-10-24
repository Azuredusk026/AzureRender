#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <array>
namespace azurerender {
enum class TextureFilterSemantic { Linear, Srgb, Normal, PackedNormal };
struct TextureMipChain { std::vector<std::uint8_t> pixels; unsigned levels=1; };
inline TextureMipChain buildTextureMipChain(const std::vector<std::uint8_t>& pixels,unsigned width,unsigned height,TextureFilterSemantic semantic,float alphaCutoff=0,bool bilinearAlphaCoverage=false) {
    if(!width||!height||pixels.size()!=std::size_t(width)*height*4)
        throw std::invalid_argument("RGBA mip input extent is invalid");
    const auto decode=[](float x){return x<=.04045F?x/12.92F:std::pow((x+.055F)/1.055F,2.4F);};
    std::array<float,256> decodeTable{};for(unsigned i=0;i<256;++i)decodeTable[i]=decode(i/255.F);
    const auto encode=[](float x){return x<=.0031308F?x*12.92F:1.055F*std::pow(x,1/2.4F)-.055F;};
    const auto byte=[](float x){return static_cast<std::uint8_t>(std::round(std::clamp(x,0.F,1.F)*255));};
    TextureMipChain result{pixels,1};std::vector<std::uint8_t> previous=pixels;
    const auto coverage=[&](const auto& data,float scale){std::size_t count=0;for(std::size_t i=3;i<data.size();i+=4)count+=byte(data[i]/255.F*scale)/255.F>=alphaCutoff;return float(count)/float(data.size()/4);};
    // Preserve sampled cutout area over bilinear texel cells. Counting texel
    // centres alone loses thin coverage as the minification footprint grows.
    const auto filteredCoverage=[&](const auto& data,unsigned width,unsigned height,float scale){
        std::size_t count=0;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            const unsigned nx=x+1==width?0:x+1,ny=y+1==height?0:y+1;
            const auto alpha=[&](unsigned sx,unsigned sy){return byte(data[(std::size_t(sy)*width+sx)*4+3]/255.F*scale)/255.F;};
            const float a=alpha(x,y),b=alpha(nx,y),c=alpha(x,ny),d=alpha(nx,ny);
            if(std::min(std::min(a,b),std::min(c,d))>=alphaCutoff){count+=16;continue;}
            if(std::max(std::max(a,b),std::max(c,d))<alphaCutoff)continue;
            for(unsigned sy=0;sy<4;++sy){const float v=(sy+.5F)*.25F;
                const float left=a+(c-a)*v,right=b+(d-b)*v;
                for(unsigned sx=0;sx<4;++sx)count+=left+(right-left)*((sx+.5F)*.25F)>=alphaCutoff;
            }
        }
        return float(count)/(float(width)*float(height)*16);
    };
    const float targetCoverage=alphaCutoff>0?coverage(pixels,1):0;
    while(width>1||height>1){
        const unsigned w=std::max(width/2,1U),h=std::max(height/2,1U);
        std::vector<std::uint8_t> next(std::size_t(w)*h*4);
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
            float mean[4]={0,0,0,0};unsigned count=0;
            const unsigned x0=x*width/w,x1=(x+1)*width/w,y0=y*height/h,y1=(y+1)*height/h;
            for(unsigned sy=y0;sy<y1;++sy)for(unsigned sx=x0;sx<x1;++sx){
                const auto i=(std::size_t(sy)*width+sx)*4;float value[4];
                for(unsigned c=0;c<4;++c)value[c]=previous[i+c]/255.F;
                if(semantic==TextureFilterSemantic::Srgb)for(unsigned c=0;c<3;++c)value[c]=decodeTable[previous[i+c]];
                if(semantic==TextureFilterSemantic::Normal)for(unsigned c=0;c<3;++c)value[c]=(value[c]*2-1)*value[3];
                if(semantic==TextureFilterSemantic::PackedNormal)for(unsigned c=0;c<4;++c)value[c]=value[c]*2-1;
                for(unsigned c=0;c<4;++c)mean[c]+=value[c];++count;
            }
            for(float& c:mean)c/=float(count);
            if(semantic==TextureFilterSemantic::Normal){
                const float length=std::sqrt(mean[0]*mean[0]+mean[1]*mean[1]+mean[2]*mean[2]);
                for(unsigned c=0;c<3;++c)mean[c]=length>1e-6F?mean[c]/length*.5F+.5F:(c==2?1.F:.5F);
                mean[3]=std::clamp(length,0.F,1.F);
            }
            if(semantic==TextureFilterSemantic::PackedNormal){
                for(unsigned c=0;c<4;c+=2){const float length=std::sqrt(mean[c]*mean[c]+mean[c+1]*mean[c+1]);if(length>1){mean[c]/=length;mean[c+1]/=length;}mean[c]=mean[c]*.5F+.5F;mean[c+1]=mean[c+1]*.5F+.5F;}
            }
            if(semantic==TextureFilterSemantic::Srgb)for(unsigned c=0;c<3;++c)mean[c]=encode(mean[c]);
            for(unsigned c=0;c<4;++c)next[(std::size_t(y)*w+x)*4+c]=byte(mean[c]);
        }
        if(alphaCutoff>0 && (semantic==TextureFilterSemantic::Srgb||semantic==TextureFilterSemantic::Linear) && targetCoverage>0 && targetCoverage<1){
            const auto sampledCoverage=[&](float scale){return bilinearAlphaCoverage?filteredCoverage(next,w,h,scale):coverage(next,scale);};
            float lo=0,hi=4;for(unsigned i=0;i<12;++i){float mid=(lo+hi)*.5F;if(sampledCoverage(mid)<targetCoverage)lo=mid;else hi=mid;}
            const auto scale=std::abs(sampledCoverage(lo)-targetCoverage)<=std::abs(sampledCoverage(hi)-targetCoverage)?lo:hi;
            for(std::size_t i=3;i<next.size();i+=4)next[i]=byte(next[i]/255.F*scale);
        }
        result.pixels.insert(result.pixels.end(),next.begin(),next.end());++result.levels;
        previous=std::move(next);width=w;height=h;
    }
    return result;
}
}
