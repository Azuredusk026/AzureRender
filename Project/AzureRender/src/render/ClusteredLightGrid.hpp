#pragma once

#include "render/LightBuffer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace azurerender {

struct ClusterGridDesc {
    std::uint32_t x = 1;
    std::uint32_t y = 1;
    std::uint32_t z = 1;
    float nearPlane = 0.1F;
    float farPlane = 100.0F;
    float projectionScaleX = 1.0F;
    float projectionScaleY = 1.0F;
    bool orthographic = false;
    float projectionOffsetX=0,projectionOffsetY=0;
    bool operator==(const ClusterGridDesc& o) const {
        return x==o.x&&y==o.y&&z==o.z&&nearPlane==o.nearPlane&&farPlane==o.farPlane
            &&projectionScaleX==o.projectionScaleX&&projectionScaleY==o.projectionScaleY
            &&orthographic==o.orthographic&&projectionOffsetX==o.projectionOffsetX&&projectionOffsetY==o.projectionOffsetY;
    }
    bool operator!=(const ClusterGridDesc& o) const {return !(*this==o);}
};

class ClusteredLightGrid final {
public:
    explicit ClusteredLightGrid(ClusterGridDesc desc) { configure(desc); }

    // Starts a new assignment while retaining storage for an equal cell count.
    // References to prior memberships are invalidated by configure()/assign().
    // Invalid configuration or allocation failure preserves the previous grid.
    void configure(ClusterGridDesc desc) {
        if (desc.x == 0 || desc.y == 0 || desc.z == 0
            || !(desc.nearPlane > 0.0F) || !(desc.farPlane > desc.nearPlane)) {
            throw std::invalid_argument("Invalid clustered light grid");
        }
        for(float offset:{desc.projectionOffsetX,desc.projectionOffsetY})if(!std::isfinite(offset)||std::abs(offset)>2)
            throw std::invalid_argument("Invalid clustered projection offset");
        const auto count=static_cast<std::size_t>(desc.x)*desc.y*desc.z;
        if(count!=clusters_.size()){
            std::vector<std::vector<std::uint32_t>> replacement(count);
            clusters_.swap(replacement);
        }else for(auto& cluster:clusters_)cluster.clear();
        desc_=desc;
        assignmentCached_=false;
    }

