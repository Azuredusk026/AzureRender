#include "render/GpuConservativeOcclusion.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool denseOccluded(const GpuOcclusionBounds& box,const std::vector<float>& depth,VkExtent2D extent,VkViewport viewport){
    std::array<double,3> low{1e300,1e300,1e300},high{-1e300,-1e300,-1e300};
    for(unsigned corner=0;corner<8;++corner){std::array<double,4> clip{};
        for(unsigned row=0;row<4;++row){clip[row]=box.clipFromLocal[12+row];
            for(unsigned a=0;a<3;++a)clip[row]+=double(box.clipFromLocal[a*4+row])*((corner&(1U<<a))?box.maximum[a]:box.minimum[a]);}
        if(clip[3]<=1e-5)return false;
        for(unsigned a=0;a<3;++a){low[a]=std::min(low[a],clip[a]/clip[3]);high[a]=std::max(high[a],clip[a]/clip[3]);}
    }
    if(high[0]<-1||low[0]>1||high[1]<-1||low[1]>1)return true;
    if(low[2]<0||high[2]>1)return false;
    std::array<int,2> first{},last{};
    const std::array<float,2> origin{viewport.x,viewport.y},scale{viewport.width,viewport.height};
    const std::array<unsigned,2> size{extent.width,extent.height};
    for(unsigned a=0;a<2;++a){const double x=origin[a]+(std::clamp(low[a],-1.,1.)*.5+.5)*scale[a];
        const double y=origin[a]+(std::clamp(high[a],-1.,1.)*.5+.5)*scale[a];
        first[a]=std::clamp(int(std::floor(std::min(x,y)))-1,0,int(size[a])-1);
        last[a]=std::clamp(int(std::ceil(std::max(x,y))),0,int(size[a])-1);}
    float farthest=0;
    for(int y=first[1];y<=last[1];++y)for(int x=first[0];x<=last[0];++x)farthest=std::max(farthest,depth[std::size_t(y)*extent.width+x]);
    return low[2]-1e-5>farthest;
}
}
int main(int argc,char** argv)try{
    require(argc==2||argc==3,"Expected shader directory and optional captured fixture");test::VulkanComputeFixture host;auto& backend=*host.backend;const std::string shaders=argv[1];
    constexpr unsigned count=4108;std::uint64_t checked=0,culled=0;unsigned cases=0;
    {
        GpuConservativeOcclusion owner(backend,count,2,readBinaryFile(shaders+"/conservative_occlusion.comp.spv"));
        auto source=host.allocator.createBuffer(count*20,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        auto readback=host.allocator.createBuffer(count*20,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(readback);});
        for(const auto extent:{VkExtent2D{65,33},VkExtent2D{17,9},VkExtent2D{1,9}}){
            std::vector<float> depth(std::size_t(extent.width)*extent.height,.25F);
            for(unsigned y=0;y<extent.height;++y)for(unsigned x=0;x<extent.width;++x)
                if(x==extent.width-1&&y==extent.height-1)depth[std::size_t(y)*extent.width+x]=1;
            test::ComputeTexture texture(backend,extent.width,extent.height,1,1,VK_FORMAT_R32_SFLOAT,depth);
            unsigned levels=1;for(auto side=std::max(extent.width,extent.height);side>1;side/=2)++levels;
            DepthPyramidGpu pyramid(backend,extent,levels,2,readBinaryFile(shaders+"/depth_pyramid.comp.spv"));
            for(unsigned flip=0;flip<(extent.width==65?2U:1U);++flip){
                const unsigned slot=cases%2;const VkViewport viewport{0,flip?float(extent.height):0,float(extent.width),flip?-float(extent.height):float(extent.height),0,1};
                std::vector<GpuOcclusionBounds> boxes(count);std::vector<VkDrawIndexedIndirectCommand> commands(count);
                std::mt19937 random(1729+cases);
                for(unsigned i=0;i<count;++i){const float x=float(random()%1800)/1000-1,y=float(random()%1800)/1000-1,z=float(random()%900)/1000+.05F;
                    boxes[i].minimum={x,y,z,0};boxes[i].maximum={x+.02F,y+.03F,z+.01F,0};
                    auto& matrix=boxes[i].clipFromLocal;matrix[0]=1.2F;matrix[1]=.03F;matrix[4]=-.02F;matrix[5]=.8F;
                    matrix[3]=.04F;matrix[7]=-.03F;matrix[11]=.02F;matrix[12]=.02F*float(i%5);matrix[13]=-.01F*float(i%3);
                    commands[i]={3+(i%11)*3,1,i*7,-7,i*3};}
                for(unsigned i=0;i<9;++i){boxes[i]=GpuOcclusionBounds{};boxes[i].minimum={-.75F,-.25F,.8F,0};boxes[i].maximum={-.5F,0,.9F,0};}
                boxes[1].minimum[2]=.1F;boxes[1].maximum[2]=.2F;boxes[2].minimum[2]=boxes[2].maximum[2]=.25F;
                boxes[3].clipFromLocal[3]=1;boxes[3].clipFromLocal[15]=0;
                boxes[4].minimum[0]=2;boxes[4].maximum[0]=3;commands[5].instanceCount=2;commands[6].instanceCount=0;
                boxes[7].clipFromLocal[0]=1e30F;boxes[7].clipFromLocal[12]=-1e30F;boxes[7].minimum[0]=1;boxes[7].maximum[0]=1.0000001F;
                boxes[8].minimum[2]=-.1F;boxes[8].maximum[2]=.2F;
                std::memcpy(source.mapped,commands.data(),count*20);host.allocator.flush(source,0,count*20);
                const auto ticket=owner.prepare(slot,source,boxes,pyramid.output(slot),viewport);
                host.submit([&](rhi::ICommandRecorder& recorder,VkCommandBuffer native){
                    recorder.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                    pyramid.record(recorder,slot,texture.view,DepthReduction::Maximum);owner.record(recorder,ticket);
                    const auto& output=owner.output(slot);
                    recorder.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                    const VkBufferCopy copy{0,0,count*20};vkCmdCopyBuffer(native,output.buffer,readback.buffer,1,&copy);
                    recorder.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
                });
                host.allocator.invalidate(readback,0,readback.size);const auto* actual=static_cast<const VkDrawIndexedIndirectCommand*>(readback.mapped);
                unsigned zero=0;
                for(unsigned i=0;i<count;++i){auto expected=commands[i];zero+=actual[i].instanceCount==0?1U:0U;
                    if(actual[i].instanceCount!=expected.instanceCount){
                        require(expected.instanceCount==1&&actual[i].instanceCount==0,"Only single-instance draws may be rejected");
                        if(!denseOccluded(boxes[i],depth,extent,viewport))throw std::runtime_error("Conservative rejection conflict at command "+std::to_string(i));
                        expected.instanceCount=0;++culled;}
                    require(std::memcmp(&actual[i],&expected,20)==0,"Occlusion modified command geometry, source identity or ordering");}
                require(owner.retiredZeroCommandCount(slot)==zero,"Retired output summary differs from explicit readback");
                require(actual[4].instanceCount==0,"Outside-frustum draw must be rejected");
                if(extent.width>1)require(actual[0].instanceCount==0,"Fully covered small bounds must be rejected");
                for(unsigned i:{1U,2U,3U,5U,8U})require(actual[i].instanceCount==commands[i].instanceCount,"Visible, near-plane or multi-instance draw was rejected");
                checked+=count;++cases;
            }
        }
        if(argc==3){
            const std::string folder=argv[2];std::ifstream input(folder+"/bounds.json");nlohmann::json fixture;input>>fixture;
            const auto& rows=fixture.at("rows");const auto n=static_cast<unsigned>(rows.size());require(n<=count,"Captured fixture exceeds bounded test storage");
            const VkExtent2D extent{fixture.at("width").get<unsigned>(),fixture.at("height").get<unsigned>()};
            const auto bytes=readBinaryFile(folder+"/depth.f32");require(bytes.size()==std::size_t(extent.width)*extent.height*4,"Captured depth byte count mismatch");
            std::vector<float> depth(bytes.size()/4);std::memcpy(depth.data(),bytes.data(),bytes.size());
            test::ComputeTexture texture(backend,extent.width,extent.height,1,VK_FORMAT_R32_SFLOAT,bytes);
            unsigned levels=1;for(auto side=std::max(extent.width,extent.height);side>1;side/=2)++levels;
            DepthPyramidGpu pyramid(backend,extent,levels,2,readBinaryFile(shaders+"/depth_pyramid.comp.spv"));
            const auto& view=fixture.at("viewport");const VkViewport viewport{view.at("x").get<float>(),view.at("y").get<float>(),view.at("width").get<float>(),view.at("height").get<float>(),0,1};
            std::vector<GpuOcclusionBounds> boxes(n);std::vector<VkDrawIndexedIndirectCommand> commands(n);
            for(unsigned i=0;i<n;++i){for(unsigned a=0;a<3;++a){boxes[i].minimum[a]=rows[i].at("minimum").at(a).get<float>();boxes[i].maximum[a]=rows[i].at("maximum").at(a).get<float>();}
                commands[i]={3,1,i*3,-2,i};}
            std::memcpy(source.mapped,commands.data(),n*20);host.allocator.flush(source,0,n*20);
            const auto ticket=owner.prepare(0,source,boxes,pyramid.output(0),viewport);
            host.submit([&](rhi::ICommandRecorder& recorder,VkCommandBuffer native){
                recorder.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                pyramid.record(recorder,0,texture.view,DepthReduction::Maximum);owner.record(recorder,ticket);
                const auto& output=owner.output(0);recorder.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                const VkBufferCopy copy{0,0,n*20};vkCmdCopyBuffer(native,output.buffer,readback.buffer,1,&copy);
                recorder.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            });
            host.allocator.invalidate(readback,0,readback.size);const auto* actual=static_cast<const VkDrawIndexedIndirectCommand*>(readback.mapped);unsigned rejected=0;double historicalMs=0;
            for(unsigned i=0;i<n;++i){auto expected=commands[i];
                if(actual[i].instanceCount==0){require(rows[i].at("pixelInvocations").get<std::uint64_t>()==0&&denseOccluded(boxes[i],depth,extent,viewport),"Captured visible draw was rejected");
                    expected.instanceCount=0;++rejected;historicalMs+=rows[i].at("baselineMs").get<double>();}
                require(std::memcmp(&actual[i],&expected,20)==0,"Captured command fields changed");}
            std::cout<<nlohmann::json{{"scope","captured-depth-and-clip-bounds"},{"draws",n},{"culled",rejected},{"historicalMs",historicalMs},{"hotspotPotential",historicalMs/fixture.at("ordinaryMainBaselineMs").get<double>()},{"conflicts",0},{"newGpuTimingExecuted",false}}.dump()<<'\n';
        }
    }
    require(culled>4000,"Occlusion fixture must exercise substantial actual rejection");
    require(host.errors.load()==0,"Vulkan synchronization validation failed");
    require(!host.allocator.statistics().liveBuffers&&!host.allocator.statistics().liveImages,"Occlusion lifecycle leaked resources");
    std::cout<<"Conservative GPU occlusion: device="<<host.properties.deviceName<<" cases="<<cases<<" commands="<<checked<<" culled="<<culled<<" conflicts=0 validationErrors=0 liveBuffers=0 liveImages=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
