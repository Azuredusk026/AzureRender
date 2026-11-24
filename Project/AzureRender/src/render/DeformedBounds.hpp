#pragma once
#include "assets/GltfLoader.hpp"
#include "assets/AssetDeformation.hpp"
#include "scene/SceneDescription.hpp"
#include <algorithm>
#include <cmath>

namespace azurerender {
// Every positive-weight joint encloses the original vertex. Their transformed
// union encloses the normalized weighted sum, without scanning vertices per pose.
struct JointBounds {
    struct MorphEnvelope {std::uint32_t node=0;std::vector<scene::AxisAlignedBounds> targets;};
    struct Envelope {
        scene::AxisAlignedBounds base{}, morph0{}, morph1{};
        bool used=false;
        std::vector<MorphEnvelope> morphs;
    };
    std::vector<Envelope> joints;
    static JointBounds build(const LoadedAsset& asset) {
        JointBounds result;result.joints.resize(std::max<std::size_t>(1,asset.jointMatrices.size()));
        for(const auto& vertex:asset.vertices)for(unsigned slot=0;slot<(asset.hasSkin?4U:1U);++slot){
            if(asset.hasSkin && vertex.weights[slot]<=0)continue;
            auto& e=result.joints.at(asset.hasSkin?vertex.joints[slot]:0);
            if(!e.used){e.base={vertex.position,vertex.position};e.morph0={vertex.morph0,vertex.morph0};e.morph1={vertex.morph1,vertex.morph1};}
            else for(unsigned axis=0;axis<3;++axis){
                e.base.minimum[axis]=std::min(e.base.minimum[axis],vertex.position[axis]);e.base.maximum[axis]=std::max(e.base.maximum[axis],vertex.position[axis]);
                e.morph0.minimum[axis]=std::min(e.morph0.minimum[axis],vertex.morph0[axis]);e.morph0.maximum[axis]=std::max(e.morph0.maximum[axis],vertex.morph0[axis]);
                e.morph1.minimum[axis]=std::min(e.morph1.minimum[axis],vertex.morph1[axis]);e.morph1.maximum[axis]=std::max(e.morph1.maximum[axis],vertex.morph1[axis]);
            }
            e.used=true;
        }
        for(const auto& range:asset.morphRanges) {
            const auto entry=result.joints.front().morphs.size();
            for(auto& joint:result.joints)joint.morphs.push_back({range.node,std::vector<scene::AxisAlignedBounds>(range.targets.size())});
            for(std::size_t i=0;i<range.vertexCount;++i){
                const auto& vertex=asset.vertices.at(range.firstVertex+i);
                for(unsigned slot=0;slot<(asset.hasSkin?4U:1U);++slot){
                    if(asset.hasSkin&&vertex.weights[slot]<=0)continue;
                    auto& targets=result.joints.at(asset.hasSkin?vertex.joints[slot]:0).morphs[entry].targets;
                    for(std::size_t t=0;t<range.targets.size();++t)if(!range.targets[t].positions.empty()){
                        const auto& delta=range.targets[t].positions.at(i);
                        for(unsigned a=0;a<3;++a){targets[t].minimum[a]=std::min(targets[t].minimum[a],delta[a]);targets[t].maximum[a]=std::max(targets[t].maximum[a],delta[a]);}
                    }
                }
            }
        }
        return result;
    }
    scene::AxisAlignedBounds evaluate(const LoadedAsset& asset,const std::array<float,2>& weights,
        const std::vector<std::array<float,16>>& matrices,const AssetPose* pose=nullptr) const {
        scene::AxisAlignedBounds result{asset.boundsMin,asset.boundsMax};
        for(std::size_t joint=0;joint<joints.size();++joint){
            const auto& e=joints[joint];if(!e.used)continue;auto bounds=e.base;
            if(!asset.morphRanges.empty()) {
                const auto& states=pose?pose->morphWeights:asset.defaultMorphWeights;
                for(const auto& morph:e.morphs){const auto& values=states.at(morph.node);
                    if(values.size()!=morph.targets.size())throw std::invalid_argument("Morph bounds shape mismatch");
                    for(std::size_t t=0;t<values.size();++t)for(unsigned axis=0;axis<3;++axis){
                        const float w=values[t];const auto& delta=morph.targets[t];
                        bounds.minimum[axis]+=w*(w>=0?delta.minimum[axis]:delta.maximum[axis]);
                        bounds.maximum[axis]+=w*(w>=0?delta.maximum[axis]:delta.minimum[axis]);
                    }
                }
            } else
            for(unsigned axis=0;axis<3;++axis)for(unsigned morph=0;morph<2;++morph){
                const auto& delta=morph==0?e.morph0:e.morph1;
                bounds.minimum[axis]+=weights[morph]*(weights[morph]>=0?delta.minimum[axis]:delta.maximum[axis]);
                bounds.maximum[axis]+=weights[morph]*(weights[morph]>=0?delta.maximum[axis]:delta.minimum[axis]);
            }
            if(asset.hasSkin)bounds=scene::transformBounds(bounds,matrices.at(joint));
            for(unsigned axis=0;axis<3;++axis){result.minimum[axis]=std::min(result.minimum[axis],bounds.minimum[axis]);result.maximum[axis]=std::max(result.maximum[axis],bounds.maximum[axis]);}
        }
        return result;
    }
};
inline scene::AxisAlignedBounds expandBounds(scene::AxisAlignedBounds bounds, float margin) {
    for (std::size_t i = 0; i < 3; ++i) {
        bounds.minimum[i] -= margin;
        bounds.maximum[i] += margin;
    }
    return bounds;
}
inline float materialDisplacementMargin(const LoadedAsset& asset) {
    float margin = 0.0F;
    for (const auto& material : asset.materials) {
        if ((material.materialFeatures & MaterialFeatureBrowOverlay) != 0)
            margin = std::max(margin, std::abs(material.featureParameters[0])
                + std::abs(std::min(material.styleParameters[0], 0.0012F)));
    }
    // The vertex fallback applies brow thickness before skinning. A row-sum
    // bound covers arbitrary affine joint scale/shear; retain post-skin size.
    float amplification = 1.0F;
    for (const auto& matrix : asset.jointMatrices)
        for (std::size_t row = 0; row < 3; ++row)
            amplification = std::max(amplification,
                std::abs(matrix[row]) + std::abs(matrix[4 + row]) + std::abs(matrix[8 + row]));
    return margin * amplification;
}
inline scene::AxisAlignedBounds includeTransformedBounds(
    scene::AxisAlignedBounds bounds, const internal::Matrix4& transform) {
    const auto transformed = scene::transformBounds(bounds, transform);
    for (std::size_t i = 0; i < 3; ++i) {
        bounds.minimum[i] = std::min(bounds.minimum[i], transformed.minimum[i]);
        bounds.maximum[i] = std::max(bounds.maximum[i], transformed.maximum[i]);
    }
    return bounds;
}
inline scene::AxisAlignedBounds morphBounds(const LoadedAsset& asset,
                                            const std::array<float, 2>& weights,
                                            const std::vector<std::array<float,16>>* pose = nullptr) {
    const auto& matrices = pose ? *pose : asset.jointMatrices;
    scene::AxisAlignedBounds bounds{asset.boundsMin, asset.boundsMax};
    for (const auto& vertex : asset.vertices) {
        std::array<float, 3> position{};
        for (std::size_t i = 0; i < 3; ++i)
            position[i] = vertex.position[i] + vertex.morph0[i] * weights[0]
                + vertex.morph1[i] * weights[1];
        if (asset.hasSkin && !matrices.empty()) {
            const auto morphed = position;
            position = {};
            for (std::size_t joint = 0; joint < 4; ++joint) {
                const auto& matrix = matrices.at(vertex.joints[joint]);
                for (std::size_t i = 0; i < 3; ++i)
                    position[i] += vertex.weights[joint] * (matrix[i] * morphed[0]
                        + matrix[4 + i] * morphed[1] + matrix[8 + i] * morphed[2] + matrix[12 + i]);
            }
        }
        for (std::size_t i = 0; i < 3; ++i) {
            bounds.minimum[i] = std::min(bounds.minimum[i], position[i]);
            bounds.maximum[i] = std::max(bounds.maximum[i], position[i]);
        }
    }
    return bounds;
}
}  // namespace azurerender