    // Only membership inputs participate: colour/intensity updates keep the list.
    void assign(const std::vector<RenderLight>& lights) {
        nextKeys_.clear();nextKeys_.reserve(lights.size());
        for(const auto& light:lights)nextKeys_.push_back(assignmentKey(light));
        if(assignmentCached_ && nextKeys_==priorKeys_)return;
        assignmentCached_=false;
        for (auto& cluster : clusters_) cluster.clear();
        for (std::size_t index = 0; index < lights.size(); ++index) {
            const RenderLight& light = lights[index];
            if(light.shape.type==LightKind::Directional||light.shape.characterResponse.isProjectedColorBlend()) {
                for(auto& cluster:clusters_)cluster.push_back(static_cast<std::uint32_t>(index));
                continue;
            }
            const float radius = std::max(light.radius, 0.0F);
            const float centerDepth = light.clusterPosition[2];
            const float minimumDepth = std::max(
                desc_.nearPlane, centerDepth - radius);
            const float maximumDepth = std::min(
                desc_.farPlane, centerDepth + radius);
            if (!(radius > 0.0F) || maximumDepth < minimumDepth
                || centerDepth + radius < desc_.nearPlane
                || centerDepth - radius > desc_.farPlane) {
                continue;
            }
            const auto axis = [](const float value, const std::uint32_t count) {
                return std::min<std::uint32_t>(
                    count - 1,
                    static_cast<std::uint32_t>(std::clamp((value + 1.0F) * 0.5F
                        * static_cast<float>(count), 0.0F,
                        static_cast<float>(count - 1))));
            };
            const float projectedRadius = desc_.orthographic
                ? radius : radius / minimumDepth;
            float minimumX = light.clusterPosition[0]
                - std::abs(desc_.projectionScaleX) * projectedRadius;
            float maximumX = light.clusterPosition[0]
                + std::abs(desc_.projectionScaleX) * projectedRadius;
            float minimumY = light.clusterPosition[1]
                - std::abs(desc_.projectionScaleY) * projectedRadius;
            float maximumY = light.clusterPosition[1]
                + std::abs(desc_.projectionScaleY) * projectedRadius;
            if (light.viewSpaceBounds) {
                const auto interval = [&](unsigned axis, float projectionScale) {
                    const float offset=axis==0?desc_.projectionOffsetX:desc_.projectionOffsetY;
                    const float low = light.viewPosition[axis] - radius;
                    const float high = light.viewPosition[axis] + radius;
                    if (desc_.orthographic) return std::array<float,2>{std::min(low*projectionScale,high*projectionScale)+offset,
                        std::max(low*projectionScale,high*projectionScale)+offset};
                    // Project the clipped view-space AABB, not a sphere center
                    // behind the camera. This is conservative at the near plane.
                    const std::array<float,4> corners{low/minimumDepth*projectionScale,low/maximumDepth*projectionScale,
                        high/minimumDepth*projectionScale,high/maximumDepth*projectionScale};
                    return std::array<float,2>{*std::min_element(corners.begin(),corners.end())+offset,
                        *std::max_element(corners.begin(),corners.end())+offset};
                };
                const auto x = interval(0, desc_.projectionScaleX);
                const auto y = interval(1, desc_.projectionScaleY);
                minimumX=x[0];maximumX=x[1];minimumY=y[0];maximumY=y[1];
            }
            if (maximumX < -1.0F || minimumX > 1.0F
                || maximumY < -1.0F || minimumY > 1.0F) {
                continue;
            }
            const std::uint32_t firstX = axis(minimumX, desc_.x);
            const std::uint32_t lastX = axis(maximumX, desc_.x);
            const std::uint32_t firstY = axis(minimumY, desc_.y);
            const std::uint32_t lastY = axis(maximumY, desc_.y);
            const auto depthSlice = [this](const float depth) {
                const float logarithmic = std::log(
                    std::max(depth, desc_.nearPlane) / desc_.nearPlane)
                    / std::log(desc_.farPlane / desc_.nearPlane);
                return std::min<std::uint32_t>(
                    desc_.z - 1,
                    static_cast<std::uint32_t>(std::clamp(
                        logarithmic, 0.0F, 0.999999F)
                        * static_cast<float>(desc_.z)));
            };
            const std::uint32_t firstZ = depthSlice(minimumDepth);
            const std::uint32_t lastZ = depthSlice(maximumDepth);
            const bool clipBox=light.clusterBox.has_value() && light.shape.influenceBox.enabled
                && light.shape.receivers.surfaces && !light.shape.receivers.characters
                && std::isfinite(desc_.projectionScaleX) && std::abs(desc_.projectionScaleX)>1e-12F
                && std::isfinite(desc_.projectionScaleY) && std::abs(desc_.projectionScaleY)>1e-12F;
            const auto interval=[&](unsigned cell,unsigned count,double scale,double offset){
                const double a=((2.0*cell/count-1)-offset-1e-5)/scale;
                const double b=((2.0*(cell+1)/count-1)-offset+1e-5)/scale;
                return std::array<double,2>{std::min(a,b),std::max(a,b)};
            };
            for (std::uint32_t z = firstZ; z <= lastZ; ++z) {
                const auto depthBounds=clipBox?std::array<double,2>{
                    desc_.nearPlane*std::pow(double(desc_.farPlane)/desc_.nearPlane,double(z)/desc_.z)*(1-1e-4),
                    desc_.nearPlane*std::pow(double(desc_.farPlane)/desc_.nearPlane,double(z+1)/desc_.z)*(1+1e-4)}:std::array<double,2>{};
                for (std::uint32_t y = firstY; y <= lastY; ++y) {
                    const auto yBounds=clipBox?interval(y,desc_.y,desc_.projectionScaleY,desc_.projectionOffsetY):std::array<double,2>{};
                    for (std::uint32_t x = firstX; x <= lastX; ++x) {
                        if(clipBox&&!clusterIntersectsInfluenceBox(*light.clusterBox,
                            interval(x,desc_.x,desc_.projectionScaleX,desc_.projectionOffsetX),yBounds,depthBounds,desc_.orthographic))continue;
                        clusters_[linearIndex(x, y, z)].push_back(
                            static_cast<std::uint32_t>(index));
                    }
                }
            }
        }
        priorKeys_.swap(nextKeys_);assignmentCached_=true;
    }

