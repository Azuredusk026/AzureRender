#include <iostream>
#include "render/IrradianceClipmapGpu.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/IrradianceClipmapFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <iomanip>
using namespace azurerender;
int main(int argc,char** argv)try{
    if(argc<2||argc>5)throw std::invalid_argument("Expected shader, optional mode, or asset/positions");
    const bool native=argc>=4,sampleOnly=argc==5&&std::string(argv[4])=="--sample-only";
    if(argc==5&&!sampleOnly)throw std::invalid_argument("Unknown raw sampling mode");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    auto field=native?IrradianceClipmap::load(std::filesystem::u8path(argv[2])):test::clipmapFixture();
    if(!native){for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)field.amplitude[0][(z*2+y)*2+1]=test::clipmapAmplitude(1);}
    const bool disabled=argc==3&&std::string(argv[2])=="--disabled";
    const bool mirrored=argc==3&&std::string(argv[2])=="--mirrored";
    if(argc==3&&!disabled&&!mirrored)throw std::invalid_argument("Unknown clipmap GPU mode");
    if(disabled)field.enabled=false;if(mirrored)field.worldToField[0][0]=-1;
    std::vector<std::array<float,4>> positions;
    if(native){std::ifstream f(std::filesystem::u8path(argv[3]));std::array<float,4> p{};
        while(f>>p[0]>>p[1]>>p[2]){positions.push_back(p);if(positions.size()>1024)throw std::invalid_argument("Position query budget exceeded");}}
    else positions={{0,0,0,0},{.25F,0,0,0},{-.25F,0,0,0},{30,0,0,0},{120,0,0,0},
        {200,0,0,0},{480,0,0,0},{500,0,0,0},{0,1.5F,0,0}};
    if(positions.empty())throw std::invalid_argument("No positions supplied");
    rhi::GpuBuffer input{},output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        host.allocator.destroyBuffer(input);host.allocator.destroyBuffer(output);
    });
    {
        IrradianceClipmapGpu volume(backend,field);
        input=host.allocator.createBuffer(positions.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        output=host.allocator.createBuffer(positions.size()*6*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(input.mapped,positions.data(),input.size);host.allocator.flush(input,0,input.size);
        setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc p;p.maxSets=1;p.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3}};
        pool=backend.createDescriptorPool(p);auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
        volume.writeTextures(set,0);
        backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,volume.parameters().buffer,volume.parameters().size});
        backend.writeDescriptorBuffer({set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
        backend.writeDescriptorBuffer({set,3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
        layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));
        pipeline=backend.createComputePipeline({shader,layout});
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
            commands.dispatch(static_cast<unsigned>(positions.size()),1,1);
            commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);
        float maximum=0;std::cout<<std::setprecision(9);
        for(std::size_t i=0;i<positions.size();++i){const auto& p0=positions[i];auto expected=field.query({p0[0],p0[1],p0[2]});
            for(unsigned k=0;k<24;++k)if(!std::isfinite(values[i*24+k]))throw std::runtime_error("Nonfinite GPU query output");
            for(unsigned c=0;c<3;++c)for(unsigned a=0;a<4;++a){
                const auto actual=values[i*24+c*4+a];if(!std::isfinite(actual))throw std::runtime_error("Nonfinite GPU coefficient");
                maximum=std::max(maximum,std::abs(actual-expected.coefficients[c][a]));}
            if(!native){const unsigned masks[]={1,1,1,3,6,4,4,0,1};
                for(unsigned layer=0;layer<3;++layer)for(unsigned c=0;c<3;++c){
                    const float expectedAlpha=disabled||!(masks[i]&(1U<<layer))?0.F:float(c==0?0:c==1?128:255)/255;
                    if(std::abs(values[i*24+12+layer*4+c]-expectedAlpha)>1e-6F)
                        throw std::runtime_error("Directional RGBA alpha lane differs");}}
            if(native){for(unsigned k=0;k<24;++k)std::cout<<values[i*24+k]<<' ';std::cout<<'\n';}
        }
        // Raw queries retain finite-output and lifetime checks. Their sampling
        // precision is compared externally with the captured hardware oracle.
        if(!sampleOnly&&maximum>(native?1e-4F:1e-5F))throw std::runtime_error("GPU clipmap coefficients differ: "+std::to_string(maximum));
        std::cerr<<"Clipmap maximum absolute coefficient error: "<<maximum<<'\n';
    }
    cleanup.reset();
    if(host.errors.load()||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)
        throw std::runtime_error("Clipmap GPU validation or resource lifetime failure");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
