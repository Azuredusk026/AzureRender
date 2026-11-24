#include "editor/viewport/EditorCameraService.hpp"
#include "render/RenderMath.hpp"
#include <cmath>
#include <iostream>
int main(){
    using namespace azurerender;
    scene::SceneDescription document;
    scene::SceneNodeDesc parent;parent.id="parent";parent.translation={10,0,0};parent.rotation={0,90,0};parent.scale={2,3,4};
    scene::SceneNodeDesc child;child.id="child";child.parentId="parent";child.translation={1,0,0};child.resourceId="mesh";
    document.nodes={parent,child};
    auto bounds=SelectionBounds::resolve(document,{"child"},[](const auto&)->std::optional<scene::AxisAlignedBounds>{return scene::AxisAlignedBounds{{-1,-1,-1},{1,1,1}};});
    if(!bounds.valid || std::abs(bounds.bounds.minimum[0]-6)>1e-4F || std::abs(bounds.bounds.maximum[0]-14)>1e-4F){std::cerr<<"World bounds must include parent rotation and scale\n";return 1;}
    for(float aspect:{.5F,1.F,2.F}) {
        const auto camera=EditorCameraService::frameSelection(bounds,{10,0,20},{10,0,0},1.F,aspect);
        if(!camera.passed)return 2;
        for(unsigned i=0;i<8;++i){
            const float x=(i&1)?bounds.bounds.maximum[0]:bounds.bounds.minimum[0];
            const float y=(i&2)?bounds.bounds.maximum[1]:bounds.bounds.minimum[1];
            const float z=(i&4)?bounds.bounds.maximum[2]:bounds.bounds.minimum[2];
            const float depth=camera.position[2]-z;
            if(std::abs(x-camera.target[0])/(depth*std::tan(.5F)*aspect)>.91F || std::abs(y-camera.target[1])/(depth*std::tan(.5F))>.91F)return 3;
        }
    }
    if(SelectionBounds::resolve(document,{}).valid)return 4;
    if(!SelectionBounds::resolve(document,{"parent"}).valid)return 5;
    if(EditorCameraService::frameSelection(bounds,{0,0,5},{0,0,0},1.F,0).passed)return 6;
    bounds.bounds={{-1000,-1000,-1000},{1000,1000,1000}};
    auto large=EditorCameraService::frameSelection(bounds,{0,0,5},{0,0,0},1.F,1);
    if(!large.passed || large.distance<1000)return 7;
    const auto coincident=EditorCameraService::frameSelection(bounds,{0,0,0},{0,0,0},1.F,1.F);
    if(!coincident.passed || !std::isfinite(coincident.position[0])){std::cerr<<"Coincident camera uses a finite default direction\n";return 8;}
    return 0;
}
