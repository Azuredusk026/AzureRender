#include "render/ReflectionProbeFieldJson.hpp"
#include "render/OctahedralEnvironmentGpu.hpp"
#include "support/OctahedralEnvironmentFixture.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <iomanip>
using namespace azurerender;
using Query=std::array<float,12>;
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=3&&argc!=5)throw std::invalid_argument("Expected shader");
    const bool native=argc==5;auto asset=native?OctahedralEnvironment::load(std::filesystem::u8path(argv[2])):test::octahedralFixture();
    if(!native)for(unsigned mip=0;mip<3;++mip)for(unsigned layer=0;layer<2;++layer){
        const auto side=std::max(1U,asset.width>>mip);const auto offset=asset.subresourceOffset(mip,layer);
        for(unsigned i=0;i<side*side;++i)asset.texels[offset+i]=(15U<<6)|(layer?32U:0U)|((16U<<6)<<11)|((17U<<5)<<22);}
    ReflectionProbeField field;ReflectionProbeVolume volume;volume.arrayLayer=1;volume.halfExtents={2,2,2};
    volume.fade=.5F;field.probes={volume};
    std::vector<Query> queries{{0,0,0,0,1,0,0,1,0,1,0,0},{1.5F,0,0,.5F,.3F,.7F,-.2F,1,0,1,0,0},
        {3,0,0,2,0,0,-1,1,0,1,0,0},{0,2,0,1,1,0,0,1,0,1,0,0}};
    if(argc==3){const std::string mode=argv[2];
        if(mode=="--sphere")field.probes[0].shape=ReflectionProbeVolume::Shape::Sphere;
        else if(mode=="--rotated")field.probes[0].worldToLocal={{{0,0,1,0},{0,1,0,0},{-1,0,0,0}}};
        else if(mode=="--no-projection")field.probes[0].projection=false;
        else if(mode=="--adapted"){field.adaptation=1;field.probes[0].luminanceSH={0,0,0,2};field.defaultLuminanceSH={0,0,0,.5F};}
        else if(mode=="--priority"){auto second=field.probes[0];second.fade=1;second.intensity=2;second.arrayLayer=0;field.probes.push_back(second);}
        else throw std::invalid_argument("Unknown probe field fixture mode");}
    if(native){std::ifstream file(std::filesystem::u8path(argv[3]));nlohmann::json metadata;file>>metadata;field=metadata.get<ReflectionProbeField>();field.validate(asset.layers);
        const auto bytes=readBinaryFile(argv[4]);if(bytes.empty()||bytes.size()%sizeof(Query)||bytes.size()>1024*sizeof(Query))throw std::invalid_argument("Invalid field query payload");
        queries.resize(bytes.size()/sizeof(Query));std::memcpy(queries.data(),bytes.data(),bytes.size());}
    for(const auto& q:queries){if(!std::isfinite(q[3]))throw std::invalid_argument("Nonfinite field LOD");
        (void)field.query({q[0],q[1],q[2]},{q[4],q[5],q[6]},{q[8],q[9],q[10]},q[7]);}
    test::VulkanComputeFixture host;auto& backend=*host.backend;rhi::GpuBuffer input{},output{},parameters{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);if(layout)backend.destroyPipelineLayout(layout);
        if(pool)backend.destroyDescriptorPool(pool);if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        host.allocator.destroyBuffer(input);host.allocator.destroyBuffer(output);host.allocator.destroyBuffer(parameters);});
    {
        OctahedralEnvironmentGpu texture(backend,asset);const auto rows=field.gpuRows(asset.uvScaleOffset,asset.foldZeroSign==OctahedralEnvironment::FoldZeroSign::Positive);
        input=host.allocator.createBuffer(queries.size()*sizeof(Query),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        output=host.allocator.createBuffer(queries.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        parameters=host.allocator.createBuffer(rows.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(input.mapped,queries.data(),input.size);host.allocator.flush(input,0,input.size);
        std::memcpy(parameters.mapped,rows.data(),parameters.size);host.allocator.flush(parameters,0,parameters.size);
        setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3}};
        pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();texture.writeTexture(set,0);
        backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
        backend.writeDescriptorBuffer({set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
        backend.writeDescriptorBuffer({set,3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,parameters.buffer,parameters.size});
        layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
            commands.dispatch(static_cast<unsigned>(queries.size()),1,1);
            commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});});
        host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);
        std::cout<<std::setprecision(9);float maximum=0;for(std::size_t i=0;i<queries.size();++i){const auto& q=queries[i];
            const auto contributions=field.query({q[0],q[1],q[2]},{q[4],q[5],q[6]},{q[8],q[9],q[10]},q[7]);
            std::array<float,3> expected{};for(const auto& p:contributions){const auto w=p.weight*p.radianceScale;expected[0]+=(p.layer?1.5F:1.F)*w;expected[1]+=2*w;expected[2]+=4*w;}
            for(unsigned axis=0;axis<3;++axis){if(!std::isfinite(values[i*4+axis]))throw std::runtime_error("Nonfinite field result");if(!native)maximum=std::max(maximum,std::abs(values[i*4+axis]-expected[axis]));}}
        if(native)for(std::size_t i=0;i<queries.size();++i)std::cout<<values[i*4]<<' '<<values[i*4+1]<<' '<<values[i*4+2]<<' '<<values[i*4+3]<<'\n';
        if(maximum>1e-4F)throw std::runtime_error("Ordered field mixture differs from independent CPU colors");
        std::cout<<"Maximum field mixture error="<<maximum<<'\n';
    }
    cleanup.reset();if(host.errors.load()||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)
        throw std::runtime_error("Field validation or lifetime failure");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
