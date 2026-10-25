#include <algorithm>
#include "scenes/CharacterSceneRenderer.hpp"
#include "rhi/NullRhi.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>

using namespace azurerender;
namespace {
constexpr auto noTile=std::numeric_limits<std::uint32_t>::max();
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class T>std::uint64_t handleValue(T value){
    if constexpr(std::is_pointer_v<T>)return reinterpret_cast<std::uintptr_t>(value);
    else return static_cast<std::uint64_t>(value);
}
class Capture final:public rhi::ICommandRecorder {
public:
    struct Draw {std::uint32_t first,count,tile;float opacity;bool fresh;};
    struct Pipeline {unsigned cull,front;};
    VkRenderPass maskPass=VK_NULL_HANDLE,pass=VK_NULL_HANDLE;
    VkRenderPass opaqueDepthPass=VK_NULL_HANDLE;
    unsigned ordinaryDepthDraws=0,depthOnlyDraws=0;
    unsigned prefillDraws=0;
    std::set<std::uint64_t> prefillPipelines;
    std::vector<bool> snapshotPrefillOrder;
    std::set<std::uint64_t> depthOnlyPipelines;
    std::set<std::uint64_t> outlinePipelines;
    std::set<std::uint64_t> depthShellPipelines;
    std::vector<Draw> depthShells;
    std::map<std::uint64_t,Pipeline> pipelineDescriptions;
    std::vector<Pipeline> coveragePipelines;
    std::uint64_t pipeline=0;
    std::uint32_t columns=0,tile=0;
    std::array<unsigned char,212> constants{};
    bool fresh=false;
    std::vector<Draw> outlines;
    std::map<std::uint32_t,std::set<std::uint32_t>> covered;
    void beginRenderPass(const rhi::RenderPassBeginDesc& desc)override{pass=desc.renderPass;}
    void endRenderPass()override{pass=VK_NULL_HANDLE;}
    void setViewport(float,float,float x=0,float y=0)override{
        tile=static_cast<unsigned>(y/192)*columns+static_cast<unsigned>(x/256);
    }
    void setScissor(VkExtent2D,VkOffset2D={})override{}
    void bindComputePipeline(VkPipeline)override{}
    void bindComputeDescriptorSet(VkPipelineLayout,VkDescriptorSet)override{}
    void bindPipeline(VkPipeline value)override{pipeline=handleValue(value);}
    void bindDescriptorSet(VkPipelineLayout,VkDescriptorSet)override{}
    void pushConstants(VkPipelineLayout,VkShaderStageFlags,unsigned offset,const void* data,unsigned size)override{
        require(offset+size<=constants.size(),"Push constants exceed the public range");
        std::memcpy(constants.data()+offset,data,size);
        if(offset==136&&size==8)fresh=true;
    }
    void bindVertexBuffer(VkBuffer,VkDeviceSize)override{}
    void bindIndexBuffer(VkBuffer,VkDeviceSize)override{}
    void draw(unsigned)override{}
    void drawIndexed(unsigned,unsigned,unsigned count=1,unsigned first=0)override{
        if(pass==opaqueDepthPass&&pass!=VK_NULL_HANDLE&&!depthShellPipelines.count(pipeline)){
            ++ordinaryDepthDraws;if(depthOnlyPipelines.count(pipeline))++depthOnlyDraws;
            const bool prefill=prefillPipelines.count(pipeline)!=0;prefillDraws+=prefill;snapshotPrefillOrder.push_back(prefill);
        }
        if(pass==maskPass&&pass!=VK_NULL_HANDLE){
            require(count==1,"Coverage tiles must rasterize only their owning instance");
            covered[first].insert(tile);
            coveragePipelines.push_back(pipelineDescriptions.at(pipeline));
        }
        if(outlinePipelines.count(pipeline)){
            Draw draw{first,count,0,0,fresh};
            std::memcpy(&draw.tile,constants.data()+136,4);
            std::memcpy(&draw.opacity,constants.data()+140,4);
            outlines.push_back(draw);
        }
        if(depthShellPipelines.count(pipeline)) {
            Draw draw{first,count,0,0,fresh};
            std::memcpy(&draw.tile,constants.data()+136,4);
            std::memcpy(&draw.opacity,constants.data()+140,4);
            depthShells.push_back(draw);
        }
        fresh=false;
    }
    void drawIndexedIndirect(VkBuffer,VkDeviceSize,unsigned,unsigned)override{}
    void dispatch(unsigned,unsigned,unsigned)override{}
    void imageBarrier(const rhi::ImageBarrierDesc&)override{}
    void bufferBarrier(const rhi::BufferBarrierDesc&)override{}
    void clearColorImage(VkImage,VkImageLayout,const VkClearColorValue&)override{}
    void copyImageToBuffer(VkImage,VkBuffer,VkExtent2D)override{}
    void writeTimestamp(VkQueryPool,unsigned,VkPipelineStageFlagBits)override{}
};

enum class Kind {Explicit,Legacy,Fading,Hidden,Blend,Platform,Mixed,DoubleSided,Mirrored,GizmoMirrored,CullingDisabled,HiddenExplicitLegacy,
    Projected,DepthOnly,ProjectedUv1,ProjectedMixed,ProjectedFading,ProjectedPlatform,ColorOnly,RefractionConsumer,TintConsumer,MaskedEye,ReflectionConsumer,ReflectionMirrored,ReflectionDoubleSided,ReflectionFading,ReflectionMasked,ReflectionNoDepth};
void verify(const std::filesystem::path& shaders,Kind kind,bool bindless,bool compute,bool multisample=false){
    rhi::NullRhi backend;
    if(multisample){backend.supportedSamples=VK_SAMPLE_COUNT_1_BIT|VK_SAMPLE_COUNT_4_BIT;backend.sampleShadingSupported=true;}
    CharacterSceneRenderer renderer;
    RenderSettings settings;
    settings.faceSdfFeature=false;settings.showcasePlatformFeature=false;
    if(kind==Kind::ProjectedPlatform)settings.showcasePlatformFeature=true;
    settings.characterPresentation.platformEnabled=false;
    settings.innerOutlineEnabled=false;
    const bool reflection=kind==Kind::ReflectionConsumer||kind==Kind::ReflectionMirrored||kind==Kind::ReflectionDoubleSided
        ||kind==Kind::ReflectionFading||kind==Kind::ReflectionMasked||kind==Kind::ReflectionNoDepth;
    settings.screenSpaceReflection.enabled=reflection;
    settings.silhouetteOutlineEnabled=kind==Kind::Legacy||kind==Kind::Fading||kind==Kind::Mixed||kind==Kind::HiddenExplicitLegacy;
    settings.sceneLightingEnabled=!settings.silhouetteOutlineEnabled;
    RenderContext context;
    context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=shaders.string();
    context.renderExtent=context.swapchainExtent={256,192};context.cullingEnabled=false;context.depthTestEnabled=kind!=Kind::ReflectionNoDepth;
    context.bindlessTextures=bindless;context.computeSkinning=compute;context.renderSettings=&settings;
    context.sceneWorldCoordinates=true;
    context.rasterizationSamples=multisample?VK_SAMPLE_COUNT_4_BIT:VK_SAMPLE_COUNT_1_BIT;
    context.depthFormat=VK_FORMAT_D32_SFLOAT;
    context.faceCullingEnabled=kind!=Kind::CullingDisabled;
    auto& limits=context.descriptorLimits;limits.maxImageDimension2D=4096;
    limits.maxPerStageDescriptorSamplers=limits.maxDescriptorSetSamplers=4096;
    limits.maxPerStageDescriptorSampledImages=limits.maxDescriptorSetSampledImages=4096;
    limits.maxPerStageResources=4096;
    const auto source=std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR);
    context.rampAtlasPath=(source/"assets_public/toon_ramp_atlas.ppm").string();
    auto mesh=loadGltfAsset((source/"assets_public/scene_inspector/assets/cube.gltf").string());
    mesh.primitives.resize(1);mesh.primitives.front().materialIndex=0;mesh.materials.resize(1);
    mesh.hasSkin=false;mesh.animations.clear();mesh.morphRanges.clear();mesh.morphTargetCount=0;
    auto& material=mesh.materials.front();material.showcasePlatform=0;
    material.materialFeatures=MaterialFeatureNeutralFallback;
    material.materialClass=AssetMaterialClass::Generic;
    material.alphaMode=AssetAlphaMode::Opaque;
    material.doubleSided=kind==Kind::DoubleSided||kind==Kind::ReflectionDoubleSided;
    const bool explicitOutline=kind!=Kind::Legacy&&kind!=Kind::Fading;
    if(explicitOutline)material.materialFeatures|=MaterialFeatureSilhouetteOutline;
    const bool projection=kind==Kind::Projected||kind==Kind::DepthOnly||kind==Kind::ProjectedUv1
        ||kind==Kind::ProjectedMixed||kind==Kind::ProjectedFading||kind==Kind::ProjectedPlatform||kind==Kind::ColorOnly||kind==Kind::MaskedEye;
    if(projection) {
        material.outlineSurface.present=true;material.outlineSurface.depthPrepass=kind!=Kind::ColorOnly;
        material.outlineSurface.width=.75F;
        if(kind==Kind::DepthOnly)material.materialFeatures&=~MaterialFeatureSilhouetteOutline;
        if(kind==Kind::ProjectedUv1||kind==Kind::ProjectedPlatform) {
            material.outlineSurface.normalSource=AssetMaterial::OutlineSurface::NormalSource::TangentUv1;
            mesh.texcoord1.resize(mesh.vertices.size(),{.25F,-.25F});mesh.primitives.front().hasTexcoord1=true;
        }
    }
    if(kind==Kind::RefractionConsumer)material.materialFeatures|=MaterialFeatureSceneRefraction;
    if(kind==Kind::TintConsumer)material.materialFeatures|=MaterialFeatureSceneTint;
    if(kind==Kind::MaskedEye){material.alphaMode=AssetAlphaMode::Mask;material.materialClass=AssetMaterialClass::Eye;material.eyeSurface.present=true;material.materialFeatures|=MaterialFeatureEyeRadialSurface;}
    if(kind==Kind::ReflectionMasked)material.alphaMode=AssetAlphaMode::Mask;
    if(kind==Kind::Blend)material.alphaMode=AssetAlphaMode::Blend;
    if(kind==Kind::Platform)material.showcasePlatform=1;
    for(auto& node:mesh.nodes)node.opacity=(kind==Kind::Fading||kind==Kind::ProjectedFading||kind==Kind::ReflectionFading)?.5F:kind==Kind::Hidden?0.F:1.F;
    if(kind==Kind::HiddenExplicitLegacy){
        mesh.nodes.front().opacity=0;
        auto node=mesh.nodes.front();node.parent=-1;node.opacity=1;
        const auto legacyNode=static_cast<std::uint32_t>(mesh.nodes.size());mesh.nodes.push_back(node);
        auto legacyMaterial=material;legacyMaterial.materialFeatures=MaterialFeatureNeutralFallback;
        auto primitive=mesh.primitives.front();primitive.nodeIndex=legacyNode;primitive.materialIndex=1;
        mesh.primitives.front().nodeIndex=0;mesh.primitives.push_back(primitive);mesh.materials.push_back(legacyMaterial);
    }
    std::map<std::string,std::shared_ptr<const LoadedAsset>> prepared;
    const auto add=[&](const std::string& id,const LoadedAsset& asset){
        const auto path="coverage-fixture/"+id+".gltf";
        prepared[path]=std::make_shared<const LoadedAsset>(asset);
        context.scene.resources.push_back({id,path});
        for(unsigned i=0;i<2;++i){scene::SceneNodeDesc node;node.id=id+"-"+std::to_string(i);
            node.resourceId=id;node.translation[0]=i?.35F:-.35F;context.scene.nodes.push_back(node);}
    };
    add("subject",mesh);
    if(kind==Kind::Mirrored||kind==Kind::ReflectionMirrored)for(auto& node:context.scene.nodes)node.scale[0]=-1;
    if(kind==Kind::Mixed){auto legacy=mesh;legacy.materials.front().materialFeatures=MaterialFeatureNeutralFallback;add("legacy",legacy);}
    if(kind==Kind::ProjectedMixed) {
        auto second=mesh;second.materials.front().outlineSurface.width=1.25F;
        add("second-projection",second);
    }
    context.preparedMeshes=&prepared;
    context.sceneRenderPass=backend.createRenderPass({});
    context.shadowRenderPass=backend.createRenderPass({});
    renderer.onLoad(context);
    SceneFrameData frame;frame.renderSettings=&settings;frame.cameraPosition[2]=3;
    if(kind==Kind::GizmoMirrored){frame.gizmoActive=true;frame.selectedPrimitiveIndex=0;frame.gizmoScale[0]=-1;}
    frame.swapchainWidth=256;frame.swapchainHeight=192;renderer.updateFrame(frame);
    Capture capture;
    for(const auto& call:backend.calls)if(call.name=="createGraphicsPipeline"){
        const auto handle=std::stoull(call.detail.substr(0,call.detail.find(' ')),nullptr,16);
        const auto cull=std::stoul(call.detail.substr(call.detail.find(" cull=")+6));
        const auto front=std::stoul(call.detail.substr(call.detail.find(" front=")+7));
        capture.pipelineDescriptions.emplace(handle,Capture::Pipeline{static_cast<unsigned>(cull),static_cast<unsigned>(front)});
        if(cull!=VK_CULL_MODE_FRONT_BIT&&call.detail.find(" masks=0/0/")!=std::string::npos){
            capture.prefillPipelines.insert(handle);
            require(call.detail.find(" depthBias=1.000000/0.000000")!=std::string::npos,
                "Opaque prefill must conservatively bias depth away from strict LESS color draws");
        }
        if(cull!=VK_CULL_MODE_FRONT_BIT&&call.detail.find(" masks=0/1/")!=std::string::npos)capture.depthOnlyPipelines.insert(handle);
        if(cull==VK_CULL_MODE_FRONT_BIT) {
            if(call.detail.find(" masks=0/1/")!=std::string::npos)capture.depthShellPipelines.insert(handle);
            else capture.outlinePipelines.insert(handle);
        }
    }
    require(capture.outlinePipelines.size()==2,"Fixture must identify both public outline winding pipelines");
    RenderGraph graph;
    SceneGraphResources resources{graph.addResource("scene-color"),graph.addResource("depth"),
        graph.addResource("normal"),graph.addResource("shadow")};
    renderer.registerPasses(graph,resources,context);
    unsigned maskPasses=0;
    for(const auto& pass:graph.passes())if(pass.name=="character-outline-mask"){
        ++maskPasses;capture.maskPass=pass.renderPass.renderPass;capture.columns=pass.renderPass.extent.width/256;
    }
    for(const auto& pass:graph.passes())if(pass.name=="character-opaque-depth")capture.opaqueDepthPass=pass.renderPass.renderPass;
    require(capture.depthShellPipelines.size()==(projection?2U:0U),"Only projection materials allocate depth shell pipelines");
    const bool masks=kind!=Kind::Legacy&&kind!=Kind::Hidden&&kind!=Kind::Blend&&kind!=Kind::Platform&&kind!=Kind::HiddenExplicitLegacy&&kind!=Kind::DepthOnly;
    if(multisample){
        for(const auto pass:{capture.opaqueDepthPass,capture.maskPass}){
            bool sampled=false;
            for(const auto& call:backend.calls)if(call.name=="createRenderPass"&&std::stoull(call.detail.substr(0,call.detail.find(' ')),nullptr,16)==handleValue(pass))
                sampled=call.detail.find(" samples=4")!=std::string::npos;
            require(sampled,"Snapshot and outline coverage passes must match the four-sample scene");
        }
        require(std::any_of(graph.passes().begin(),graph.passes().end(),[](const auto& pass){return pass.name=="character-opaque-resolve";}),
            "Snapshot colors and nearest depth require a shared resolve producer");
    }
    require(maskPasses==unsigned(masks||multisample),"Explicit opaque silhouettes and fading instances require a mask pass");
    std::string error;require(graph.compile(error),"Fixture render graph must compile");graph.execute(&capture);
    const bool colorConsumer=kind==Kind::RefractionConsumer||kind==Kind::TintConsumer||reflection;
    require(capture.prefillPipelines.size()==3,"Snapshot prefill must support ordinary, mirrored and double-sided winding");
    if(colorConsumer&&kind!=Kind::ReflectionMasked&&kind!=Kind::ReflectionNoDepth){
        require(capture.prefillDraws>0&&capture.ordinaryDepthDraws==2*capture.prefillDraws,
            "Color snapshots must prefill opaque coverage before fully shading the same geometry");
        const auto firstColor=std::find(capture.snapshotPrefillOrder.begin(),capture.snapshotPrefillOrder.end(),false);
        require(firstColor!=capture.snapshotPrefillOrder.end()&&std::none_of(firstColor,capture.snapshotPrefillOrder.end(),[](bool v){return v;}),
            "Opaque prefill must finish before the first full color draw");
    }else require(capture.prefillDraws==0,"Depth consumers and transformed masked eyes retain their single coverage pass");
    if(colorConsumer||kind==Kind::MaskedEye)require(capture.ordinaryDepthDraws>0&&capture.depthOnlyDraws==0,"Color consumers retain fully shaded snapshots");
    else require(capture.depthOnlyDraws==capture.ordinaryDepthDraws,"Depth consumers must avoid unused snapshot color shading");
    const bool visible=kind!=Kind::Hidden&&kind!=Kind::Blend&&kind!=Kind::Platform&&kind!=Kind::DepthOnly;
    const unsigned instances=kind==Kind::ProjectedMixed?4:2;
    require(capture.covered.size()==(masks?instances:0U),"Only eligible visible instances may own coverage tiles");
    std::set<unsigned> tiles;
    for(const auto& [instance,coverage]:capture.covered){
        require(coverage.size()==1,"Each instance must rasterize exactly its own tile");
        tiles.insert(*coverage.begin());
    }
    require(tiles.size()==(masks?instances:0U),"Overlapping instances must retain distinct tiles");
    for(const auto& pipeline:capture.coveragePipelines){
        const bool mirrored=kind==Kind::Mirrored||kind==Kind::GizmoMirrored||kind==Kind::ReflectionMirrored;
        const auto cull=kind==Kind::DoubleSided||kind==Kind::CullingDisabled||kind==Kind::ReflectionDoubleSided?VK_CULL_MODE_NONE:VK_CULL_MODE_BACK_BIT;
        const auto front=mirrored?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
        require(pipeline.cull==static_cast<unsigned>(cull)&&pipeline.front==static_cast<unsigned>(front),
            "Coverage must share primary opaque double-sided and transform parity rules");
    }
    unsigned outlined=0,unmaskedBatches=0;
    for(const auto& draw:capture.outlines){
        outlined+=draw.count;
        require(draw.fresh,"Every outline draw must push its tile and opacity explicitly");
        require(draw.opacity==((kind==Kind::Fading||kind==Kind::ProjectedFading||kind==Kind::ReflectionFading)?.5F:1.F),"Outline opacity must match the primitive run");
        if(draw.tile==noTile){
            ++unmaskedBatches;
            require(draw.count==2,"Consecutive fully opaque legacy instances must retain their original batch");
            for(unsigned i=0;i<draw.count;++i)
                require(!capture.covered.count(draw.first+i),"A rasterized explicit instance must select its tile");
        }else{
            require(draw.count==1,"Distinct coverage tiles require individual outline draws");
            const auto coverage=capture.covered.find(draw.first);
            require(coverage!=capture.covered.end()&&coverage->second.count(draw.tile),
                "Outline sampling requires a rasterized tile owned by that instance");
        }
    }
    require(outlined==(visible?(kind==Kind::Mixed?4U:instances):0U),"Every eligible fixture instance must retain its outline");
    unsigned depthInstances=0;
    for(const auto& draw:capture.depthShells) {
        require(draw.fresh&&draw.tile==noTile,"Depth shells require independent coverage without a color-mask tile");
        require(draw.opacity==(kind==Kind::ProjectedFading?.5F:1.F),"Depth shells must consume the current opacity run");
        depthInstances+=draw.count;
    }
    require(depthInstances==((projection&&kind!=Kind::ColorOnly)?instances:0U),"Depth participation must be independent of color outlines");
    if(kind==Kind::ColorOnly)require(capture.ordinaryDepthDraws==0,"Color-only shells without depth consumers must skip ordinary depth rendering");
    bool projectionBuffer=false,vertexMask=false;
    for(const auto& layout:backend.descriptorLayouts)for(const auto& binding:layout) {
        if(binding.binding==30){projectionBuffer=true;require(binding.stages==VK_SHADER_STAGE_VERTEX_BIT,"Projection storage is vertex-only");}
        if(binding.binding==31){vertexMask=true;require(binding.stages==VK_SHADER_STAGE_VERTEX_BIT,"Width mask is vertex-only");}
    }
    require(projectionBuffer==projection&&vertexMask==projection,"Legacy layouts must retain their optional resource boundary");
    require(unmaskedBatches==unsigned(kind==Kind::Legacy||kind==Kind::Mixed||kind==Kind::HiddenExplicitLegacy),
        "Legacy and explicit draw states must retain separate ownership");
    renderer.onUnload(context);
    std::set<std::uint64_t> created,destroyed;
    for(const auto& call:backend.calls){
        if(call.name=="createGraphicsPipeline"||call.name=="createComputePipeline")
            created.insert(std::stoull(call.detail.substr(0,call.detail.find(' ')),nullptr,16));
        if(call.name=="destroyPipeline")destroyed.insert(std::stoull(call.detail,nullptr,16));
    }
    require(created==destroyed,"Every winding and double-sided coverage pipeline must retire with the renderer");
    require(!backend.allocator().statistics().liveImages&&!backend.allocator().statistics().liveBuffers,
        "Coverage fixture must release every GPU resource");
}
}
int main(int argc,char** argv){try{
    require(argc==2,"Expected compiled shader directory");
    for(bool multisample:{false,true})for(bool bindless:{false,true})for(bool compute:{false,true})
        for(auto kind:{Kind::Explicit,Kind::Legacy,Kind::Fading,Kind::Hidden,Kind::Blend,Kind::Platform,Kind::Mixed,
                Kind::DoubleSided,Kind::Mirrored,Kind::GizmoMirrored,Kind::CullingDisabled,Kind::HiddenExplicitLegacy,
                Kind::Projected,Kind::DepthOnly,Kind::ProjectedUv1,Kind::ProjectedMixed,Kind::ProjectedFading,Kind::ProjectedPlatform,Kind::ColorOnly,Kind::RefractionConsumer,Kind::TintConsumer,Kind::MaskedEye,Kind::ReflectionConsumer,Kind::ReflectionMirrored,Kind::ReflectionDoubleSided,Kind::ReflectionFading,Kind::ReflectionMasked,Kind::ReflectionNoDepth})
            verify(argv[1],kind,bindless,compute,multisample);
    std::cout<<"Explicit silhouettes retain independent coverage and legacy batching\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
