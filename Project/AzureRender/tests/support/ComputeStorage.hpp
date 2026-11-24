#pragma once
#include "rhi/Rhi.hpp"
#include <cstring>
#include <stdexcept>
#include <vector>
namespace azurerender::test {
class ComputeStorage final {
public:
    rhi::IRhi& backend;rhi::GpuImage image{};VkImageView view{};
    ComputeStorage(rhi::IRhi& r,unsigned width,unsigned height,VkFormat format,unsigned texelBytes,const std::vector<char>& data):backend(r){
        rhi::GpuBuffer staging{};
        try{
            if(!backend.supportsStorageImage(format))throw std::runtime_error("Storage image format unavailable");
            staging=backend.allocator().createBuffer(data.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
            std::memcpy(staging.mapped,data.data(),data.size());backend.allocator().flush(staging,0,staging.size);
            image=backend.allocator().createImage2D(width,height,format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_STORAGE_BIT);
            backend.transitionImageLayout(image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
            backend.copyBufferToImage(staging,image,width,height,1,texelBytes);
            backend.executeOneShot([&](rhi::ICommandRecorder& commands){commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,1,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});});
            backend.allocator().destroyBuffer(staging);view=backend.createImageView(image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,1);
        }catch(...){backend.allocator().destroyBuffer(staging);release();throw;}
    }
    ~ComputeStorage(){release();}
    ComputeStorage(const ComputeStorage&)=delete;ComputeStorage& operator=(const ComputeStorage&)=delete;
private:
    void release(){if(view)backend.destroyImageView(view);view={};backend.allocator().destroyImage(image);}
};
}
