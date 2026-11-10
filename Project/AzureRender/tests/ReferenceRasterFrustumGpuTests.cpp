#include "render/ReferenceRasterFrustum.hpp"
#include "render/ReferenceRasterProjection.hpp"
#include "render/GpuCullingResources.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <cstring>
#include <iostream>
using namespace azurerender;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught,"Expected raster frustum input rejection");}
struct Batch {PreparedReferenceRasterFrustum frustum;std::vector<GpuCullBounds> bounds;std::vector<bool> mustKeep;};
bool clipInside(const std::array<float,4>& p){return p[3]>0&&p[0]>=-p[3]&&p[0]<=p[3]&&p[1]>=-p[3]&&p[1]<=p[3]&&p[2]>=0&&p[2]<=p[3];}
}
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=5)throw std::invalid_argument("Expected cull shader and optional source projection/frame/world records");
    ReferenceCameraView identity;identity.extent={2560,1440};ReferenceCameraView device=identity;device.depthConvention=ReferenceDepthConvention::Forward;device.projection.parameters.columns[3][2]=.5F;
    ReferenceCameraView perspective=identity;perspective.projection.parameters.columns={ReferenceClipProjectionParameters::Row{4,0,0,0},{0,4,0,0},{0,0,0,-1},{0,0,.1F,0}};
    std::vector<Batch> batches;
    for(unsigned mode=0;mode<5;++mode){auto ref=(mode==0||mode==4)?identity:perspective,dev=device;if(mode==2)dev.projection.parameters.columns[3][3]=-1;if(mode==3)dev=ref;if(mode==4)dev=perspective;
        const PreparedReferenceCameraView reference(ref),physical(dev);Batch b{prepareReferenceRasterFrustum(reference,physical,{-8,-8,-8},{8,8,8},true),{}, {}};
        if(mode==2)check(!b.frustum.usesDeviceDepth(),"Negative device W must retain conservative depth behavior");
        if(mode==4)check(b.frustum.intersects({3,0,0},{3,0,0}),"Device W-zero interval must remain visible even outside reference XY");
        for(int i=0;i<4096;++i){const std::array<float,3> point{float(i%31-15)/8,float(i%37-18)/8,float(i%29-14)/8};
            b.bounds.push_back({{point[0],point[1],point[2],0},{point[0],point[1],point[2],0}});const auto a=reference.clip(point),d=physical.clip(point);b.mustKeep.push_back(clipInside(composeReferenceRasterPosition(a,d,d)));}
        batches.push_back(std::move(b));}
    if(argc==5){ReferenceCameraView ref=identity;auto p=readBinaryFile(argv[2]),f=readBinaryFile(argv[3]),positions=readBinaryFile(argv[4]);check(p.size()==80&&f.size()==32&&!positions.empty()&&positions.size()%16==0,"Invalid native frustum fixture sizes");std::memcpy(&ref.projection.parameters,p.data(),80);std::memcpy(&ref.frame.parameters,f.data(),32);
        const PreparedReferenceCameraView camera(ref);std::array<float,3> minimum{1.e10F,1.e10F,1.e10F},maximum{-1.e10F,-1.e10F,-1.e10F};std::vector<std::array<float,4>> world(positions.size()/16);std::memcpy(world.data(),positions.data(),positions.size());for(const auto& v:world)for(unsigned a=0;a<3;++a){minimum[a]=std::min(minimum[a],v[a]);maximum[a]=std::max(maximum[a],v[a]);}
        Batch b{prepareReferenceRasterFrustum(camera,camera,minimum,maximum,true),{}, {}};for(const auto& v:world){b.bounds.push_back({v,v});b.mustKeep.push_back(clipInside(camera.clip({v[0],v[1],v[2]})));}batches.push_back(std::move(b));}
    const PreparedReferenceCameraView ref(perspective),dev(device);const auto fallback=prepareReferenceRasterFrustum(ref,dev,{-1,-1,0},{1,1,0},true);
    check(fallback.intersects({.25F,.25F,0},{.25F,.25F,0}),"Reference W-zero fallback must remain visible");
    rejects([&]{(void)prepareReferenceRasterFrustum(ref,dev,{1,0,0},{0,1,1},true);});
    rejects([&]{(void)fallback.parameters({{{2,0,0,0},{2,0,0,0}}});});
    check(fallback.intersects({2,0,0},{2,0,0}),"Out-of-domain CPU query must remain conservative");
    auto extreme=identity;extreme.frame.parameters.offset[0]=std::numeric_limits<float>::max();extreme.projection.parameters.columns[3][0]=-std::numeric_limits<float>::max();const PreparedReferenceCameraView wide(extreme);
    auto uncertain=prepareReferenceRasterFrustum(wide,dev,{-1,-1,-1},{1,1,1},true);check(!uncertain.bounded()&&uncertain.intersects({.5F,.25F,.25F},{.5F,.25F,.25F}),"Unbounded projection intermediates must expose an all-retained result");batches.push_back({std::move(uncertain),{{{.5F,.25F,.25F,0},{.5F,.25F,.25F,0}}},{true}});
    test::VulkanComputeFixture host(true);std::size_t points=0,kept=0,rejected=0;
    for(const auto& batch:batches){const auto parameters=batch.frustum.parameters(batch.bounds);std::vector<VkDrawIndexedIndirectCommand> draws;std::vector<unsigned> expected;
        for(std::size_t i=0;i<batch.bounds.size();++i){draws.push_back({3,1,unsigned(i*3),0,unsigned(i)});const auto count=referenceCullCommand(draws.back(),batch.bounds[i],parameters).instanceCount;check(!batch.mustKeep[i]||count==1,"Mixed frustum falsely rejected a visible composed clip point");expected.push_back(count);kept+=count;rejected+=1-count;}
        {GpuCullingResources resources(*host.backend);resources.initialize(readBinaryFile(argv[1]),unsigned(draws.size()),false,true);resources.upload(batch.bounds,draws);
            host.submit([&](rhi::ICommandRecorder& c,VkCommandBuffer){for(const auto* b:{&resources.bounds(),&resources.source()})c.bufferBarrier({b->buffer,0,b->size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});resources.record(c,parameters);const auto& b=resources.output();c.bufferBarrier({b.buffer,0,b.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});});
            host.allocator.invalidate(resources.output(),0,resources.output().size);const auto* actual=static_cast<const VkDrawIndexedIndirectCommand*>(resources.output().mapped);for(std::size_t i=0;i<draws.size();++i){auto value=draws[i];value.instanceCount=expected[i];check(std::memcmp(&actual[i],&value,20)==0,"GPU mixed frustum differs from CPU conservative result");}}
        points+=draws.size();check(host.allocator.statistics().liveBuffers==0,"Raster frustum resources leaked");}
    check(kept&&rejected&&!host.errors.load(),"Raster frustum must both preserve visible points and cull proven outside points");
    std::cout<<host.properties.deviceName<<": "<<points<<" exact mixed-frustum draw records; visiblePointsKept="<<kept<<" outsideRejected="<<rejected<<"; source/device zero-W and domain contracts passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
