#pragma once
#include "rhi/Rhi.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace azurerender::test {
class ComputeTexture {
public:
    rhi::IRhi& backend;rhi::GpuImage image{};VkImageView view{};
    ComputeTexture(rhi::IRhi& r,unsigned width,unsigned height,unsigned levels,unsigned channels,VkFormat format,const std::vector<float>& values):backend(r){
        initialize(width,height,levels,channels*4,format,values.data(),values.size()*4);
    }
    ComputeTexture(rhi::IRhi& r,unsigned width,unsigned height,unsigned levels,VkFormat format,const std::vector<char>& values,unsigned texelBytes=4):backend(r){
        initialize(width,height,levels,texelBytes,format,values.data(),values.size());
    }
    ~ComputeTexture(){release();}
    ComputeTexture(const ComputeTexture&)=delete;ComputeTexture& operator=(const ComputeTexture&)=delete;
private:
    void initialize(unsigned width,unsigned height,unsigned levels,unsigned texelBytes,VkFormat format,const void* data,std::size_t bytes){
        std::size_t expected=0;for(unsigned mip=0;mip<levels;++mip)expected+=std::size_t(std::max(1U,width>>mip))*std::max(1U,height>>mip)*texelBytes;
        if(bytes!=expected)throw std::invalid_argument("Fixture mip storage must match its dimensions");rhi::GpuBuffer staging{};
        try{
            staging=backend.allocator().createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
            std::memcpy(staging.mapped,data,bytes);backend.allocator().flush(staging,0,staging.size);
            image=backend.allocator().createImage2D(width,height,format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,levels);
            backend.transitionImageLayout(image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,levels);
            backend.copyBufferToImage(staging,image,width,height,levels,texelBytes);
            backend.executeOneShot([&](rhi::ICommandRecorder& commands){commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,levels,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});});
            backend.allocator().destroyBuffer(staging);view=backend.createImageView(image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,levels);
        }catch(...){backend.allocator().destroyBuffer(staging);release();throw;}
    }
    void release(){if(view)backend.destroyImageView(view);view=VK_NULL_HANDLE;backend.allocator().destroyImage(image);}
};
}
