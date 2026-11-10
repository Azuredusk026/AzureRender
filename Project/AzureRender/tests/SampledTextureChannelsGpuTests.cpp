#include "support/VulkanComputeFixture.hpp"
#include "render/SampledTextureCache.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cstring>
#include <iostream>

using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=2)throw std::invalid_argument("Expected texture channel probe shader");
    test::VulkanComputeFixture host(false,false,false,false,false,false,true,true);
    auto& r=*host.backend;
    rhi::GpuBuffer output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)r.destroyPipeline(pipeline);if(shader)r.destroyShaderModule(shader);
        if(layout)r.destroyPipelineLayout(layout);if(pool)r.destroyDescriptorPool(pool);
        if(setLayout)r.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);
    });
    constexpr unsigned samples=4096;
    output=host.allocator.createBuffer(samples*4*sizeof(std::uint32_t),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    setLayout=r.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    pool=r.createDescriptorPool(desc);const auto set=r.allocateDescriptorSets(pool,setLayout,1).front();
    r.writeDescriptorBuffer({set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    layout=r.createPipelineLayout(setLayout,nullptr);shader=r.createShaderModule(readBinaryFile(argv[1]));
    pipeline=r.createComputePipeline({shader,layout});
    unsigned cases=0,compacted=0;
    for(const auto size:std::array<std::array<unsigned,2>,3>{{{4,4},{3,1},{7,5}}})
    for(unsigned pattern=0;pattern<9;++pattern)for(unsigned filter=0;filter<3;++filter)
    for(auto address:{VK_SAMPLER_ADDRESS_MODE_REPEAT,VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE})
    for(unsigned anisotropy:{1U,8U}) {
        SampledTextureUpload u;u.width=size[0];u.height=size[1];u.mipLevels=0;
        std::vector<std::uint8_t> pixels;
        for(unsigned w=u.width,h=u.height;;w=std::max(1U,w/2),h=std::max(1U,h/2)) {
            const auto level=u.mipLevels++;
            for(unsigned i=0;i<w*h;++i) {
                const auto v=std::uint8_t((i*43+level*61)%256),q=std::uint8_t((i*97+level*13+29)%256);
                std::array<std::uint8_t,4> p{v,v,v,255};
                if(pattern==1)p={255,v,q,255};
                if(pattern==2)p={0,0,0,v};
                if(pattern==3)p={v,v,q,255};
                if(pattern==4)p={0,0,0,255};
                if(pattern==5)p={v,q,std::uint8_t(v^q),255};
                if(pattern==6&&level)p={v,q,std::uint8_t(v^q),255};
                if(pattern==7)p={v,q,0,v};
                if(pattern==8)p={255,v,255,255};
                pixels.insert(pixels.end(),p.begin(),p.end());
            }
            if(w==1&&h==1)break;
        }
        u.sampler.filter=filter?VK_FILTER_LINEAR:VK_FILTER_NEAREST;
        if(filter==2)u.sampler.magFilter=VK_FILTER_NEAREST;
        u.sampler.mipmapLinear=filter!=0;u.sampler.maxLod=float(u.mipLevels-1);
        u.sampler.addressU=u.sampler.addressV=u.sampler.addressW=address;
        u.sampler.maxAnisotropy=float(anisotropy);u.sampler.mipLodBias=filter==2?-.375F:0;
        SampledTextureCache original(host.allocator,r,false),compact(host.allocator,r,true);
        const auto a=original.acquire(pixels,u),b=compact.acquire(pixels,u);
        if(pattern!=5&&pattern!=6&&compact.statistics().compactedTextures!=1)
            throw std::runtime_error("Expected linear channel case must exercise compact representation");
        compacted+=unsigned(compact.statistics().compactedTextures);
        r.writeDescriptorImage({set,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,a.view,a.sampler});
        r.writeDescriptorImage({set,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,b.view,b.sampler});
        std::memset(output.mapped,0xCD,output.size);host.allocator.flush(output,0,output.size);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(samples/64,1,1);
            commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const std::uint32_t*>(output.mapped);
        for(unsigned i=0;i<samples*4;++i)if(values[i])throw std::runtime_error("Channel sample mismatch: case="+std::to_string(cases)+" component="+std::to_string(i));
        original.seal();compact.seal();++cases;
    }
    cleanup.reset();
    if(host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)throw std::runtime_error("GPU allocations remain live");
    const std::string device=host.properties.deviceName;
    host.close();if(host.errors.load())throw std::runtime_error("Vulkan validation warning or error");
    std::cout<<"PASS "<<device<<" cases="<<cases<<" compacted="<<compacted<<" bitComparisons="<<std::uint64_t(cases)*samples*4<<" validation=0 liveAllocations=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
