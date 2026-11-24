#include "scene/RenderBatching.hpp"
#include <stdexcept>
#include <iostream>
using namespace azurerender::scene;
int main() { try {
    FrustumPlanes nearView, farView;
    nearView.planes[0]={1,0,0,0}; nearView.planes[1]={-1,0,0,2};
    farView.planes[0]={1,0,0,-8}; farView.planes[1]={-1,0,0,10};
    std::vector<SceneInstance> instances(4);
    for (unsigned i=0;i<4;++i) {
        instances[i].sourceIndex=i;
        const float x=i<2?float(i):float(i+6);
        instances[i].worldBounds={{x,0,0},{x+0.5F,1,1}};
    }
    const auto near=visibleInstanceSpans(instances,nearView,true);
    const auto far=visibleInstanceSpans(instances,farView,true);
    if(near!=std::vector<std::array<std::uint32_t,3>>{{0,0,2}}
        || far!=std::vector<std::array<std::uint32_t,3>>{{0,2,2}})
        throw std::runtime_error("Each consuming view must select its own contiguous instance spans");
    if(visibleInstanceSpans(instances,nearView,false)!=std::vector<std::array<std::uint32_t,3>>{{0,0,4}})
        throw std::runtime_error("Culling disabled must retain every instance");
    instances[1].meshKey=1;
    if(visibleInstanceSpans(instances,nearView,true)!=std::vector<std::array<std::uint32_t,3>>{{0,0,1},{1,1,1}})
        throw std::runtime_error("Distinct mesh resources must have distinct draw spans");
    instances[1].worldBounds={{-5,0,0},{-4,1,1}};
    if(visibleInstanceSpans(instances,nearView,true)!=std::vector<std::array<std::uint32_t,3>>{{0,0,1}})
        throw std::runtime_error("Hidden gaps must not enter a visible draw span");
    std::cout<<"Per-view spans, mesh boundaries and culling bypass passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;} }
