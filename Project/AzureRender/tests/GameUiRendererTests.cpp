#include "render/GameUiRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
std::vector<std::string> bindings(const azurerender::rhi::NullCommandRecorder& recorder,const char* name){
 std::vector<std::string> result;for(const auto& call:recorder.calls)if(call.name==name)result.push_back(call.detail);return result;
}
void expectOffsets(const azurerender::rhi::NullCommandRecorder& commands,const std::vector<unsigned>& vertices,const std::vector<unsigned>& indices){
 const auto vb=bindings(commands,"bindVertexBuffer"),ib=bindings(commands,"bindIndexBuffer");
 if(vb.size()!=vertices.size() || ib.size()!=indices.size())throw std::runtime_error("UI draw bindings must match submitted meshes");
 std::string page;
 for(std::size_t i=0;i<vb.size();++i){
  const auto vertexSplit=vb[i].find(" +"),indexSplit=ib[i].find(" +");
  if(vertexSplit==std::string::npos || indexSplit==std::string::npos)throw std::runtime_error("UI bindings must expose buffer offsets");
  if(page.empty())page=vb[i].substr(0,vertexSplit);
  if(vb[i].substr(0,vertexSplit)!=page || ib[i].substr(0,indexSplit)!=page
    || std::stoull(vb[i].substr(vertexSplit+2))!=vertices[i] || std::stoull(ib[i].substr(indexSplit+2))!=indices[i])
   throw std::runtime_error("UI geometry must share one buffer with isolated vertex and index ranges");
 }
}
int main(int argc,char** argv){if(argc!=2)return 2;try{
 azurerender::rhi::NullRhi rhi;auto& allocator=rhi.allocator();
 {azurerender::GameUiRenderer renderer(rhi,VK_NULL_HANDLE,argv[1]);renderer.beginFrame(1,{640,360});
  auto statistics=renderer.statistics();
  if(statistics.activeGeometry || statistics.retiredGeometry || statistics.buffers || statistics.bufferBytes)
   throw std::runtime_error("Empty UI must not own geometry buffers");
  Rml::Vertex vertices[3]{};vertices[0].position={0,0};vertices[1].position={100,0};vertices[2].position={0,100};int indices[3]={0,1,2};
  auto handle=renderer.CompileGeometry({vertices,3},{indices,3});renderer.RenderGeometry(handle,{16,16},0);
  auto other=renderer.CompileGeometry({vertices,3},{indices,3});
  statistics=renderer.statistics();
  if(statistics.buffers!=1)throw std::runtime_error("Small UI geometries must share one GPU allocation");
  if(statistics.activeGeometry!=2 || statistics.retiredGeometry || statistics.buffers!=allocator.statistics().liveBuffers
    || statistics.bufferBytes!=allocator.statistics().liveBufferBytes)
   throw std::runtime_error("UI geometry ownership must match actual allocator residency");
  azurerender::rhi::NullCommandRecorder commands;renderer.record(commands);
  if(renderer.drawCalls()!=1)throw std::runtime_error("UI geometry must submit a draw");
  renderer.ReleaseGeometry(handle);renderer.ReleaseGeometry(other);auto live=allocator.statistics().liveBuffers;
  statistics=renderer.statistics();
  if(statistics.activeGeometry || statistics.retiredGeometry!=2 || statistics.buffers!=live)
   throw std::runtime_error("Retired UI geometry must remain visible in residency statistics");
  renderer.beginFrame(2,{640,360});if(allocator.statistics().liveBuffers!=live)throw std::runtime_error("In-flight geometry must survive release");
  renderer.beginFrame(3,{640,360});if(allocator.statistics().liveBuffers!=live)throw std::runtime_error("UI buffers must survive until the full retirement interval");
  renderer.beginFrame(4,{640,360});if(allocator.statistics().liveBuffers!=0)throw std::runtime_error("Retired geometry must release GPU buffers");
  statistics=renderer.statistics();
  if(statistics.activeGeometry || statistics.retiredGeometry || statistics.buffers || statistics.bufferBytes)
   throw std::runtime_error("UI geometry statistics must clear at retirement");
 }
 {
  azurerender::GameUiRenderer renderer(rhi,VK_NULL_HANDLE,argv[1]);renderer.beginFrame(1,{640,360});
  Rml::Vertex vertices[3]{};int indices[3]={0,1,2};
  auto first=renderer.CompileGeometry({vertices,3},{indices,3});auto second=renderer.CompileGeometry({vertices,3},{indices,3});
  renderer.RenderGeometry(first,{0,0},0);renderer.RenderGeometry(second,{0,0},0);
  azurerender::rhi::NullCommandRecorder original;renderer.record(original);expectOffsets(original,{0,72},{60,132});
  renderer.ReleaseGeometry(first);renderer.beginFrame(2,{640,360});
  auto third=renderer.CompileGeometry({vertices,3},{indices,3});renderer.RenderGeometry(second,{0,0},0);renderer.RenderGeometry(third,{0,0},0);
  azurerender::rhi::NullCommandRecorder inFlight;renderer.record(inFlight);expectOffsets(inFlight,{72,144},{132,204});
  renderer.beginFrame(4,{640,360});auto reused=renderer.CompileGeometry({vertices,3},{indices,3});
  renderer.RenderGeometry(second,{0,0},0);renderer.RenderGeometry(third,{0,0},0);renderer.RenderGeometry(reused,{0,0},0);
  azurerender::rhi::NullCommandRecorder retired;renderer.record(retired);expectOffsets(retired,{72,144,0},{132,204,60});
  if(renderer.statistics().buffers!=1)throw std::runtime_error("Retired UI ranges must be reused without extra allocations");
  renderer.ReleaseGeometry(second);renderer.ReleaseGeometry(third);renderer.ReleaseGeometry(reused);renderer.beginFrame(7,{640,360});
  if(allocator.statistics().liveBuffers)throw std::runtime_error("An empty UI arena must release its pages");
  auto small=renderer.CompileGeometry({vertices,3},{indices,3});std::vector<Rml::Vertex> large(4096);
  auto expanded=renderer.CompileGeometry({large.data(),large.size()},{indices,3});
  if(renderer.statistics().buffers!=2 || renderer.statistics().bufferBytes!=allocator.statistics().liveBufferBytes)
   throw std::runtime_error("UI arena expansion must account for actual GPU pages");
  renderer.ReleaseGeometry(small);renderer.ReleaseGeometry(expanded);renderer.beginFrame(10,{640,360});
  if(allocator.statistics().liveBuffers)throw std::runtime_error("UI arena expansion must release every page");
 }
 if(allocator.statistics().liveBuffers || allocator.statistics().liveImages)throw std::runtime_error("UI shutdown must release every allocation");
 std::cout<<"UI geometry submission, deferred retirement and resource cleanup passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
