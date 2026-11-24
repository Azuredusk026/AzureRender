#include "render/VisibilityPrototype.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid visibility input accepted");}
}
int main() try {
    VkPhysicalDeviceLimits capacity{};
    require(!VisibilityPrototype::supported(capacity),"Unknown compute capacity must keep default visibility");
    capacity.maxPerStageDescriptorStorageBuffers=5;capacity.maxDescriptorSetStorageBuffers=5;
    capacity.maxPerStageResources=5;capacity.maxPushConstantsSize=128;
    require(VisibilityPrototype::supported(capacity),"Valid surface output capacity rejected");
    capacity.maxPerStageDescriptorStorageBuffers=4;
    require(!VisibilityPrototype::supported(capacity),"Five-buffer surface layout must check compute capacity");
    const VisibilityFrameKey frame{1,2,3};
    const std::vector<GpuCullBounds> bounds={{{-1,-1,-1,0},{1,1,1,0}},{{5,5,5,0},{6,6,6,0}}};
    const std::vector<VkDrawIndexedIndirectCommand> commands={{6,1,0,0,0},{3,1,6,0,0},{6,1,9,0,1}};
    const std::vector<GpuSurfaceIdentity> identities={{0,0,7,0},{0,1,8,0},{1,0,9,0}};
    VisibilityPrototype prototype(frame,bounds,commands,identities);
    GpuCullParameters parameters;
    parameters.planes={{{1,0,0,2},{-1,0,0,2},{0,1,0,2},{0,-1,0,2},{0,0,1,2},{0,0,-1,2}}};
    const auto reference=prototype.reference(parameters);
    require(reference.size()==3 && reference[0].visible==1 && reference[1].visible==1 && reference[2].visible==0,"CPU visibility reference incorrect");
    require(prototype.pick(frame,reference,1)->material==8,"Surface to material picking failed");
    require(!prototype.pick(frame,reference,2),"Hidden surface was pickable");
    rejects([&]{prototype.pick({2,2,3},reference,0);});
    rejects([&]{prototype.pick({1,3,3},reference,0);});
    rejects([&]{prototype.pick({1,2,4},reference,0);});
    auto swapped=reference;std::swap(swapped[0],swapped[1]);
    rejects([&]{prototype.pick(frame,swapped,0);});
    auto invalid=identities;invalid[0].instance=1;
    rejects([&]{VisibilityPrototype bad(frame,bounds,commands,invalid);});
    auto multi=commands;multi[0].instanceCount=2;
    rejects([&]{VisibilityPrototype bad(frame,bounds,multi,identities);});
    auto broken=bounds;broken[0].minimum[0]=std::numeric_limits<float>::quiet_NaN();
    rejects([&]{VisibilityPrototype bad(frame,broken,commands,identities);});
    rejects([&]{VisibilityPrototype::validateSources(broken,commands,identities);});
    rejects([&]{prototype.pick(frame,reference,3);});
    parameters.cullingEnabled=0;
    require(prototype.reference(parameters)[2].visible==1,"Disabled culling must keep every source surface");
    std::cout<<"Stable surface identities, picking, scene/camera/frame history rejection and bounds passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
