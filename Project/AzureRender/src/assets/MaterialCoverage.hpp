#pragma once
#include <cstdint>
#include <stdexcept>
namespace azurerender {
struct MaterialCoverageTexture {
    enum class Channel : std::uint8_t { Red,Alpha };
    bool present=false;
    Channel channel=Channel::Red;
};
inline void validateMaterialCoverageTexture(const MaterialCoverageTexture& value,unsigned profileVersion,
    bool masked,bool supported,bool conflict) {
    if(value.channel!=MaterialCoverageTexture::Channel::Red&&value.channel!=MaterialCoverageTexture::Channel::Alpha)
        throw std::invalid_argument("coverageTexture requires red or alpha");
    if(value.present&&(profileVersion!=2||!masked||!supported||conflict))
        throw std::invalid_argument("coverageTexture requires a version two MASK generic, fabric or hair surface with exclusive texture ownership");
}
}
