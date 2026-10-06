#include "AzureRenderApp.hpp"
#if AZURE_WITH_EDITOR
#include "editor/EditorSession.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "scenes/BuiltinRendererCatalog.hpp"
#include "assets/GeneratorRegistry.hpp"
#include "AzureRenderInternal.hpp"
#include <algorithm>
#include <fstream>
#include <cmath>
#include <set>
#include <stb_image_write.h>
using namespace azurerender;
using namespace azurerender::internal;
namespace {
const char* reloadStateName(ShaderReloadState state){
    switch(state){case ShaderReloadState::Idle:return "Idle";case ShaderReloadState::Compiling:return "Compiling";
    case ShaderReloadState::Ready:return "Ready";case ShaderReloadState::Error:return "Error";case ShaderReloadState::Cancelled:return "Cancelled";}
    throw std::logic_error("Unknown shader reload state");
}
}
RenderViewDescriptor AzureRenderApp::previewDescriptor(const nlohmann::json& parameters){
    RenderContext base;buildRenderContext(base);RenderViewDescriptor descriptor;
    descriptor.rendererId=parameters.value("renderer",std::string(sceneTypeName(renderSettings_.sceneType)));
    if(base.scene.resources.empty() && !parameters.contains("renderer"))descriptor.rendererId="sample";
    descriptor.scene=base.scene;descriptor.settings=effectiveRenderSettings_;
    descriptor.sceneWorldCoordinates=base.sceneWorldCoordinates;
    if(parameters.contains("grade")){
        const auto& grade=parameters.at("grade");auto& result=descriptor.settings.grade;
        if(!grade.is_object())throw EditRejection("Preview grade requires an object");
        const std::set<std::string> fields{"exposureEv","saturation","contrast","tint","toneMappingEnabled"};
        for(const auto& field:grade.items())if(!fields.count(field.key()))throw EditRejection("Unknown preview grade field");
        result.exposureEv=grade.value("exposureEv",result.exposureEv);result.saturation=grade.value("saturation",result.saturation);
        result.contrast=grade.value("contrast",result.contrast);result.tint=grade.value("tint",result.tint);
        result.toneMappingEnabled=grade.value("toneMappingEnabled",result.toneMappingEnabled);validateRenderSettings(descriptor.settings);
    }
    if(parameters.contains("extent")){const auto size=parameters.at("extent").get<std::array<std::uint32_t,2>>();descriptor.extent={size[0],size[1]};}
    if(parameters.contains("position"))descriptor.cameraPosition=parameters.at("position").get<std::array<float,3>>();
    else descriptor.cameraPosition=cameraPosition_;
    if(parameters.contains("target"))descriptor.cameraTarget=parameters.at("target").get<std::array<float,3>>();
    else descriptor.cameraTarget=cameraTarget_;
    // UNORM presentation expects encoded samples; sRGB presentation encodes linear samples.
    descriptor.transfer=(swapchainFormat_==VK_FORMAT_B8G8R8A8_SRGB || swapchainFormat_==VK_FORMAT_R8G8B8A8_SRGB)
        ?ViewColorTransfer::Linear:ViewColorTransfer::SrgbEncoded;
    if(parameters.value("transfer",std::string())=="srgb")descriptor.transfer=ViewColorTransfer::SrgbEncoded;
    return descriptor;
}
void AzureRenderApp::initializeDevtools(){
    RenderContext base;buildRenderContext(base);
    renderViews_=std::make_unique<RenderViewService>(base,[](const std::string& id){return BuiltinRendererCatalog::createRegistry().create(id);},
        [this]{vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(previews)");},engineSettings_->get("developer.maxViews").get<std::size_t>());
    if(!runOptions_.shaderReloadConfig.empty())shaderReloader_=std::make_unique<ShaderHotReloader>(loadShaderReloadOptions(std::filesystem::u8path(runOptions_.shaderReloadConfig)));
    if(!runOptions_.previewViews.empty()){
        const auto path=std::filesystem::u8path(runOptions_.previewViews);
        if(std::filesystem::file_size(path)>2097152)throw std::invalid_argument("Preview descriptors exceed source budget");
        std::ifstream input(path);input>>toolViewDescriptors_;
        if(toolViewDescriptors_.at("schemaVersion")!=1 || !toolViewDescriptors_.at("views").is_array() || toolViewDescriptors_.at("views").size()>3)
            throw std::invalid_argument("Preview descriptors require version 1 and at most three views");
        for(const auto& descriptor:toolViewDescriptors_.at("views")){const auto handle=renderViews_->create(previewDescriptor(descriptor));toolViews_.push_back(handle);renderViews_->request(handle);}
    }
    if(auto session=runOptions_.editorSession){
        previewDocumentRevision_=session->context().revision();
        thumbnails_=std::make_unique<AssetThumbnailService>(*renderViews_);
        session->setDeveloperServices({[this](std::uint64_t value){
            PreviewImage image;const auto description=renderViews_->describe(value);
            if(!description.at("renderCount").get<std::uint64_t>())return image;
            image.handle=value;image.image=renderViews_->sample(value,graphicsSubmission_+1);image.sampler=screenAttachmentSampler_;
            image.width=description.at("extent").at(0).get<std::uint32_t>();image.height=description.at("extent").at(1).get<std::uint32_t>();return image;
        },[this]{return previewOperation("developer.describe",nlohmann::json::object());},
            [this](const std::string& name,const nlohmann::json& parameters){return previewOperation(name,parameters);}});
    }
}
nlohmann::json AzureRenderApp::previewOperation(const std::string& operation,const nlohmann::json& parameters){
    if(operation=="developer.describe"){
        auto views=nlohmann::json::array();for(const auto handle:toolViews_)views.push_back(renderViews_->describe(handle));
        const auto status=shaderReloader_?shaderReloader_->status():ShaderReloadStatus{};
        return {{"schemaVersion",1},{"shaderAvailable",static_cast<bool>(shaderReloader_)},{"shaderState",reloadStateName(status.state)},
            {"shaderGeneration",status.activeGeneration},{"shaderReplacements",shaderReplacements_},{"shaderDiagnostic",status.diagnostic},
            {"views",views},{"camera",cameraPreview_?renderViews_->describe(cameraPreview_):nlohmann::json(nullptr)},
            {"thumbnails",thumbnails_?thumbnails_->describe():nlohmann::json::array()},
            {"completedSubmission",graphicsCompleted_},{"diagnostic",previewDiagnostic_}};
    }
    if(operation=="developer.shader-rebuild"){
        if(!shaderReloader_)throw EditRejection("Shader source service is unavailable");shaderReloader_->requestRebuild();return previewOperation("developer.describe",{});
    }
    if(operation=="developer.shader-cancel"){
        if(!shaderReloader_)throw EditRejection("Shader source service is unavailable");shaderReloader_->cancel();return true;
    }
    if(operation=="preview.create"){
        const auto handle=renderViews_->create(previewDescriptor(parameters));toolViews_.push_back(handle);renderViews_->request(handle);return {{"handle",handle}};
    }
    if(operation=="preview.request"){renderViews_->request(parameters.at("handle").get<std::uint64_t>());return true;}
    if(operation=="preview.inspect")return renderViews_->describe(parameters.at("handle").get<std::uint64_t>());
    if(operation=="preview.capture"){
        const auto handle=parameters.at("handle").get<std::uint64_t>();
        const auto label=parameters.at("label").get<std::string>();
        if(label.empty() || label.size()>128 || label.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=std::string::npos)
            throw EditRejection("Preview label requires a portable basename");
        const auto description=renderViews_->describe(handle);
        vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(view capture)");
        graphicsCompleted_=graphicsSubmission_;renderViews_->complete(graphicsCompleted_);
        auto pixels=renderViews_->readPixels(handle);
        if(description.at("outputFormat")=="rgba8-linear")for(std::size_t i=0;i<pixels.size();++i)if(i%4!=3){
            const auto linear=pixels[i]/255.F;
            const auto encoded=linear<=.0031308F?linear*12.92F:1.055F*std::pow(linear,1.F/2.4F)-.055F;
            pixels[i]=static_cast<unsigned char>(std::clamp(std::round(encoded*255.F),0.F,255.F));
        }
        const auto output=resourceLocator_.captureDirectory()/(label+".png");std::filesystem::create_directories(output.parent_path());
        const auto width=description.at("extent").at(0).get<int>(),height=description.at("extent").at(1).get<int>();
        if(!stbi_write_png(output.u8string().c_str(),width,height,4,pixels.data(),width*4))throw EditRejection("Cannot write preview capture");
        return {{"path",output.u8string()},{"view",description}};
    }
    if(operation=="preview.resize"){
        const auto extent=parameters.at("extent").get<std::array<std::uint32_t,2>>();renderViews_->resize(parameters.at("handle").get<std::uint64_t>(),{extent[0],extent[1]});return true;
    }
    if(operation=="preview.release"){
        const auto handle=parameters.at("handle").get<std::uint64_t>();
        if(std::find(toolViews_.begin(),toolViews_.end(),handle)==toolViews_.end())
            throw EditRejection("Release requires a view owned by preview.create; cached views use their owning service");
        renderViews_->release(handle);
        toolViews_.erase(std::remove(toolViews_.begin(),toolViews_.end(),handle),toolViews_.end());return true;
    }
    if(operation=="preview.camera"){
        if(!parameters.at("enabled").get<bool>()){if(cameraPreview_)renderViews_->release(cameraPreview_);cameraPreview_=0;return nullptr;}
        if(!cameraPreview_){auto descriptor=previewDescriptor(parameters);descriptor.extent={320,180};cameraPreview_=renderViews_->create(descriptor);}
        renderViews_->setCamera(cameraPreview_,parameters.value("position",cameraPosition_),parameters.value("target",cameraTarget_));
        renderViews_->request(cameraPreview_);return {{"handle",cameraPreview_}};
    }
    if(operation=="preview.asset"){
        if(!thumbnails_ || !runOptions_.editorSession->context().isProject())throw EditRejection("Asset thumbnails require a project database");
        auto& assets=runOptions_.editorSession->context().assets();auto id=parameters.at("asset").get<std::string>();
        if(!assets.records().count(id)){
            const auto resources=runOptions_.editorSession->context().resourceStatuses();
            const auto reference=std::find_if(resources.begin(),resources.end(),[&](const auto& value){return value.id==id;});
            if(reference==resources.end())throw EditRejection("Unknown model asset reference");
            const auto record=std::find_if(assets.records().begin(),assets.records().end(),[&](const auto& entry){
                std::error_code error;return std::filesystem::equivalent(entry.second.path,reference->path,error) && !error;});
            if(record==assets.records().end())throw EditRejection("Model reference has no registered asset");
            id=record->first;
        }
        const auto& record=assets.records().at(id);
        if(record.path.extension()!=".gltf" && record.path.extension()!=".glb")throw EditRejection("Asset preview requires a model");
        auto descriptor=previewDescriptor(nlohmann::json::object());descriptor.rendererId="character";descriptor.extent={128,128};descriptor.scene={};
        descriptor.sceneWorldCoordinates=false;
        descriptor.scene.resources.push_back({id,record.path.u8string()});scene::SceneNodeDesc node;node.resourceId=id;descriptor.scene.nodes.push_back(node);
        descriptor.cameraPosition={2.8F,2.1F,3.2F};descriptor.cameraTarget={0,0,0};
        const auto handle=thumbnails_->request({id,std::to_string(record.fingerprint),"128x128-bounds-fit-camera"},descriptor);
        return {{"handle",handle},{"path",record.path.u8string()},{"view",renderViews_->describe(handle)}};
    }
    if(operation=="preview.clear"){invalidatePreviews();return true;}
    throw EditRejection("Unknown development operation");
}
void AzureRenderApp::invalidatePreviews(){
    if(thumbnails_)thumbnails_->clear();if(renderViews_)renderViews_->clear();toolViews_.clear();cameraPreview_=0;
}
void AzureRenderApp::pollDevtools(){
    if(renderViews_)renderViews_->complete(graphicsCompleted_);
    if(auto session=runOptions_.editorSession;session && previewDocumentRevision_!=session->context().revision()){
        invalidatePreviews();previewDocumentRevision_=session->context().revision();
    }
    if(!shaderReloader_)return;
    shaderReloader_->poll();const auto candidate=shaderReloader_->candidate();if(!candidate)return;
    RenderContext context;buildRenderContext(context);context.shaderDirectory=candidate->directory.u8string();
    std::unique_ptr<ISceneRenderer> replacement;
    std::unique_ptr<RenderViewService> nextViews;
    std::unique_ptr<AssetThumbnailService> nextThumbnails;
    auto nextDirectory=candidate->directory.u8string();
    try{
        const auto id=sceneRenderer_?std::string(sceneRenderer_->name()):std::string("sample");
        replacement=BuiltinRendererCatalog::createRegistry().create(id);validateSceneRendererCapabilities(replacement->capabilities());
        replacement->onLoad(context);
        nextViews=std::make_unique<RenderViewService>(context,[](const auto& renderer){return BuiltinRendererCatalog::createRegistry().create(renderer);},
            [this]{vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(previews)");},engineSettings_->get("developer.maxViews").get<std::size_t>());
        if(runOptions_.editorSession)nextThumbnails=std::make_unique<AssetThumbnailService>(*nextViews);
        vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(shader candidate)");graphicsCompleted_=graphicsSubmission_;
        invalidatePreviews();if(renderViews_)renderViews_->complete(graphicsCompleted_);
        if(editorLayer_)editorLayer_->completePreviewTextures(graphicsCompleted_);
        if(preparedRenderer_){RenderContext previous;buildRenderContext(previous);preparedRenderer_->onUnload(previous);preparedRenderer_.reset();preparedRendererReady_=false;}
        RenderContext previous;buildRenderContext(previous);sceneRenderer_->onUnload(previous);sceneRenderer_=std::move(replacement);
        activeShaderDirectory_.swap(nextDirectory);
        thumbnails_.reset();renderViews_.swap(nextViews);thumbnails_.swap(nextThumbnails);
        shaderReloader_->finishCandidate(true,{});++shaderReplacements_;
    }catch(const std::exception& error){
        if(replacement){try{replacement->onUnload(context);}catch(...){}}
        shaderReloader_->finishCandidate(false,error.what());RuntimeDiagnostics::instance().info("shader-reload",error.what());
    }
}
void AzureRenderApp::finishDevtools(){
    if(!renderViews_)return;
    vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(preview capture)");graphicsCompleted_=graphicsSubmission_;renderViews_->complete(graphicsCompleted_);
    devtoolsReport_=previewOperation("developer.describe",{});
    if(!runOptions_.previewViews.empty()){
        const auto output=std::filesystem::u8path(runOptions_.runtimeReportPath).parent_path()/"preview-images";
        std::filesystem::create_directories(output);
        for(std::size_t i=0;i<toolViews_.size();++i){
            const auto description=renderViews_->describe(toolViews_[i]);const auto pixels=renderViews_->readPixels(toolViews_[i]);
            const auto width=description.at("extent").at(0).get<int>(),height=description.at("extent").at(1).get<int>();
            const auto image=output/(std::to_string(i)+".png");
            if(!stbi_write_png(image.u8string().c_str(),width,height,4,pixels.data(),width*4))throw std::runtime_error("Cannot write preview capture");
        }
        devtoolsReport_["captures"]=toolViews_.size();
    }
}
#endif
