#include "render/TemporalReconstruction.hpp"
#include "render/ReflectionTemporal.hpp"
#include "render/MotionPoseData.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include <cstring>
#include <iostream>
using namespace azurerender;
namespace {
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
constexpr VkExtent2D extent{64,48};
struct SceneImages {
    rhi::IRhi& backend;rhi::GpuImage color{},depth{};VkImageView colorView{},depthView{};
    explicit SceneImages(test::VulkanComputeFixture& host):backend(*host.backend){
        try{
            color=host.allocator.createImage2D(extent.width,extent.height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            depth=host.allocator.createImage2D(extent.width,extent.height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            colorView=backend.createImageView(color.image,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
            depthView=backend.createImageView(depth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
                commands.imageBarrier({color.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT});
                commands.imageBarrier({depth.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
                VkClearColorValue value{};value.float32[0]=.25F;value.float32[1]=.125F;value.float32[2]=.0625F;value.float32[3]=1;
                commands.clearColorImage(color.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,value);
                VkClearDepthStencilValue depthValue{1,0};VkImageSubresourceRange range{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
                vkCmdClearDepthStencilImage(native,depth.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&depthValue,1,&range);
                commands.imageBarrier({color.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                commands.imageBarrier({depth.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
            });
        }catch(...){reset();throw;}
    }
    void reset(){if(colorView)backend.destroyImageView(colorView);colorView={};if(depthView)backend.destroyImageView(depthView);depthView={};backend.allocator().destroyImage(color);backend.allocator().destroyImage(depth);}
    ~SceneImages(){reset();}
};
void verify(test::VulkanComputeFixture& host,const SceneImages& scene,const ReflectionPyramidViews& output){
    constexpr unsigned pixels=extent.width*extent.height;std::array<rhi::GpuBuffer,3> buffers{};
    try{
        buffers[0]=host.allocator.createBuffer(pixels*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        buffers[1]=host.allocator.createBuffer(pixels*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        buffers[2]=host.allocator.createBuffer(pixels*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        const VkImage images[]={scene.color.image,output.colorImage,output.confidenceImage};
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){for(unsigned i=0;i<3;++i){
            commands.imageBarrier({images[i],VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
            commands.copyImageToBuffer(images[i],buffers[i].buffer,extent);
            commands.imageBarrier({images[i],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
            commands.bufferBarrier({buffers[i].buffer,0,buffers[i].size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});}});
        for(auto& buffer:buffers)host.allocator.invalidate(buffer,0,buffer.size);
        const auto* hdr=static_cast<const std::uint16_t*>(buffers[0].mapped);const auto* reflection=static_cast<const float*>(buffers[1].mapped);const auto* confidence=static_cast<const float*>(buffers[2].mapped);
        const std::array<std::uint16_t,4> expectedHdr{0x3400,0x3000,0x2c00,0x3c00};const std::array<float,4> expected{.25F,.125F,.0625F,1};
        for(unsigned i=0;i<pixels;++i){for(unsigned c=0;c<4;++c){require(hdr[i*4+c]==expectedHdr[c],"Temporal resolve changed the constant scene HDR signal");require(std::isfinite(reflection[i*4+c])&&std::abs(reflection[i*4+c]-expected[c])<2e-6F,"Shared-motion reflection colour differs");}require(confidence[i]==1,"Shared surface confidence changed");}
        for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);
    }catch(...){for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);throw;}
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Expected shader directory");test::VulkanComputeFixture host(true);const std::string directory=argv[1];
    {
        SceneImages scene(host);
        {
            MotionVectorTarget shared(*host.backend,host.allocator,extent,VK_FORMAT_D32_SFLOAT,{scene.depthView});
            const ReflectionPyramidShaders signalShaders{readBinaryFile(directory+"/reflection_signal_pyramid_packed.comp.spv"),readBinaryFile(directory+"/reflection_signal_pyramid_float.comp.spv")};
            const ReflectionTemporalShaders reflectionShaders{readBinaryFile(directory+"/reflection_temporal_packed.comp.spv"),readBinaryFile(directory+"/reflection_temporal_float.comp.spv")};
            ReflectionSignalPyramidGpu signals(*host.backend,extent,1,2,ReflectionPyramidPrecision::Float32,signalShaders);
            ReflectionTemporalGpu reflection(*host.backend,extent,2,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32_SFLOAT,reflectionShaders);
            {
                TemporalReconstruction temporal(*host.backend,host.allocator,extent,VK_FORMAT_D32_SFLOAT,{{scene.color.image,scene.colorView,scene.depth.image,scene.depthView}},directory,&shared);
                require(&temporal.motion()==&shared&&host.allocator.statistics().liveImages==13,"The two consumers duplicated their motion owner");
                TemporalViewState camera;TemporalFrameInfo info;info.width=extent.width;info.height=extent.height;info.sceneGeneration=1;info.projection=info.viewProjection=motionIdentityMatrix();
                for(unsigned frame=0;frame<2;++frame){temporal.prepare(camera.prepare(info),frame,0);require(temporal.preparedHistoryValid()==(frame!=0),"Shared history submission did not advance");
                    RenderGraph graph;auto motion=temporal.importMotion(graph,"shared");rhi::ImageBarrierDesc initial{};initial.image=scene.color.image;initial.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    initial.dstStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;initial.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;const auto color=graph.importImage("scene-color",initial);
                    initial.image=scene.depth.image;initial.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;initial.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;const auto depth=graph.importImage("scene-depth",initial);
                    rhi::RenderPassBeginDesc pass;pass.renderPass=shared.renderPass();pass.framebuffer=shared.framebuffer(0);pass.extent=extent;VkClearValue vectors{},surface{};surface.color.float32[0]=1;pass.clearValues={vectors,surface};
                    const auto producer=graph.addGraphicsPass("motion-producer",pass,[](rhi::ICommandRecorder&){});
                    for(auto resource:motion)graph.attachment(producer,resource,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    graph.use(producer,depth,RenderGraphUsage::DepthReadAttachment,false);
                    const auto consumer=graph.addCommandPass("reflection-consumer",[&,frame](rhi::ICommandRecorder& commands){signals.beginBaseWrite(commands,frame);
                        ReflectionTemporalParameters parameters;parameters.historyValid=true;parameters.varianceScale=0;
                        reflection.record(commands,frame,{scene.colorView,shared.surfaceDepthView(),scene.colorView,shared.surfaceDepthView(),shared.view(),shared.surfaceDepthView(),extent.width,extent.height},signals.base(frame),parameters);
                        signals.record(commands,frame,shared.surfaceDepthView(),1,{});});
                    for(auto resource:{color,motion[0],motion[1]})graph.use(consumer,resource,RenderGraphUsage::ComputeSampled,false);
                    temporal.schedule(graph,color,depth,motion,"shared");std::string error;require(graph.compile(error),"Shared motion graph failed compilation");
                    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
                    require(temporal.commit(frame+1)&&camera.commit(frame+1),"Shared consumers did not commit actual submissions");
                }
                verify(host,scene,signals.base(1));
            }
            require(host.allocator.statistics().liveImages==8&&shared.compatible(*host.backend,extent,VK_FORMAT_D32_SFLOAT,{scene.depthView}),"Temporal destruction released a borrowed image");
        }
        require(host.allocator.statistics().liveImages==2,"Shared owners did not release their outputs");
    }
    require(host.errors==0&&!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"Shared motion validation or retirement failed");
    std::cout<<"PASS: two submitted frames, shared TAA/reflection motion, exact HDR, validation=0 resources=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
