#pragma once

#include "extensions/ISceneRenderer.hpp"
#include "render/ReferenceDepth.hpp"
#include <cstring>
#include <limits>

namespace azurerender {

class SceneReferenceDepthTargets final {
    struct Slot {
        rhi::GpuImage raster;
        rhi::GpuImage hardware;
        VkImageView rasterView{};
        VkImageView hardwareView{};
        VkFramebuffer framebuffer{};
    };

public:
    SceneReferenceDepthTargets(rhi::IRhi& backend,VkExtent2D extent,unsigned frames)
        :backend_(backend),extent_(extent) {
        if(!ReferenceDepthGpu::supported(backend,extent,frames))
            throw std::invalid_argument("Scene reference depth exceeds attachment capabilities or budget");
        constexpr auto colorUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        constexpr auto depthUsage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if(!(backend.imageSampleCounts(VK_FORMAT_R32_SFLOAT,colorUsage)&VK_SAMPLE_COUNT_1_BIT)
            ||!(backend.imageSampleCounts(VK_FORMAT_D32_SFLOAT,depthUsage)&VK_SAMPLE_COUNT_1_BIT))
            throw std::invalid_argument("Scene reference depth requires readable R32 and D32 attachments");
        try{
            rhi::RenderPassDesc desc;
            desc.attachments={
                {VK_FORMAT_R32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
                {VK_FORMAT_D32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true}};
            desc.depthAttachment=1;
            desc.externalReadDependency=true;
            pass_=backend.createRenderPass(desc);
            slots_.resize(frames);
            for(auto& slot:slots_){
                slot.raster=backend.allocator().createImage2D(extent.width,extent.height,VK_FORMAT_R32_SFLOAT,colorUsage);
                slot.hardware=backend.allocator().createImage2D(extent.width,extent.height,VK_FORMAT_D32_SFLOAT,depthUsage);
                slot.rasterView=backend.createImageView(slot.raster.image,VK_FORMAT_R32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
                slot.hardwareView=backend.createImageView(slot.hardware.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
                slot.framebuffer=backend.createFramebuffer({pass_,{slot.rasterView,slot.hardwareView},extent.width,extent.height});
            }
        }catch(...){
            release();
            throw;
        }
    }

    ~SceneReferenceDepthTargets(){release();}
    SceneReferenceDepthTargets(const SceneReferenceDepthTargets&)=delete;
    SceneReferenceDepthTargets& operator=(const SceneReferenceDepthTargets&)=delete;

    VkRenderPass renderPass() const noexcept{return pass_;}
    VkImage rasterImage(unsigned slot) const{return slots_.at(slot).raster.image;}
    VkImage hardwareImage(unsigned slot) const{return slots_.at(slot).hardware.image;}

    rhi::RenderPassBeginDesc begin(unsigned slot) const {
        rhi::RenderPassBeginDesc result;
        result.renderPass=pass_;
        result.framebuffer=slots_.at(slot).framebuffer;
        result.extent=extent_;
        result.clearValues.resize(2);
        result.clearValues[0].color.float32[0]=0;
        result.clearValues[1].depthStencil={0,0};
        return result;
    }

    SceneReferenceDepthCapture read(unsigned slot,std::uint64_t submission) const {
        SceneReferenceDepthCapture result;
        result.extent=extent_;
        result.submission=submission;
        result.rasterDepth=readPlane(rasterImage(slot),false);
        result.hardwareDepth=readPlane(hardwareImage(slot),true);
        return result;
    }

private:
    std::vector<std::uint32_t> readPlane(VkImage image,bool depth) const {
        const auto pixels=std::size_t(extent_.width)*extent_.height;
        const auto bytes=VkDeviceSize(pixels)*sizeof(std::uint32_t);
        auto& allocator=backend_.allocator();
        auto buffer=allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        try{
            backend_.executeOneShot([&](rhi::ICommandRecorder& commands){
                rhi::ImageBarrierDesc barrier{};
                barrier.image=image;
                barrier.aspectMask=depth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.oldLayout=depth?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcStageMask=depth?VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask=depth?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                commands.imageBarrier(barrier);
                if(depth)commands.copyDepthImageToBuffer(image,buffer.buffer,extent_);
                else commands.copyImageToBuffer(image,buffer.buffer,extent_);
                commands.bufferBarrier({buffer.buffer,0,bytes,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
                std::swap(barrier.oldLayout,barrier.newLayout);
                barrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                commands.imageBarrier(barrier);
            });
            allocator.invalidate(buffer,0,bytes);
            std::vector<std::uint32_t> result(pixels);
            std::memcpy(result.data(),buffer.mapped,static_cast<std::size_t>(bytes));
            allocator.destroyBuffer(buffer);
            return result;
        }catch(...){
            allocator.destroyBuffer(buffer);
            throw;
        }
    }

    void release() noexcept {
        for(auto& slot:slots_){
            if(slot.framebuffer)backend_.destroyFramebuffer(slot.framebuffer);
            if(slot.hardwareView)backend_.destroyImageView(slot.hardwareView);
            if(slot.rasterView)backend_.destroyImageView(slot.rasterView);
            backend_.allocator().destroyImage(slot.hardware);
            backend_.allocator().destroyImage(slot.raster);
        }
        if(pass_)backend_.destroyRenderPass(pass_);
    }

    rhi::IRhi& backend_;
    VkExtent2D extent_{};
    VkRenderPass pass_{};
    std::vector<Slot> slots_;
};
}
