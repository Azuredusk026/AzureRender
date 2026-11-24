#pragma once
#include <cstdint>
#include <stdexcept>

namespace azurerender {
enum class MaterialDisplayTransform : std::uint32_t { Scene, Linear };
inline void validateMaterialDisplayTransform(MaterialDisplayTransform value,bool inheritedComposite=false) {
    if(value!=MaterialDisplayTransform::Scene&&value!=MaterialDisplayTransform::Linear)
        throw std::invalid_argument("displayTransform requires scene or linear");
    if(inheritedComposite&&value!=MaterialDisplayTransform::Scene)
        throw std::invalid_argument("displayTransform inherits the destination for multiplicative or additive surfaces");
}
inline const char* materialDisplayTransformName(MaterialDisplayTransform value) {
    validateMaterialDisplayTransform(value);
    return value==MaterialDisplayTransform::Linear?"linear":"scene";
}
}
