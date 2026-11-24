#include "support/VulkanComputeFixture.hpp"
#include "render/ShadowTileBounds.hpp"
#include "resources/BinaryFile.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=3)throw std::invalid_argument("Expected shader directory and optional --timing");
    const bool timing=argc==3&&std::string(argv[2])=="--timing",tailCase=argc==3&&std::string(argv[2])=="--tail";
    if(argc==3&&!timing&&!tailCase)throw std::invalid_argument("Unknown batch fixture option");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    if(timing&&!host.timestampBits)return 77;
    const unsigned side=timing?4096:tailCase?40:64;
    auto image=host.allocator.createImage2D(side,side,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    auto staging=host.allocator.createBuffer(side*side*sizeof(float),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    auto view=backend.createImageView(image.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    rhi::SamplerDesc settings;settings.filter=VK_FILTER_NEAREST;auto sampler=backend.createSampler(settings);
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){backend.destroySampler(sampler);backend.destroyImageView(view);host.allocator.destroyBuffer(staging);host.allocator.destroyImage(image);});
    std::vector<float> values(side*side);
    for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x)values[y*side+x]=x<8&&y<8?1.F:float((x*71U+y*29U+(x^y)*13U)%1025U)/1024.F;
    values[7*side+7]=0.F;
    std::vector<float> expected;
    for(unsigned ty=0;ty<side/8;++ty)for(unsigned tx=0;tx<side/8;++tx){float low=1,high=0;
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){const auto v=values[(ty*8+y)*side+tx*8+x];low=std::min(low,v);high=std::max(high,v);}expected.insert(expected.end(),{low,high});}
    std::memcpy(staging.mapped,values.data(),staging.size);host.allocator.flush(staging,0,staging.size);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
        commands.bufferBarrier({staging.buffer,0,staging.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
        commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.imageExtent={side,side,1};vkCmdCopyBufferToImage(native,staging.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    });
    constexpr unsigned warmup=2048,samples=128,iterations=warmup+samples;
    VkQueryPool timingPool=VK_NULL_HANDLE;
    auto timingCleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){if(timingPool)vkDestroyQueryPool(host.device,timingPool,nullptr);});
    if(timing){VkQueryPoolCreateInfo desc{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};desc.queryType=VK_QUERY_TYPE_TIMESTAMP;desc.queryCount=2*iterations;test::vkRequire(vkCreateQueryPool(host.device,&desc,nullptr,&timingPool),"Create batch timing pool");}
    nlohmann::json timings=nlohmann::json::array();
    for(const unsigned mode:timing?std::vector<unsigned>{0,1,1,0}:std::vector<unsigned>{0,1,0}){
        ShadowTileBounds bounds(backend);const auto path=std::string(argv[1])+(mode?"/shadow_bounds_batch.comp.spv":"/shadow_bounds.comp.spv");
        bounds.initialize(readBinaryFile(path),view,sampler,side,1,mode?8U:1U);
        const auto& buffer=bounds.buffer();const auto tailOffset=bounds.objectRecordsOffset();std::vector<unsigned char> tail(buffer.size-tailOffset);
        std::memcpy(tail.data(),static_cast<const unsigned char*>(buffer.mapped)+tailOffset,tail.size());
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
            commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
            if(timing)vkCmdResetQueryPool(native,timingPool,0,2*iterations);
            for(unsigned i=0;i<(timing?iterations:1U);++i){
                if(i)commands.bufferBarrier({buffer.buffer,0,bounds.depthBoundsBytes(),VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
                if(timing)commands.writeTimestamp(timingPool,2*i,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
                bounds.record(commands);
                if(timing)commands.writeTimestamp(timingPool,2*i+1,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
            }
            commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        if(timing){
            std::array<std::uint64_t,2*iterations> ticks{};
            test::vkRequire(vkGetQueryPoolResults(host.device,timingPool,0,2*iterations,sizeof(ticks),ticks.data(),sizeof(std::uint64_t),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"Read batch timing");
            const auto mask=host.timestampBits>=64?std::numeric_limits<std::uint64_t>::max():(std::uint64_t{1}<<host.timestampBits)-1;
            std::vector<double> milliseconds;double total=0;
            for(unsigned i=warmup;i<iterations;++i){const double ms=double((ticks[2*i+1]-ticks[2*i])&mask)*host.properties.limits.timestampPeriod*1.e-6;milliseconds.push_back(ms);total+=ms;}
            timings.push_back({{"mode",mode},{"warmup",warmup},{"samples",samples},{"meanMs",total/samples},{"milliseconds",milliseconds}});
        }
        host.allocator.invalidate(buffer,0,buffer.size);
        if(std::memcmp(buffer.mapped,expected.data(),expected.size()*sizeof(float))!=0)throw std::runtime_error("Single-tile and batched bounds differ from exact depth extrema");
        if(std::memcmp(static_cast<const unsigned char*>(buffer.mapped)+tailOffset,tail.data(),tail.size())!=0)throw std::runtime_error("Reduction overwrote the adjacent object records");
    }
    if(timing)std::cout<<nlohmann::json{{"extent",side},{"timings",timings}}.dump()<<'\n';
    timingCleanup.reset();
    cleanup.reset();
    if(host.errors||host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)throw std::runtime_error("Batch validation or release failed");
    std::cout<<"Shadow extrema: finite depth tiles, isolated texel, batch and tail-safe groups exact; object tail retained\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
