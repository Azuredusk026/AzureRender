#include "SelectionBounds.hpp"
#include <cmath>
#include <set>
namespace azurerender {
SelectionBoundsResult SelectionBounds::resolve(const scene::SceneDescription& document,const std::vector<std::string>& selection,const Provider& provider,float radius){
    if(selection.empty())return {false,{},"Select an object to frame"};
    if(!std::isfinite(radius)||radius<=0)return {false,{},"Invalid fallback radius"};
    const auto matrices=scene::resolveNodeWorldTransforms(document);
    scene::AxisAlignedBounds combined{{1e30F,1e30F,1e30F},{-1e30F,-1e30F,-1e30F}};
    const std::set<std::string> ids(selection.begin(),selection.end());
    std::set<std::string> included=ids;
    for(std::size_t pass=0;pass<document.nodes.size();++pass)
        for(const auto& node:document.nodes)if(included.count(node.parentId))included.insert(node.id);
    bool valid=false;
    for(std::size_t index=0;index<document.nodes.size();++index){
        const auto& node=document.nodes[index];if(!included.count(node.id))continue;
        auto local=provider&&!node.resourceId.empty()?provider(node.resourceId):std::optional<scene::AxisAlignedBounds>{};
        scene::AxisAlignedBounds bounds;
        if(local) {
            for(unsigned axis=0;axis<3;++axis)
                if(!std::isfinite(local->minimum[axis])||!std::isfinite(local->maximum[axis])||local->minimum[axis]>local->maximum[axis])return {false,{},"Invalid asset bounds"};
            bounds=scene::transformBounds(*local,matrices[index]);
        }
        else {
            const auto centre=internal::transformPosition(matrices[index],{0,0,0});
            for(std::size_t axis=0;axis<3;++axis){bounds.minimum[axis]=centre[axis]-radius;bounds.maximum[axis]=centre[axis]+radius;}
        }
        for(std::size_t axis=0;axis<3;++axis){
            if(!std::isfinite(bounds.minimum[axis])||!std::isfinite(bounds.maximum[axis])||bounds.minimum[axis]>bounds.maximum[axis])return {false,{},"Invalid selection bounds"};
            combined.minimum[axis]=std::min(combined.minimum[axis],bounds.minimum[axis]);
            combined.maximum[axis]=std::max(combined.maximum[axis],bounds.maximum[axis]);
        }
        valid=true;
    }
    return {valid,combined,valid?"":"Selected objects are unavailable"};
}
}
