#include "support/VulkanComputeFixture.hpp"
#include "render/GpuTransparentIndexSort.hpp"
#include "assets/AssetDeformation.hpp"
#include "resources/BinaryFile.hpp"
#include <cstring>
#include <iostream>
#include <numeric>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t depthKey(float depth){std::uint32_t bits=0;if(depth!=0)std::memcpy(&bits,&depth,4);return (bits&0x80000000U)?bits:(bits^0x7fffffffU);}
LoadedAsset makeAsset(bool morph){
    LoadedAsset asset;asset.vertices.resize(2052);asset.indices.resize(2052);std::iota(asset.indices.begin(),asset.indices.end(),0U);
    for(unsigned i=0;i<asset.vertices.size();++i){auto& v=asset.vertices[i];
        v.position={float(int(i%17)-8)*.03125F,float(int(i%13)-6)*.012345F,float(i%11)*.04321F};
        v.joints={0,1,2,UINT32_MAX};v.weights={.51F,.24999F,.24001F,0};if(i%20==0)v.weights={0,0,0,0};
        v.morph0={.00125F,-.00321F,.0023F};v.morph1={-.00345F,.00013F,.0012F};
    }
    if(morph){
        asset.morphRanges.resize(1);auto& range=asset.morphRanges[0];range.firstVertex=0;range.vertexCount=1026;range.node=0;range.targets.resize(2);
        range.targets[0].positions.resize(1026);for(unsigned i=0;i<1026;++i)range.targets[0].positions[i]={float(i%7)*.00123F,-.00231F,.00017F};
        asset.defaultMorphWeights={{.25F,-.5F}};
    }return asset;
}
AssetPose makePose(float shift){
    AssetPose pose;pose.jointMatrices.resize(3,internal::Matrix4{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    for(unsigned i=0;i<3;++i){auto& m=pose.jointMatrices[i];m[0]=.87F;m[1]=.13F;m[4]=-.13F;m[5]=.87F;m[12]=.123456F+shift*float(i+1);m[13]=-.0312345F;m[14]=shift;}
    pose.morphWeights={{.2F,.7F}};return pose;
}
}
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=3)throw std::invalid_argument("Expected shader directory and optional source asset");
    const std::string shaders=argv[1];test::VulkanComputeFixture host;auto& backend=*host.backend;
    auto a=argc==3?loadGltfAsset(argv[2]):makeAsset(false);auto b=argc==3?LoadedAsset{}:makeAsset(true);
    std::vector<const LoadedAsset*> assets{&a};if(argc==2)assets.push_back(&b);
    std::vector<TransparentTopology> topologies;topologies.reserve(assets.size());
    for(const auto* asset:assets){
        std::vector<TransparentPrimitiveRange> ranges;
        if(asset->primitives.empty())ranges={{0,1023},{1023,1029}};
        else for(const auto& primitive:asset->primitives)ranges.push_back({primitive.firstIndex,primitive.indexCount});
        topologies.push_back(prepareTransparentTopology(asset->indices,asset->vertices.size(),ranges));
    }
    auto poseA=argc==3?sampleAnimationPose(a,0,.137F,true):makePose(.002345F);
    auto poseB=argc==3?sampleAnimationPose(a,0,.271F,true):makePose(-.004321F);
    if(argc==2)poseB.morphWeights={{-.31F,.51F}};
    const auto jointCount=poseA.jointMatrices.size()+poseB.jointMatrices.size();
    std::uint64_t positionsChecked=0,indicesChecked=0;unsigned cases=0;
    {
        GpuTransparentSortLimits limits;limits.vertices=argc==3?262144:8192;limits.triangles=argc==3?262144:4096;
        limits.instances=8;limits.morphWeights=1024;limits.slots=2;
        GpuTransparentSortShaders code{readBinaryFile(shaders+"/transparent_deform.comp.spv"),readBinaryFile(shaders+"/transparent_depth_keys.comp.spv"),
            readBinaryFile(shaders+"/transparent_index_scatter.comp.spv"),readBinaryFile(shaders+"/stable_key_sort.comp.spv")};
        std::vector<GpuTransparentSortGeometry> geometry;for(unsigned i=0;i<assets.size();++i)geometry.push_back({assets[i],&topologies[i]});
        GpuTransparentIndexSort owner(backend,geometry,limits,code);
        auto joints=host.allocator.createBuffer(std::max<std::size_t>(64,jointCount*64),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        auto positions=host.allocator.createBuffer(VkDeviceSize(limits.vertices)*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        auto keys=host.allocator.createBuffer(VkDeviceSize(limits.triangles)*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        auto indices=host.allocator.createBuffer(VkDeviceSize(limits.triangles)*12,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){for(auto* buffer:{&joints,&positions,&keys,&indices})host.allocator.destroyBuffer(*buffer);});
        std::memcpy(joints.mapped,poseA.jointMatrices.data(),poseA.jointMatrices.size()*64);
        std::memcpy(static_cast<unsigned char*>(joints.mapped)+poseA.jointMatrices.size()*64,poseB.jointMatrices.data(),poseB.jointMatrices.size()*64);
        host.allocator.flush(joints,0,joints.size);
        for(unsigned iteration=0;iteration<3;++iteration){
            const unsigned slot=iteration%2;
            GpuTransparentSortInstance first,second;first.geometry=0;second.geometry=static_cast<unsigned>(assets.size()-1);
            first.pose=iteration==1?nullptr:&poseA;second.pose=&poseB;second.jointBase=static_cast<unsigned>(poseA.jointMatrices.size());
            first.morph={.31F,-.13F};second.morph={-.27F,.43F};
            first.model[0]=.9321F;first.model[4]=.1234F;first.model[12]=.0312345F;
            second.model[1]=-.123F;second.model[13]=.0234567F;
            const std::vector<GpuTransparentSortInstance> instances=iteration==2?std::vector<GpuTransparentSortInstance>{second,first}:std::vector<GpuTransparentSortInstance>{first,second};
            const internal::Vector3 camera{.12345F,-.02345F,.032134F},forward{.17321F,.23145F,.95678F};
            std::vector<std::array<float,4>> expectedPositions;std::vector<std::array<std::uint32_t,2>> expectedKeys;std::vector<std::uint32_t> expectedIndices;
            for(const auto& instance:instances){const auto& asset=*assets[instance.geometry];const auto& topology=topologies[instance.geometry];const auto base=expectedPositions.size();
                for(auto vertex:topology.vertices){const auto p=internal::transformPosition(instance.model,deformedVertexPosition(asset,vertex,instance.pose,instance.morph));expectedPositions.push_back({p[0],p[1],p[2],1});}
                for(const auto& primitive:topology.primitives)for(unsigned t=0;t<primitive.range.indexCount/3;++t){
                    const auto& x=expectedPositions[base+primitive.localIndices[t*3]];const auto& y=expectedPositions[base+primitive.localIndices[t*3+1]];const auto& z=expectedPositions[base+primitive.localIndices[t*3+2]];
                    const internal::Vector3 centroid{(x[0]+y[0]+z[0])*(1.F/3.F),(x[1]+y[1]+z[1])*(1.F/3.F),(x[2]+y[2]+z[2])*(1.F/3.F)};
                    expectedKeys.push_back({depthKey(internal::dot(internal::subtract(centroid,camera),forward)),static_cast<unsigned>(expectedKeys.size())});
                }
                std::size_t count=0;for(const auto& primitive:topology.primitives)count+=primitive.range.indexCount;
                const auto offset=expectedIndices.size();expectedIndices.resize(offset+count);TransparentTopologyWorkspace scratch;
                writeSortedTransparentIndices(topology,[&](unsigned vertex){return internal::transformPosition(instance.model,deformedVertexPosition(asset,vertex,instance.pose,instance.morph));},
                    camera,forward,expectedIndices.data()+offset,scratch);
            }
            const auto ticket=owner.prepare(slot,joints,instances,camera,forward);
            require(ticket.vertexCount==expectedPositions.size()&&ticket.indexCount==expectedIndices.size(),"Transparent prepared workload mismatch");
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
                commands.bufferBarrier({joints.buffer,0,joints.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                owner.record(commands,ticket);
                const auto copy=[&](const rhi::GpuBuffer& source,const rhi::GpuBuffer& destination,VkDeviceSize bytes){
                    commands.bufferBarrier({source.buffer,0,bytes,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                    const VkBufferCopy region{0,0,bytes};vkCmdCopyBuffer(native,source.buffer,destination.buffer,1,&region);
                    commands.bufferBarrier({destination.buffer,0,bytes,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});};
                copy(owner.positions(slot),positions,expectedPositions.size()*16);copy(owner.keys(slot),keys,expectedKeys.size()*8);copy(owner.indices(slot),indices,expectedIndices.size()*4);
            });
            for(const auto* buffer:{&positions,&keys,&indices})host.allocator.invalidate(*buffer,0,buffer->size);
            if(std::memcmp(positions.mapped,expectedPositions.data(),expectedPositions.size()*16)!=0){
                const auto* actual=static_cast<const std::uint32_t*>(positions.mapped);const auto* expected=reinterpret_cast<const unsigned char*>(expectedPositions.data());
                for(unsigned i=0;i<expectedPositions.size()*4;++i){std::uint32_t wanted;std::memcpy(&wanted,expected+i*4,4);
                    if(actual[i]!=wanted)throw std::runtime_error("Transparent world mismatch at component "+std::to_string(i)+" expected="+std::to_string(wanted)+" actual="+std::to_string(actual[i]));}
            }
            require(std::memcmp(keys.mapped,expectedKeys.data(),expectedKeys.size()*8)==0,"Transparent depth keys differ from CPU");
            require(std::memcmp(indices.mapped,expectedIndices.data(),expectedIndices.size()*4)==0,"Transparent indices differ from CPU");
            positionsChecked+=expectedPositions.size();indicesChecked+=expectedIndices.size();++cases;
        }
    }
    require(host.errors.load()==0,"Vulkan validation errors");require(!host.allocator.statistics().liveBuffers&&!host.allocator.statistics().liveImages,"Transparent preparation leaked resources");
    std::cout<<"GPU transparent preparation: device="<<host.properties.deviceName<<" cases="<<cases<<" positions="<<positionsChecked<<" indices="<<indicesChecked<<" validationErrors=0 liveBuffers=0 liveImages=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
