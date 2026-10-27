#include "render/OctahedralEnvironmentGpu.hpp"
#include "support/OctahedralEnvironmentFixture.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <iomanip>
using namespace azurerender;
using Query=std::array<float,8>;
int main(int argc,char** argv)try{
    if(argc<2||argc>4)throw std::invalid_argument("Expected shader, optional fold mode, or asset and queries");
    const bool native=argc==4;auto asset=native?OctahedralEnvironment::load(std::filesystem::u8path(argv[2])):test::octahedralFixture();
    if(argc==3){if(std::string(argv[2])!="--mathematical")throw std::invalid_argument("Unknown fold mode");asset.foldZeroSign=OctahedralEnvironment::FoldZeroSign::Mathematical;}
    std::vector<Query> queries;
    if(native){std::ifstream in(std::filesystem::u8path(argv[3]));Query q{};while(in>>q[0]>>q[1]>>q[2]>>q[3]>>q[4]>>q[5]>>q[6]>>q[7]){
        if(queries.size()>=1024)throw std::invalid_argument("Octahedral query budget exceeded");queries.push_back(q);}}
    else queries={Query{1,0,0,0,0,0,0,0},Query{-.3F,.7F,-.2F,.5F,1,0,0,0},Query{0,0,-1,1,1,0,0,0},
        Query{1,0,0,2,0,0,0,0},Query{1,0,0,20,1,0,0,0},Query{0,1,0,-1,0,0,0,0},Query{1,1,0,.75F,1,0,0,0},Query{-1,-1,-1,1.5F,0,0,0,0}};
    if(queries.empty())throw std::invalid_argument("No octahedral queries supplied");
    for(const auto& q:queries){for(float f:q)if(!std::isfinite(f))throw std::invalid_argument("Nonfinite octahedral query");
        if(q[4]<0||q[4]>=asset.layers||q[4]!=std::floor(q[4])||(q[7]!=0&&q[7]!=1))throw std::invalid_argument("Invalid query layer or mode");
        if(q[7]==0)(void)asset.directionUv({q[0],q[1],q[2]});
        else if(q[5]<0||q[5]>1||q[6]<0||q[6]>1)throw std::invalid_argument("Invalid direct UV query");}
    test::VulkanComputeFixture host;auto& backend=*host.backend;rhi::GpuBuffer input{},output{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(input);host.allocator.destroyBuffer(output);});
    {
        OctahedralEnvironmentGpu texture(backend,asset);
        input=host.allocator.createBuffer(queries.size()*sizeof(Query),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        output=host.allocator.createBuffer(queries.size()*32,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(input.mapped,queries.data(),input.size);host.allocator.flush(input,0,input.size);
        setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};
        pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();texture.writeTexture(set,0);
        backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,input.buffer,input.size});
        backend.writeDescriptorBuffer({set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
        const rhi::PushConstantRangeDesc constants{VK_SHADER_STAGE_COMPUTE_BIT,sizeof(OctahedralEnvironmentGpu::Parameters)};
        layout=backend.createPipelineLayout(setLayout,&constants);shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
            commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,texture.parameters().data(),sizeof(texture.parameters()));
            commands.dispatch(static_cast<unsigned>(queries.size()),1,1);
            commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});});
        host.allocator.invalidate(output,0,output.size);const auto* values=static_cast<const float*>(output.mapped);float maxUv=0,maxColor=0;
        std::cout<<std::setprecision(9);
        for(std::size_t i=0;i<queries.size();++i){const auto& q=queries[i];
            for(unsigned k=0;k<8;++k)if(!std::isfinite(values[i*8+k]))throw std::runtime_error("Nonfinite octahedral GPU output");
            const auto uv=q[7]==0?asset.directionUv({q[0],q[1],q[2]}):std::array<float,2>{q[5],q[6]};
            for(unsigned axis=0;axis<2;++axis)maxUv=std::max(maxUv,std::abs(values[i*8+4+axis]-uv[axis]));
            if(!native){
                const auto lod=std::clamp(q[3],0.F,float(asset.mipLevels-1));const auto lo=unsigned(lod),hi=std::min(lo+1,asset.mipLevels-1);const auto f=lod-lo;float red=0;
                for(unsigned k=0;k<2;++k){const auto mip=k?hi:lo;const auto side=std::max(1U,asset.width>>mip);const auto offset=asset.subresourceOffset(mip,unsigned(q[4]));
                    const float px=uv[0]*side-.5F,py=uv[1]*side-.5F;const auto x=int(std::floor(px)),y=int(std::floor(py));const auto fx=px-x,fy=py-y;
                    for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto index=offset+std::clamp(x+dx,0,int(side)-1)+std::size_t(side)*std::clamp(y+dy,0,int(side)-1);
                        const float value=1+float(asset.texels[index]&63)/64;red+=value*(dx?fx:1-fx)*(dy?fy:1-fy)*(k?f:1-f);}}
                maxColor=std::max({maxColor,std::abs(values[i*8]-red),std::abs(values[i*8+1]-2),std::abs(values[i*8+2]-4)});
            }else{for(unsigned k=0;k<8;++k)std::cout<<values[i*8+k]<<' ';std::cout<<'\n';}
        }
        if(maxUv>2e-6F||maxColor>1e-4F)throw std::runtime_error("Octahedral CPU/GPU projection or synthetic sampling differs");
        std::cerr<<"Octahedral maximum UV error="<<maxUv<<" synthetic color error="<<maxColor<<'\n';
    }
    cleanup.reset();if(host.errors.load()||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)
        throw std::runtime_error("Octahedral GPU validation or resource lifetime failure");return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
