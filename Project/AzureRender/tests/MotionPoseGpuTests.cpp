#include "render/MotionPoseData.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include <array>
#include <cmath>
#include <cstring>
using namespace azurerender;
struct alignas(16) PoseProbe {
    std::array<std::uint32_t,4> selectors{};
    std::array<float,4> position{0,0,.25F,1},morph0{.2F,0,0,0},morph1{0,.4F,0,0};
    std::array<std::uint32_t,4> joints{};
    std::array<float,4> weights{1,0,0,0};
    float browMask=0,browOffset=0;std::uint32_t features=0,padding=0;
    std::array<float,4> current{0,0,.25F,1},normal{0,0,1,0},tangent{1,0,0,1};
    std::array<float,4> expectedClip{},expectedMotion{},expectedNormal{0,0,1,1},expectedTangent{1,0,0,1},expectedOutline{};
};
static_assert(sizeof(PoseProbe)==240);
MotionPoseState makeGpuPose(const char* name){MotionPoseState p;p.identity.node=name;p.vertexCount=3;p.joints={motionIdentityMatrix()};return p;}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected motion pose shader");
    auto legacy=makeGpuPose("legacy");legacy.model[12]=.1F;legacy.legacyMorph={.5F,.25F};
    auto general=makeGpuPose("general");general.model[12]=-.2F;general.morphTableBase=1;general.morphWeights={.5F};
    general.joints={motionIdentityMatrix(),motionIdentityMatrix()};general.joints[0][13]=.4F;general.joints[1][12]=.4F;
    auto brow=makeGpuPose("brow"),mirror=legacy,behind=makeGpuPose("behind"),corrupt=makeGpuPose("corrupt"),fresh=makeGpuPose("new");
    mirror.identity.node="mirror";mirror.model[0]=-1;
    auto scaled=makeGpuPose("scaled");scaled.model[0]=2;scaled.model[5]=3;scaled.model[10]=.5F;
    scaled.joints[0][0]=0;scaled.joints[0][1]=1;scaled.joints[0][4]=-1;scaled.joints[0][5]=0;
    MotionPoseView view;view.cameraPosition={0,0,2};view.selectedPrimitive=1;view.gizmo[13]=.2F;
    view.outlinePixels=2.25F;
    MotionPoseHistory history;history.stage({legacy,general,brow,mirror,behind,corrupt,scaled},view);history.commit(3);
    history.stage({general,legacy,brow,mirror,behind,fresh,corrupt,scaled},{});auto packed=history.pack(motionIdentityMatrix(),3,8192);
    packed.words[28+6*40+32]=UINT32_MAX;
    // Behind-camera coverage belongs to projection; joint transforms are affine.
    packed.words[28+4*40+16+15]=0xbf800000U;
    // Independent immutable morph table: 3 vertices, one position target.
    std::vector<std::uint32_t> targets(49,0);
    for(unsigned vertex=0;vertex<3;++vertex){targets[1+vertex*3]=10+vertex*9;targets[3+vertex*3]=1;
        const float delta=.5F,normalDelta=.2F,tangentDelta=.3F;
        std::memcpy(&targets[10+vertex*9+vertex],&delta,4);
        std::memcpy(&targets[10+vertex*9+4],&normalDelta,4);std::memcpy(&targets[10+vertex*9+8],&tangentDelta,4);}
    targets[40]=UINT32_MAX;targets[42]=1;targets[43]=10;targets[44]=UINT32_MAX;targets[45]=1;
    targets[46]=10;targets[48]=UINT32_MAX;
    std::vector<PoseProbe> probes;
    const auto add=[&](unsigned object,unsigned vertex,unsigned table,unsigned primitive,std::array<float,4> expected){
        PoseProbe p;p.selectors={object,vertex,table,primitive};p.expectedClip=expected;
        if(object==0)p.weights={.25F,.75F,0,0},p.joints={0,1,0,0};
        if(expected==std::array<float,4>{}){p.expectedNormal={};p.expectedTangent={};}
        else if(object==0){p.expectedNormal={0,.1F/std::sqrt(1.01F),1/std::sqrt(1.01F),1};
            p.expectedTangent={1/std::sqrt(1.0225F),0,.15F/std::sqrt(1.0225F),1};}
        else if(object==3)p.expectedTangent={-1,0,0,-1};
        if(expected[3]>0&&std::abs(expected[0])<=expected[3]&&std::abs(expected[1])<=expected[3]&&expected[2]>=0&&expected[2]<=expected[3])
            p.expectedMotion={-.5F*expected[0]/expected[3],-.5F*expected[1]/expected[3],expected[2]/expected[3],1};
        probes.push_back(p);
    };
    add(0,0,1,0,{.35F,.1F,.25F,1});add(0,1,1,0,{.1F,.35F,.25F,1});add(0,2,1,0,{.1F,.1F,.5F,1});
    add(0,0,1,1,{.35F,.3F,.25F,1});add(1,0,0,0,{.2F,.1F,.25F,1});add(1,0,0,1,{.2F,.3F,.25F,1});
    add(2,0,0,0,{0,0,.3F,1});probes.back().browMask=1;probes.back().browOffset=.05F;probes.back().features=64;
    add(2,0,0,0,{0,0,.25F,1});probes.back().browMask=.5F;probes.back().browOffset=.05F;probes.back().features=64;
    add(3,0,0,0,{-.2F,.1F,.25F,1});probes.back().position[0]=.2F;probes.back().current[0]=.2F;probes.back().expectedMotion[0]=.2F;
    add(4,0,0,0,{0,0,.25F,-1});add(5,0,0,0,{});add(6,0,0,0,{});
    add(99,0,0,0,{});add(1,3,0,0,{});add(0,0,UINT32_MAX,0,{});
    add(0,0,40,0,{});add(0,0,43,0,{});add(0,0,46,0,{});
    add(1,0,0,0,{});probes.back().joints[0]=UINT32_MAX;
    add(1,0,0,0,{.2F,.1F,.25F,1});probes.back().joints[1]=UINT32_MAX;
    add(7,0,0,0,{0,0,.125F,1});probes.back().normal={0,1,0,0};probes.back().expectedNormal={-1,0,0,1};probes.back().expectedTangent={0,1,0,1};
    add(7,0,0,1,{0,.6F,.125F,1});probes.back().normal={0,1,0,0};probes.back().expectedNormal={-1,0,0,1};probes.back().expectedTangent={0,1,0,1};
    for(auto& probe:probes){probe.expectedOutline=probe.expectedClip;
        if(probe.selectors[0]==0&&probe.expectedClip[3]!=0)probe.expectedOutline[1]+=.045F;
        if(probe.selectors[0]==2&&probe.expectedClip[3]!=0)probe.expectedOutline[2]=.25F;
        if(probe.selectors[0]==7)probe.expectedOutline[0]-=.045F;
    }
    const auto profile=[&](unsigned object,std::array<float,4> n,std::array<float,4> expected){
        add(object,0,0,0,object==1?std::array<float,4>{.2F,.1F,.25F,1}:std::array<float,4>{-.2F,.1F,.25F,1});
        auto& p=probes.back();p.padding=1;p.normal=n;p.expectedNormal=n;p.expectedNormal[3]=1;p.expectedOutline=expected;
        if(object==3){p.position[0]=.2F;p.expectedNormal[0]=-n[0];p.expectedMotion[0]=.1F;}
    };
    profile(1,{1,0,0,0},{.2035F,.1F,.255F,1});
    profile(1,{0,1,0,0},{.2F,.1035F,.255F,1});
    profile(1,{0,0,1,0},{.2F,.1F,.255F,1});
    profile(3,{1,0,0,0},{-.2035F,.1F,.255F,1});
    std::vector<std::size_t> homogeneousProbes;
    for(const float weight:{std::nextafter(1.F,0.F),std::nextafter(1.F,2.F)}){
        add(2,0,0,0,{0,0,.25F*weight,1});
        probes.back().weights={weight,0,0,0};
        probes.back().expectedOutline=probes.back().expectedClip;
        homogeneousProbes.push_back(probes.size()-1);
    }
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    std::array<rhi::GpuBuffer,4> buffers{};VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};
    VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);
    });
    const std::array<std::size_t,4> sizes{packed.words.size()*4,targets.size()*4,probes.size()*160,probes.size()*80};
    for(unsigned i=0;i<4;++i)buffers[i]=host.allocator.createBuffer(sizes[i],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memcpy(buffers[0].mapped,packed.words.data(),sizes[0]);std::memcpy(buffers[1].mapped,targets.data(),sizes[1]);
    for(std::size_t i=0;i<probes.size();++i)std::memcpy(static_cast<char*>(buffers[2].mapped)+i*160,&probes[i],160);
    std::vector<rhi::DescriptorBindingDesc> bindings;
    for(unsigned i=0;i<4;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});
    setLayout=backend.createDescriptorSetLayout(bindings);rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    for(unsigned i=0;i<4;++i)backend.writeDescriptorBuffer({set,i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,buffers[i].buffer,buffers[i].size});
    layout=backend.createPipelineLayout(setLayout,{});shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
    for(unsigned variant=0;variant<3;++variant){
        auto* words=static_cast<std::uint32_t*>(buffers[0].mapped);
        words[0]=variant==1?99:1;words[24]=variant==2?UINT32_MAX:static_cast<std::uint32_t>(packed.words.size());
        std::memset(buffers[3].mapped,0xcd,sizes[3]);
        for(const auto& buffer:buffers)host.allocator.flush(buffer,0,buffer.size);
        backend.executeOneShot([&](rhi::ICommandRecorder& commands){
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(static_cast<std::uint32_t>(probes.size()),1,1);
            rhi::BufferBarrierDesc barrier{};barrier.buffer=buffers[3].buffer;barrier.size=buffers[3].size;
            barrier.srcStageMask=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_HOST_BIT;
            barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;commands.bufferBarrier(barrier);
        });
        host.allocator.invalidate(buffers[3],0,buffers[3].size);const auto* output=static_cast<const float*>(buffers[3].mapped);
        bool passed=true;for(std::size_t i=0;i<probes.size();++i)for(unsigned axis=0;axis<20;++axis){
            const auto expected=variant?0:axis<4?probes[i].expectedClip[axis]:axis<8?probes[i].expectedMotion[axis-4]
                :axis<12?probes[i].expectedNormal[axis-8]:axis<16?probes[i].expectedTangent[axis-12]:probes[i].expectedOutline[axis-16];
            const auto value=output[i*20+axis];if(!std::isfinite(value)||std::abs(value-expected)>1e-6F){passed=false;
                std::cerr<<"Motion pose variant "<<variant<<" probe "<<i<<" axis "<<axis<<": "<<value<<" expected "<<expected<<'\n';}
        }
        if(variant==0)for(const auto index:homogeneousProbes)
            if(output[index*20+3]!=1.F)
                throw std::runtime_error("Affine skinned positions must preserve exact homogeneous one despite weight rounding");
        if(!passed)throw std::runtime_error("Previous motion pose GPU contract failed");
    }
    if(host.errors.load())throw std::runtime_error("Motion pose Vulkan validation errors");
    std::cout<<"Previous motion pose: "<<probes.size()<<" cases, 3 buffer variants and exact affine homogeneous checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
