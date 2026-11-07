#include "render/ScreenObjectVisibility.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <cstring>
#include <iostream>
using namespace azurerender;
namespace {
struct Inputs {
    test::VulkanComputeFixture& host;
    std::vector<rhi::GpuImage> images;
    std::vector<VkImageView> views;
    explicit Inputs(test::VulkanComputeFixture& value):host(value){}
    ~Inputs(){for(auto view:views)host.backend->destroyImageView(view);for(auto& image:images)host.allocator.destroyImage(image);}
    VkImageView upload(unsigned width,unsigned height,VkFormat format,const void* data,std::size_t bytes){
        auto& backend=*host.backend;
        auto staging=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
        try{
            std::memcpy(staging.mapped,data,bytes);host.allocator.flush(staging,0,bytes);
            images.push_back(host.allocator.createImage2D(width,height,format,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT));
            auto& image=images.back();backend.transitionImageLayout(image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
            backend.copyBufferToImage(staging,image,width,height,1,static_cast<std::uint32_t>(bytes));
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});});
            views.push_back(backend.createImageView(image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,1));
        }catch(...){host.allocator.destroyBuffer(staging);throw;}
        host.allocator.destroyBuffer(staging);return views.back();
    }
};
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::array<float,16> identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Expected production shader directory");
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
    constexpr unsigned width=19,height=11;unsigned checked=0;
    {
        Inputs uploaded(host);
        std::vector<float> depths(width*height,.5F);
        std::vector<std::uint32_t> normals(width*height,512U|(512U<<10));
        std::vector<std::uint8_t> ids(width*height);const unsigned pattern[]={0,1,2,8,9,255};
        for(unsigned i=0;i<ids.size();++i)ids[i]=std::uint8_t(pattern[i%6]);
        std::vector<std::uint16_t> atlas(16*16);
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)atlas[y*16+x]=x<8?0:65535;
        ScreenObjectVisibilityInputs inputs;inputs.extent={width,height};
        inputs.depth=uploaded.upload(width,height,VK_FORMAT_R32_SFLOAT,depths.data(),depths.size()*4);
        inputs.normals=uploaded.upload(width,height,VK_FORMAT_A2B10G10R10_UNORM_PACK32,normals.data(),normals.size()*4);
        inputs.objectIds=uploaded.upload(width,height,VK_FORMAT_R8_UINT,ids.data(),ids.size());
        inputs.atlas=uploaded.upload(16,16,VK_FORMAT_R16_UNORM,atlas.data(),atlas.size()*2);
        std::vector<std::uint16_t> alternateNormals(width*height*2,32768);
        std::vector<std::uint32_t> alternateIds(ids.begin(),ids.end());
        const auto normals16=uploaded.upload(width,height,VK_FORMAT_R16G16_UNORM,alternateNormals.data(),alternateNormals.size()*2);
        const auto ids32=uploaded.upload(width,height,VK_FORMAT_R32_UINT,alternateIds.data(),alternateIds.size()*4);
        ScreenObjectVisibilityView view;view.clipToWorld=identity();
        ScreenObjectVisibilityRecord first;first.worldToUnitShadow={0,0,0,0,0,0,0,0,0,0,0,0,.5F,.5F,.5F,1};
        first.atlasRect={.1F,.1F,.3F,.8F};first.filterRadiusUv={.01F,.01F};
        auto second=first;second.atlasRect[0]=.6F;
        ScreenObjectVisibilityGpu owner(backend,{width,height},2,ScreenObjectVisibilityShaders::load(argv[1]));
        auto readback=host.allocator.createBuffer(width*height*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        try{
            for(unsigned scenario=0;scenario<6;++scenario){
                const auto slot=scenario%2;
                std::vector<ScreenObjectVisibilityRecord> records(scenario==0?8:2,second);records[0]=first;
                inputs.objectDepth=scenario==2?ScreenObjectDepth::Forward:ScreenObjectDepth::Reversed;
                if(scenario==3)records.clear();
                if(scenario==4){inputs.normals=normals16;inputs.normalFormat=VK_FORMAT_R16G16_UNORM;inputs.objectIds=ids32;inputs.objectIdFormat=VK_FORMAT_R32_UINT;}
                if(scenario==5){records.resize(1);records[0].filterRadiusUv={.75F,.75F};}
                host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
                    owner.record(commands,slot,inputs,view,records);const auto output=owner.output(slot);
                    commands.imageBarrier({output.image,output.layout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};
                    vkCmdCopyImageToBuffer(native,output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
                    commands.imageBarrier({output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,output.layout,1,VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
                    commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
                });
                host.allocator.invalidate(readback,0,readback.size);
                const auto* bytes=static_cast<const std::uint8_t*>(readback.mapped);
                for(unsigned i=0;i<ids.size();++i){
                    unsigned expected=255;
                    if(ids[i]>0&&ids[i]<=records.size())expected=inputs.objectDepth==ScreenObjectDepth::Forward?(ids[i]==1?0:255):(ids[i]==1?255:0);
                    if(bytes[i*2]!=255||bytes[i*2+1]!=expected)
                        throw std::runtime_error("Visibility synthetic mismatch at scenario "+std::to_string(scenario)+" pixel "+std::to_string(i)+" expected "+std::to_string(expected)+" got "+std::to_string(bytes[i*2+1]));
                    ++checked;
                }
            }
            // A nonuniform atlas makes the 4x4 rotation phase observable.
            std::vector<std::uint16_t> irregular(16*16);
            for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)irregular[y*16+x]=(x*29+y*41+x*y*11)%127<60?65535:0;
            std::vector<std::uint32_t> phaseIds(width*height,1);
            inputs.atlas=uploaded.upload(16,16,VK_FORMAT_R16_UNORM,irregular.data(),irregular.size()*2);
            inputs.objectIds=uploaded.upload(width,height,VK_FORMAT_R32_UINT,phaseIds.data(),phaseIds.size()*4);
            auto phaseRecord=first;phaseRecord.atlasRect={0,0,1,1};phaseRecord.filterRadiusUv={.2F,.2F};
            const auto phaseImage=[&](unsigned slot,ScreenObjectVisibilityPhaseOrigin origin){
                view.phaseOrigin=origin;
                host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
                    owner.record(commands,slot,inputs,view,{phaseRecord});const auto output=owner.output(slot);
                    commands.imageBarrier({output.image,output.layout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                    VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width,height,1};
                    vkCmdCopyImageToBuffer(native,output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
                    commands.imageBarrier({output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,output.layout,1,VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
                    commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
                });
                host.allocator.invalidate(readback,0,readback.size);const auto* bytes=static_cast<const std::uint8_t*>(readback.mapped);
                return std::vector<std::uint8_t>(bytes,bytes+readback.size);
            };
            const auto top=phaseImage(0,ScreenObjectVisibilityPhaseOrigin::TopLeft);
            const auto bottom=phaseImage(1,ScreenObjectVisibilityPhaseOrigin::BottomLeft);
            bool varies=false;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
                const auto a=(y*width+x)*2,b=((height-1-y)*width+x)*2;
                varies|=top[a+1]!=top[1];
                require(top[a]==bottom[b]&&top[a+1]==bottom[b+1],"Object visibility phase origin did not mirror image rows");
            }
            require(varies,"Nonuniform phase fixture produced no observable phase variation");checked+=width*height*2;
        }catch(...){host.allocator.destroyBuffer(readback);throw;}
        host.allocator.destroyBuffer(readback);
    }
    require(!host.errors.load(),"Visibility synchronization validation failed");
    require(!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"Visibility GPU allocations leaked");
    std::cout<<"PASS: "<<host.properties.deviceName<<", "<<checked<<" pixels, forward/reverse, IDs, frame reuse, zero validation/leaks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
