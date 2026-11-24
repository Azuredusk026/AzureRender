#include "editor/viewport/SelectionBounds.hpp"
#include <cmath>
#include <iostream>
int main() {
    using namespace azurerender;
    scene::SceneDescription document;
    scene::SceneNodeDesc root;root.id="root";root.translation={10,0,0};root.scale={-2,3,4};
    scene::SceneNodeDesc child;child.id="child";child.parentId="root";child.translation={1,0,0};child.resourceId="mesh";
    scene::SceneNodeDesc distant;distant.id="distant";distant.translation={100,0,0};
    document.nodes={root,child,distant};
    const auto provider=[](const std::string&)->std::optional<scene::AxisAlignedBounds>{return scene::AxisAlignedBounds{{-1,-1,-1},{1,1,1}};};
    const auto group=SelectionBounds::resolve(document,{"root","child"},provider);
    if(!group.valid||group.bounds.minimum[0]!=6||group.bounds.maximum[0]!=10.5F||group.bounds.minimum[1]!=-3||group.bounds.maximum[2]!=4)return 1;
    const auto combined=SelectionBounds::resolve(document,{"child","distant"},provider);
    if(!combined.valid||combined.bounds.maximum[0]!=100.5F)return 2;
    if(SelectionBounds::resolve(document,{}).valid||SelectionBounds::resolve(document,{"unknown"}).valid)return 3;
    const auto tiny=SelectionBounds::resolve(document,{"distant"},{},.0001F);
    if(!tiny.valid||std::abs(tiny.bounds.maximum[1]-.0001F)>1e-7F)return 4;
    if(SelectionBounds::resolve(document,{"root"},{},NAN).valid)return 5;
    const auto invalid=SelectionBounds::resolve(document,{"child"},[](const auto&)->std::optional<scene::AxisAlignedBounds>{return scene::AxisAlignedBounds{{NAN,0,0},{1,1,1}};});
    if(invalid.valid){std::cerr<<"Invalid asset bounds must remain unavailable\n";return 6;}
    return 0;
}
