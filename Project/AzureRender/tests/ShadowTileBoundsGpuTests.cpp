#include "support/VulkanComputeFixture.hpp"
#include "render/ShadowTileBounds.hpp"
#include "resources/BinaryFile.hpp"
#include <cstring>
#include <cmath>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected production shadow bounds shader");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    auto image=host.allocator.createImage2D(32,32,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    auto staging=host.allocator.createBuffer(32*32*sizeof(float),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    auto view=backend.createImageView(image.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    rhi::SamplerDesc settings;settings.filter=VK_FILTER_NEAREST;auto sampler=backend.createSampler(settings);
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){backend.destroySampler(sampler);backend.destroyImageView(view);host.allocator.destroyBuffer(staging);host.allocator.destroyImage(image);});
    float values[1024];for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x)values[y*32+x]=y<16?(x<16?.9F:.2F):1.F;
    values[7*32+9]=.1F; // One texel must survive conservative reduction.
    std::memcpy(staging.mapped,values,sizeof(values));host.allocator.flush(staging,0,staging.size);
    ShadowTileBounds bounds(backend);bounds.initialize(readBinaryFile(argv[1]),view,sampler,32);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
        commands.bufferBarrier({staging.buffer,0,staging.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
        commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.imageExtent={32,32,1};vkCmdCopyBufferToImage(native,staging.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
        bounds.record(commands);
        commands.bufferBarrier({bounds.buffer().buffer,0,bounds.buffer().size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(bounds.buffer(),0,bounds.buffer().size);
    const auto actual=static_cast<const float*>(bounds.buffer().mapped);
    const float expected[]={.9F,.9F,.1F,.9F,.2F,.2F,.2F,.2F,.9F,.9F,.9F,.9F,.2F,.2F,.2F,.2F,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    for(unsigned i=0;i<32;++i)if(std::abs(actual[i]-expected[i])>1e-6F)throw std::runtime_error("Depth bounds lose a thin blocker, cross a tile boundary or exclude visible texels");
    if(host.errors.load())throw std::runtime_error("Vulkan validation errors");
    std::cout<<"Conservative production depth bounds preserve a single-texel blocker and tile boundaries\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
