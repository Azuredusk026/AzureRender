#include "RenderViewOutput.hpp"
#include <fstream>
#include <stdexcept>
namespace azurerender {
namespace {
std::vector<char> load(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("View resolve shader unavailable: "+path.u8string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
}
RenderViewOutput::RenderViewOutput(const RenderContext& context,VkImageView hdr,VkSampler sampler,ViewColorTransfer transfer)
    :context_(context),transfer_(transfer){
    VkShaderModule vertex=VK_NULL_HANDLE,fragment=VK_NULL_HANDLE;
    auto& rhi=*context.rhi;
    try{
        const auto extent=context.swapchainExtent;
        const auto binding=context.renderSettings?context.renderSettings->grade.lut:ColorGradingLutSettings{};
        const auto path=binding.resolvedAsset.empty()?std::filesystem::u8path(binding.asset):binding.resolvedAsset;
        lut_=std::make_unique<ColorGradingLutGpu>(rhi,binding.asset.empty()?makeIdentityColorGradingLut():loadColorGradingLut(path),
            binding.asset.empty()?ColorGradingLutUpload::IdentityPlaceholder:ColorGradingLutUpload::Float32);
        image_=context.allocator->createImage2D(extent.width,extent.height,VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        view_=rhi.createImageView(image_.image,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT,1);
        rhi::RenderPassDesc pass;pass.attachments={{VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,false,true,false}};
        pass.externalReadDependency=true;pass_=rhi.createRenderPass(pass);
        framebuffer_=rhi.createFramebuffer({pass_,{view_},extent.width,extent.height});
        spatial_=std::make_unique<SpatialAntiAliasing>(rhi,context.renderExtent,context.shaderDirectory,pass_);
        setLayout_=rhi.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT}});
        pool_=rhi.createDescriptorPool({{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2}},1});
        set_=rhi.allocateDescriptorSets(pool_,setLayout_,1).at(0);
        rhi.writeDescriptorImage({set_,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,hdr,sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        rhi.writeDescriptorImage({set_,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,lut_->view(),lut_->sampler(),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        const rhi::PushConstantRangeDesc push{VK_SHADER_STAGE_FRAGMENT_BIT,48};layout_=rhi.createPipelineLayout(setLayout_,&push);
        vertex=rhi.createShaderModule(load(std::filesystem::u8path(context.shaderDirectory)/"blackhole.vert.spv"));
        fragment=rhi.createShaderModule(load(std::filesystem::u8path(context.shaderDirectory)/"view_resolve.frag.spv"));
        rhi::GraphicsPipelineDesc pipeline;pipeline.vertexShader=vertex;pipeline.fragmentShader=fragment;
        pipeline.cullMode=VK_CULL_MODE_NONE;pipeline.depthTest=false;pipeline.depthWrite=false;
        pipeline.colorAttachmentCount=1;pipeline.renderPass=spatial_->inputPass();pipeline.layout=layout_;
        pipeline_=rhi.createGraphicsPipeline(pipeline);
        rhi.destroyShaderModule(vertex);vertex=VK_NULL_HANDLE;rhi.destroyShaderModule(fragment);fragment=VK_NULL_HANDLE;
    }catch(...){if(vertex)rhi.destroyShaderModule(vertex);if(fragment)rhi.destroyShaderModule(fragment);destroy();throw;}
}
RenderViewOutput::~RenderViewOutput(){destroy();}
void RenderViewOutput::destroy() noexcept{
    auto& rhi=*context_.rhi;
    if(pipeline_)rhi.destroyPipeline(pipeline_);
    spatial_.reset();
    if(layout_)rhi.destroyPipelineLayout(layout_);
    if(pool_)rhi.destroyDescriptorPool(pool_);
    lut_.reset();
    if(setLayout_)rhi.destroyDescriptorSetLayout(setLayout_);
    if(framebuffer_)rhi.destroyFramebuffer(framebuffer_);
    if(pass_)rhi.destroyRenderPass(pass_);
    if(view_)rhi.destroyImageView(view_);
    if(image_.image)context_.allocator->destroyImage(image_);
}
void RenderViewOutput::schedule(RenderGraph& graph,RenderGraph::ResourceId hdr,const std::string& name,bool temporalReady){
    rhi::ImageBarrierDesc initial{};initial.image=image_.image;
    const auto output=graph.importImage(name+"-ldr",initial);
    initial.image=spatial_->inputImage().image;
    const auto linear=graph.importImage(name+"-linear-display",initial);
    const auto gradePass=graph.addCommandPass(name+"-grade",[this](rhi::ICommandRecorder& commands){
        rhi::RenderPassBeginDesc begin{spatial_->inputPass(),spatial_->inputFramebuffer(),context_.renderExtent,{}};
        commands.beginRenderPass(begin);commands.setViewport(float(context_.renderExtent.width),float(context_.renderExtent.height));
        commands.setScissor(context_.renderExtent);commands.bindPipeline(pipeline_);commands.bindDescriptorSet(layout_,set_);
        const auto grade=context_.renderSettings?context_.renderSettings->grade:GradeSettings{};
        const std::array<float,12> transfer{grade.exposureEv,grade.saturation,grade.contrast,grade.toneMappingEnabled?1.F:0.F,
            grade.tint[0],grade.tint[1],grade.tint[2],0,
            grade.lut.asset.empty()?0.F:static_cast<float>(lut_->dimension()),grade.lut.exposureMultiplier,static_cast<float>(lut_->inputDomain()),0};
        commands.pushConstants(layout_,VK_SHADER_STAGE_FRAGMENT_BIT,0,transfer.data(),48);commands.draw(3);commands.endRenderPass();
    });
    graph.use(gradePass,hdr,RenderGraphUsage::Sampled,false);
    graph.attachment(gradePass,linear,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto resolvePass=graph.addCommandPass(name+"-resolve",[this,temporalReady](rhi::ICommandRecorder& commands){
        commands.beginRenderPass({pass_,framebuffer_,context_.swapchainExtent,{}});
        commands.setViewport(float(image_.width),float(image_.height));commands.setScissor(context_.swapchainExtent);
        spatial_->resolve(commands,spatialAntiAliasingMode(context_.renderSettings?context_.renderSettings->antiAliasing:1,temporalReady,context_.rasterizationSamples==VK_SAMPLE_COUNT_4_BIT),
            transfer_==ViewColorTransfer::SrgbEncoded,context_.renderSettings&&context_.renderSettings->diagnosticView!=0);
        commands.endRenderPass();
    });
    graph.use(resolvePass,linear,RenderGraphUsage::Sampled,false);
    graph.attachment(resolvePass,output,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
}
