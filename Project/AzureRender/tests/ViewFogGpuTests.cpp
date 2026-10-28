#include "render/ViewFogGpu.hpp"
#include "render/FogVolumeData.hpp"
#include "rhi/NullRhi.hpp"
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
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
