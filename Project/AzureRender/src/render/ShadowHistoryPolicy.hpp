#pragma once
#include "render/LinearMath.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>
namespace azurerender {
class ShadowHistoryPrimitiveRanges {
    std::uint64_t next_=0;
public:
    std::uint32_t reserve(std::uint32_t count)noexcept{
        if(!count||count%3||next_+count>UINT32_MAX)return UINT32_MAX;
        const auto base=static_cast<std::uint32_t>(next_);next_+=count;return base;
    }
    static std::uint32_t key(std::uint32_t base,std::uint32_t count,std::uint32_t first,std::uint32_t triangle)noexcept{
        const auto offset=std::uint64_t(first)+std::uint64_t(triangle)*3;
        if(base==UINT32_MAX||!count||count%3||first%3||std::uint64_t(base)+count>UINT32_MAX||offset+2>=count)return UINT32_MAX;
        return base+static_cast<std::uint32_t>(offset);
    }
};
class ShadowHistoryViewIdentity {
    std::vector<float> previous_;VkExtent2D extent_{};std::uint64_t generation_=0;bool committed_=false;
public:
    // Inputs include every view, sampling, light and static-content value
    // affecting visibility. The caller changes generation on resource reload.
    bool begin(const std::vector<float>& values,VkExtent2D extent,std::uint64_t generation){
        if(values.empty()||!extent.width||!extent.height||!generation
            ||!std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);})){invalidate();return false;}
        const bool reuse=committed_&&generation==generation_&&extent.width==extent_.width&&extent.height==extent_.height
            &&values.size()==previous_.size()&&std::memcmp(values.data(),previous_.data(),values.size()*sizeof(float))==0;
        previous_=values;extent_=extent;generation_=generation;committed_=false;return reuse;
    }
    // Call only after this view's history-writing commands are submitted.
    // Same-queue barriers order GPU readers; this does not wait for the GPU.
    void commit()noexcept{committed_=generation_!=0;}
    void invalidate()noexcept{previous_.clear();generation_=0;extent_={};committed_=false;}
};
struct ShadowHistoryDirtyRect {
    std::uint32_t left=0,top=0,right=0,bottom=0;
    bool empty()const noexcept{return left>=right||top>=bottom;}
    bool intersects(float x,float y,float radius)const noexcept{
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(radius)||radius<0)return true;
        if(empty())return false;
        const double reach=std::ceil(double(radius)+.5)+2;
        return double(x)+reach>=left&&double(x)-reach<right&&double(y)+reach>=top&&double(y)-reach<bottom;
    }
    void include(ShadowHistoryDirtyRect other)noexcept{
        if(other.empty())return;if(empty()){*this=other;return;}
        left=std::min(left,other.left);top=std::min(top,other.top);right=std::max(right,other.right);bottom=std::max(bottom,other.bottom);
    }
};
// Local cascade coordinates, before atlas placement. Positive homogeneous w
// makes each linear-fractional coordinate attain its extrema at box corners.
inline ShadowHistoryDirtyRect shadowHistoryProjectBounds(const internal::Vector3& low,const internal::Vector3& high,const internal::Matrix4& matrix,VkExtent2D extent)noexcept{
    const ShadowHistoryDirtyRect full{0,0,extent.width,extent.height};
    for(unsigned axis=0;axis<3;++axis)if(!std::isfinite(low[axis])||!std::isfinite(high[axis])||low[axis]>high[axis])return full;
    for(float value:matrix)if(!std::isfinite(value))return full;
    std::array<double,2> minimum{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()};
    std::array<double,2> maximum{-minimum[0],-minimum[1]};
    for(unsigned corner=0;corner<8;++corner){
        const std::array<double,4> point{corner&1?high[0]:low[0],corner&2?high[1]:low[1],corner&4?high[2]:low[2],1};
        std::array<double,4> clip{},error{};
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
            const double term=double(matrix[column*4+row])*point[column];clip[row]+=term;error[row]+=std::abs(term);
        }
        // Cover separate float multiply/add and FMA orders, division and
        // subnormal rounding. Severe cancellation expands to the full view.
        for(unsigned row=0;row<4;++row){
            if(error[row]>std::numeric_limits<float>::max())return full;
            error[row]=error[row]*(16*std::numeric_limits<float>::epsilon())+16*std::numeric_limits<float>::min();
        }
        const double wLow=clip[3]-error[3],wHigh=clip[3]+error[3];if(wLow<=0||!std::isfinite(wHigh))return full;
        for(unsigned axis=0;axis<2;++axis){
            const double a=clip[axis]-error[axis],b=clip[axis]+error[axis];
            const std::array<double,4> projected{a/wLow,a/wHigh,b/wLow,b/wHigh};
            minimum[axis]=std::min(minimum[axis],*std::min_element(projected.begin(),projected.end()));
            maximum[axis]=std::max(maximum[axis],*std::max_element(projected.begin(),projected.end()));
        }
    }
    const auto bound=[](double value,unsigned size,bool upper){
        const double pixel=(value*.5+.5)*size;return static_cast<std::uint32_t>(std::clamp(upper?std::ceil(pixel)+2:std::floor(pixel)-2,0.0,double(size)));
    };
    return {bound(minimum[0],extent.width,false),bound(minimum[1],extent.height,false),bound(maximum[0],extent.width,true),bound(maximum[1],extent.height,true)};
}
}
