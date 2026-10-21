#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Capture final:public rhi::ICommandRecorder {
public:
    struct Draw {unsigned pass,first;std::array<float,16> transform;};
    std::vector<Draw> draws;
    std::array<unsigned char,212> constants{};
    unsigned pass=0;
    void beginRenderPass(const rhi::RenderPassBeginDesc&) override{++pass;}
    void endRenderPass() override{}
    void setViewport(float,float,float=0,float=0) override{}
    void setScissor(VkExtent2D,VkOffset2D={}) override{}
    void bindComputePipeline(VkPipeline) override{}
    void bindComputeDescriptorSet(VkPipelineLayout,VkDescriptorSet) override{}
    void bindPipeline(VkPipeline) override{}
    void bindDescriptorSet(VkPipelineLayout,VkDescriptorSet) override{}
    void pushConstants(VkPipelineLayout,VkShaderStageFlags,unsigned offset,const void* data,unsigned size) override {
        require(offset+size<=constants.size(),"Push constants exceed the shared range");
        std::memcpy(constants.data()+offset,data,size);
    }
    void bindVertexBuffer(VkBuffer,VkDeviceSize) override{}
    void bindIndexBuffer(VkBuffer,VkDeviceSize) override{}
    void draw(unsigned) override{}
    void drawIndexed(unsigned,unsigned first,unsigned=1,unsigned=0) override {
        Draw draw{pass,first,{}};
        std::memcpy(draw.transform.data(),constants.data()+144,sizeof(draw.transform));
        draws.push_back(draw);
    }
    void drawIndexedIndirect(VkBuffer,VkDeviceSize,unsigned,unsigned) override{}
    void dispatch(unsigned,unsigned,unsigned) override{}
    void imageBarrier(const rhi::ImageBarrierDesc&) override{}
    void bufferBarrier(const rhi::BufferBarrierDesc&) override{}
    void clearColorImage(VkImage,VkImageLayout,const VkClearColorValue&) override{}
    void copyImageToBuffer(VkImage,VkBuffer,VkExtent2D) override{}
    void writeTimestamp(VkQueryPool,unsigned,VkPipelineStageFlagBits) override{}
};

void verify(const std::filesystem::path& shaders,bool bindless,bool compute) {
    rhi::NullRhi backend;CharacterSceneRenderer renderer;RenderContext context;
    context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders.string();
    context.renderExtent={256,192};context.bindlessTextures=bindless;context.computeSkinning=compute;
    context.cullingEnabled=false;
    auto& limits=context.descriptorLimits;
    limits.maxImageDimension2D=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    limits.maxPerStageResources=4096;
    const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);
    const auto path=(source/"assets_public/scene_inspector/assets/cube.gltf").generic_string();
    context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();
    auto asset=std::make_shared<LoadedAsset>(loadGltfAsset(path));
    asset->nodes[0].opacity=.5F;
    for(auto& material:asset->materials)material.materialFeatures|=MaterialFeatureSilhouetteOutline;
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared{{path,asset}};
    context.preparedMeshes=&prepared;
    context.scene.resources.push_back({"cube",path});
    scene::SceneNodeDesc node;node.id="cube";node.resourceId="cube";context.scene.nodes.push_back(node);
    RenderSettings settings;settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;
    settings.morphWeights={.4F,.6F};context.renderSettings=&settings;
    renderer.onLoad(context);
    SceneFrameData frame;frame.renderSettings=&settings;frame.swapchainWidth=256;frame.swapchainHeight=192;
    frame.gizmoActive=true;frame.selectedPrimitiveIndex=0;
    frame.gizmoTranslation[0]=.75F;frame.gizmoRotation[1]=30;frame.gizmoScale[0]=2;
    renderer.updateFrame(frame);
    Capture capture;context.commands=&capture;renderer.recordScene(context);
    unsigned selected=0,unselected=0;
    for(const auto& draw:capture.draws) {
        if(draw.first==asset->primitives[0].firstIndex) {
            ++selected;
            if(std::abs(draw.transform[12]-.75F)>.00001F) {
                std::cerr<<"Gizmo translation lost in pass "<<draw.pass<<'\n';
                throw std::runtime_error("Every consuming pass must use the selected primitive transform");
            }
            require(std::abs(draw.transform[0]-std::sqrt(3.F))<.00001F,
                "Every consuming pass must retain gizmo rotation and scale");
        } else if(draw.first==asset->primitives[1].firstIndex) {
            ++unselected;
            require(draw.transform[12]==0&&draw.transform[0]==1,
                "Other primitives must retain their identity gizmo transform");
        }
    }
    require(selected>=6&&unselected>=6,"Fixture must consume both primitives in shadow, mask, surface and outline");
    renderer.onUnload(context);
    require(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,
        "Gizmo opacity fixture must release all renderer resources");
}
}
int main(int argc,char** argv){try {
    require(argc==2,"Expected compiled shader directory");
    for(bool bindless:{false,true})for(bool compute:{false,true})verify(argv[1],bindless,compute);
    std::cout<<"Node opacity consuming passes share selected primitive transforms\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
