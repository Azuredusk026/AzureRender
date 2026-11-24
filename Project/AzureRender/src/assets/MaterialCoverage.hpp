#pragma once
#include <cstdint>
#include <stdexcept>
namespace azurerender {
struct MaterialCoverageTexture {
    enum class Channel : std::uint8_t { Red,Alpha };
    enum class LodPolicy : std::uint8_t { Device,EllipseMajor };
    bool present=false;
    Channel channel=Channel::Red;
    LodPolicy lodPolicy=LodPolicy::Device;
};
inline void validateMaterialCoverageTexture(const MaterialCoverageTexture& value,unsigned profileVersion,
    bool masked,bool supported,bool conflict) {
    if(value.channel!=MaterialCoverageTexture::Channel::Red&&value.channel!=MaterialCoverageTexture::Channel::Alpha)
        throw std::invalid_argument("coverageTexture requires red or alpha");
    if(value.lodPolicy!=MaterialCoverageTexture::LodPolicy::Device
        &&(!value.present||value.lodPolicy!=MaterialCoverageTexture::LodPolicy::EllipseMajor))
        throw std::invalid_argument("coverageTexture has an invalid LOD policy");
    if(value.present&&(profileVersion!=2||!masked||!supported||conflict))
        throw std::invalid_argument("coverageTexture requires a version two MASK generic, fabric or hair surface with exclusive texture ownership");
}
}
