#pragma once
#include "render/OctahedralEnvironment.hpp"
namespace azurerender::test {
inline OctahedralEnvironment octahedralFixture(){
    OctahedralEnvironment image;image.width=image.height=4;image.layers=2;image.mipLevels=3;
    for(unsigned mip=0;mip<3;++mip){const auto side=std::max(1U,4U>>mip);
        for(unsigned layer=0;layer<2;++layer)for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x){
            const auto mantissa=(mip*16+layer*8+x+y)%64;
            image.texels.push_back(((15U<<6)|mantissa)|((16U<<6)<<11)|((17U<<5)<<22));
        }}
    return image;
}
}
