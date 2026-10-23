#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <fstream>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc!=3)throw std::invalid_argument("Expected surface shader and showcase catalog");
    std::ifstream input(argv[2]);nlohmann::json catalog;input>>catalog;
    const float contrast=catalog.at("looks").at(1).at("grade").at("contrast");
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer output{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);host.allocator.destroyBuffer(output);
    });
    constexpr unsigned probeCount=260;
    output=host.allocator.createBuffer(probeCount*sizeof(float),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memset(output.mapped,0xCD,output.size);host.allocator.flush(output,0,output.size);
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,sizeof(float)};
    layout=backend.createPipelineLayout(setLayout,&range);shader=backend.createShaderModule(readBinaryFile(argv[1]));
    pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);
        commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,&contrast,sizeof(contrast));commands.dispatch(probeCount,1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto values=static_cast<const float*>(output.mapped);
    const double pi=3.141592653589793;
    const double horizontalEpsilon=std::ldexp(1.,-14);
    const double horizontalTiltLength=std::sqrt(.36+horizontalEpsilon*horizontalEpsilon);
    const double horizontalTwoAxisLength=std::sqrt(1.+horizontalEpsilon*horizontalEpsilon);
    const double expected[]={0,.04,.04/(4*pi*std::pow(.2,4)),.04/(4*pi*std::pow(.8,4)),0,0, 1,.875,.8125, 1,.875,.8125, 1,1,1, 1,1,1, 1,1,1, 1,1,1,
        0,1,.5,.1,.75,.5,1,.7,.55,.5,.9,.1,-.004,.004,
        1,0,.15625,.15625,.5,.8,.475,.2,.8,.8,std::sqrt(.5),6.103515625e-5/std::sqrt(.5),std::sqrt(.5),.5,
        1,std::pow(.9999,2)/2,std::pow(.9375,2)/26,0,1,std::pow(.99,3.5),std::pow(.75,3.5),0,
        .70710677855236947,.000086316745428756039,.70710677855236947,
        -.70710677855236947,.000086316745428756039,.70710677855236947,
        .70710677855236947,.000086316745428756039,-.70710677855236947,
        .38268343123473569,.000046714286039396447,.92387953179848326,
        0,0,1,
        .5257311114567752,.00003208808053325044,.85065080815618976,
        0,.000061035156136313162,.99999999813735485,
        -.99999999813735485,.000061035156136313162,0,
        0,-.000061035156136313162,-.99999999813735485,
        0.77432429290778382,0.47797856812641898,0.28939402739553682,1,1,
        0.19049991294588386,0.083847590117621981,0.031985361032821888,0,0,
        0.26879999399185184,0.13439999699592592,0.067199998497962959,0,0,
        0.37262987663533831,0.20483735835674546,0.11230666579253823,0.25,0.20000000000000001,
        0.27086027783485278,0.13375833246884361,0.067487095852088286,0,1,
        0.77432429290778382,0.47797856812641898,0.28939402739553682,1,1,
        0.58553224226349321,0.348894261355635,0.20617546114912519,0.625,0.625,
        0,0,0,1,1,
        0,horizontalEpsilon/horizontalTiltLength,.6/horizontalTiltLength,
        .6/horizontalTiltLength,horizontalEpsilon/horizontalTiltLength,0,
        -.6/horizontalTwoAxisLength,horizontalEpsilon/horizontalTwoAxisLength,-.8/horizontalTwoAxisLength,
        0,1,0, 0,1,0, 0,1,0,
        .99999999813735485,.000061035156136313162,0,
        0,.00010172525989033873,.99999999482598578,
        .70710677767431018,.00011508899352390593,.70710677533281863,
        -.99999999813735485,.000061035156136313162,0,
        0,1,0,
        0,.00000000006103515625,-1,
        .5018435120582581,.08908353745937347,.09507887065410614,
        2.0871262550354004,2.0894813537597656,2.098165512084961,
        1.0474107265472412,.18613839149475098,.1994912028312683,
        0,0,1,2/(5*std::sqrt(5.0)),
        .04436401650309563,-.9026122093200684,.42816224694252014,.20751208066940308,
        1,0,0,.25, 0,0,1,0, 0,0,1,.2,
        .3304,.1768,.1384,1,
        .1652,.0884,.0692,1,
        0,0,0,1, 0,0,0,1,
        .06608,.03536,.02768,1,
        .054201337445134945,.05016733109238065,.04915067285397564,1,
        0,0,0,1, .3304,.1768,.1384,1, 0,0,0,1, 0,0,0,1,
        .4296633303165436,.23663552105426788,.18798868358135223,1,
        .3,.1,.05,1,
        0,.5,1,.625,0,1,.375,.7120813131332397,
        0,.5,.5,0,
        .5986881256103516,.8304386138916016,.6729488372802734,.34885740280151367,
        .9323333501815796,.9421425461769104,.9000055193901062,.9149537682533264};
    static_assert(sizeof(expected)/sizeof(*expected)==probeCount);
    bool passed=true;
    for(unsigned i=0;i<probeCount;++i){
        std::cout<<"Surface probe "<<i<<": "<<values[i]<<" (expected "<<expected[i]<<")\n";
        passed&=std::isfinite(values[i]) && std::abs(values[i]-expected[i])<std::max(1e-6,std::abs(expected[i])*1e-4);
    }
    if(!passed)throw std::runtime_error("Surface response numeric contract failed");
    if(host.errors.load())throw std::runtime_error("Vulkan validation errors");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
