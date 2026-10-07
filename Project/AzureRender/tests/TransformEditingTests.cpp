#include "scene/TransformEditing.hpp"
#include <iostream>
int main() {
    using namespace azurerender;
    for(const internal::Vector3 rotation: {internal::Vector3{17,-33,61},{40,90,20},{-40,-90,70},{180,0,-180}})
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
    if(!rejects(shear)||!rejects(nan)||!rejects(projective)||!rejects(internal::uniformScale(0)))return 2;
    return 0;
}
