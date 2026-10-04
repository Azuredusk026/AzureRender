#include "app/AzureRenderApp.hpp"
#include "app/AzureRenderInternal.hpp"
#include "extensions/ISceneRenderer.hpp"
#include "scenes/BuiltinRendererCatalog.hpp"
#include "platform/GlfwFrontend.hpp"
#if AZURE_WITH_EDITOR
#include "editor/EditorSession.hpp"
#include "editor/EditorAutomation.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#endif
using namespace azurerender::internal;
azurerender::RuntimeLifecycle* AzureRenderApp::activeRuntime(){
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)return runOptions_.editorSession->runtime();
#endif
    return &runtime_;
}
azurerender::GameRuntime* AzureRenderApp::activeGame(){
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)return runOptions_.editorSession->game();
#endif
    return gameRuntime_.get();
}
azurerender::ScriptRuntime* AzureRenderApp::activeScripts(){
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)return runOptions_.editorSession->scripts();
#endif
    return scriptRuntime_.get();
}
azurerender::PresentationRuntime* AzureRenderApp::activePresentation(){
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession)return runOptions_.editorSession->presentation();
#endif
    return presentationRuntime_.get();
}
azurerender::AssetDatabase* AzureRenderApp::activeAssets(){
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession){auto* levels=runOptions_.editorSession->levels();return levels?&levels->assets():nullptr;}
#endif
    return levelSession_?&levelSession_->assets():nullptr;
}
void AzureRenderApp::prepareLevelRenderer(const azurerender::Level& level){
    vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(level preparation)");
    const auto previousSettings=renderSettings_;azurerender::RenderContext context;buildRenderContext(context);context.scene=level.scene.renderDescription();
    auto registry=azurerender::BuiltinRendererCatalog::createRegistry();auto prepared=registry.create(azurerender::sceneTypeName(level.scene.renderSettings.sceneType));
    try{renderSettings_=level.scene.renderSettings;context.renderSettings=&renderSettings_;azurerender::validateSceneRendererCapabilities(prepared->capabilities());prepared->onLoad(context);}
    catch(...){prepared->onUnload(context);renderSettings_=previousSettings;throw;}
    if(sceneRenderer_)sceneRenderer_->onUnload(context);sceneRenderer_=std::move(prepared);
}
void AzureRenderApp::synchronizeEditorRuntime(){
#if AZURE_WITH_EDITOR
    if(!runOptions_.editorSession)return;
    auto& session=*runOptions_.editorSession;
    if(editorAutomation_)editorAutomation_->advance(gameplayFrame_,session);
    std::string resources;for(const auto& resource:session.viewScene().resources)resources+=resource.id+resource.path.generic_string();
    for(const auto& node:session.viewScene().nodes)resources+=node.id+":"+node.resourceId+";";
    if(!session.playing() && resources!=editorResourceSignature_){editorResourceSignature_=resources;static_cast<void>(session.execute(azurerender::EditorCommand::ReloadAssets));}
    if(session.consumeRuntimeReset()){
        vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(editor runtime)");gameUi_.reset();gameUiPath_.clear();
        azurerender::RenderContext context;buildRenderContext(context);if(sceneRenderer_){sceneRenderer_->onUnload(context);sceneRenderer_.reset();}
        renderSettings_=session.viewScene().renderSettings;createSceneRenderer();
        if(session.playing()){if(session.levels())session.levels()->setPrepareHandler([this](const auto& level){prepareLevelRenderer(level);});}
        else session.context().attachRenderSettings(renderSettings_);
    }
    if(!session.playing())session.context().attachRenderSettings(renderSettings_);
    if(!session.playing() && session.context().reloadChangedAssets())static_cast<void>(session.execute(azurerender::EditorCommand::ReloadAssets));
#endif
}
void AzureRenderApp::synchronizeGameUi(){
    auto* presentation=activePresentation();auto* assets=activeAssets();const auto reference=presentation?presentation->uiDocument():std::string();
    const auto path=assets&&!reference.empty()?assets->resolveReference(reference).generic_string():std::string();
    if(path!=gameUiPath_){
        gameUi_.reset();gameUiPath_.clear();
        if(!path.empty()){
            if(!gameUiRenderer_)gameUiRenderer_=std::make_unique<azurerender::GameUiRenderer>(*rhi_,postProcessRenderPass_,resourceLocator_.shaderDirectory());
            gameUi_=std::make_unique<azurerender::GameUi>(path,resourceLocator_.publicAsset("fonts/LatoLatin-Regular.ttf"),*gameUiRenderer_);gameUiPath_=path;
            gameUi_->setActionHandler([this](std::string action){auto* runtime=activeRuntime();if(!runtime)return;if(action=="pause"){if(runtime->state()==azurerender::RuntimeLifecycle::State::Running)runtime->pause();else if(runtime->state()==azurerender::RuntimeLifecycle::State::Paused)runtime->resume();}});
        }
    }
    if(auto* scripts=activeScripts())scripts->setUiHandler([this](std::string id,std::string text){if(gameUi_)gameUi_->setText(id,text);});
#if AZURE_WITH_EDITOR
    if(editorLayer_)editorLayer_->setGameUi(gameUi_.get());
#endif
}
void AzureRenderApp::updateGameUi(double delta){
    synchronizeGameUi();
    if(auto* presentation=activePresentation()){
        animationFrames_+=presentation->animations().size();audioStarts_=std::max(audioStarts_,presentation->audioStarts());
        for(const auto& error:presentation->errors())if(std::find(presentationErrors_.begin(),presentationErrors_.end(),error)==presentationErrors_.end())presentationErrors_.push_back(error);
    }
    if(!gameUi_)return;
    gameUiRenderer_->beginFrame(++uiSerial_,renderExtent_);
    float densityX=1,densityY=1;glfwGetWindowContentScale(frontend_->nativeHandle(),&densityX,&densityY);
    gameUi_->resize(static_cast<int>(renderExtent_.width),static_cast<int>(renderExtent_.height),densityX);
#if AZURE_WITH_EDITOR
    if(!runOptions_.editorSession)
#endif
    {
        double x=0,y=0;int width=1,height=1;glfwGetCursorPos(frontend_->nativeHandle(),&x,&y);glfwGetWindowSize(frontend_->nativeHandle(),&width,&height);
        gameUi_->pointer(static_cast<int>(x*renderExtent_.width/std::max(width,1)),static_cast<int>(y*renderExtent_.height/std::max(height,1)),glfwGetMouseButton(frontend_->nativeHandle(),GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS);
    }
    gameUi_->update(delta);gameUi_->render();
}