    [[nodiscard]] std::size_t clusterCount() const noexcept {
        return static_cast<std::size_t>(desc_.x) * desc_.y * desc_.z;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& indicesFor(
        std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
        if (x >= desc_.x || y >= desc_.y || z >= desc_.z) throw std::out_of_range("Cluster index");
        return clusters_[linearIndex(x, y, z)];
    }
    [[nodiscard]] std::vector<std::uint32_t> gpuIndexData() const {
        std::vector<std::uint32_t> result;
        gpuIndexData(result);
        return result;
    }
    void gpuIndexData(std::vector<std::uint32_t>& result) const {
        std::size_t count=0;for(const auto& cluster:clusters_)count+=cluster.size();
        result.reserve(count);result.clear();
        for (const auto& cluster : clusters_) result.insert(result.end(), cluster.begin(), cluster.end());
    }
    [[nodiscard]] std::vector<std::array<std::uint32_t, 2>> gpuHeaderData() const {
        std::vector<std::array<std::uint32_t, 2>> result;
        gpuHeaderData(result);
        return result;
    }
    void gpuHeaderData(std::vector<std::array<std::uint32_t, 2>>& result) const {
        result.reserve(clusters_.size());
        result.clear();
        std::uint32_t offset = 0;
        for (const auto& cluster : clusters_) {
            result.push_back({offset, static_cast<std::uint32_t>(cluster.size())});
            offset += static_cast<std::uint32_t>(cluster.size());
        }
    }
    [[nodiscard]] const ClusterGridDesc& desc() const noexcept { return desc_; }

private:
    struct AssignmentKey {
        std::array<float,3> clusterPosition{},viewPosition{};
        float radius=0;
        LightKind type{};
        bool viewSpace=false,projectedBlend=false,surfaces=false,characters=false,boxEnabled=false;
        std::optional<ClusterLightInfluenceBox> box;
        bool operator==(const AssignmentKey& o) const {
            return clusterPosition==o.clusterPosition&&viewPosition==o.viewPosition&&radius==o.radius&&type==o.type
                &&viewSpace==o.viewSpace&&projectedBlend==o.projectedBlend&&surfaces==o.surfaces
                &&characters==o.characters&&boxEnabled==o.boxEnabled&&box==o.box;
        }
    };
    static AssignmentKey assignmentKey(const RenderLight& light){
        return {{light.clusterPosition[0],light.clusterPosition[1],light.clusterPosition[2]},
            light.viewPosition,light.radius,light.shape.type,light.viewSpaceBounds,
            light.shape.characterResponse.isProjectedColorBlend(),light.shape.receivers.surfaces,
            light.shape.receivers.characters,light.shape.influenceBox.enabled,light.clusterBox};
    }
    std::vector<AssignmentKey> priorKeys_,nextKeys_;
    bool assignmentCached_=false;
    [[nodiscard]] std::size_t linearIndex(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept {
        return (static_cast<std::size_t>(z) * desc_.y + y) * desc_.x + x;
    }
    ClusterGridDesc desc_;
    std::vector<std::vector<std::uint32_t>> clusters_;
};

}  // namespace azurerender
