#include "render/GameUiRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){if(argc!=2)return 2;try{
 azurerender::rhi::NullRhi rhi;auto& allocator=rhi.allocator();
 {azurerender::GameUiRenderer renderer(rhi,VK_NULL_HANDLE,argv[1]);renderer.beginFrame(1,{640,360});
  Rml::Vertex vertices[3]{};vertices[0].position={0,0};vertices[1].position={100,0};vertices[2].position={0,100};int indices[3]={0,1,2};
  auto handle=renderer.CompileGeometry({vertices,3},{indices,3});renderer.RenderGeometry(handle,{16,16},0);
  azurerender::rhi::NullCommandRecorder commands;renderer.record(commands);
  if(renderer.drawCalls()!=1)throw std::runtime_error("UI geometry must submit a draw");
  renderer.ReleaseGeometry(handle);auto live=allocator.statistics().liveBuffers;
  renderer.beginFrame(2,{640,360});if(allocator.statistics().liveBuffers!=live)throw std::runtime_error("In-flight geometry must survive release");
  renderer.beginFrame(5,{640,360});if(allocator.statistics().liveBuffers!=0)throw std::runtime_error("Retired geometry must release GPU buffers");
 }
 if(allocator.statistics().liveBuffers || allocator.statistics().liveImages)throw std::runtime_error("UI shutdown must release every allocation");
 std::cout<<"UI geometry submission, deferred retirement and resource cleanup passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
