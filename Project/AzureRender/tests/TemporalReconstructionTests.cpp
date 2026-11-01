#include "render/TemporalReconstruction.hpp"
#include "render/MotionPoseData.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
void temporalCheck(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected shaders");
    rhi::NullRhi backend;
    TemporalSceneSource source{reinterpret_cast<VkImage>(100),reinterpret_cast<VkImageView>(101),reinterpret_cast<VkImage>(102),reinterpret_cast<VkImageView>(103)};
    {
        TemporalReconstruction temporal(backend,backend.allocator(),{64,48},VK_FORMAT_D32_SFLOAT,{source},argv[1]);
        temporalCheck(temporal.byteSize()==64*48*52,"Temporal budget includes motion, surface depth, scratch and both histories");
        temporalCheck(backend.allocator().statistics().liveImages==7,"View owns seven independent image allocations");
        TemporalViewState camera;TemporalFrameInfo info;info.width=64;info.height=48;info.sceneGeneration=1;info.projection=info.viewProjection=motionIdentityMatrix();
        temporal.prepare(camera.prepare(info),0,0);temporalCheck(!temporal.preparedHistoryValid(),"First frame rejects history");
        temporalCheck(!temporal.commit(1),"Unrecorded preparation cannot advance GPU history");
        RenderGraph graph;auto motion=temporal.importMotion(graph,"fixture");
        auto color=graph.addResource("color"),depth=graph.addResource("depth");
        bool missingRejected=false;try{temporal.schedule(graph,color,depth,motion,"fixture");}catch(const std::logic_error&){missingRejected=true;}
        temporalCheck(missingRejected&&graph.passes().empty()&&graph.resources().size()==4,"Missing motion writers reject atomically before graph mutation");
        const auto producer=graph.addPass("motion-producer");graph.attachment(producer,motion[0],RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        missingRejected=false;try{temporal.schedule(graph,color,depth,motion,"fixture");}catch(const std::logic_error&){missingRejected=true;}
        temporalCheck(missingRejected&&graph.passes().size()==1&&graph.resources().size()==4,"Missing surface-depth writer rejects before graph mutation");
        graph.attachment(producer,motion[1],RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        temporal.schedule(graph,color,depth,motion,"fixture");
        std::string error;temporalCheck(graph.compile(error),"Temporal graph compiles");rhi::NullCommandRecorder commands;graph.execute(&commands);
        temporalCheck(temporal.commit(3)&&camera.commit(3),"Recorded state advances with actual submission");
        auto index=temporal.historyIndex();temporalCheck(temporal.historySubmission()==3&&!temporal.commit(3),"Duplicate submission cannot rotate history");
        temporal.prepare(camera.prepare(info),1,0);temporalCheck(temporal.preparedHistoryValid(),"Consecutive render uses submitted history");
        temporal.cancel();camera.cancel();temporalCheck(temporal.historyIndex()==index&&temporal.historySubmission()==3,"Canceled work preserves committed history");
        ++info.cameraCut;temporal.prepare(camera.prepare(info),1,0);temporalCheck(!temporal.preparedHistoryValid(),"Cut rejects previous camera history");
        temporal.cancel();camera.cancel();info.cameraCut=0;
        TemporalPreparedFrame mismatch=camera.prepare(info);mismatch.previousSubmission=99;temporal.prepare(mismatch,1,0);
        temporalCheck(!temporal.preparedHistoryValid(),"Camera and attachment submissions must match");
        auto other=graph.addResource("wrong-view-motion"),otherDepth=graph.addResource("wrong-view-depth");
        bool foreignRejected=false;try{temporal.schedule(graph,color,depth,{other,otherDepth},"wrong-view");}catch(const std::invalid_argument&){foreignRejected=true;}
        temporalCheck(foreignRejected,"Foreign motion resources cannot enter this view's resolve");
    }
    temporalCheck(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"Temporal ownership releases all resources");
    {
        const TemporalSceneSource second{reinterpret_cast<VkImage>(200),reinterpret_cast<VkImageView>(201),reinterpret_cast<VkImage>(202),reinterpret_cast<VkImageView>(203)};
        MotionVectorTarget shared(backend,backend.allocator(),{64,48},VK_FORMAT_D32_SFLOAT,{source.depthView,second.depthView});
        const auto borrowedView=shared.view();
        {
            TemporalReconstruction temporal(backend,backend.allocator(),{64,48},VK_FORMAT_D32_SFLOAT,{source,second},argv[1],&shared);
            temporalCheck(&temporal.motion()==&shared,"Temporal resolve must use the caller's shared motion owner");
            temporalCheck(temporal.byteSize()==64*48*32&&shared.byteSize()==64*48*20,"Shared budget counts each image owner once");
            temporalCheck(backend.allocator().statistics().liveImages==7,"Shared target adds five history images without duplicating motion");
        }
        temporalCheck(backend.allocator().statistics().liveImages==2&&shared.view()==borrowedView,"Temporal destruction preserves the borrowed motion owner");
        const auto rejectsShared=[&](VkExtent2D extent,VkFormat depth,std::vector<TemporalSceneSource> sources){
            bool bad=false;try{TemporalReconstruction temporal(backend,backend.allocator(),extent,depth,std::move(sources),argv[1],&shared);}catch(const std::invalid_argument&){bad=true;}
            temporalCheck(bad&&backend.allocator().statistics().liveImages==2&&backend.allocator().statistics().liveBuffers==0,"Incompatible shared motion must reject before allocation");
        };
        rejectsShared({63,48},VK_FORMAT_D32_SFLOAT,{source,second});
        rejectsShared({64,48},VK_FORMAT_D16_UNORM,{source,second});
        rejectsShared({64,48},VK_FORMAT_D32_SFLOAT,{second,source});
        rejectsShared({64,48},VK_FORMAT_D32_SFLOAT,{source});
        rhi::NullRhi foreign;bool badBackend=false;
        try{TemporalReconstruction temporal(foreign,foreign.allocator(),{64,48},VK_FORMAT_D32_SFLOAT,{source,second},argv[1],&shared);}catch(const std::invalid_argument&){badBackend=true;}
        temporalCheck(badBackend&&foreign.allocator().statistics().liveImages==0,"A shared motion owner cannot cross RHI devices");
    }
    temporalCheck(backend.allocator().statistics().liveImages==0,"Shared motion owner releases its images last");
    {
        auto displaySource=source;displaySource.displayMask=reinterpret_cast<VkImage>(110);displaySource.displayMaskView=reinterpret_cast<VkImageView>(111);
        TemporalReconstruction temporal(backend,backend.allocator(),{64,48},VK_FORMAT_D32_SFLOAT,{displaySource},argv[1]);
        temporalCheck(temporal.hasDisplayMask()&&temporal.byteSize()==64*48*60&&backend.allocator().statistics().liveImages==9,
            "Display history owns two independent dual-channel images within the unchanged combined cap");
        temporalCheck(temporal.displayMaskView(0)&&temporal.displayMaskView(0)!=temporal.displayMaskView(1),"Display histories retain distinct views");
        bool bad=false;try{temporal.displayMaskView(UINT32_MAX);}catch(const std::out_of_range&){bad=true;}
        temporalCheck(bad,"Display history indices reject before arithmetic overflow");
        TemporalViewState camera;TemporalFrameInfo info;info.width=64;info.height=48;info.sceneGeneration=1;info.projection=info.viewProjection=motionIdentityMatrix();
        temporal.prepare(camera.prepare(info),0,0);const auto next=temporal.preparedDisplayMaskIndex();
        RenderGraph graph;const auto motion=temporal.importMotion(graph,"display");
        const auto color=graph.addResource("color"),depth=graph.addResource("depth");
        const auto producer=graph.addPass("source-producer");for(auto resource:motion)graph.attachment(producer,resource,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const auto count=graph.resources().size();bad=false;
        try{temporal.schedule(graph,color,depth,motion,"display");}catch(const std::invalid_argument&){bad=true;}
        temporalCheck(bad&&graph.resources().size()==count&&graph.passes().size()==1,"Missing display source rejects atomically");
        rhi::ImageBarrierDesc input;input.image=displaySource.displayMask;const auto mask=graph.importImage("display-mask",input);
        bad=false;try{temporal.schedule(graph,color,depth,motion,"display",mask);}catch(const std::logic_error&){bad=true;}
        temporalCheck(bad&&graph.passes().size()==1,"Display history requires its actual mask producer");
        graph.attachment(producer,mask,RenderGraphUsage::ColorAttachment,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const auto resolved=temporal.schedule(graph,color,depth,motion,"display",mask);
        temporalCheck(resolved&&*resolved!=mask,"Resolve exports a distinct display history for its consumer");
        std::string error;temporalCheck(graph.compile(error),"Display history graph compiles");rhi::NullCommandRecorder commands;graph.execute(&commands);
        temporalCheck(temporal.commit(3)&&camera.commit(3)&&temporal.historyIndex()==next,"Color and display histories commit the same actual submission");
        temporal.prepare(camera.prepare(info),1,0);temporalCheck(temporal.preparedHistoryValid(),"Consecutive display history follows the camera submission");
        temporal.cancel();camera.cancel();temporalCheck(temporal.historyIndex()==next,"Canceled display work preserves its committed history");
    }
    temporalCheck(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,"Display histories release their complete owned allocation set");
    bool rejected=false;try{TemporalReconstruction oversized(backend,backend.allocator(),{100000,100000},VK_FORMAT_D32_SFLOAT,{source},argv[1]);}catch(const std::invalid_argument&){rejected=true;}
    temporalCheck(rejected&&backend.allocator().statistics().liveImages==0,"Oversized history rejects before allocation");
    std::cout<<"Temporal reconstruction: ownership, budget, graph, actual submission, cancel and camera reset passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
