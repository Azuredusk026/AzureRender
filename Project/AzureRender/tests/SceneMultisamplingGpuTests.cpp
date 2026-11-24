#include "support/VulkanComputeFixture.hpp"
#include "render/SceneMultisampling.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
using namespace azurerender;
namespace {
float halfFloat(std::uint16_t value){unsigned exponent=(value>>10)&31;float fraction=float(value&1023)/1024;return (value&32768?-1.F:1.F)*(exponent?std::ldexp(1+fraction,int(exponent)-15):std::ldexp(fraction,-14));}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shaders");test::VulkanComputeFixture host(true,true);auto& backend=*host.backend;
    if(!backend.supportsSampleShading()||!host.properties.limits.standardSampleLocations)return 77;
    std::cout<<"Scene multisampling device: "<<host.properties.deviceName<<'\n';unsigned checked=0;
    for(unsigned minimum=0;minimum<3;++minimum){
        std::array<VkFormat,3> formats{VK_FORMAT_R16G16B16A16_SFLOAT,VK_FORMAT_D32_SFLOAT,minimum==2?VK_FORMAT_R32G32B32A32_SFLOAT:minimum?VK_FORMAT_R32_SFLOAT:VK_FORMAT_R8G8B8A8_UNORM};
        for(unsigned i=0;i<3;++i)if(!(backend.imageSampleCounts(formats[i],VK_IMAGE_USAGE_SAMPLED_BIT|(i==1?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))&VK_SAMPLE_COUNT_4_BIT))return 77;
        std::array<rhi::GpuImage,6> images{};std::array<rhi::GpuBuffer,3> buffers{};std::vector<SceneMultisampleDestination> destinations(2);
        VkDescriptorSetLayout emptySet{};VkShaderModule vertex{},fragment{};VkPipelineLayout layout{};VkPipeline pipeline{};std::unique_ptr<SceneMultisampling> target;
        auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
            if(pipeline)backend.destroyPipeline(pipeline);if(layout)backend.destroyPipelineLayout(layout);
            if(emptySet)backend.destroyDescriptorSetLayout(emptySet);if(fragment)backend.destroyShaderModule(fragment);if(vertex)backend.destroyShaderModule(vertex);target.reset();
            for(const auto& destination:destinations)for(auto view:destination.views)if(view)backend.destroyImageView(view);
            for(auto& image:images)if(image.image)host.allocator.destroyImage(image);for(auto& buffer:buffers)if(buffer.buffer)host.allocator.destroyBuffer(buffer);
        });
        for(unsigned d=0;d<2;++d)for(unsigned i=0;i<3;++i){
            images[d*3+i]=host.allocator.createImage2D(32,32,formats[i],VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|(i==1?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
            destinations[d].images[i]=images[d*3+i].image;destinations[d].views[i]=backend.createImageView(images[d*3+i].image,formats[i],i==1?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,1);
        }
        for(unsigned i=0;i<3;++i)buffers[i]=host.allocator.createBuffer(32*32*(i==0?8:i==2&&minimum==2?16:4),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        target=std::make_unique<SceneMultisampling>(backend,host.allocator,VkExtent2D{32,32},formats[1],formats[2],minimum==2?SceneSecondaryResolve::MinimumDepthSample:minimum?SceneSecondaryResolve::Minimum:SceneSecondaryResolve::Average,2,destinations,argv[1]);
        require(target->byteSize()==32*32*(minimum==2?112ULL:64ULL)*2,"Source budget must account for the complete secondary format");
        require(target->sourceView(0,2)!=VK_NULL_HANDLE&&target->sourceView(1,2)!=target->sourceView(0,2),"Source views belong to distinct in-flight slots");
        try{target->sourceView(2,0);throw std::runtime_error("Invalid source slot accepted");}catch(const std::out_of_range&){}
        try{target->sourceView(0,3);throw std::runtime_error("Invalid source attachment accepted");}catch(const std::out_of_range&){}
        std::cout<<"Source estimated bytes="<<target->byteSize()<<", allocated bytes="<<target->allocatedByteSize()<<" secondaryMinimum="<<minimum<<'\n';
        vertex=backend.createShaderModule(readBinaryFile(std::string(argv[1])+"/multisample_probe.vert.spv"));fragment=backend.createShaderModule(readBinaryFile(std::string(argv[1])+"/scene_multisample_probe.frag.spv"));
        rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_FRAGMENT_BIT,8};emptySet=backend.createDescriptorSetLayout({});layout=backend.createPipelineLayout(emptySet,&push);
        rhi::GraphicsPipelineDesc p;p.vertexShader=vertex;p.fragmentShader=fragment;p.layout=layout;p.renderPass=target->renderPass();p.colorAttachmentCount=2;
        p.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;p.minSampleShading=1;p.cullMode=VK_CULL_MODE_NONE;p.depthCompare=VK_COMPARE_OP_ALWAYS;pipeline=backend.createGraphicsPipeline(p);
        for(unsigned phase=0;phase<2;++phase){
            RenderGraph graph;auto sources=target->importSources(graph,phase,"samples");std::array<RenderGraph::ResourceId,3> outputs{};
            for(unsigned i=0;i<3;++i){rhi::ImageBarrierDesc initial;initial.image=destinations[phase].images[i];if(i==1)initial.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;outputs[i]=graph.importImage("resolved",initial);}
            rhi::RenderPassBeginDesc begin;begin.renderPass=target->renderPass();begin.framebuffer=target->framebuffer(phase);begin.extent={32,32};begin.clearValues.resize(3);begin.clearValues[1].depthStencil={1,0};
            auto producer=graph.addGraphicsPass("scene",begin,[&](rhi::ICommandRecorder& commands){commands.setViewport(32,32);commands.setScissor({32,32});commands.bindPipeline(pipeline);
                const std::array<unsigned,2> values{phase,minimum};commands.pushConstants(layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,values.data(),8);commands.draw(3);});
            for(unsigned i=0;i<3;++i)graph.attachment(producer,sources[i],i==1?RenderGraphUsage::DepthAttachment:RenderGraphUsage::ColorAttachment,i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            target->schedule(graph,phase,phase,sources,outputs,"resolve");std::string error;require(graph.compile(error),error.c_str());
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){graph.execute(&commands);});
        }
        // Read both destinations after both source slots have rendered.
        for(unsigned phase=0;phase<2;++phase){
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){for(unsigned i=0;i<3;++i){
                rhi::ImageBarrierDesc barrier;barrier.image=destinations[phase].images[i];barrier.aspectMask=i==1?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.oldLayout=i==1?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcStageMask=VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(barrier);
                VkBufferImageCopy copy{};copy.imageSubresource={barrier.aspectMask,0,0,1};copy.imageExtent={32,32,1};
                vkCmdCopyImageToBuffer(raw,barrier.image,barrier.newLayout,buffers[i].buffer,1,&copy);
                rhi::BufferBarrierDesc read;read.buffer=buffers[i].buffer;read.size=buffers[i].size;read.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;read.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
                read.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;read.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(read);
            }});
            for(auto& buffer:buffers)host.allocator.invalidate(buffer,0,buffer.size);
            auto hdr=static_cast<const std::uint16_t*>(buffers[0].mapped);auto depth=static_cast<const float*>(buffers[1].mapped);
            auto normal=static_cast<const std::uint8_t*>(buffers[2].mapped);auto linear=static_cast<const float*>(buffers[2].mapped);
            const std::array<float,4> expected{.5F+2*phase,.5F,.375F,1};
            for(unsigned pixel=0;pixel<1024;++pixel){for(unsigned c=0;c<4;++c){require(std::abs(halfFloat(hdr[pixel*4+c])-expected[c])<.002F,"HDR sample mean or source-slot isolation differs");++checked;}
                require(std::abs(depth[pixel]-(.2F+.3F*phase))<1e-6F,"Depth resolve must select nearest covered sample");++checked;
                if(minimum==2){const std::array<float,4> metadata{.3F,4,7,.9F};
                    for(unsigned c=0;c<4;++c){require(std::abs(linear[pixel*4+c]-metadata[c])<1e-6F,"Resolved metadata must come from one minimum-depth sample");++checked;}}
                else if(minimum){require(std::abs(linear[pixel]-.3F)<1e-6F,"Linear secondary depth must resolve minimum");++checked;}
                else for(unsigned c=0;c<4;++c){require(std::abs(int(normal[pixel*4+c])-128)<=1,"Normal attachment must resolve arithmetic mean");++checked;}
            }
        }
        cleanup.reset();require(!host.errors.load()&&!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"Scene target validation or resource retirement failed");
    }
    std::cout<<"Scene multisampling GPU: "<<checked<<" HDR, normal and nearest-depth values; two in-flight source slots, preserved destinations and resource retirement passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
