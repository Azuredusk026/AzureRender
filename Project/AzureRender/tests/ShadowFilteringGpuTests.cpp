#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=2)throw std::invalid_argument("Expected shadow comparison shader");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    rhi::GpuBuffer staging{};rhi::GpuImage depth{};VkImageView depthView{};VkSampler depthSampler{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);
        if(depthSampler)backend.destroySampler(depthSampler);if(depthView)backend.destroyImageView(depthView);
        host.allocator.destroyImage(depth);host.allocator.destroyBuffer(staging);
    });
    constexpr unsigned count=16+4*33+4+10+32+3+12+3*17*4;
    output=host.allocator.createBuffer(count*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memset(output.mapped,0xCD,output.size);host.allocator.flush(output,0,output.size);
    const float depths[]={.1F,.2F,.3F,.4F};
    staging=host.allocator.createBuffer(sizeof(depths),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    std::memcpy(staging.mapped,depths,sizeof(depths));host.allocator.flush(staging,0,sizeof(depths));
    depth=host.allocator.createImage2D(2,2,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    depthView=backend.createImageView(depth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    rhi::SamplerDesc sampler;sampler.filter=VK_FILTER_NEAREST;
    depthSampler=backend.createSampler(sampler);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer command){
        commands.bufferBarrier({staging.buffer,0,staging.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
        commands.imageBarrier({depth.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.imageExtent={2,2,1};
        vkCmdCopyBufferToImage(command,staging.buffer,depth.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        commands.imageBarrier({depth.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    });
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    backend.writeDescriptorImage({set,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,depthView,depthSampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(count,1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto values=static_cast<const float*>(output.mapped);
    for(unsigned i=0;i<15;++i)if(std::abs(values[i]-float(i%5)/4)>1e-6F)throw std::runtime_error("Depth step must preserve fractional shadow visibility");
    if(std::abs(values[15]-1.F)>1e-6F)throw std::runtime_error("An unoccluded sloped receiver must remain fully visible across the filter footprint");
    double maximumError=0,maximumStep=0;
    for(unsigned direction=0;direction<4;++direction)for(unsigned sample=0;sample<33;++sample){
        const double x=double(sample)/16-1;
        const double reference=.5+(std::asin(x)+x*std::sqrt(std::max(1-x*x,0.0)))/3.141592653589793;
        const float actual=values[16+direction*33+sample];
        maximumError=std::max(maximumError,std::abs(actual-reference));
        if(sample)maximumStep=std::max(maximumStep,double(std::abs(actual-values[16+direction*33+sample-1])));
    }
    std::cout<<"Wide penumbra maximum disk error="<<maximumError<<", adjacent step="<<maximumStep<<'\n';
    if(maximumError>.065 || maximumStep>.09)throw std::runtime_error("Wide shadow footprint forms directional bands rather than a continuous penumbra");
    for(unsigned index=0;index<2;++index){
        const float texel=index==0?.01F:.02F;
        if(values[148+index]<texel*1.5F || values[148+index]>texel*2.1F)
            throw std::runtime_error("Grazing receiver bias must account for the world-space shadow texel footprint");
    }
    if(std::abs(values[150]-1.F)>1e-6F || values[151]>.01F)
        throw std::runtime_error("Receiver-plane extrapolation must stay on flat surfaces and fade across curved footprints");
    for(unsigned sample=0;sample<5;++sample){
        const float fraction=1.F-float(sample)/4;
        if(std::abs(values[152+sample]-fraction)>1e-6F || std::abs(values[157+sample]-.1F*fraction)>1e-6F)
            throw std::runtime_error("Blocker mass and depth must vary continuously across a shadow texel boundary");
    }
    for(unsigned sample=0;sample<32;++sample)
        if(std::abs(values[162+sample]-1.F/(3.141592653589793F*64))>.001F)
            throw std::runtime_error("A thin blocker must retain its area contribution as the wide footprint moves");
    const float contactRadii[]={3.F,6.F,3.F};
    for(unsigned sample=0;sample<3;++sample)
        if(std::abs(values[194+sample]-contactRadii[sample])>1e-6F)
            throw std::runtime_error("Contact softness must preserve its world footprint when shadow resolution increases and respect the filter limit");
    const float quadDepths[]={.1F,.2F,.3F,.4F,.1F,.1F,.3F,.3F,.1F,.1F,.1F,.1F};
    for(unsigned sample=0;sample<12;++sample)
        if(std::abs(values[197+sample]-quadDepths[sample])>1e-6F)
            throw std::runtime_error("Depth gather must preserve row-major texel order and stay inside the selected atlas tile");
    for(unsigned probe=0;probe<51;++probe)for(unsigned corner=0;corner<4;++corner){
        const double radius=probe/17==0?2.0:(probe/17==1?6.0:8.0);
        const double x=(double(probe%17)/16*2-1)*(radius+1)+corner%2;
        const double y=.37+corner/2;
        const double expected=std::clamp(radius+.5-std::sqrt(x*x+y*y),0.0,1.0);
        if(std::abs(values[209+probe*4+corner]-expected)>1e-5)
            throw std::runtime_error("Quad integration weights must match the analytic circular footprint");
    }
    if(host.errors.load())throw std::runtime_error("Vulkan validation errors");
    std::cout<<"15 GPU depth-step samples passed at three receiver depths\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
