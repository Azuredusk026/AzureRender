#include "assets/TextureMipChain.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try {
    const std::vector<std::uint8_t> checker={0,0,0,255,255,255,255,255,255,255,255,255,0,0,0,255};
    auto colorMips=buildTextureMipChain(checker,2,2,TextureFilterSemantic::Srgb);
    require(colorMips.levels==2,"Texture minification requires a full mip chain");
    require(std::abs(int(colorMips.pixels[16])-188)<=1,"Color mips must average in linear space");
    const std::vector<std::uint8_t> normal={230,128,204,255,25,128,204,255,230,128,204,255,25,128,204,255};
    auto normalMips=buildTextureMipChain(normal,2,2,TextureFilterSemantic::Normal);
    require(normalMips.pixels[18]>=254,"Normal mip vectors must be normalized");
    require(normalMips.pixels[19]<200,"Normal variance must survive mip filtering");
    std::cout<<"Linear color, normal normalization and variance passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
