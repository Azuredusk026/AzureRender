#pragma once

#include "extensions/ISceneRenderer.hpp"
#include "render/ReferenceDepth.hpp"
#include <cstring>

namespace azurerender {

class SceneReferenceDepthTargets final {
    struct Slot {
        rhi::GpuImage raster;
        rhi::GpuImage hardware;
        rhi::GpuImage packedNormals;
        rhi::GpuImage objectIds;
        VkImageView rasterView{};
        VkImageView hardwareView{};
        VkImageView packedNormalView{};
        VkImageView objectIdView{};
        VkFramebuffer framebuffer{};
    };

public:
    static constexpr VkFormat kNormalFormat=VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    static constexpr VkFormat kObjectIdFormat=VK_FORMAT_R8_UINT;

    SceneReferenceDepthTargets(rhi::IRhi& backend, VkExtent2D extent, unsigned frames, bool visibilityInputs, bool stencilEnabled)
        :backend_(backend),extent_(extent),visibilityInputs_(visibilityInputs),stencilEnabled_(stencilEnabled) {
        if(!ReferenceDepthGpu::supported(backend,extent,frames))
            throw std::invalid_argument("Scene reference depth exceeds attachment capabilities or budget");
        constexpr auto colorUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        constexpr auto depthUsage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        const auto hardwareFormat = stencilEnabled_ ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT;
        if (!(backend.imageSampleCounts(VK_FORMAT_R32_SFLOAT, colorUsage) & VK_SAMPLE_COUNT_1_BIT)
            || !(backend.imageSampleCounts(hardwareFormat, depthUsage) & VK_SAMPLE_COUNT_1_BIT))
            throw std::invalid_argument(stencilEnabled_
                ? "Scene reference stencil requires readable R32 and D32S8 attachments"
                : "Scene reference depth requires readable R32 and D32 attachments");
        const std::uint64_t bytesPerPixel = 8 + (stencilEnabled_ ? 4 : 0) + (visibilityInputs_ ? 5 : 0);
        if (std::uint64_t(extent.width) * extent.height * frames * bytesPerPixel > 256ULL * 1024 * 1024)
            throw std::invalid_argument("Scene reference attachments exceed the memory budget");
        if(visibilityInputs_){
            for(const auto format:{kNormalFormat,kObjectIdFormat})
                if(!backend.supportsSampledImage(format,false)||!(backend.imageSampleCounts(format,colorUsage)&VK_SAMPLE_COUNT_1_BIT))
                    throw std::invalid_argument("Scene reference visibility inputs require readable packed normals and integer IDs");
        }
        try{
            rhi::RenderPassDesc desc;
            desc.attachments={
                {VK_FORMAT_R32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false},
                {hardwareFormat,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,true,true,true}};
            desc.depthAttachment=1;
            desc.attachments[1].clearStencil = stencilEnabled_;
            if(visibilityInputs_){
                for(const auto format:{kNormalFormat,kObjectIdFormat})
                    desc.attachments.push_back({format,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,true,true,false});
            }
            desc.externalReadDependency=true;
            pass_=backend.createRenderPass(desc);
            slots_.resize(frames);
            for(auto& slot:slots_){
                slot.raster=backend.allocator().createImage2D(extent.width,extent.height,VK_FORMAT_R32_SFLOAT,colorUsage);
                slot.hardware=backend.allocator().createImage2D(extent.width,extent.height,hardwareFormat,depthUsage);
                slot.rasterView=backend.createImageView(slot.raster.image,VK_FORMAT_R32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
                slot.hardwareView=backend.createImageView(slot.hardware.image,hardwareFormat,hardwareAspects(),1);
                std::vector<VkImageView> attachments{slot.rasterView,slot.hardwareView};
                if(visibilityInputs_){
                    slot.packedNormals=backend.allocator().createImage2D(extent.width,extent.height,kNormalFormat,colorUsage);
                    slot.objectIds=backend.allocator().createImage2D(extent.width,extent.height,kObjectIdFormat,colorUsage);
                    slot.packedNormalView=backend.createImageView(slot.packedNormals.image,kNormalFormat,VK_IMAGE_ASPECT_COLOR_BIT,1);
                    slot.objectIdView=backend.createImageView(slot.objectIds.image,kObjectIdFormat,VK_IMAGE_ASPECT_COLOR_BIT,1);
                    attachments.push_back(slot.packedNormalView);
                    attachments.push_back(slot.objectIdView);
                }
                slot.framebuffer=backend.createFramebuffer({pass_,std::move(attachments),extent.width,extent.height});
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
    VkImage packedNormalImage(unsigned slot) const{return slots_.at(slot).packedNormals.image;}
    VkImage objectIdImage(unsigned slot) const{return slots_.at(slot).objectIds.image;}
    VkImageView rasterView(unsigned slot) const{return slots_.at(slot).rasterView;}
    VkImageView packedNormalView(unsigned slot) const{return slots_.at(slot).packedNormalView;}
    VkImageView objectIdView(unsigned slot) const{return slots_.at(slot).objectIdView;}
    bool visibilityInputs() const noexcept{return visibilityInputs_;}
    bool stencilEnabled() const noexcept { return stencilEnabled_; }
    VkImageAspectFlags hardwareAspects() const noexcept {
        return stencilEnabled_ ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    rhi::RenderPassBeginDesc begin(unsigned slot) const {
        rhi::RenderPassBeginDesc result;
        result.renderPass=pass_;
        result.framebuffer=slots_.at(slot).framebuffer;
        result.extent=extent_;
        result.clearValues.resize(visibilityInputs_?4:2);
        result.clearValues[0].color.float32[0]=0;
        result.clearValues[1].depthStencil={0,0};
        return result;
    }

    SceneReferenceDepthCapture read(unsigned slot,std::uint64_t submission,bool readVisibility,VkImage objectVisibility) const {
        SceneReferenceDepthCapture result;
        result.extent=extent_;
        result.submission=submission;
        result.rasterDepth=readPlane<std::uint32_t>(rasterImage(slot),false);
        result.hardwareDepth=readPlane<std::uint32_t>(hardwareImage(slot),true);
        if(readVisibility){
            if(!visibilityInputs_)throw std::logic_error("Scene reference visibility attachments are unavailable");
            result.packedNormals=readPlane<std::uint32_t>(packedNormalImage(slot),false);
            result.objectIds=readPlane<std::uint8_t>(objectIdImage(slot),false);
        }
        if(objectVisibility)result.objectVisibility=readPlane<std::uint16_t>(objectVisibility,false);
        return result;
    }

private:
    template<class T> std::vector<T> readPlane(VkImage image,bool depth) const {
        const auto pixels=std::size_t(extent_.width)*extent_.height;
        const auto bytes=VkDeviceSize(pixels)*sizeof(T);
        auto& allocator=backend_.allocator();
        auto buffer=allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        try{
            backend_.executeOneShot([&](rhi::ICommandRecorder& commands){
                rhi::ImageBarrierDesc barrier{};
                barrier.image=image;
                barrier.aspectMask=depth?hardwareAspects():VK_IMAGE_ASPECT_COLOR_BIT;
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
            std::vector<T> result(pixels);
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
            if(slot.objectIdView)backend_.destroyImageView(slot.objectIdView);
            if(slot.packedNormalView)backend_.destroyImageView(slot.packedNormalView);
            if(slot.hardwareView)backend_.destroyImageView(slot.hardwareView);
            if(slot.rasterView)backend_.destroyImageView(slot.rasterView);
            backend_.allocator().destroyImage(slot.hardware);
            backend_.allocator().destroyImage(slot.raster);
            backend_.allocator().destroyImage(slot.objectIds);
            backend_.allocator().destroyImage(slot.packedNormals);
        }
        if(pass_)backend_.destroyRenderPass(pass_);
    }

    rhi::IRhi& backend_;
    VkExtent2D extent_{};
    bool visibilityInputs_=false;
    bool stencilEnabled_=false;
    VkRenderPass pass_{};
    std::vector<Slot> slots_;
};
}
