#include "render/MotionVectorPass.hpp"
#include "render/SceneInstanceGpuData.hpp"
#include "render/TemporalReconstruction.hpp"
#include "render/RenderMath.hpp"
#include "assets/GltfLoader.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shader directory");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    std::array<rhi::GpuImage,3> images{};std::array<VkImageView,3> views{};
    std::array<rhi::GpuBuffer,11> buffers{};VkSampler sampler=VK_NULL_HANDLE;
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(sampler)backend.destroySampler(sampler);for(auto view:views)if(view)backend.destroyImageView(view);
        for(auto& image:images)if(image.image)host.allocator.destroyImage(image);
        for(auto& buffer:buffers)if(buffer.buffer)host.allocator.destroyBuffer(buffer);
    });
    images[0]=host.allocator.createImage2D(32,32,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    images[1]=host.allocator.createImage2D(1,1,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    images[2]=host.allocator.createImage2D(32,32,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    views[0]=backend.createImageView(images[0].image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    views[1]=backend.createImageView(images[1].image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,1);sampler=backend.createSampler({});
    views[2]=backend.createImageView(images[2].image,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
    const std::array<std::size_t,11> sizes{sizeof(SceneInstanceGpuData),64,4,4,3*sizeof(AssetVertex),12,32*32*20,32*32*4,80,32*32*8,32*32*8};
    for(unsigned i=0;i<11;++i)buffers[i]=host.allocator.createBuffer(sizes[i],i<4||i==8?VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:i==4?VK_BUFFER_USAGE_VERTEX_BUFFER_BIT:i==5?VK_BUFFER_USAGE_INDEX_BUFFER_BIT:i==9?VK_BUFFER_USAGE_TRANSFER_SRC_BIT:VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    SceneInstanceGpuData instance;instance.model=instance.modelViewProjection=motionIdentityMatrix();
    std::memcpy(buffers[0].mapped,&instance,sizeof(instance));auto identity=motionIdentityMatrix();std::memcpy(buffers[1].mapped,identity.data(),64);
    std::memset(buffers[2].mapped,0,4);std::memset(buffers[3].mapped,0,4);
    std::array<AssetVertex,3> vertices{};
    for(unsigned i=0;i<3;++i){vertices[i].position={i==1?3.F:-1.F,i==2?3.F:-1.F,.25F};vertices[i].normal={1,0,0};vertices[i].tangent={0,1,0,1};vertices[i].weights={1,0,0,0};}
    std::memcpy(buffers[4].mapped,vertices.data(),sizeof(vertices));const std::uint32_t indices[]{0,1,2};std::memcpy(buffers[5].mapped,indices,12);
    for(unsigned i=0;i<6;++i)host.allocator.flush(buffers[i],0,buffers[i].size);
    std::memset(buffers[8].mapped,0,80);auto* outline=static_cast<float*>(buffers[8].mapped);
    outline[4]=.75F;outline[5]=.35F;outline[6]=.02F;outline[7]=1;outline[12]=outline[13]=1;
    host.allocator.flush(buffers[8],0,80);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
        rhi::ImageBarrierDesc b;b.srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;b.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.image=images[0].image;b.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;commands.imageBarrier(b);
        VkClearDepthStencilValue value{.5F,0};VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};vkCmdClearDepthStencilImage(raw,b.image,b.newLayout,&value,1,&range);
        b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
        b.dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;commands.imageBarrier(b);
        b={};b.image=images[1].image;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;b.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(b);
        VkClearColorValue white{{1,1,1,1}};commands.clearColorImage(b.image,b.newLayout,white);
        b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.dstStageMask=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(b);
    });
    {
        TemporalReconstruction temporal(backend,host.allocator,{32,32},VK_FORMAT_D32_SFLOAT,{{images[2].image,views[2],images[0].image,views[0]}},argv[1]);
        auto& target=temporal.motion();
        MotionVectorPass pass(backend,host.allocator,target,argv[1],1,2,false,true);
        MotionVectorInputs input;input.instances=&buffers[0];input.joints=&buffers[1];input.weights=&buffers[2];input.targets=&buffers[3];input.outline=&buffers[8];
        input.colors=input.masks={{views[1],sampler}};input.coverage=input.colors.front();
        MotionPoseState old;old.identity.node="triangle";old.vertexCount=3;old.model[12]=-.2F;old.joints={identity};
        auto now=old;now.model=identity;MotionPoseView oldView;oldView.outlinePixels=.8F;
        MotionPoseHistory history;history.stage({old},oldView);history.commit(3);history.stage({now},{});
        MotionVectorDraw draw;draw.vertices=buffers[4].buffer;draw.indices=buffers[5].buffer;draw.indexCount=3;
        draw.push.camera[2]=3;const std::uint32_t noTile=UINT32_MAX;std::memcpy(&draw.push.camera[3],&noTile,4);draw.push.outline={.8F,32,32,1};
        for(unsigned variant=0;variant<12;++variant){
            auto packed=history.pack(identity,variant==0?0:3,8192);pass.prepare(variant%2,packed,input);draw.push.flags=variant==2?16:variant==3?512:variant==4?768:variant==5?18:0;
            draw.shell=variant==3||variant==4;
            const std::uint32_t filtered=variant>=9?(1U<<30U):variant>=6?MaterialFeatureHairPixelIntegral:0U;std::memcpy(&draw.push.detail[1],&filtered,4);
            draw.push.outline[3]=variant==11?0.F:variant==10?.5F:1.F;
            if(variant>=6){
                draw.push.flags=variant>=9?18:variant==8?16:0;
                for(unsigned i=0;i<3;++i)vertices[i].normal=variant==7?std::array<float,3>{i==0?1.F:0.F,i==1?1.F:0.F,i==2?1.F:0.F}:std::array<float,3>{1,0,0};
                std::memcpy(buffers[4].mapped,vertices.data(),sizeof(vertices));host.allocator.flush(buffers[4],0,sizeof(vertices));
            }
            if(variant==5||variant==9)backend.executeOneShot([&](rhi::ICommandRecorder& commands){rhi::ImageBarrierDesc b;b.image=images[1].image;
                b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b.srcStageMask=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
                b.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(b);
                VkClearColorValue transparent{{1,1,1,variant==5?0.F:1.F}};commands.clearColorImage(b.image,b.newLayout,transparent);
                b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                b.dstStageMask=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(b);});
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
                for(unsigned i=0;i<6;++i){rhi::BufferBarrierDesc b;b.buffer=buffers[i].buffer;b.size=buffers[i].size;b.srcStageMask=VK_PIPELINE_STAGE_HOST_BIT;
                    b.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;b.dstStageMask=i<4?VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
                    b.dstAccessMask=i<4?VK_ACCESS_SHADER_READ_BIT:i==4?VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:VK_ACCESS_INDEX_READ_BIT;commands.bufferBarrier(b);}
                rhi::BufferBarrierDesc previous;previous.buffer=pass.previous(variant%2).buffer;previous.size=pass.previous(variant%2).size;previous.srcStageMask=VK_PIPELINE_STAGE_HOST_BIT;
                previous.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;previous.dstStageMask=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;previous.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.bufferBarrier(previous);
                pass.record(commands,variant%2,0,{32,32},{draw});
                for(unsigned attachment=0;attachment<2;++attachment){rhi::ImageBarrierDesc b;b.image=attachment?target.surfaceDepthImage().image:target.image().image;
                    b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    b.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(b);
                    VkBufferImageCopy copy{};copy.bufferOffset=attachment?32*32*16:0;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={32,32,1};
                    vkCmdCopyImageToBuffer(raw,b.image,b.newLayout,buffers[6].buffer,1,&copy);}
                rhi::BufferBarrierDesc b;b.buffer=buffers[6].buffer;b.size=buffers[6].size;b.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
                b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(b);
            });
            host.allocator.invalidate(buffers[6],0,buffers[6].size);const auto* values=static_cast<const float*>(buffers[6].mapped);
            for(unsigned y=1;y<31;++y)for(unsigned x=5;x<30;++x){auto pixel=y*32+x;const auto* motion=values+pixel*4;
                const bool invisible=variant==5||variant==11;
                const bool reactive=variant==2||variant==8||variant>=9;
                const float response=invisible?0.F:reactive?-2.F:variant==6?-.5F:variant==0?-1.F:1.F;
                const float expected[]{variant&&!invisible?.1F:0,0,variant==4?.27F:variant&&!invisible?.25F:0,response};
                for(unsigned axis=0;axis<4;++axis)if(!std::isfinite(motion[axis])||std::abs(motion[axis]-expected[axis])>2e-6F)
                    {std::cerr<<"variant="<<variant<<" pixel="<<x<<','<<y<<" axis="<<axis<<" actual="<<motion[axis]<<" expected="<<expected[axis]<<'\n';
                    throw std::runtime_error("Actual motion attachment differs from independent translation, depth or reactive reference");}
                if(std::abs(values[4096+pixel]-(invisible?1.F:variant==4?.27F:.25F))>1e-6F)throw std::runtime_error("Motion color-surface depth differs from drawn surface");}
        }
        draw.push.detail[1]=0;draw.push.outline[3]=1;
        TemporalViewState camera;TemporalFrameInfo info;info.width=info.height=32;info.sceneGeneration=1;
        info.projection=info.viewProjection=identity;MotionPoseHistory temporalPoses;
        const MotionMatrix farProjection{1,0,0,0,0,-1,0,0,0,0,1000.F/(.1F-1000.F),-1,0,0,100.F/(.1F-1000.F),0};
        // Phases 14/15 expose an opaque surface after a coplanar reactive layer.
        // Depth and neighbourhood bounds cannot distinguish that stale colour.
        for(unsigned phase=0;phase<16;++phase){
            const auto slot=phase%2;info.cameraCut=phase>=14?2:phase>=3?1:0;info.exposureScale=phase==8?2.F:phase==9?4.F:1.F;
            info.projection=info.viewProjection=phase==10||phase==11?farProjection:identity;
            const auto prepared=camera.prepare(info);
            if(phase==9&&(!prepared.previous||prepared.previous->exposureScale!=2.F))throw std::runtime_error("Exposure changes must preserve unexposed HDR history");
            auto currentPose=now;if(phase==6||phase==7)currentPose.model[12]=.2F;if(phase==7)currentPose.model[14]=.5F;
            if(phase==10||phase==11){currentPose.model[0]=currentPose.model[5]=50;currentPose.model[14]=phase==10?-50.25F:-200.25F;}
            if(phase==12||phase==13){currentPose.model[0]=currentPose.model[5]=.25F;currentPose.model[12]=phase==12?-.5F:-.375F;}
            instance.model=currentPose.model;instance.modelViewProjection=phase==10||phase==11?internal::multiply(farProjection,currentPose.model):currentPose.model;
            std::memcpy(buffers[0].mapped,&instance,sizeof(instance));host.allocator.flush(buffers[0],0,sizeof(instance));
            temporal.prepare(prepared,slot,0);temporalPoses.stage({currentPose},{});
            pass.prepare(slot,temporalPoses.pack(prepared.previous?prepared.previous->viewProjection:identity,prepared.previousSubmission,8192),input);
            draw.shell=false;draw.push.flags=phase==2||phase==5||phase==14?16:0;
            auto* upload=static_cast<std::uint16_t*>(buffers[9].mapped);
            for(unsigned pixel=0;pixel<1024;++pixel){auto value=((pixel%32+pixel/32+phase)%2)?std::uint16_t(0x3c00):std::uint16_t(0);
                if(phase==6)value=value?0x3a66:0x3266;
                if(phase==12||phase==13){const float x=2*(float(pixel%32)+.5F)/32-1,y=2*(float(pixel/32)+.5F)/32-1,cx=currentPose.model[12];
                    value=x>=cx-.25F&&y>=-.25F&&x+y<=cx+.5F?0x3c00:0;}
                upload[pixel*4]=upload[pixel*4+1]=upload[pixel*4+2]=value;upload[pixel*4+3]=0x3800;}
            host.allocator.flush(buffers[9],0,buffers[9].size);
            RenderGraph graph;auto motion=temporal.importMotion(graph,"temporal-gpu");
            rhi::ImageBarrierDesc state;state.image=images[2].image;state.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            state.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;state.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            const auto color=graph.importImage("current-hdr",state);state.image=images[0].image;state.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
            state.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;state.dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            state.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;const auto depth=graph.importImage("hardware-depth",state);
            const auto motionPass=graph.addCommandPass("actual-motion",[&](rhi::ICommandRecorder& commands){pass.record(commands,slot,0,{32,32},{draw});});
            graph.attachment(motionPass,depth,RenderGraphUsage::DepthReadAttachment,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
            for(auto resource:motion)graph.attachment(motionPass,resource,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            temporal.schedule(graph,color,depth,motion,"temporal-gpu");std::string error;if(!graph.compile(error))throw std::runtime_error(error);
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
                if(phase==10||phase==12){rhi::ImageBarrierDesc depthBarrier;depthBarrier.image=images[0].image;depthBarrier.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
                    depthBarrier.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;depthBarrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    depthBarrier.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    depthBarrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    depthBarrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;depthBarrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(depthBarrier);
                    const float depth=phase==10?(-100*farProjection[10]+farProjection[14])/100:1;
                    VkClearDepthStencilValue clear{depth,0};VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
                    vkCmdClearDepthStencilImage(raw,images[0].image,depthBarrier.newLayout,&clear,1,&range);
                    depthBarrier.oldLayout=depthBarrier.newLayout;depthBarrier.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    depthBarrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;depthBarrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                    depthBarrier.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    depthBarrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;commands.imageBarrier(depthBarrier);
                }
                rhi::BufferBarrierDesc uploadBarrier;uploadBarrier.buffer=buffers[9].buffer;uploadBarrier.size=buffers[9].size;
                uploadBarrier.srcStageMask=VK_PIPELINE_STAGE_HOST_BIT;uploadBarrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;
                uploadBarrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;uploadBarrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.bufferBarrier(uploadBarrier);
                rhi::ImageBarrierDesc b;b.image=images[2].image;b.oldLayout=phase?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
                b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.srcStageMask=phase?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                b.srcAccessMask=phase?VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:0;b.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;commands.imageBarrier(b);
                VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={32,32,1};
                vkCmdCopyBufferToImage(raw,buffers[9].buffer,images[2].image,b.newLayout,1,&copy);
                b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                b.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(b);
                rhi::BufferBarrierDesc previous;previous.buffer=pass.previous(slot).buffer;previous.size=pass.previous(slot).size;
                previous.srcStageMask=VK_PIPELINE_STAGE_HOST_BIT;previous.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;previous.dstStageMask=VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;previous.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.bufferBarrier(previous);
                previous.buffer=buffers[0].buffer;previous.size=buffers[0].size;commands.bufferBarrier(previous);
                graph.execute(&commands);
                b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                b.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;b.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(b);
                commands.copyImageToBuffer(images[2].image,buffers[10].buffer,{32,32});
                b.oldLayout=b.newLayout;b.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;b.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(b);
                previous.buffer=buffers[10].buffer;previous.size=buffers[10].size;previous.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;previous.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
                previous.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;previous.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(previous);
            });
            host.allocator.invalidate(buffers[10],0,buffers[10].size);const auto* pixels=static_cast<const std::uint16_t*>(buffers[10].mapped);
            for(unsigned pixel=0;pixel<1024;++pixel)for(unsigned axis=0;axis<4;++axis){
                if((phase==6||phase==7)&&(pixel%32<5||pixel%32>=30||pixel/32<1||pixel/32>=31))continue;
                auto expected=axis==3?std::uint16_t(0x3800):upload[pixel*4+axis];
                if(axis<3&&(phase==1||phase==4||phase==9))expected=expected?0x2e66:0x3b33;
                if((phase==12||phase==13)&&expected==0&&pixels[pixel*4+axis]<=0x21)continue;
                if(std::abs(int(pixels[pixel*4+axis])-int(expected))>1){std::cerr<<"Temporal phase="<<phase<<" pixel="<<pixel<<" axis="<<axis<<" actual="<<pixels[pixel*4+axis]<<" expected="<<expected<<'\n';
                    throw std::runtime_error("HDR temporal output differs from independent history blend, reactive or camera-cut reference");}
            }
            if(!temporal.commit(10+phase)||!camera.commit(10+phase)||!temporalPoses.commit(10+phase))throw std::runtime_error("Actual GPU temporal submission did not publish history");
        }
    }
    if(host.errors.load())throw std::runtime_error("Motion vector Vulkan validation errors");
    std::cout<<"Motion vector GPU: 45000 motion checks and 63344 HDR history checks, including coplanar reactive release, far depth and moving silhouette rejection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
