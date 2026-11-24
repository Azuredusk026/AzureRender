#include "scene/TransformEditing.hpp"
#include <iostream>
int main() {
    using namespace azurerender;
    try {
    // This orthogonal imported frame has no cosine terms at its exact pole.
    // Reciprocal scale normalization can round its sine away from +/-1.
    const internal::Matrix4 imported{
        0,.7059292793273926F,0,0,
        0,0,-9.756230354309082F,0,
        -10.965432167053223F,0,0,0,
        -80.19000244140625F,9.139999389648438F,63.03000259399414F,1};
    const auto importedValue=scene::decomposeTrs(imported);
    const auto importedRebuilt=scene::composeTrs(importedValue.translation,importedValue.rotation,importedValue.scale);
    for(unsigned i=0;i<16;++i)if(std::abs(importedRebuilt[i]-imported[i])>2e-6F) {
        std::cerr<<"Exact-pole imported frame must retain its off-axis rotation and scale\n";return 6;
    }
    // Rounded sine is exactly one, while the small cosine still represents
    // an authored 89.99 degree rotation. asin(sine) alone loses that angle.
    for(const float side:{-1.F,1.F}) {
        const internal::Matrix4 matrix{
            .00034906584F,0,-2*side,0,
            0,3,0,0,
            4*side,0,.00069813168F,0,
            10,-20,30,1};
        const auto value=scene::decomposeTrs(matrix);
        const auto rebuilt=scene::composeTrs(value.translation,value.rotation,value.scale);
        if(std::abs(value.rotation[1]-89.99F*side)>2e-5F) {
            std::cerr<<"Near-pole rotation must retain the cosine encoded in the matrix\n";return 3;
        }
        for(unsigned i=0;i<16;++i)if(std::abs(rebuilt[i]-matrix[i])>2e-6F) {
            std::cerr<<"Near-pole authored matrix must reconstruct within its existing precision budget\n";return 4;
        }
    }
    for(const internal::Vector3 rotation: {internal::Vector3{17,-33,61},{40,90,20},{-40,-90,70},{180,0,-180},
            {37,89.999F,61},{37,-89.999F,61},{-57,89.99F,-21},{-57,-89.99F,-21},
            {37,90.001F,61},{37,-90.001F,61}})
        for(const internal::Vector3 scale: {internal::Vector3{2,3,4},{-2,3,4},{2,-3,-4},{-2,-3,-4}}) {
            const auto matrix=scene::composeTrs({10,-20,30},rotation,scale);
            const auto value=scene::decomposeTrs(matrix,scale);
            const auto rebuilt=scene::composeTrs(value.translation,value.rotation,value.scale);
            const auto identity=internal::multiply(matrix,scene::inverseAffine(matrix));
            for(unsigned i=0;i<16;++i) {
                if(std::abs(rebuilt[i]-matrix[i])>1e-4F || std::abs(identity[i]-(i%5==0?1.F:0.F))>1e-4F) {
                    std::cerr<<"TRS roundtrip and affine inverse must retain signed scales and gimbal rotations\n";return 1;
                }
            }
        }
    const auto rejects=[](internal::Matrix4 matrix) {
        try{scene::decomposeTrs(matrix);return false;}catch(const std::invalid_argument&){return true;}
    };
    auto shear=scene::identityMatrix();shear[4]=.1F;
    auto nan=scene::identityMatrix();nan[5]=NAN;
    auto projective=scene::identityMatrix();projective[3]=.1F;
    auto imprecise=internal::uniformScale(4);imprecise[4]=.00036F;
    if(!rejects(shear)||!rejects(nan)||!rejects(projective)||!rejects(internal::uniformScale(0))||!rejects(imprecise))return 2;
    return 0;
    }catch(const std::exception& error) {
        std::cerr<<"Valid near-pole TRS rejected: "<<error.what()<<'\n';return 5;
    }
}
