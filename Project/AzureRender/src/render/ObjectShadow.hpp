#pragma once
#include "ecs/ObjectShadow.hpp"
#include "render/LinearMath.hpp"
#include <algorithm>
#include <cstddef>
#include <set>
#include <type_traits>
#include <vector>

namespace azurerender {
inline constexpr unsigned kObjectShadowCapacity=8;
inline constexpr unsigned kObjectShadowTileSize=1024;
inline constexpr unsigned kObjectShadowColumns=4;
inline constexpr unsigned kObjectShadowRows=2;
struct alignas(16) ObjectShadowGpuRecord {
    internal::Matrix4 worldToUnitShadow{};
    std::array<float,4> biasAndFilter{};
    std::array<float,4> atlasRect{};
};
static_assert(std::is_trivially_copyable_v<ObjectShadowGpuRecord>);
static_assert(sizeof(ObjectShadowGpuRecord)==96);
static_assert(offsetof(ObjectShadowGpuRecord,biasAndFilter)==64);
static_assert(offsetof(ObjectShadowGpuRecord,atlasRect)==80);
struct ObjectShadowInput {
    ecs::ObjectShadowComponent settings;
    internal::Matrix4 model;
    std::uint32_t instanceIndex=0;
};
struct ObjectShadowPage {
    ObjectShadowGpuRecord gpu;
    internal::Matrix4 worldToClip;
    std::uint32_t instanceIndex=0;
    unsigned page=0;
};
struct ObjectShadowFrame { std::vector<ObjectShadowPage> pages; };

// Projection extents are world metres in the main-light frame. Only the local
// centre follows the instance transform; mesh scale does not resize the box.
inline ObjectShadowFrame buildObjectShadowFrame(const std::vector<ObjectShadowInput>& inputs,
    const internal::Vector3& outgoingDirection) {
    using namespace internal;
    ObjectShadowFrame result;
    std::array<bool,kObjectShadowCapacity> occupied{};
    std::set<std::uint32_t> instances;
    for(const auto& input:inputs){
        input.settings.validate();
        if(!input.settings.enabled)continue;
        if(!instances.insert(input.instanceIndex).second||instances.size()>kObjectShadowCapacity)
            throw std::invalid_argument("Object shadow instance capacity or identity conflict");
        for(float value:input.model)if(!std::isfinite(value))throw std::invalid_argument("Nonfinite object shadow transform");
        if(input.model[3]!=0||input.model[7]!=0||input.model[11]!=0||input.model[15]!=1)
            throw std::invalid_argument("Object shadow requires an affine instance transform");
        if(input.settings.atlasPage<kObjectShadowCapacity){
            auto& used=occupied[static_cast<unsigned>(input.settings.atlasPage)];
            if(used)throw std::invalid_argument("Duplicate explicit object shadow atlas page");
            used=true;
        }
    }
    if(instances.empty())return result;
    const float length=vectorLength(outgoingDirection);
    if(!std::isfinite(length)||length<1e-6F)throw std::invalid_argument("Object shadow requires a finite nonzero light direction");
    const auto direction=scaleVector(outgoingDirection,1/length);
    const Vector3 up=std::abs(direction[1])>.99F?Vector3{0,0,1}:Vector3{0,1,0};
    const auto right=normalize(cross(up,direction));
    const auto vertical=cross(direction,right);
    const std::array<Vector3,3> axes{right,vertical,direction};
    for(const auto& input:inputs){
        const auto& settings=input.settings;if(!settings.enabled)continue;
        unsigned page=static_cast<unsigned>(settings.atlasPage);
        if(settings.atlasPage==kObjectShadowCapacity){page=static_cast<unsigned>(std::find(occupied.begin(),occupied.end(),false)-occupied.begin());occupied.at(page)=true;}
        ObjectShadowPage entry{};entry.page=page;entry.instanceIndex=input.instanceIndex;
        auto& matrix=entry.gpu.worldToUnitShadow;matrix[15]=1;
        const auto center=transformPosition(input.model,settings.center);
        for(float value:center)if(!std::isfinite(value))throw std::invalid_argument("Object shadow world center overflow");
        for(unsigned row=0;row<3;++row){
            const auto axis=scaleVector(axes[row],.5F/settings.halfExtents[row]);
            for(unsigned column=0;column<3;++column)matrix[column*4+row]=axis[column];
            matrix[12+row]=.5F-dot(axis,center);
        }
        for(float value:matrix)if(!std::isfinite(value))throw std::invalid_argument("Object shadow projection overflow");
        entry.worldToClip=matrix;
        for(unsigned column=0;column<4;++column)for(unsigned row=0;row<2;++row)
            entry.worldToClip[column*4+row]=2*matrix[column*4+row]-(column==3?1.F:0.F);
        entry.gpu.biasAndFilter={settings.normalBias,settings.lightBias,settings.filterRadiusX,settings.filterRadiusY};
        entry.gpu.atlasRect={float(page%kObjectShadowColumns)/kObjectShadowColumns,float(page/kObjectShadowColumns)/kObjectShadowRows,1.F/kObjectShadowColumns,1.F/kObjectShadowRows};
        result.pages.push_back(entry);
    }
    return result;
}
}
