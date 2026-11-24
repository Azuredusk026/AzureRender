#include "app/AzureRenderApp.hpp"
#include "app/AzureRenderInternal.hpp"
#include "ecs/GameComponents.hpp"
#include "runtime/EngineSettings.hpp"
#include "extensions/ISceneRenderer.hpp"
#include "scenes/BuiltinRendererCatalog.hpp"
#include "platform/GlfwFrontend.hpp"
#include <algorithm>
#include <utility>
#if AZURE_WITH_EDITOR
#include "editor/EditorSession.hpp"
#include "app/EditorSceneReload.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "editor/EditorAutomation.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#endif
using namespace azurerender::internal;
namespace {
const azurerender::game::WindowPresentation* windowPresentation(azurerender::RuntimeLifecycle* runtime) {
    const auto* windows = runtime ? runtime->world().findComponentArray<azurerender::game::WindowPresentation>() : nullptr;
    return windows && !windows->dense().empty() ? &windows->dense().front() : nullptr;
}
bool hasGlitchCamera(const azurerender::Level& level) {
    return std::any_of(level.components.begin(),level.components.end(),[](const auto& node) {
        return node.second.contains("azure.screen-glitch");
    });
}
}
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
azurerender::IScriptRuntime* AzureRenderApp::activeScripts(){
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
std::unique_ptr<azurerender::RenderViewService> AzureRenderApp::createRoomGlitchView(
    const azurerender::RenderContext& context,const std::string& rendererId,
    azurerender::RenderViewHandle& handle,bool deferLoad) {
    azurerender::RenderViewDescriptor descriptor;
    descriptor.rendererId=rendererId;
    descriptor.extent={512,288};
    descriptor.scene=context.scene;
    descriptor.sceneWorldCoordinates=context.sceneWorldCoordinates;
    descriptor.useFrameScene=true;
    descriptor.settings=*context.renderSettings;
    descriptor.settings.antiAliasing=1;
    descriptor.transfer=azurerender::ViewColorTransfer::Linear;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_,&properties);
    glitchVendorId_=properties.vendorID;
    auto base=context;base.preparedMeshes=nullptr;base.renderSettings=nullptr;base.scene={};
    auto views=std::make_unique<azurerender::RenderViewService>(std::move(base),
        [](const auto& id){return azurerender::BuiltinRendererCatalog::createRegistry().create(id);},
        [this]{vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(room glitch camera)");},1);
    handle=deferLoad?views->createDeferred(descriptor):views->create(descriptor);
    return views;
}
bool AzureRenderApp::preloadRoomGlitchView(const azurerender::Level& level,double budgetMs) {
    if(!hasGlitchCamera(level))return true;
    const auto key=std::make_pair(level.resourceKey,level.reloadKey());
    if(preparedGlitchKey_!=key)releasePreparedGlitchView();
    try{
        if(!preparedGlitchViews_){
            azurerender::RenderContext context;buildRenderContext(context);
            context.scene=level.scene.renderDescription();context.sceneWorldCoordinates=true;
            const auto settings=azurerender::resolveRenderSettings(*engineSettings_,level.scene.renderSettings);
            context.renderSettings=&settings;
            preparedGlitchViews_=createRoomGlitchView(context,
                azurerender::BuiltinRendererCatalog::documentRendererId(level.scene.renderSettings.sceneType,!context.scene.resources.empty()),
                preparedGlitchView_,true);
            preparedGlitchKey_=key;
        }
        return preparedGlitchViews_->prepare(preparedGlitchView_,&level.preparedMeshes,budgetMs);
    }catch(...){releasePreparedGlitchView();throw;}
}
bool AzureRenderApp::preloadLevelRenderer(const azurerender::Level& level,bool allowActiveReuse){
    if(allowActiveReuse && rendererResourceKey_==level.resourceKey && renderSettings_.sceneType==level.scene.renderSettings.sceneType && sceneRenderer_ && sceneRenderer_->name()==azurerender::BuiltinRendererCatalog::documentRendererId(level.scene.renderSettings.sceneType,!level.scene.resources.empty())){
        return preloadRoomGlitchView(level,4.0);
    }
    azurerender::RenderContext context;buildRenderContext(context);context.scene=level.scene.renderDescription();context.preparedMeshes=&level.preparedMeshes;
    if(!preparedRenderer_ || preparedResourceKey_!=level.resourceKey || preparedRenderSettings_.sceneType!=level.scene.renderSettings.sceneType){
        releasePreparedRenderer();
        auto registry=azurerender::BuiltinRendererCatalog::createRegistry();
        preparedRenderer_=registry.create(azurerender::BuiltinRendererCatalog::documentRendererId(level.scene.renderSettings.sceneType,!context.scene.resources.empty()));
        preparedRenderSettings_=azurerender::resolveRenderSettings(*engineSettings_,level.scene.renderSettings);preparedResourceKey_=level.resourceKey;preparedRendererReady_=false;
    }
    if(!preparedRendererReady_){
        context.renderSettings=&preparedRenderSettings_;
        try{preparedRendererReady_=preparedRenderer_->prepareLoad(context,4.0);}
        catch(...){preparedRenderer_->onUnload(context);preparedRenderer_.reset();preparedResourceKey_.clear();throw;}
    }
    return preparedRendererReady_ && preloadRoomGlitchView(level,4.0);
}
void AzureRenderApp::prepareLevelRenderer(const azurerender::Level& level){
    const auto publishGlitch=[&] {
        const bool camera=hasGlitchCamera(level);
        if(camera)while(!preloadRoomGlitchView(level,4.0)){}
        clearRoomGlitch();
        if(camera){
            roomGlitchViews_=std::move(preparedGlitchViews_);
            roomGlitchView_=std::exchange(preparedGlitchView_,0);
            preparedGlitchKey_={};
        }
    };
#if AZURE_WITH_EDITOR
    invalidatePreviews();
#endif
    azurerender::RenderContext context;buildRenderContext(context);context.scene=level.scene.renderDescription();
    auto targetSettings=azurerender::resolveRenderSettings(*engineSettings_,level.scene.renderSettings);context.renderSettings=&targetSettings;
    if(rendererResourceKey_==level.resourceKey && sceneRenderer_ && sceneRenderer_->name()==azurerender::BuiltinRendererCatalog::documentRendererId(level.scene.renderSettings.sceneType,!context.scene.resources.empty()) && sceneRenderer_->reuseScene(context)){
        publishGlitch();
        renderSettings_=level.scene.renderSettings;return;
    }
    if(preparedResourceKey_!=level.resourceKey || !preparedRendererReady_){
        // A rejected active instance cannot satisfy the fallback preparation.
        while(!preloadLevelRenderer(level,false)){}
    }
    // The previous scene remains cached. Only reusing a renderer with work
    // still in flight needs GPU retirement; ordinary alternating loads reuse
    // a renderer whose last submission completed many frames earlier.
    if(preparedRendererLastSubmission_>graphicsCompleted_ ||
        (screenBlackout_ && graphicsSubmission_>graphicsCompleted_)){
        vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(renderer reuse)");
        graphicsCompleted_=graphicsSubmission_;
    }
    publishGlitch();
    screenBlackout_.reset();
    static_cast<void>(preparedRenderer_->reuseScene(context));
    sceneRenderer_.swap(preparedRenderer_);
    preparedResourceKey_=rendererResourceKey_;rendererResourceKey_=level.resourceKey;
    preparedRenderSettings_=renderSettings_;renderSettings_=level.scene.renderSettings;
    preparedRendererReady_=static_cast<bool>(preparedRenderer_);
    preparedRendererLastSubmission_=graphicsSubmission_;
}
void AzureRenderApp::releasePreparedRenderer(){
    releasePreparedGlitchView();
    if(preparedRenderer_){
        if(preparedRendererLastSubmission_>graphicsCompleted_){
            vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(renderer eviction)");
            graphicsCompleted_=graphicsSubmission_;
        }
        azurerender::RenderContext context;buildRenderContext(context);
        preparedRenderer_->onUnload(context);preparedRenderer_.reset();
    }
    preparedResourceKey_.clear();preparedRendererReady_=false;preparedRendererLastSubmission_=0;
}
void AzureRenderApp::releasePreparedGlitchView(){
    preparedGlitchViews_.reset();
    preparedGlitchView_=0;
    preparedGlitchKey_={};
}
#if AZURE_WITH_EDITOR
void AzureRenderApp::requestEditorSceneReload(bool retainCompatiblePreviews) {
    // Refresh the views against the changed assets now. Publishing the pending
    // renderer leaves views created during preparation available.
    editorReloadQueued_=true;
    preservingEditorScene_=true;
    invalidatePreviews(retainCompatiblePreviews);
}
void AzureRenderApp::discardEditorSceneReload(bool restart) {
    if (editorSceneReload_) {
        auto& pending=*editorSceneReload_;
        if (pending.renderer)pending.renderer->onUnload(pending.context);
        editorSceneReload_.reset();
        editorReloadQueued_=restart;
    }
    if (!restart)editorReloadQueued_=false;
    if (editorLayer_)editorLayer_->setSceneResourcesPending(false);
}
void AzureRenderApp::rebuildEditorSceneRenderer() {
    vkCheck(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle(asset reload)");
    graphicsCompleted_=graphicsSubmission_;
    releasePreparedRenderer();
    if (renderViews_)renderViews_->complete(graphicsCompleted_);
    screenBlackout_.reset();
    if (sceneRenderer_) {
        azurerender::RenderContext old;
        buildRenderContext(old,nullptr,nullptr,false);
        sceneRenderer_->onUnload(old);
        sceneRenderer_.reset();
    }
    createSceneRenderer();
    azurerender::RuntimeDiagnostics::instance().info("editor","Renderer resources reloaded");
}
void AzureRenderApp::advanceEditorSceneReload() {
    auto& session=*runOptions_.editorSession;
    if (session.playing())return;
    if (!editorSceneReload_ && editorReloadQueued_) {
        editorReloadQueued_=false;
        // These targets have renderer-dependent ownership or history. Their
        // replacement still uses the render-target rebuilding transaction.
        if (temporalRequested_ || multisampleRequested_) {
            rebuildEditorSceneRenderer();
            return;
        }
        auto next=std::make_unique<EditorSceneReload>();
        next->settings=effectiveRenderSettings_;
        const auto document=session.viewScene();
        // Explicit asset refreshes can change an on-disk target for the same reference.
        next->resources.load(document,[]{});
        try {
            sceneResourcePaths_.reset();
            buildRenderContext(next->context,&next->settings);
            const auto id=azurerender::BuiltinRendererCatalog::documentRendererId(
                next->settings.sceneType,!next->context.scene.resources.empty());
            next->renderer=azurerender::BuiltinRendererCatalog::createRegistry().create(id);
            azurerender::validateSceneRendererCapabilities(next->renderer->capabilities());
        } catch (const std::exception& error) {
            rejectedEditorResources_=std::move(next->resources);
            session.context().log(std::string("Renderer resource preparation failed: ")+error.what());
            return;
        }
        next->context.backgroundPipelinePreparation=true;
        preservingEditorScene_=true;
        editorSceneReload_=std::move(next);
        if (editorLayer_)editorLayer_->setSceneResourcesPending(true);
        azurerender::RuntimeDiagnostics::instance().info("editor","Preparing renderer resources");
        return; // Publish the first busy frame before beginning preparation.
    }
    if (!editorSceneReload_)return;
    auto& pending=*editorSceneReload_;
    try {
        if (!pending.renderer->prepareLoad(pending.context,3.0))return;
        if (editorReloadQueued_ || pending.resources.needsReload(session.viewScene())) {
            discardEditorSceneReload(true);
            return;
        }
        const auto capabilities=pending.renderer->capabilities();
        session.setViewportDiagnosticModes(capabilities.diagnosticViewNames);
        vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(editor resource publication)");
        graphicsCompleted_=graphicsSubmission_;
        releasePreparedRenderer();
        if (renderViews_)renderViews_->complete(graphicsCompleted_);
        clearRoomGlitch();
        screenBlackout_.reset();
        if (sceneRenderer_)sceneRenderer_->onUnload(pending.context);
        sceneRenderer_=std::move(pending.renderer);
        editorRenderResources_=std::move(pending.resources);
        rejectedEditorResources_={};
        editorSceneReload_.reset();
        preservingEditorScene_=false;
    } catch (const std::exception& error) {
        rejectedEditorResources_=std::move(pending.resources);
        const bool restart=editorReloadQueued_ || rejectedEditorResources_.needsReload(session.viewScene());
        discardEditorSceneReload(restart);
        const std::string reason=error.what();
        session.context().log("Renderer resource preparation failed: "+reason);
        azurerender::RuntimeDiagnostics::instance().warning("editor","Renderer resource preparation failed: "+reason);
        return;
    }
    if (editorLayer_)editorLayer_->setSceneResourcesPending(false);
    azurerender::RuntimeDiagnostics::instance().info("editor","Renderer resources reloaded");
}
#endif
void AzureRenderApp::synchronizeEditorRuntime(){
#if AZURE_WITH_EDITOR
    if(!runOptions_.editorSession)return;
    auto& session=*runOptions_.editorSession;
    session.pollTasks();
    if(editorAutomation_)editorAutomation_->advance(gameplayFrame_,session);
    if (!session.playing() && !editorSceneReload_ && !editorReloadQueued_) {
        const auto document=session.viewScene();
        if (editorRenderResources_.needsReload(document) && rejectedEditorResources_.needsReload(document))
            static_cast<void>(session.execute(azurerender::EditorCommand::ReloadAssets));
    }
    if(session.consumeRuntimeReset()){
        discardEditorSceneReload();
        editorDisplayedScene_.reset();
        preservingEditorScene_=false;
        if(session.playing() && !editorWasPlaying_){savedEditorCamera_=cameraPosition_;savedEditorTarget_=cameraTarget_;}
        if(!session.playing() && editorWasPlaying_){cameraPosition_=savedEditorCamera_;cameraTarget_=savedEditorTarget_;}
        editorWasPlaying_=session.playing();gameViewportFocus_=false;gameCursorPrimed_=false;
        glfwSetInputMode(frontend_->nativeHandle(),GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        vkCheck(vkDeviceWaitIdle(device_),"vkDeviceWaitIdle(editor runtime)");gameUi_.reset();gameUiPath_.clear();
        invalidatePreviews();graphicsCompleted_=graphicsSubmission_;renderViews_->complete(graphicsCompleted_);
        releasePreparedRenderer();
        screenBlackout_.reset();
        azurerender::RenderContext context;buildRenderContext(context);if(sceneRenderer_){sceneRenderer_->onUnload(context);sceneRenderer_.reset();}
        applyProjectReferenceViews(session.referenceViews());
        renderSettings_=session.viewScene().renderSettings;createSceneRenderer();
        if(session.playing()){if(session.levels()){
                rendererResourceKey_=session.levels()->current().resourceKey;
                session.levels()->setPrepareHandler([this](const auto& level){prepareLevelRenderer(level);});
                session.levels()->setPreloadHandler([this](const auto& level){return preloadLevelRenderer(level);});
                session.levels()->setReadinessHandler([this](const auto& level){return (sceneRenderer_ && sceneRenderer_->name()==azurerender::BuiltinRendererCatalog::documentRendererId(level.scene.renderSettings.sceneType,!level.scene.resources.empty()) && rendererResourceKey_==level.resourceKey) || (preparedRendererReady_ && preparedRenderSettings_.sceneType==level.scene.renderSettings.sceneType && preparedResourceKey_==level.resourceKey);});
            }}
        else session.context().attachRenderSettings(renderSettings_);
    }
    if(!session.playing())session.context().attachRenderSettings(renderSettings_);
#endif
}
unsigned AzureRenderApp::effectiveFrameRateLimit() {
    constexpr auto key = "runtime.maxFrameRate";
    if (!fixedSimulation_ && engineSettings_->source(key) == azurerender::SettingSource::Default) {
        if (const auto* window = windowPresentation(activeRuntime())) {
            if (effectiveVsync()) return 0;
            const auto rate = frontend_->isForeground() ? window->activeFrameRate : window->inactiveFrameRate;
            return std::clamp(rate, 10U, 250U);
        }
    }
    return engineSettings_->get(key).get<unsigned>();
}
bool AzureRenderApp::effectiveVsync() {
    constexpr auto key = "runtime.vsync";
    if (!fixedSimulation_ && engineSettings_->source(key) == azurerender::SettingSource::Default) {
        if (const auto* window = windowPresentation(activeRuntime())) return window->vsync;
    }
    return engineSettings_->get(key).get<bool>();
}
void AzureRenderApp::synchronizeGameUi(){
    if (!fixedSimulation_) {
        constexpr auto menuAction = "display-topmost-menu";
        auto* game = activeGame();
        bool menuEnabled = false;
        bool keepDisplayAwake = false;
        std::string_view menuCaption;
        if (auto* runtime = activeRuntime()) {
            auto& world = runtime->world();
            const auto* windows = world.findComponentArray<azurerender::game::WindowPresentation>();
            if (windows && !windows->dense().empty()) {
                auto& state = *world.tryGet<azurerender::game::WindowPresentation>(windows->entities().front());
                if (state.pending) {
                    frontend_->applyWindowPresentation(static_cast<azurerender::WindowMode>(state.mode),
                        static_cast<azurerender::WindowResolution>(state.resolution), state.alwaysOnTop);
                    state.pending = false;
                }
                const auto applied = frontend_->windowPresentation();
                state.mode = static_cast<std::uint32_t>(applied.mode);
                state.resolution = static_cast<std::uint32_t>(applied.resolution);
                state.alwaysOnTop = applied.alwaysOnTop;
                menuEnabled = game && game->input().hasAction(menuAction);
                menuCaption = state.menuCaption;
                keepDisplayAwake = state.screenSleepMode == 1;
            }
        }
        frontend_->setTopmostMenu(menuEnabled, menuCaption);
        if (menuEnabled && frontend_->consumeTopmostMenuToggle()) game->input().activate(menuAction);
        frontend_->setDisplayAwake(keepDisplayAwake);
        if (frontend_->updatePresentationSync(effectiveVsync())) framebufferResized_ = true;
    }
    auto* presentation=activePresentation();auto* assets=activeAssets();const auto reference=presentation?presentation->uiDocument():std::string();
    const auto path=assets&&!reference.empty()?assets->resolveReference(reference).generic_u8string():std::string();
    if(path!=gameUiPath_){
        gameUi_.reset();gameUiPath_.clear();
        if(!path.empty()){
            if(!gameUiRenderer_)gameUiRenderer_=std::make_unique<azurerender::GameUiRenderer>(*rhi_,postProcessRenderPass_,resourceLocator_.shaderDirectory());
            gameUi_=std::make_unique<azurerender::GameUi>(std::filesystem::u8path(path),resourceLocator_.publicAsset("fonts/LatoLatin-Regular.ttf"),*gameUiRenderer_);gameUiPath_=path;
            gameUi_->setActionHandler([this](std::string action){
                auto* runtime=activeRuntime();if(!runtime)return;
                if(action=="pause"){
                    if(runtime->state()==azurerender::RuntimeLifecycle::State::Running)runtime->pause();
                    else if(runtime->state()==azurerender::RuntimeLifecycle::State::Paused)runtime->resume();
                }else if(action=="restart"){
                    auto* levels=levelSession_.get();
#if AZURE_WITH_EDITOR
                    if(runOptions_.editorSession)levels=runOptions_.editorSession->levels();
#endif
                    if(levels){levels->request(levels->currentReference());
                        if(runtime->state()==azurerender::RuntimeLifecycle::State::Paused)runtime->resume();}
                }else if(auto* game=activeGame()){
                    game->input().activate(action);
                }
            });
        }
    }
    if(auto* scripts=activeScripts()){
        scripts->setUiHandler([this](std::string id,std::string text){if(gameUi_)gameUi_->setText(id,text);});
        scripts->setUiProvider([this]{return gameUi_.get();});
    }
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
