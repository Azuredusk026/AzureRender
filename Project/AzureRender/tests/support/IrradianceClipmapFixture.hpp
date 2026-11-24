#pragma once
#include "render/IrradianceClipmap.hpp"
namespace azurerender::test {
inline std::uint32_t clipmapAmplitude(unsigned exponent){
    return ((15U+exponent)<<6)|(((16U+exponent)<<6)<<11)|(((17U+exponent)<<5)<<22);
}
inline IrradianceClipmap clipmapFixture(){
    IrradianceClipmap v;v.dimensions={2,2,2};
    for(unsigned i=0;i<3;++i){
        v.layers[i].centerSpacing={0,0,0,float(1U<<i)};
        v.layers[i].fade={float(29U<<(i*2)),float(13U<<(i*2)),float(2U<<(i*2)),0};
        v.amplitude[i].assign(8,clipmapAmplitude(i));v.directional[i].resize(8*3*4);
        const std::array<std::array<unsigned char,4>,3> colors={{{255,128,0,0},{0,255,128,128},{128,0,255,255}}};
        for(unsigned z=0;z<2;++z)for(unsigned c=0;c<3;++c)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
            for(unsigned lane=0;lane<4;++lane)v.directional[i][((z*6+c*2+y)*2+x)*4+lane]=colors[c][lane];
    }
    v.defaultCoefficients={{{0,0,0,.1F},{0,0,0,.2F},{0,0,0,.3F}}};v.validate();return v;
}
}
