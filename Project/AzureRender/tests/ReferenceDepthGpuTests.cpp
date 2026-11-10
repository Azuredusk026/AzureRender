#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ReferenceDepth.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=6)throw std::invalid_argument("Expected shader directory, optional profile/positions/uint32 indices/output prefix");
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;const auto shaders=ReferenceDepthShaders::load(argv[1]);
    rhi::GpuBuffer positions{},indices{},readback{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){host.allocator.destroyBuffer(positions);host.allocator.destroyBuffer(indices);host.allocator.destroyBuffer(readback);});
    unsigned rejects=0;const auto rejected=[&](auto operation){try{operation();}catch(const std::invalid_argument&){++rejects;}catch(const std::out_of_range&){++rejects;}};
    for(unsigned kind=0;kind<4;++kind)rejected([&]{ReferenceDepthConfig config;VkExtent2D extent{33,35};unsigned frames=2;
        if(kind==0)config.schemaVersion=2;if(kind==1)config.convention=static_cast<ReferenceDepthConvention>(99);
        if(kind==2)extent.width=0;if(kind==3)frames=9;ReferenceDepthGpu producer(backend,extent,frames,config,shaders);});
    std::size_t checked=0;
    const auto draw=[&](ReferenceDepthGpu& producer,unsigned slot,const ReferenceClipProjectionProfile& profile,const std::vector<std::array<float,4>>& vertices,const std::vector<std::uint32_t>& triangles,bool empty){
        if(positions.buffer)host.allocator.destroyBuffer(positions);positions={};if(indices.buffer)host.allocator.destroyBuffer(indices);indices={};
        positions=host.allocator.createBuffer(vertices.size()*16,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,true);
        indices=host.allocator.createBuffer(triangles.size()*4,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,true);
        std::memcpy(positions.mapped,vertices.data(),vertices.size()*16);std::memcpy(indices.mapped,triangles.data(),triangles.size()*4);
        host.allocator.flush(positions,0,positions.size);host.allocator.flush(indices,0,indices.size);
        const auto output=producer.output(slot);const auto bytes=VkDeviceSize(output.extent.width)*output.extent.height*4;
        if(readback.buffer)host.allocator.destroyBuffer(readback);readback={};readback=host.allocator.createBuffer(bytes*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
            commands.bufferBarrier({positions.buffer,0,positions.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT});
            commands.bufferBarrier({indices.buffer,0,indices.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_INDEX_READ_BIT});
            if(!checked){
                for(unsigned kind=0;kind<5;++kind)rejected([&]{ReferenceDepthGeometry g{positions.buffer,indices.buffer,static_cast<unsigned>(vertices.size()),static_cast<unsigned>(triangles.size())};
                    if(kind==0)g.positions={};if(kind==1)g.indices={};if(kind==2)g.vertexCount=0;if(kind==3)g.indexCount=2;
                    producer.record(commands,kind==4?2:slot,profile,{g});});
                rejected([&]{(void)producer.output(2);});
            }
            producer.record(commands,slot,profile,empty?std::vector<ReferenceDepthGeometry>{}:std::vector<ReferenceDepthGeometry>{{positions.buffer,indices.buffer,static_cast<unsigned>(vertices.size()),static_cast<unsigned>(triangles.size())}});
            for(unsigned layer=0;layer<2;++layer){
                const auto image=layer?output.depthImage:output.image;const auto layout=layer?output.depthLayout:output.layout;
                rhi::ImageBarrierDesc barrier;barrier.image=image;barrier.aspectMask=layer?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.oldLayout=layout;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(barrier);
                VkBufferImageCopy copy{};copy.bufferOffset=layer*bytes;copy.imageSubresource={barrier.aspectMask,0,0,1};copy.imageExtent={output.extent.width,output.extent.height,1};
                vkCmdCopyImageToBuffer(raw,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
                barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=layout;barrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(barrier);
            }
            commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(readback,0,readback.size);
    };
    const std::vector<std::uint32_t> triangles{0,1,2,2,1,3,4,5,6,6,5,7};
    for(bool perspective:{false,true})for(bool reversed:{false,true})for(bool flip:{false,true}){
        ReferenceDepthConfig config;config.convention=reversed?ReferenceDepthConvention::Reversed:ReferenceDepthConvention::Forward;config.flipNdcY=flip;
        ReferenceDepthGpu producer(backend,{33,35},2,config,shaders);if(producer.byteBudget()!=33*35*8*2)throw std::runtime_error("Depth budget mismatch");
        ReferenceClipProjectionProfile profile;
        if(perspective){profile.parameters.columns={ReferenceClipProjectionParameters::Row{1,0,0,0},{0,1,0,0},{0,0,reversed?0.F:-1.F,-1},{0,0,reversed?std::nextafter(.0625F,1.F):-.0625F,0}};}
        const float near=reversed?std::nextafter(.03125F,1.F):(perspective?.96875F:std::nextafter(.96875F,0.F)),far=reversed?.015625F:.984375F;
        std::vector<std::array<float,4>> vertices;
        for(unsigned layer=0;layer<2;++layer){const float w=perspective?(layer?4.F:2.F):1.F;
            for(auto xy:{std::array<float,2>{-1,-1},{1,-1},{-1,1},{1,1}})vertices.push_back({xy[0]*w,xy[1]*w,perspective?-w:(layer?far:near),1});}
        for(unsigned iteration=0;iteration<4;++iteration){const bool empty=iteration==2;auto input=vertices;
            if(iteration==3)for(auto& vertex:input)vertex[1]=-.75F*(perspective?-vertex[2]:1.F)+.25F*vertex[1];
            draw(producer,iteration%2,profile,input,triangles,empty);
            const auto values=static_cast<const float*>(readback.mapped);
            for(unsigned i=0;i<33*35*2;++i){const unsigned y=(i%(33*35))/33;
                const bool covered=!empty&&(iteration!=3||(flip?y>=26:y<9));const float expected=covered?near:(reversed?0.F:1.F);
                if(std::memcmp(values+i,&expected,4))throw std::runtime_error("Reference depth clear/overlap/precision/axis mismatch");++checked;}
        }
    }
    if(rejects!=10)throw std::runtime_error("Reference depth rejection contract failed: "+std::to_string(rejects));
    if(argc==6){
        auto profileBytes=readBinaryFile(argv[2]),vertexBytes=readBinaryFile(argv[3]),indexBytes=readBinaryFile(argv[4]);
        if(profileBytes.size()!=80||vertexBytes.empty()||vertexBytes.size()%16||indexBytes.empty()||indexBytes.size()%12)throw std::invalid_argument("Invalid native depth fixture");
        ReferenceClipProjectionProfile profile;std::memcpy(&profile.parameters,profileBytes.data(),80);
        std::vector<std::array<float,4>> vertices(vertexBytes.size()/16);std::memcpy(vertices.data(),vertexBytes.data(),vertexBytes.size());
        std::vector<std::uint32_t> nativeIndices(indexBytes.size()/4);std::memcpy(nativeIndices.data(),indexBytes.data(),indexBytes.size());
        for(auto index:nativeIndices)if(index>=vertices.size())throw std::invalid_argument("Native depth index outside positions");
        ReferenceDepthConfig nativeConfig;nativeConfig.flipNdcY=false;
        ReferenceDepthGpu producer(backend,{2560,1440},1,nativeConfig,shaders);draw(producer,0,profile,vertices,nativeIndices,false);
        std::ofstream output(std::string(argv[5])+"-depth.bin",std::ios::binary);output.write(static_cast<const char*>(readback.mapped),static_cast<std::streamsize>(readback.size));if(!output)throw std::runtime_error("Depth export failed");
    }
    cleanup.reset();if(host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)throw std::runtime_error("Reference depth resources leaked");
    if(host.errors.load())throw std::runtime_error("Reference depth Vulkan validation errors");
    std::cout<<host.properties.deviceName<<": "<<checked<<" exact R32/D32 values; forward/reversed overlap, clear, slot reuse, ten rejects, cleanup passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
