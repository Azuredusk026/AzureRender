#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "assets/GltfLoader.hpp"
#include "render/MaterialSampling.hpp"
#include <array>
#include <cstring>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shader directory");
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
    rhi::GpuImage image{};rhi::GpuBuffer output{},readback{},features{};
    VkImageView view{};VkRenderPass pass{};VkFramebuffer framebuffer{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};
    VkShaderModule vertex{},fragment{},compute{};VkPipeline graphics{},dispatch{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        for(auto p:{graphics,dispatch})if(p)backend.destroyPipeline(p);
        for(auto s:{vertex,fragment,compute})if(s)backend.destroyShaderModule(s);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);if(framebuffer)backend.destroyFramebuffer(framebuffer);
        if(pass)backend.destroyRenderPass(pass);if(view)backend.destroyImageView(view);
        host.allocator.destroyImage(image);host.allocator.destroyBuffer(output);host.allocator.destroyBuffer(readback);host.allocator.destroyBuffer(features);
    });
    image=host.allocator.createImage2D(4,4,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    view=backend.createImageView(image.image,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
    readback=host.allocator.createBuffer(16*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    output=host.allocator.createBuffer(16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    constexpr auto neutral=std::uint32_t{MaterialFeatureNeutralFallback};
    constexpr auto emission=std::uint32_t{MaterialFeatureEmissiveMask};
    constexpr auto temporal=kTemporalMaterialSamples,coverage=kTemporalAlphaCoverage;
    const std::array<std::uint32_t,12> inputFeatures={0,neutral,0xffffffffU,temporal|neutral,
        coverage|neutral,temporal|coverage|neutral,0x3f000020U,0x80002000U,
        emission,emission|neutral,emission|temporal,emission|coverage|neutral};
    const std::array<std::uint32_t,12> foldedFeatures={0,neutral,temporal|coverage|neutral|emission,
        temporal|neutral,coverage|neutral,temporal|coverage|neutral,neutral,temporal,
        emission,emission|neutral,emission|temporal,emission|coverage|neutral};
    features=host.allocator.createBuffer(inputFeatures.size()*sizeof(std::uint32_t),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    rhi::RenderPassDesc render;
    render.attachments={{VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,true,true,false}};
    pass=backend.createRenderPass(render);framebuffer=backend.createFramebuffer({pass,{view},4,4});
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc poolDesc;poolDesc.maxSets=1;poolDesc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
    pool=backend.createDescriptorPool(poolDesc);auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,features.buffer,features.size});
    layout=backend.createPipelineLayout(setLayout,nullptr);
    const auto shader=[&](const char* suffix){return backend.createShaderModule(readBinaryFile(std::string(argv[1])+"/specialization_test."+suffix+".spv"));};
    vertex=shader("vert");fragment=shader("frag");compute=shader("comp");
    for(bool specialized:{false,true}){
        std::memcpy(features.mapped,inputFeatures.data(),features.size);host.allocator.flush(features,0,features.size);
        rhi::ShaderSpecialization values;if(specialized)values={{15,0xbf400000U},{0,VK_TRUE},{7,123}};
        dispatch=backend.createComputePipeline({compute,layout,values});
        rhi::GraphicsPipelineDesc desc;desc.vertexShader=vertex;desc.fragmentShader=fragment;desc.layout=layout;
        desc.renderPass=pass;desc.colorAttachmentCount=1;desc.depthTest=false;desc.depthWrite=false;desc.cullMode=VK_CULL_MODE_NONE;
        desc.fragmentSpecialization=values;if(specialized)desc.vertexSpecialization={{0,0x3f400000U}};
        graphics=backend.createGraphicsPipeline(desc);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
            commands.bufferBarrier({features.buffer,0,features.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
            commands.bindComputePipeline(dispatch);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(1,1,1);
            commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            commands.bufferBarrier({features.buffer,0,features.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            rhi::RenderPassBeginDesc begin;begin.renderPass=pass;begin.framebuffer=framebuffer;begin.extent={4,4};begin.clearValues.resize(1);
            commands.beginRenderPass(begin);commands.setViewport(4,4);commands.setScissor({4,4});commands.bindPipeline(graphics);commands.draw(3);commands.endRenderPass();
            commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_IMAGE_ASPECT_COLOR_BIT});
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={4,4,1};
            vkCmdCopyImageToBuffer(raw,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
            commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(output,0,output.size);host.allocator.invalidate(readback,0,readback.size);
        host.allocator.invalidate(features,0,features.size);
        const auto& expectedFeatures=specialized?foldedFeatures:inputFeatures;
        if(std::memcmp(features.mapped,expectedFeatures.data(),features.size)!=0)
            throw std::runtime_error("Material specialization must retain view bits and full-path unknown inputs");
        const std::array<float,4> expected=specialized?std::array<float,4>{1,123,-.75F,.75F}:std::array<float,4>{0,3,.5F,.25F};
        const auto* actual=static_cast<const float*>(readback.mapped);const auto* computed=static_cast<const float*>(output.mapped);
        for(unsigned c=0;c<4;++c){
            if(computed[c]!=(c==3?.25F:expected[c]))throw std::runtime_error("Compute specialization mismatch");
            for(unsigned p=0;p<16;++p)if(actual[p*4+c]!=expected[c])throw std::runtime_error("Graphics stage specialization mismatch");
        }
        backend.destroyPipeline(graphics);graphics={};backend.destroyPipeline(dispatch);dispatch={};
    }
    if(host.errors.load())throw std::runtime_error("Shader specialization validation errors");
    std::cout<<"Shader specialization: defaults, bool/uint/float and stage isolation passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
