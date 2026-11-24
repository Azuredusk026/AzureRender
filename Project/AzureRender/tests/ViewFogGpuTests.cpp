#include "render/ViewFogGpu.hpp"
#include "render/FogVolumeData.hpp"
#include "rhi/NullRhi.hpp"
#include "render/CameraProjection.hpp"
#include <iostream>
#include <cstring>
#include <stdexcept>
using namespace azurerender;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F> void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Invalid fog view GPU input must fail");}
PackedViewFogFrame frame(const ViewFogGpu& fog,unsigned index){PackedViewFogFrame result;std::memcpy(&result,fog.parameters(index).mapped,sizeof(result));return result;}
}
int main()try{
 rhi::NullRhi backend;ViewFogBinding binding;binding.enabled=true;binding.asset="assets:/field.azfog";
 binding.captureView=CameraViewDescriptor{};binding.projection.extent={2560,1440};
 FogVolumeData data;data.dimensions={1,1,1};data.rgba16f={0,0,0,0x3c00};
 {
  ViewFogGpu fog(backend,binding,data,2);
  check(backend.allocator().statistics().liveImages==1&&backend.allocator().statistics().liveBuffers==2,"View fog retains one image and one parameter buffer per in-flight frame");
  check(fog.updateFrame(0,*binding.captureView,{1280,720})==FogViewMatch::Matched,"Equivalent viewport aspect supports captured fog");
  auto packed=frame(fog,0);check(packed.rows[3][0]==1280&&packed.rows[3][1]==720&&packed.rows[3][2]==1,"Packed frame records current viewport and activation");
  const auto projection=binding.projection.pack();for(unsigned row=0;row<3;++row)check(packed.rows[row]==projection.rows[row],"Frame retains exact captured projection");
  auto camera=*binding.captureView;camera.position[0]+=.1F;
  check(fog.updateFrame(1,camera,{1280,720})==FogViewMatch::CameraMismatch&&frame(fog,1).rows[3][2]==0,"Camera mismatch disables current frame sampling");
  check(frame(fog,0).rows[3][2]==1,"In-flight frame parameters remain isolated");
  check(fog.updateFrame(1,*binding.captureView,{720,720})==FogViewMatch::AspectMismatch&&frame(fog,1).rows[3][2]==0,"Viewport mismatch disables captured fog");
  check(fog.updateFrame(1,*binding.captureView,{2560,1440})==FogViewMatch::Matched&&frame(fog,1).rows[3][2]==1,"Returning to captured view restores sampling");
  fog.writeDescriptors(VK_NULL_HANDLE,0,34,35);
  reject([&]{fog.updateFrame(2,*binding.captureView,{2560,1440});});
  reject([&]{fog.updateFrame(0,*binding.captureView,{0,1440});});
 }
 check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"View fog releases every allocation");
 reject([&]{ViewFogGpu invalid(backend,binding,data,0);});
 binding.enabled=false;reject([&]{ViewFogGpu invalid(backend,binding,data,2);});
 binding.enabled=true;backend.calls.clear();backend.unavailableSampledFormats.insert(VK_FORMAT_R16G16B16A16_SFLOAT);
 reject([&]{ViewFogGpu invalid(backend,binding,data,2);});
 check(backend.calls.empty(),"Unsupported fog sampling is rejected before allocation");
 check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Rejected view fog allocates no retained resources");
 std::cout<<"View fog GPU frame ownership, mismatch, resize and release passed\n";
 backend.unavailableSampledFormats.clear();
 {ViewFogBinding analytic;analytic.enabled=true;analytic.analytic=AnalyticFogSettings{};analytic.analytic->aerial=ExponentialAerialFog{};
  analytic.analytic->aerial->density=.1F;ViewFogGpu fog(backend,analytic,data,2);
  check(PackedViewFogFrame::layoutVersion==2&&sizeof(PackedViewFogFrame)==336,"Composed fog has a versioned bounded frame layout");
  check(fog.updateFrame(0,CameraViewDescriptor{},{128,128})==FogViewMatch::AnalyticOnly,"Independent analytic view is active");
  auto p=frame(fog,0);check(p.rows[3][2]==0&&p.rows[3][3]==1,"Captured volume and analytic activation are separate");
  check(p.rows[13][1]==1&&p.rows[15][3]==.1F,"Frame carries normalized atmosphere parameters");
  for(bool ortho:{false,true}){
   CameraViewDescriptor camera;camera.position={1,4,3};camera.target={-2,1,-4};camera.up={.1F,1,.2F};
   camera.projection=ortho?CameraProjectionKind::Orthographic:CameraProjectionKind::Perspective;
   camera.projectionOffsetX=.23F;camera.projectionOffsetY=-.17F;camera.fov=1.1F;camera.orthoHeight=4.2F;camera.farPlane=500;
   fog.updateFrame(0,camera,{320,180});const auto dataFrame=frame(fog,0);
   const auto forward=internal::normalize(internal::subtract(camera.target,camera.position));
   for(const auto uv:{std::array<float,2>{.1F,.2F},std::array<float,2>{.5F,.5F},std::array<float,2>{.8F,.9F}})for(float depth:{.5F,3.F,200.F}){
    const auto ray=cameraPickRay(camera,uv[0],uv[1],320.F/180);const float d=internal::dot(ray.direction,forward);
    for(unsigned i=0;i<3;++i){
     const float offset=dataFrame.rows[6][i]*(2*uv[0]-1-camera.projectionOffsetX)+dataFrame.rows[7][i]*(1-2*uv[1]+camera.projectionOffsetY);
     const float world=dataFrame.rows[4][i]+(ortho?offset:0)+(dataFrame.rows[5][i]+(ortho?0:offset))*depth;
     const float expected=ray.origin[i]+ray.direction[i]*depth/d;
     check(std::abs(world-expected)<3e-4F,"Fog world reconstruction follows the shared camera projection");
    }
   }
  }
 }
 {auto combined=binding;combined.analytic=AnalyticFogSettings{};combined.analytic->aerial=ExponentialAerialFog{};
  ViewFogGpu fog(backend,combined,data,1);auto moved=*combined.captureView;moved.position[0]+=.5F;
  check(fog.updateFrame(0,moved,{128,128})==FogViewMatch::CameraMismatch,"Captured field reports its view mismatch");
  const auto p=frame(fog,0);check(p.rows[3][2]==0&&p.rows[3][3]==1,"World media remain active independently of volume mismatch");
 }
 check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Analytic-only resources retire through the shared owner");
 {
  ViewFogBinding generated;generated.enabled=true;generated.analytic=AnalyticFogSettings{};
  generated.analytic->height=LayeredHeightFog{{{.19F,.13F,1,{.19F,.29F,.48F}}}};
  generated.generatedHeightVolume=GeneratedHeightFogSettings{{48,32,48},8};
  reject([&]{ViewFogGpu missingProgram(backend,generated,data,2);});
  const std::vector<char> program{0};
  ViewFogGpu fog(backend,generated,data,2,program);
  check(backend.allocator().statistics().liveImages==2&&backend.allocator().statistics().liveBuffers==2,
      "Generated height fog owns one image and parameter buffer per frame");
  check(fog.updateFrame(0,CameraViewDescriptor{},{128,128})==FogViewMatch::GeneratedHeight,"Generation follows the current frame");
  check(frame(fog,0).rows[13][2]==1&&frame(fog,0).rows[13][3]==8&&frame(fog,1).rows[13][2]==0,
      "Only the updated slot carries generated fog parameters");
  const auto& buffer=fog.parameters(0);
  RenderGraph graph;
  const auto parameters=graph.importBuffer("view-fog",{buffer.buffer,0,buffer.size,0,VK_PIPELINE_STAGE_HOST_BIT,0,VK_ACCESS_HOST_WRITE_BIT});
  const auto volume=fog.addGenerationPass(graph,0,parameters);
  check(volume.has_value(),"Generation registers its output image");
  const auto surface=graph.addPass("surface");
  graph.use(surface,parameters,RenderGraphUsage::FragmentStorage,false);
  graph.use(surface,*volume,RenderGraphUsage::Sampled,false);
  std::string error;
  check(graph.compile(error),"Generated fog render graph compiles");
  const auto& upload=graph.bufferBarriers().at(0).buffer;
  check(upload.srcAccessMask==VK_ACCESS_HOST_WRITE_BIT
      &&upload.dstStageMask==(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),
      "Frame parameters are visible to compute and fragment readers");
  const auto& publish=graph.barriers().at(1).image;
  check(publish.srcAccessMask==VK_ACCESS_SHADER_WRITE_BIT&&publish.dstAccessMask==VK_ACCESS_SHADER_READ_BIT
      &&publish.dstStageMask==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      "Generated volume is visible to the fragment reader");
 }
 check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Generated fog releases both slots");
 std::cout<<"Generated height fog frame ownership and fragment synchronization passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
