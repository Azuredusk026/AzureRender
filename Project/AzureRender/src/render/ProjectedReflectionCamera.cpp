#include "render/ProjectedReflectionCamera.hpp"
#include "render/LinearMath.hpp"
#include <algorithm>
namespace azurerender {
namespace {
using internal::Matrix4;
struct Transforms {Matrix4 projection,inverseProjection,rotation,cameraToWorld;};
Transforms transforms(const ReflectionTraceView& source){
    (void)source.pack(1,1,ScreenSpaceReflectionSettings{},ReflectionColorEncoding::Linear);
    const auto& p=source.projection;
    if(p[11]!=-1||p[15]!=0||p[12]!=0||p[13]!=0||p[10]>-1||p[14]>=0)
        throw std::invalid_argument("Projected reflection camera requires forward-depth perspective projection");
    Transforms result;result.projection=p;
    for(unsigned column=0;column<4;++column)result.projection[column*4+2]=p[column*4+3]-p[column*4+2];
    const auto& reverse=result.projection;
    result.inverseProjection={1.F/reverse[0],0,0,0,0,1.F/reverse[5],0,0,0,0,0,1.F/reverse[14],
        reverse[8]/reverse[0],reverse[9]/reverse[5],-1,reverse[10]/reverse[14]};
    const auto& right=source.right;const auto& up=source.up;const auto& forward=source.forward;
    result.rotation={right[0],up[0],-forward[0],0,right[1],up[1],-forward[1],0,right[2],up[2],-forward[2],0,0,0,0,1};
    result.cameraToWorld={right[0],right[1],right[2],0,up[0],up[1],up[2],0,-forward[0],-forward[1],-forward[2],0,source.position[0],source.position[1],source.position[2],1};
    return result;
}
template<class T>void columns(T& target,const Matrix4& matrix){for(unsigned column=0;column<target.size();++column)std::copy_n(matrix.data()+column*4,4,target[column].data());}
}
ProjectedReflectionView projectedReflectionView(const ReflectionTraceView& current,const ReflectionTraceView* previous){
    const auto present=transforms(current);const auto past=previous?transforms(*previous):present;ProjectedReflectionView result;
    columns(result.worldNormalToView,present.rotation);columns(result.viewDirectionToClip,present.projection);
    columns(result.clipToView,present.inverseProjection);columns(result.relativeWorldToClip,internal::multiply(present.projection,present.rotation));
    columns(result.clipToWorld,internal::multiply(present.cameraToWorld,present.inverseProjection));
    columns(result.previousClipToWorld,internal::multiply(past.cameraToWorld,past.inverseProjection));
    result.position=current.position;result.depthReciprocal={1.F/present.projection[14],present.projection[10]/present.projection[14]};
    result.convention=ProjectedReflectionConvention::VulkanUpperLeft;
    (void)result.pack({1,1},{1,1},ProjectedReflectionSettings{});return result;
}
}
