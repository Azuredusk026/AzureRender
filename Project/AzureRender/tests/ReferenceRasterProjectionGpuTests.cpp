#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ReferenceRasterProjection.hpp"
#include "render/ReferenceFmaShader.hpp"
#include <cstring>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
using Vec=std::array<float,4>;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void cpu(){
    const Vec source{2,-4,.25F,8},base{1,2,3,4},final{2,1,1,4};
    require(composeReferenceRasterPosition(source,base,final)==Vec{4,-6,2,8},"Reference XYW/depth/offset composition failed");
    require(composeReferenceRasterPosition(source,base,base)==Vec{2,-4,6,8},"Undeformed reference XYW must stay exact");
    require(composeReferenceRasterPosition(source,base,final,false)==final,"Disabled reference must preserve device bits");
    for(unsigned which=0;which<3;++which){auto s=source,b=base,f=final;(which==0?s:which==1?b:f)[3]=0;
        require(composeReferenceRasterPosition(s,b,f)==f,"Zero W must return original final clip");}
    auto huge=source;huge[3]=std::numeric_limits<float>::max();auto tiny=final;tiny[3]=std::numeric_limits<float>::min();
    require(composeReferenceRasterPosition(huge,base,tiny)==tiny,"Overflow must return original final clip");
    unsigned rejects=0;for(unsigned which=0;which<3;++which){auto s=source,b=base,f=final;(which==0?s:which==1?b:f)[0]=std::numeric_limits<float>::infinity();
        try{(void)composeReferenceRasterPosition(s,b,f);}catch(const std::invalid_argument&){++rejects;}}
    require(rejects==3,"Nonfinite inputs must be rejected");
    ReferenceCameraView v;v.flipNdcY=true;v.frame.parameters.axes={-1,2,3,0};PreparedReferenceCameraView camera(v);
    require(projectReferenceRasterPosition(camera,{2,4,.25F},base,final)==Vec{-1.75F,-4.25F,.25F,1},"Prepared camera raster adapter failed");
}
struct Record {Vec world,base,final,reference;};
struct Batch {ReferenceCameraView camera;std::vector<Record> records;bool enabled=true;};
std::vector<Batch> analytic(){
    std::vector<Batch> batches;
    for(unsigned mode=0;mode<8;++mode){Batch b;b.enabled=mode<4;b.camera.flipNdcY=(mode&1)!=0;if(mode&2)b.camera.frame.parameters.axes={-2,3,1,0};
        for(int i=0;i<4096;++i){Record r{{float(i%31-15)/8,float(i%47-23)/16,float(i%53+1)/8,1},{1,2,.5F,4},{1,2,.5F,4},{}};
            if(i&1){r.final[0]+=.5F;r.final[1]-=.25F;}if(i%7==0)r.final[2]=2;
            r.reference=PreparedReferenceCameraView(b.camera).clip({r.world[0],r.world[1],r.world[2]});b.records.push_back(r);}
        Record z{{1,2,3,1},{1,2,3,0},{4,5,6,2},{}};z.reference=PreparedReferenceCameraView(b.camera).clip({1,2,3});b.records.push_back(z);
        z.base[3]=4;z.final[3]=0;b.records.push_back(z);
        z.final={4,5,-6,-2};b.records.push_back(z);
        z.final={-0.F,0.F,1,std::numeric_limits<float>::min()};z.base[3]=std::numeric_limits<float>::max();b.records.push_back(z);
        batches.push_back(std::move(b));}
    return batches;
}
}
int main(int argc,char** argv)try{
    cpu();if(argc==2&&std::string(argv[1])=="--cpu-only"){std::cout<<"CPU depth, offset, disable, zero-W, overflow and three invalid-input contracts passed\n";return 0;}
    if(argc!=2&&argc!=5)throw std::invalid_argument("Expected shader, optional 80-byte projection,32-byte frame and 64-byte records");
    auto batches=analytic();if(argc==5){Batch b;auto p=readBinaryFile(argv[2]),f=readBinaryFile(argv[3]),r=readBinaryFile(argv[4]);
        require(p.size()==80&&f.size()==32&&!r.empty()&&r.size()%64==0&&r.size()<=16*1024*1024,"Invalid external raster fixture sizes");
        std::memcpy(&b.camera.projection.parameters,p.data(),80);std::memcpy(&b.camera.frame.parameters,f.data(),32);(void)PreparedReferenceCameraView(b.camera);
        b.records.resize(r.size()/64);std::memcpy(b.records.data(),r.data(),r.size());batches.push_back(std::move(b));}
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};rhi::GpuBuffer source{},destination{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(destination);});
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,112};layout=backend.createPipelineLayout(setLayout,&range);
    auto prepared=prepareReferenceFmaShader(readBinaryFile(argv[1]));require(prepared.fusedResults==3&&prepareReferenceFmaShader(prepared.code).code==prepared.code,"Raster shader requires three idempotently prepared reference FMAs");shader=backend.createShaderModule(prepared.code);pipeline=backend.createComputePipeline({shader,layout});
    std::size_t points=0;for(const auto& batch:batches){
        source=host.allocator.createBuffer(batch.records.size()*sizeof(Record),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);destination=host.allocator.createBuffer(batch.records.size()*32,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(source.mapped,batch.records.data(),source.size);std::memset(destination.mapped,0xcd,destination.size);host.allocator.flush(source,0,source.size);host.allocator.flush(destination,0,destination.size);
        backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,source.buffer,source.size});backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,destination.buffer,destination.size});
        const PreparedReferenceCameraView camera(batch.camera);const auto parameters=referenceRasterProjectionParameters(camera,batch.enabled);
        host.submit([&](rhi::ICommandRecorder& c,VkCommandBuffer){c.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});c.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});c.bindComputePipeline(pipeline);c.bindComputeDescriptorSet(layout,set);c.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,&parameters,112);c.dispatch((static_cast<unsigned>(batch.records.size())+63)/64,1,1);c.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});});
        host.allocator.invalidate(destination,0,destination.size);const auto result=static_cast<const Vec*>(destination.mapped);
        for(std::size_t i=0;i<batch.records.size();++i){const auto& r=batch.records[i];
            require(std::memcmp(result[i*2].data(),result[i*2+1].data(),16)==0,"GPU raster composition differs from independent device oracle");
            const auto expected=camera.clip({r.world[0],r.world[1],r.world[2]});require(std::memcmp(expected.data(),r.reference.data(),16)==0,"External source reference clip differs from public CPU projection");}
        points+=batch.records.size();host.allocator.destroyBuffer(source);source={};host.allocator.destroyBuffer(destination);destination={};}
    cleanup.reset();require(!host.errors.load()&&!host.allocator.statistics().liveBuffers&&!host.allocator.statistics().liveImages,"Raster fixture validation or cleanup failed");
    std::cout<<host.properties.deviceName<<": "<<points<<" records / "<<points*4<<" exact GPU raster components; CPU contracts and cleanup passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
