#include "ImGuiEditorLayer.hpp"
#include "EditorTheme.hpp"
#include "EditorToolbar.hpp"
#include "resources/ResourceLocator.hpp"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "ecs/Components.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "extensions/ExtensionRegistry.hpp"
#include "editor/GameplayDebugGeometry.hpp"

#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#ifdef IMGUI_HAS_DOCK
#include <imgui_internal.h>
#endif
#if __has_include(<backends/imgui_impl_glfw.h>)
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <set>
#endif

namespace azurerender {

#ifdef AZURERENDER_HAS_IMGUI

namespace {

#ifndef IMGUI_HAS_DOCK
void setFallbackPanelRect(
    const float x,
    const float y,
    const float width,
    const float height) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x * x, display.y * y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {display.x * width, display.y * height}, ImGuiCond_Always);
}
#endif

class CallbackEditorPanel final : public IEditorPanel {
public:
    CallbackEditorPanel(
        const std::string_view id,
        const std::string_view title,
        std::function<void()> draw)
        : id_(id), title_(title), draw_(std::move(draw)) {}

    [[nodiscard]] std::string_view id() const noexcept override { return id_; }
    [[nodiscard]] std::string_view title() const noexcept override {
        return title_;
    }
    void draw(EditorContext&) override { draw_(); }

private:
    std::string_view id_;
    std::string_view title_;
    std::function<void()> draw_;
};

}  // namespace

ImGuiEditorLayer::ImGuiEditorLayer(std::shared_ptr<EditorSession> session)
    : session_(std::move(session)) {
    if (session_ == nullptr) {
        throw std::invalid_argument("ImGui editor requires an editor session");
    }
    context_ = &session_->context();
    EditorPanelRegistry registry;
    const auto addPanel = [&registry](
                              const char* id,
                              const char* title,
                              std::function<void()> draw) {
        registry.registerFactory(
            {std::string("panel.") + id, 1, {"editor.panel"}, {}},
            [id, title, draw = std::move(draw)] {
                return std::make_unique<CallbackEditorPanel>(
                    id, title, draw);
            });
    };
    addPanel("viewport", "Viewport", [this] { drawViewportPanel(); });
    addPanel("outliner", "Scene Outliner", [this] { drawOutlinerPanel(); });
    addPanel("inspector", "Inspector", [this] { drawInspectorPanel(); });
    addPanel("assets", "Asset Browser", [this] { drawAssetBrowserPanel(); });
    addPanel("capture", "Capture", [this] { drawCapturePanel(); });
    addPanel("console", "Console", [this] { drawConsolePanel(); });
    addPanel("build", "Build Game", [this] { drawBuildPanel(); });
    addPanel("animation", "Animation Preview", [this] { drawAnimationPanel(); });
    addPanel("gameplay-debug", "Gameplay Debug", [this] { drawGameplayDebugPanel(); });
    panels_ = registry.createAll();
}

ImGuiEditorLayer::~ImGuiEditorLayer() {
    shutdownVulkan();
}

void ImGuiEditorLayer::initialize(
    GLFWwindow* window,
    const VkInstance instance,
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const std::uint32_t queueFamily,
    const VkQueue queue,
    const VkRenderPass renderPass,
    const std::uint32_t imageCount, const VkFormat colorFormat) {
    if (initialized_) {
        shutdownVulkan();
    }
    device_ = device;
    dockingLayoutInitialized_ = false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    window_=window;
    configDirectory_=EditorWorkspace::configDirectory();
    const bool restored=workspace_.load(configDirectory_);
    std::filesystem::create_directories(configDirectory_);
    iniPath_=(configDirectory_/"layout.ini").u8string();
    io.IniFilename=nullptr;
    if(restored && std::filesystem::exists(configDirectory_/"layout.ini")) {
        std::ifstream file(configDirectory_/"layout.ini");std::string contents((std::istreambuf_iterator<char>(file)),{});
        if(contents.find("[Docking][Data]")!=std::string::npos)ImGui::LoadIniSettingsFromMemory(contents.c_str());
        else { workspace_.diagnostic="Invalid docking data. Default layout loaded.";workspace_.reset(); }
    }
    if(!workspace_.diagnostic.empty())context_->log(workspace_.diagnostic);
    if(const auto* value=std::getenv("AZURERENDER_EDITOR_DPI"))dpiOverride_=std::stof(value);
    float scaleY=1;glfwGetWindowContentScale(window_,&dpi_,&scaleY);if(dpiOverride_>0)dpi_=dpiOverride_;
    EditorTheme::apply(dpi_);
    const auto font=ResourceLocator{}.publicAsset("fonts/NotoSansCJKsc-Regular.otf");
    if(io.Fonts->AddFontFromFileTTF(font.u8string().c_str(),14)==nullptr)throw std::runtime_error("Editor font could not be loaded");
    if(uiFrame_==0)if(const auto* path=std::getenv("AZURERENDER_EDITOR_UI_ACTIONS")) {
        std::ifstream file(std::filesystem::u8path(path));file>>uiActions_;
        if(!uiActions_.is_array())throw std::invalid_argument("UI actions must be an array");
    }

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui GLFW backend initialization failed");
    }

    constexpr std::array<VkDescriptorPoolSize, 11> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    }};
    const VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        nullptr,
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        1000 * static_cast<std::uint32_t>(poolSizes.size()),
        static_cast<std::uint32_t>(poolSizes.size()),
        poolSizes.data(),
    };
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool_)
        != VK_SUCCESS) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui descriptor pool creation failed");
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamily;
    initInfo.Queue = queue;
    initInfo.DescriptorPool = descriptorPool_;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = imageCount;
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    const bool srgb=colorFormat==VK_FORMAT_B8G8R8A8_SRGB || colorFormat==VK_FORMAT_R8G8B8A8_SRGB;
    const auto shader=ResourceLocator{}.shaderDirectory()/(srgb?"editor_ui_srgb.frag.spv":"editor_ui.frag.spv");
    std::ifstream bytecode(shader,std::ios::binary|std::ios::ate);
    const auto bytes=bytecode.tellg();
    if(bytes<=0 || bytes%4!=0)throw std::runtime_error("Editor UI shader is invalid");
    uiFragmentCode_.resize(static_cast<std::size_t>(bytes)/4);bytecode.seekg(0);
    bytecode.read(reinterpret_cast<char*>(uiFragmentCode_.data()),bytes);
    initInfo.CustomShaderFragCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    initInfo.CustomShaderFragCreateInfo.codeSize=uiFragmentCode_.size()*4;
    initInfo.CustomShaderFragCreateInfo.pCode=uiFragmentCode_.data();
    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui Vulkan backend initialization failed");
    }
    initialized_ = true;
}

void ImGuiEditorLayer::shutdownVulkan() {
    if (!initialized_) {
        return;
    }
    try { ImGui::SaveIniSettingsToDisk(iniPath_.c_str());workspace_.save(configDirectory_); }
    catch(const std::exception& error){context_->log(std::string("Workspace: ")+error.what());}
    for (const VkDescriptorSet texture : viewportTextures_) {
        ImGui_ImplVulkan_RemoveTexture(texture);
    }
    viewportTextures_.clear();
    ImGui_ImplVulkan_Shutdown();
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    device_ = VK_NULL_HANDLE;
    initialized_ = false;
}

void ImGuiEditorLayer::newFrame() {
    if (!initialized_) {
        return;
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    float nextDpi=1,scaleY=1;glfwGetWindowContentScale(window_,&nextDpi,&scaleY);
    if(dpiOverride_>0)nextDpi=dpiOverride_;
    if(std::abs(nextDpi-dpi_)>.01F){dpi_=nextDpi;EditorTheme::apply(dpi_);dockingLayoutInitialized_=false;workspaceRebuildRequested_=true;}
    ++uiFrame_;injectUiEvents();
    ImGui::NewFrame();
}

void ImGuiEditorLayer::drawPanels() {
    if (!initialized_) {
        return;
    }
    widgets_=nlohmann::json::object();
    EditorToolbar::draw(*session_,workspace_,dpi_,[this](const std::string& id){observeWidget(id);});
    drawWorkspace();
    const ImGuiIO& io = ImGui::GetIO();
    if(!io.WantTextInput && !session_->playing() && !session_->building()){
        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D,false))context_->duplicateSelection();
        if(ImGui::IsKeyPressed(ImGuiKey_Delete,false))context_->deleteSelection();
    }
    if(!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P,false))static_cast<void>(session_->execute(session_->playing()?EditorCommand::Stop:EditorCommand::Play));
    if (!io.WantTextInput && EditorToolbar::enabled(*session_,EditorCommand::Save) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        static_cast<void>(session_->execute(EditorCommand::Save));
    }
    if (!io.WantTextInput && EditorToolbar::enabled(*session_,EditorCommand::Undo) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        static_cast<void>(session_->execute(EditorCommand::Undo));
    }
    if (!io.WantTextInput && EditorToolbar::enabled(*session_,EditorCommand::Redo) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        static_cast<void>(session_->execute(EditorCommand::Redo));
    }
    for (const std::unique_ptr<IEditorPanel>& panel : panels_) {
        if(!workspace_.visible(std::string(panel->id())))continue;
        const bool editable=std::string(panel->id())=="viewport" || std::string(panel->id())=="console" || std::string(panel->id())=="capture";
        ImGui::BeginDisabled((session_->playing() || session_->building()) && !editable && panel->id()!="build");panel->draw(*context_);ImGui::EndDisabled();
    }    EditorToolbar::status(*session_,dpi_);
#ifdef IMGUI_HAS_DOCK
    for(const auto& panel:workspace_.panels())if(const auto* window=ImGui::FindWindowByName(panel.title.c_str())) {
        widgets_["panelrect."+panel.id]={window->Pos.x,window->Pos.y,window->Size.x,window->Size.y};
        if(window->DockNode && window->DockNode->TabBar) {
            const auto* bar=window->DockNode->TabBar;
            for(const auto& tab:bar->Tabs)if(tab.Window==window)
                widgets_["tab."+panel.id]={bar->BarRect.Min.x+tab.Offset,bar->BarRect.Min.y,tab.Width,bar->BarRect.GetHeight()};
        }
    }
#endif
    if(!uiActions_.empty())uiHistory_.push_back({{"frame",uiFrame_},{"playing",session_->playing()},
        {"paused",session_->runtime()&&session_->runtime()->state()==RuntimeLifecycle::State::Paused},
        {"selected",context_->selectedNode()?context_->selectedNode()->id:""},{"dirty",context_->dirty()},
        {"nodeCount",context_->scene().nodes.size()},{"translation",context_->gizmoTranslation()},
        {"rotation",context_->gizmoRotation()},{"scale",context_->gizmoScale()},
        {"mouse",{io.MousePos.x,io.MousePos.y}},{"mouseDown",io.MouseDown[0]},
        {"activeId",ImGui::GetCurrentContext()->ActiveId},
        {"movingWindow",ImGui::GetCurrentContext()->MovingWindow?ImGui::GetCurrentContext()->MovingWindow->Name:""}});
}

void ImGuiEditorLayer::render(const VkCommandBuffer commandBuffer) {
    if (!initialized_) {
        return;
    }
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiEditorLayer::setViewportImages(
    const VkSampler sampler,
    const std::vector<VkImageView>& imageViews,
    const std::uint32_t width,
    const std::uint32_t height) {
    if (!initialized_) {
        return;
    }
    clearViewportImages();
    viewportTextures_.reserve(imageViews.size());
    for (const VkImageView imageView : imageViews) {
        viewportTextures_.push_back(ImGui_ImplVulkan_AddTexture(
            sampler,
            imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }
    viewportWidth_ = std::max(width, 1U);
    viewportHeight_ = std::max(height, 1U);
    viewportImageIndex_ = 0;
}

void ImGuiEditorLayer::clearViewportImages() {
    if (!initialized_) {
        viewportTextures_.clear();
        return;
    }
    for (const VkDescriptorSet texture : viewportTextures_) {
        ImGui_ImplVulkan_RemoveTexture(texture);
    }
    viewportTextures_.clear();
    viewportImageIndex_ = 0;
}

void ImGuiEditorLayer::setViewportImageIndex(
    const std::uint32_t imageIndex) {
    if (imageIndex < viewportTextures_.size()) {
        viewportImageIndex_ = imageIndex;
    }
}

EditorViewportInput ImGuiEditorLayer::consumeViewportInput() noexcept {
    EditorViewportInput input = viewportInput_;
    viewportInput_ = {};
    return input;
}

bool ImGuiEditorLayer::consumeViewportResizeRequest(
    std::uint32_t& width,
    std::uint32_t& height) noexcept {
    if (!viewportResizePending_) {
        return false;
    }
    width = resizeCandidateWidth_;
    height = resizeCandidateHeight_;
    viewportResizePending_ = false;
    resizeStableFrames_ = 0;
    return true;
}

void ImGuiEditorLayer::drawViewportPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.20F, 0.0F, 0.56F, 0.72F);
#endif
    if(!ImGui::Begin("Viewport###viewport",workspace_.open("viewport"))){ImGui::End();return;}
    viewportFocused_ = ImGui::IsWindowFocused(
        ImGuiFocusedFlags_RootAndChildWindows);
    if (!viewportTextures_.empty()) {
        if(ImGui::Button("Focus Selected")) {
            viewportInput_.frameRequested=true;viewportInput_.frameTarget=context_->gizmoTranslation();
        }observeWidget("focus");ImGui::SameLine();ImGui::TextDisabled("Perspective");
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
        const std::uint32_t desiredWidth = static_cast<std::uint32_t>(
            std::max(std::floor(available.x * framebufferScale.x), 64.0F));
        const std::uint32_t desiredHeight = static_cast<std::uint32_t>(
            std::max(std::floor(available.y * framebufferScale.y), 64.0F));
        if (desiredWidth == resizeCandidateWidth_
            && desiredHeight == resizeCandidateHeight_) {
            resizeStableFrames_ = std::min(resizeStableFrames_ + 1, 60U);
        } else {
            resizeCandidateWidth_ = desiredWidth;
            resizeCandidateHeight_ = desiredHeight;
            resizeStableFrames_ = 0;
        }
        if (resizeStableFrames_ >= 4
            && (desiredWidth != viewportWidth_
                || desiredHeight != viewportHeight_)
            && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            viewportResizePending_ = true;
        }
        const float aspect = static_cast<float>(viewportWidth_)
            / static_cast<float>(viewportHeight_);
        ImVec2 imageSize{available.x, available.x / aspect};
        if (imageSize.y > available.y) {
            imageSize.y = available.y;
            imageSize.x = available.y * aspect;
        }
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos({
            cursor.x + std::max((available.x - imageSize.x) * 0.5F, 0.0F),
            cursor.y + std::max((available.y - imageSize.y) * 0.5F, 0.0F),
        });
        ImGui::Image(
            reinterpret_cast<ImTextureID>(
                viewportTextures_[viewportImageIndex_]),
            imageSize);
        if(!session_->playing() && !session_->building() && ImGui::BeginDragDropTarget()){
            if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_RESOURCE"))try{context_->placeResource(static_cast<const char*>(payload->Data));}catch(const std::exception& error){context_->log(std::string("ERROR: ")+error.what());}
            ImGui::EndDragDropTarget();
        }
        // Draw the viewport gizmo handles (if any selected primitive has a
        // valid screen projection). We compute endpoints here so the click
        // and drag logic can hit-test against them.
        const auto& gizmoScreen = context_->gizmoScreen();
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        imageRect_={itemMin.x,itemMin.y,imageSize.x,imageSize.y};
        widgets_["viewport.image"]=imageRect_;
        if(context_->isProject()) {
            const auto description=context_->scene().renderDescription();
            const auto transforms=scene::resolveNodeWorldTransforms(description);
            for(std::size_t index=0;index<description.nodes.size();++index) {
                if(description.nodes[index].resourceId.empty())continue;
                const auto& matrix=transforms[index];
                const auto projected=context_->projectDebugPoint({matrix[12],matrix[13]+.8F,matrix[14]});
                if(projected)widgets_["pick."+description.nodes[index].id]={itemMin.x+(*projected)[0]*imageSize.x-1,itemMin.y+(*projected)[1]*imageSize.y-1,2,2};
            }
        }
        if(session_->debugOverlay){
            auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(itemMin,{itemMin.x+imageSize.x,itemMin.y+imageSize.y},true);
            auto line=[&](const DebugLine& edge,ImU32 color){auto a=context_->projectDebugPoint(edge.from),b=context_->projectDebugPoint(edge.to);
                if(a&&b){draw->AddLine({itemMin.x+(*a)[0]*imageSize.x,itemMin.y+(*a)[1]*imageSize.y},{itemMin.x+(*b)[0]*imageSize.x,itemMin.y+(*b)[1]*imageSize.y},color,1.5F);++debugLineCount_;}};
            const auto scene=session_->viewScene();auto* runtime=session_->runtime();const auto registry=reflection::makeRuntimeRegistry();
            for(const auto& node:scene.nodes){
                ecs::TransformComponent transform{node.translation,node.rotation,node.scale};
                game::RigidBody body;game::Character character;bool hasBody=false,hasCharacter=false;
                if(runtime){const auto entity=runtime->entity(node.id);if(const auto* value=runtime->world().tryGet<game::RigidBody>(entity)){body=*value;hasBody=true;}if(const auto* value=runtime->world().tryGet<game::Character>(entity)){character=*value;hasCharacter=true;}}
                else{auto data=context_->componentData(node.id,"azure.rigid-body");if(!data.is_null()){registry.decode("azure.rigid-body",&body,{{"type","azure.rigid-body"},{"version",1},{"data",data}});hasBody=true;}
                    data=context_->componentData(node.id,"azure.character");if(!data.is_null()){registry.decode("azure.character",&character,{{"type","azure.character"},{"version",registry.type("azure.character").version},{"data",data}});hasCharacter=true;}}
                if(hasBody)for(const auto& edge:debugBox(transform,body))line(edge,body.trigger?IM_COL32(255,210,60,220):IM_COL32(70,220,120,220));
                if(hasCharacter)for(const auto& edge:debugCapsule(transform,character))line(edge,IM_COL32(80,180,255,230));
            }
            if(auto* game=session_->game();game && game->hasCamera())line({game->camera().target(),game->camera().position()},IM_COL32(255,80,170,255));
            draw->PopClipRect();
        }
        if(session_->playing() && gameUi_ && imageSize.x>0 && imageSize.y>0){
            const auto mouse=ImGui::GetMousePos();const bool consumed=gameUi_->pointer(static_cast<int>((mouse.x-itemMin.x)*viewportWidth_/imageSize.x),static_cast<int>((mouse.y-itemMin.y)*viewportHeight_/imageSize.y),ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left));
            if(consumed||gameUi_->wantsKeyboard())viewportAcceptsShortcuts_=false;
            if(viewportFocused_){for(auto character:ImGui::GetIO().InputQueueCharacters)gameUi_->character(character);}
        }
        ImVec2 gizmoCenter{0.0F, 0.0F};
        ImVec2 pixelAxes[3]{};float worldPerPixel[3]{};
        ImVec2 gizmoAxisEnds[3] = {{0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}};
        bool gizmoDrawn = false;
        if (!session_->playing() && !session_->building() && gizmoScreen.valid && imageSize.x > 0.0F && imageSize.y > 0.0F) {
            gizmoCenter = ImVec2(
                itemMin.x + gizmoScreen.centerX * imageSize.x,
                itemMin.y + gizmoScreen.centerY * imageSize.y);
            const float axes[3][2] = {
                {gizmoScreen.axisXScreenX, gizmoScreen.axisXScreenY},
                {gizmoScreen.axisYScreenX, gizmoScreen.axisYScreenY},
                {gizmoScreen.axisZScreenX, gizmoScreen.axisZScreenY},
            };
            const ImU32 colors[3] = {
                IM_COL32(230, 80, 80, 255),
                IM_COL32(80, 220, 110, 255),
                IM_COL32(90, 140, 230, 255),
            };
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (int axis = 0; axis < 3; ++axis) {
                const float x=axes[axis][0]*imageSize.x,y=axes[axis][1]*imageSize.y;
                const float length=std::hypot(x,y);
                pixelAxes[axis]=length>1e-4F?ImVec2{x/length,y/length}:ImVec2{0,0};
                worldPerPixel[axis]=length>1e-4F?.3F/length:0;
                gizmoAxisEnds[axis] = ImVec2(
                    gizmoCenter.x + pixelAxes[axis].x * 40.0F*dpi_,
                    gizmoCenter.y + pixelAxes[axis].y * 40.0F*dpi_);
                drawList->AddLine(
                    gizmoCenter, gizmoAxisEnds[axis], colors[axis], 3.0F);
                drawList->AddCircleFilled(
                    gizmoAxisEnds[axis], 5.0F, colors[axis]);
                widgets_["gizmo."+std::to_string(axis)]={gizmoAxisEnds[axis].x-5,gizmoAxisEnds[axis].y-5,10,10};
            }
            widgets_["gizmo.center"]={gizmoCenter.x-1,gizmoCenter.y-1,2,2};
            gizmoDrawn = true;
        }
        if (ImGui::IsItemHovered()) {
            const ImGuiIO& io = ImGui::GetIO();
            viewportInput_.zoomSteps += io.MouseWheel;
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                viewportInput_.orbitDeltaX += io.MouseDelta.x;
                viewportInput_.orbitDeltaY += io.MouseDelta.y;
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                viewportInput_.panDeltaX += io.MouseDelta.x;
                viewportInput_.panDeltaY += io.MouseDelta.y;
            }
            bool pickThisClick = true;
            if (gizmoDrawn
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 mousePosition = io.MousePos;
                float bestDistance = 12.0F;
                std::int32_t bestAxis = -1;
                for (int axis = 0; axis < 3; ++axis) {
                    const float distance = std::hypot(
                        mousePosition.x - gizmoAxisEnds[axis].x,
                        mousePosition.y - gizmoAxisEnds[axis].y);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestAxis = axis;
                    }
                }
                if (bestAxis >= 0) {
                    gizmoDragAxis_ = bestAxis;
                    gizmoDragStartMouse_ = mousePosition;
                    gizmoDragStartTranslation_ = context_->gizmoTranslation();
                    gizmoDragStartRotation_=context_->gizmoRotation();gizmoDragStartScale_=context_->gizmoScale();
                    viewportGizmoDragActive_ = true;
                    pickThisClick = false;
                }
            }
            if (!session_->playing() && !session_->building() && pickThisClick
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 mousePosition = io.MousePos;
                if (imageSize.x > 0.0F && imageSize.y > 0.0F) {
                    viewportInput_.pickX = std::clamp(
                        (mousePosition.x - itemMin.x) / imageSize.x,
                        0.0F,
                        1.0F);
                    viewportInput_.pickY = std::clamp(
                        (mousePosition.y - itemMin.y) / imageSize.y,
                        0.0F,
                        1.0F);
                    viewportInput_.pickRequested = true;
                }
            }
            if (viewportGizmoDragActive_
                && gizmoDragAxis_ >= 0
                && gizmoScreen.valid
                && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const float axes[3][2] = {
                    {gizmoScreen.axisXScreenX, gizmoScreen.axisXScreenY},
                    {gizmoScreen.axisYScreenX, gizmoScreen.axisYScreenY},
                    {gizmoScreen.axisZScreenX, gizmoScreen.axisZScreenY},
                };
                const float projection =
                    (io.MousePos.x-gizmoDragStartMouse_.x) * pixelAxes[gizmoDragAxis_].x
                    + (io.MousePos.y-gizmoDragStartMouse_.y) * pixelAxes[gizmoDragAxis_].y;
                const float worldDelta = projection * worldPerPixel[gizmoDragAxis_];
                std::array<float, 3> translation =
                    gizmoDragStartTranslation_;
                translation[gizmoDragAxis_] += worldDelta;
                if(context_->gizmoMode()==EditorContext::GizmoMode::Translate)context_->setGizmoTranslation(translation);
                else if(context_->gizmoMode()==EditorContext::GizmoMode::Rotate) {
                    auto value=gizmoDragStartRotation_;value[gizmoDragAxis_]+=projection*.5F;context_->setGizmoRotation(value);
                }else {
                    auto value=gizmoDragStartScale_;value[gizmoDragAxis_]+=worldDelta;
                    if(std::abs(value[gizmoDragAxis_])<.001F)value[gizmoDragAxis_]=.001F;
                    context_->setGizmoScale(value);
                }
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                viewportGizmoDragActive_ = false;
                gizmoDragAxis_ = -1;
            }
        }
    }
    viewportAcceptsShortcuts_ = viewportFocused_
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive();
    ImGui::End();
}

void ImGuiEditorLayer::drawOutlinerPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.0F, 0.20F, 0.72F);
#endif
    if(!ImGui::Begin("Scene Outliner###outliner",workspace_.open("outliner"))){ImGui::End();return;}
    outlinerFilter_.Draw("##Search objects",-1);observeWidget("outliner.search");
    const auto& nodes = context_->scene().nodes;
    if (ImGui::Button("Add Child")) {
        if (!nodes.empty()) {
            try {
                context_->addChildNode(context_->selectedNodeIndex());
            } catch (const std::exception& exception) {
                context_->log(std::string("ERROR: ") + exception.what());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && !nodes.empty()) {
        context_->deleteSelection();
    }
    ImGui::SameLine();if(ImGui::Button("Duplicate"))context_->duplicateSelection();
    ImGui::Separator();
    if (nodes.empty()) {
        ImGui::TextUnformatted("(empty scene)");
        ImGui::End();
        return;
    }
    // Recursive tree draw: children are nodes whose parentId equals the
    // current node id. Use an index-based recursion to avoid iterator
    // invalidation while nodes stay stable during a frame.
    const auto drawNode = [&](const auto& self,
                              const std::string& parentId,
                              const int depth) -> void {
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            if (nodes[index].parentId != parentId) {
                continue;
            }
            const bool selected = std::find(context_->selectedNodes().begin(),context_->selectedNodes().end(),index)!=context_->selectedNodes().end();
            bool hasChildren = false;
            for (const SceneNode& candidate : nodes) {
                if (candidate.parentId == nodes[index].id) {
                    hasChildren = true;
                    break;
                }
            }
            ImGui::PushID(static_cast<int>(index));
            bool open = false;
            if (hasChildren) {
                const bool clicked = ImGui::TreeNodeEx(
                    nodes[index].name.c_str(),
                    ImGuiTreeNodeFlags_OpenOnArrow
                        | ImGuiTreeNodeFlags_SpanAvailWidth
                        | (selected ? ImGuiTreeNodeFlags_Selected : 0));
                open = clicked;
            } else {
                ImGui::Selectable(
                    nodes[index].name.c_str(),
                    selected,
                    ImGuiSelectableFlags_SpanAvailWidth);
            }
            observeWidget("node."+nodes[index].id);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                if(ImGui::GetIO().KeyCtrl){auto selection=context_->selectedNodes();auto found=std::find(selection.begin(),selection.end(),index);if(found==selection.end())selection.push_back(index);else selection.erase(found);context_->selectNodes(std::move(selection));}else context_->selectNode(index);
            }
            if (open) {
                self(self, nodes[index].id, depth + 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    };
    if(outlinerFilter_.IsActive()) {
        for(std::size_t i=0;i<nodes.size();++i)if(outlinerFilter_.PassFilter(nodes[i].name.c_str()) || outlinerFilter_.PassFilter(nodes[i].id.c_str())) {
            ImGui::PushID(nodes[i].id.c_str());
            if(ImGui::Selectable(nodes[i].name.c_str(),context_->selectedNodeIndex()==i))context_->selectNode(i);
            observeWidget("node."+nodes[i].id);ImGui::SameLine();ImGui::TextDisabled("%s",nodes[i].resourceId.empty()?"Node":"Mesh");ImGui::PopID();
        }
    }else drawNode(drawNode, "", 0);
    ImGui::End();
}

void ImGuiEditorLayer::drawInspectorPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.76F, 0.0F, 0.24F, 0.72F);
#endif
    if(!ImGui::Begin("Details###inspector",workspace_.open("inspector"))){ImGui::End();return;}
    ImGui::BeginDisabled(session_->playing() || session_->building());
    if (const SceneNode* node = context_->selectedNode(); node != nullptr) {
        ImGui::Text("Node: %s", node->name.c_str());
        ImGui::Text("Id: %s", node->id.c_str());
        bool visible = node->visible;
        if (ImGui::Checkbox("Visible", &visible)) {
            context_->setSelectedNodeVisible(visible);
        }
        ImGui::TextUnformatted("Name");
        std::array<char, 128> nameBuffer{};
        const std::size_t copyLength = std::min(
            node->name.size(), nameBuffer.size() - 1);
        std::memcpy(
            nameBuffer.data(), node->name.data(), copyLength);
        if (ImGui::InputText("##name", nameBuffer.data(), nameBuffer.size())) {
            context_->setSelectedNodeName(nameBuffer.data());
        }
        observeWidget("name");
        ImGui::TextUnformatted("Prefab Source");
        std::array<char, 256> prefabBuffer{};
        std::memcpy(prefabBuffer.data(), node->prefabSource.data(),
            std::min(node->prefabSource.size(), prefabBuffer.size() - 1));
        if (ImGui::InputText("##prefab", prefabBuffer.data(), prefabBuffer.size())) {
            context_->setSelectedNodePrefab(prefabBuffer.data());
        }
        ImGui::TextUnformatted("Instance Of");
        std::array<char, 128> instanceBuffer{};
        std::memcpy(instanceBuffer.data(), node->instanceOf.data(),
            std::min(node->instanceOf.size(), instanceBuffer.size() - 1));
        if (ImGui::InputText("##instance", instanceBuffer.data(), instanceBuffer.size())) {
            context_->setSelectedNodeInstance(instanceBuffer.data());
        }
    }
    if(context_->isProject() && context_->selectedNode()){
        std::vector<std::string> types;
        for(const auto& entry:runtimeComponentRegistry().metadata().types())
            if(entry.first!="azure.transform" && entry.first!="azure.renderable") types.push_back(entry.first);
        ImGui::BeginDisabled(session_->playing() || session_->building());
        if(ImGui::BeginCombo("Add Component","Choose type")){for(const auto& type:types)if(ImGui::Selectable(type.c_str()))try{context_->addGameplayComponent(type);}catch(const std::exception& error){context_->log(std::string("ERROR: ")+error.what());}ImGui::EndCombo();}
        const auto& componentRegistry=runtimeComponentRegistry().metadata();
        for(const auto& type:types){auto data=context_->componentData(context_->selectedNode()->id,type);if(data.is_null())continue;
            ImGui::PushID(type.c_str());if(ImGui::CollapsingHeader(type.c_str()))for(const auto& field:componentRegistry.type(type).properties){
                if(!field.toolVisible)continue;
                ImGui::BeginDisabled(field.readOnly);
                auto value=data.at(field.name);bool changed=false;
                if(value.is_boolean()){bool v=value.get<bool>();changed=ImGui::Checkbox(field.label.c_str(),&v);value=v;}
                else if(value.is_number_integer()){int v=value.get<int>();changed=ImGui::DragInt(field.label.c_str(),&v,1,static_cast<int>(field.minimum),static_cast<int>(field.maximum));value=v;}
                else if(value.is_number()){float v=value.get<float>();changed=ImGui::DragFloat(field.label.c_str(),&v,0.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum));value=v;}
                else if(value.is_array() && value.size()==3){auto v=value.get<std::array<float,3>>();changed=ImGui::DragFloat3(field.label.c_str(),v.data(),0.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum));value=v;}
                else if(value.is_string()){
                    const auto text=value.get<std::string>();
                    if(field.name=="target"){
                        if(ImGui::BeginCombo(field.label.c_str(),text.c_str())){for(const auto& node:context_->scene().nodes)if(ImGui::Selectable(node.id.c_str(),node.id==text)){value=node.id;changed=true;}ImGui::EndCombo();}
                    }else if(field.name=="asset"){
                        const std::string extension=type=="azure.script"?".lua":type=="azure.animator"?".json":type=="azure.audio-source"?".wav":".rml";
                        if(ImGui::BeginCombo(field.label.c_str(),text.c_str())){for(const auto& [id,record]:context_->assets().records())if(record.path.extension()==extension && ImGui::Selectable(record.virtualPath.c_str(),id==text)){value=id;changed=true;}ImGui::EndCombo();}
                    }else{
                        std::array<char,512> v{};std::memcpy(v.data(),text.data(),std::min(text.size(),v.size()-1));
                        changed=ImGui::InputText(field.label.c_str(),v.data(),v.size(),ImGuiInputTextFlags_EnterReturnsTrue);value=v.data();
                    }
                }
                ImGui::EndDisabled();
                if(!field.tooltip.empty() && ImGui::IsItemHovered())ImGui::SetTooltip("%s",field.tooltip.c_str());
                if(changed)try{context_->setComponentField(type,field.name,value);}catch(const std::exception& error){context_->log(std::string("ERROR: ")+error.what());}
            }ImGui::PopID();
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    ImGui::Text("Transform | Position (m), Rotation (deg), Scale");
    if(ImGui::Button("Reset Transform")){context_->setGizmoTranslation({0,0,0});context_->setGizmoRotation({0,0,0});context_->setGizmoScale({1,1,1});}
    ImGui::SetNextItemWidth(-110*dpi_);
    static const auto registry = reflection::makeRuntimeRegistry();
    ecs::TransformComponent transform{context_->gizmoTranslation(), context_->gizmoRotation(), context_->gizmoScale()};
    for (const auto& property : registry.type("azure.transform").properties) {
        ImGui::SetNextItemWidth(-110*dpi_);
        auto value = property.read(&transform).get<std::array<float, 3>>();
        if (ImGui::DragFloat3(property.label.c_str(), value.data(), 0.01F,
                static_cast<float>(property.minimum), static_cast<float>(property.maximum))) {
            property.write(&transform, value);
            context_->setGizmoTranslation(transform.translation);
            context_->setGizmoRotation(transform.rotation);
            context_->setGizmoScale(transform.scale);
        }
    }
    RenderSettings& settings = context_->renderSettings();
    if (ImGui::BeginCombo(
            "Showcase Look",
            std::string(showcasePresetName(settings.showcasePreset)).c_str())) {
        for (std::uint32_t preset = 0; preset < 5; ++preset) {
            const bool selected = settings.showcasePreset == preset;
            const std::string name(showcasePresetName(preset));
            if (ImGui::Selectable(name.c_str(), selected)) {
                context_->beginEdit();
                applyShowcasePresetLook(settings, preset);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    bool background = settings.characterPresentation.backgroundEnabled;
    if (ImGui::Checkbox("Background", &background)) {
        context_->beginEdit();
        settings.characterPresentation.backgroundEnabled = background;
    }
    bool platform = settings.characterPresentation.platformEnabled;
    if (ImGui::Checkbox("Showcase Platform", &platform)) {
        context_->beginEdit();
        settings.characterPresentation.platformEnabled = platform;
    }
    bool faceSdf = settings.faceSdf.enabled;
    if (ImGui::Checkbox("Face SDF", &faceSdf)) {
        context_->beginEdit();
        settings.faceSdf.enabled = faceSdf;
    }
    float threshold = settings.faceSdf.threshold;
    if (ImGui::SliderFloat("Face SDF Threshold", &threshold, 0.0F, 1.0F)) {
        context_->beginEdit();
        settings.faceSdf.threshold = threshold;
    }
    float softness = settings.faceSdf.softness;
    if (ImGui::SliderFloat("Face SDF Softness", &softness, 0.001F, 0.5F)) {
        context_->beginEdit();
        settings.faceSdf.softness = softness;
    }
    float outline = settings.outline.strength;
    if (ImGui::SliderFloat("Outline", &outline, 0.0F, 2.0F)) {
        context_->beginEdit();
        settings.outline.strength = outline;
    }
    float shadowRadius = settings.shadow.maximumFilterRadiusTexels;
    if (ImGui::SliderFloat(
            "Shadow Softness", &shadowRadius, 1.0F, 16.0F, "%.1f texels")) {
        context_->beginEdit();
        settings.shadow.maximumFilterRadiusTexels = shadowRadius;
    }
    float exposure = settings.grade.exposureEv;
    if (ImGui::SliderFloat("Exposure EV", &exposure, -8.0F, 8.0F)) {
        context_->beginEdit();
        settings.grade.exposureEv = exposure;
    }
    ImGui::EndDisabled();
    ImGui::End();
}

void ImGuiEditorLayer::drawAssetBrowserPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.72F, 0.50F, 0.28F);
#endif
    if(!ImGui::Begin("Content Browser###assets",workspace_.open("assets"))){ImGui::End();return;}
    assetFilter_.Draw("Search assets",250*dpi_);observeWidget("assets.search");ImGui::SameLine();
    ImGui::SetNextItemWidth(120*dpi_);
    const char* types[]={"All","Models","Scripts","Prefabs"};
    if(ImGui::BeginCombo("Type",types[assetType_])){
        for(int type=0;type<4;++type){if(ImGui::Selectable(types[type],assetType_==type))assetType_=type;observeWidget(std::string("type.")+types[type]);}
        ImGui::EndCombo();
    }observeWidget("assets.type");ImGui::SameLine();
    ImGui::Checkbox("Grid",&assetGrid_);
    if(context_->isProject() && ImGui::CollapsingHeader("Import / Create")){
        static std::array<char,1024> source{};ImGui::InputText("glTF / GLB path",source.data(),source.size());
        ImGui::BeginDisabled(context_->importing());if(ImGui::Button("Import"))try{context_->startImport(source.data());}catch(const std::exception& error){context_->log(std::string("ERROR: ")+error.what());}ImGui::EndDisabled();
        if(context_->importing()){ImGui::ProgressBar(context_->importProgress());if(ImGui::Button("Cancel Import"))context_->cancelImport();}
        try{if(auto imported=context_->pollImport())context_->log("Import ready: "+*imported);}catch(const std::exception& error){context_->log(std::string("Import: ")+error.what());}
        const auto& summary=context_->importSummary();
        if(summary.contains("vertices")){
            ImGui::Text("Admission: %zu vertices, %zu joints, %zu materials",summary.at("vertices").get<std::size_t>(),summary.at("joints").get<std::size_t>(),summary.at("materials").get<std::size_t>());
            for(const auto& clip:summary.at("clips"))ImGui::Text("Clip %u: %s (%.3f s)",clip.at("index").get<unsigned>(),clip.at("name").get<std::string>().c_str(),clip.at("duration").get<double>());
        }
        static std::array<char,128> instance{};
        ImGui::InputText("Prefab instance",instance.data(),instance.size());
        ImGui::BeginDisabled(session_->playing() || session_->building());
        if(ImGui::Button("Create empty node"))try{context_->createNode(instance.data());}catch(const std::exception& error){context_->log(std::string("ERROR: ")+error.what());}
        if(ImGui::BeginCombo("Place Prefab","Choose asset")){
            for(const auto& [id,record]:context_->assets().records())if(record.path.extension()==".azureprefab" && ImGui::Selectable(record.virtualPath.c_str()))
                try{context_->placePrefab(id,instance.data());}catch(const std::exception& error){context_->log(std::string("ERROR: ")+error.what());}
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
    }

    if (ImGui::Button("Reload Assets")) {
        static_cast<void>(session_->execute(EditorCommand::ReloadAssets));
    }
    auto resources=context_->resourceStatuses();
    std::map<std::string,std::string> labels;
    for(const auto& resource:resources)labels[resource.id]=resource.path.filename().u8string();
    if(context_->isProject())for(const auto& [id,record]:context_->assets().records()) {
        const auto found=std::find_if(resources.begin(),resources.end(),[&](const auto& resource){return resource.path==record.path;});
        if(found==resources.end()){
            EditorContext::ResourceStatus status;status.id=id;status.path=record.path;status.exists=std::filesystem::is_regular_file(record.path);
            resources.push_back(status);labels[id]=record.virtualPath;
        }else labels[found->id]=record.virtualPath;
    }
    std::set<std::string> directories;
    for(const auto& resource:resources)directories.insert(resource.path.parent_path().u8string());
    ImGui::SameLine();ImGui::SetNextItemWidth(250*dpi_);
    if(ImGui::BeginCombo("Directory",assetDirectory_.empty()?"All directories":std::filesystem::u8path(assetDirectory_).filename().u8string().c_str())){
        if(ImGui::Selectable("All directories",assetDirectory_.empty()))assetDirectory_.clear();
        for(const auto& directory:directories)if(ImGui::Selectable(directory.c_str(),directory==assetDirectory_))assetDirectory_=directory;
        ImGui::EndCombo();
    }
    visibleAssets_=nlohmann::json::array();
    auto place=[&](const EditorContext::ResourceStatus& resource){
        if(session_->playing() || session_->building())return;
        try{
            const auto extension=resource.path.extension();
            if(extension==".azureprefab")context_->placePrefab(resource.id,"prefab-"+std::to_string(context_->scene().nodes.size()));
            else if(extension==".gltf" || extension==".glb"){
                auto id=resource.id;
                if(std::none_of(context_->scene().resources.begin(),context_->scene().resources.end(),[&](const auto& entry){return entry.id==id;}))id=context_->importAsset(resource.path);
                context_->placeResource(id);
            }
        }catch(const std::exception& error){context_->log(std::string("ERROR: ")+error.what());}
    };
    ImGui::BeginChild("asset-list",{0,0});
    if(ImGui::BeginTable("assets",assetGrid_?std::max(1,static_cast<int>(ImGui::GetContentRegionAvail().x/(180*dpi_))):4,
        ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY)){
        if(!assetGrid_){for(const auto* title:{"Asset","Type","State","Users"})ImGui::TableSetupColumn(title);ImGui::TableHeadersRow();}
        for(const auto& resource:resources){
            const auto extension=resource.path.extension().u8string();const auto label=labels.at(resource.id);
            if(!assetFilter_.PassFilter(label.c_str()))continue;
            if(!assetDirectory_.empty() && resource.path.parent_path().u8string()!=assetDirectory_)continue;
            if((assetType_==1&&extension!=".gltf"&&extension!=".glb")||(assetType_==2&&extension!=".lua")||(assetType_==3&&extension!=".azureprefab"))continue;
            visibleAssets_.push_back({{"id",resource.id},{"path",label},{"type",extension},{"ready",resource.exists}});
            ImGui::PushID(resource.id.c_str());
            if(assetGrid_)ImGui::TableNextColumn();else{ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);}
            if(ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick,assetGrid_?ImVec2{0,44*dpi_}:ImVec2{0,0}) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))place(resource);
            observeWidget("asset."+resource.id);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s",label.c_str(),resource.exists?"Ready":"ERROR: Missing source");
            const bool model=extension==".gltf" || extension==".glb";
            const bool sceneResource=std::any_of(context_->scene().resources.begin(),context_->scene().resources.end(),[&](const auto& entry){return entry.id==resource.id;});
            if(model && sceneResource && ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("AZURE_RESOURCE",resource.id.c_str(),resource.id.size()+1);ImGui::TextUnformatted(label.c_str());ImGui::EndDragDropSource();}
            if(!assetGrid_){ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(extension.c_str());ImGui::TableSetColumnIndex(2);ImGui::TextUnformatted(resource.exists?"Ready":"ERROR: Missing");ImGui::TableSetColumnIndex(3);ImGui::Text("%zu",resource.dependentNodeCount);}
            ImGui::PopID();
        }ImGui::EndTable();
    }
    if(visibleAssets_.empty())ImGui::TextDisabled("No assets match the directory, search and type filters.");
    ImGui::EndChild();ImGui::End();
}

void ImGuiEditorLayer::drawCapturePanel() {
    if(!ImGui::Begin("Capture###capture",workspace_.open("capture"))){ImGui::End();return;}
    std::array<char, 128> label{};
    const std::string& current = session_->captureLabel();
    std::memcpy(label.data(), current.data(),
        std::min(current.size(), label.size() - 1));
    if (ImGui::InputText("Label", label.data(), label.size())) {
        session_->setCaptureLabel(label.data());
    }
    if (ImGui::Button("Capture Viewport")) {
        static_cast<void>(session_->execute(EditorCommand::Capture));
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("PNG + semantic label");
    ImGui::End();
}
void ImGuiEditorLayer::drawAnimationPanel(){
    ImGui::SetNextWindowSize({360,260},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Animation Preview###animation",workspace_.open("animation"))){ImGui::End();return;}
    if(context_->isProject() && context_->selectedNode()){
        try{
            const auto data=context_->componentData(context_->selectedNode()->id,"azure.animator");
            if(!data.is_null() && !data.at("asset").get<std::string>().empty()){
                std::ifstream input(context_->assets().resolveReference(data.at("asset").get<std::string>()));nlohmann::json graph;input>>graph;
                static std::string selected,previous,node;static float time=0,crossfade=.18F;
                if(node!=context_->selectedNode()->id){node=context_->selectedNode()->id;selected=data.at("state").get<std::string>();previous=selected;time=0;}
                ImGui::BeginDisabled(session_->playing() || session_->building());
                bool changed=false;
                if(ImGui::BeginCombo("Semantic state",selected.c_str())){
                    for(const auto& state:graph.at("states")){
                        const auto name=state.at("name").get<std::string>();
                        if(ImGui::Selectable(name.c_str(),name==selected)){previous=selected;selected=name;time=0;changed=true;}
                    }ImGui::EndCombo();
                }
                changed=ImGui::DragFloat("Time (seconds)",&time,.01F,0,600)||changed;
                changed=ImGui::SliderFloat("Crossfade (seconds)",&crossfade,0,2)||changed;
                if(ImGui::Button("Preview pose") || changed)context_->previewAnimation(selected,time,previous,crossfade);
                ImGui::SameLine();if(ImGui::Button("Clear preview"))context_->clearAnimationPreview();
                if(ImGui::Button("Set initial semantic"))context_->setComponentField("azure.animator","state",selected);
                ImGui::EndDisabled();
                for(const auto& state:graph.at("states"))ImGui::Text("%s: clip %u, %s",state.at("name").get<std::string>().c_str(),state.at("clip").get<unsigned>(),state.value("loop",true)?"loop":"once");
                if(const auto& preview=context_->animationPreview();preview)ImGui::Text("Clip %u, time %.3f, blend %.3f",preview->clip,preview->time,preview->blend);
            }else ImGui::TextUnformatted("Select a node with a configured animator.");
        }catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}
    }
    ImGui::End();
}
void ImGuiEditorLayer::drawGameplayDebugPanel(){
    ImGui::SetNextWindowSize({370,280},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Gameplay Debug###gameplay-debug",workspace_.open("gameplay-debug"))){ImGui::End();return;}
    ImGui::Checkbox("Collision and camera overlay",&session_->debugOverlay);
    const auto scene=session_->viewScene();auto* runtime=session_->runtime();auto* game=session_->game();
    for(std::size_t index=0;index<scene.nodes.size();++index){const auto& node=scene.nodes[index];
        const auto entity=runtime?runtime->entity(node.id):ecs::kInvalidEntity;
        const bool character=runtime?runtime->world().has<game::Character>(entity):!context_->componentData(node.id,"azure.character").is_null();
        const bool body=runtime?runtime->world().has<game::RigidBody>(entity):!context_->componentData(node.id,"azure.rigid-body").is_null();
        if(!character&&!body)continue;
        ImGui::PushID(node.id.c_str());
        if(ImGui::Selectable(node.id.c_str(),context_->selectedNode() && context_->selectedNode()->id==node.id)){
            for(std::size_t edit=0;edit<context_->scene().nodes.size();++edit)if(context_->scene().nodes[edit].id==node.id){context_->selectNode(edit);break;}
        }
        if(character && game){const auto v=game->physics().velocity(entity);ImGui::Text("Velocity %.2f %.2f %.2f, grounded %s",v[0],v[1],v[2],game->physics().grounded(entity)?"yes":"no");}
        ImGui::PopID();
    }
    if(game && game->hasCamera()){
        const auto& camera=game->camera();ImGui::Text("Camera distance %.2f / %.2f",camera.actualDistance(),camera.distance());
        const auto target=camera.target();ImGui::Text("Camera target %.2f %.2f %.2f",target[0],target[1],target[2]);
    }
    if(game && game->interactionTarget()){
        const auto& target=*game->interactionTarget();ImGui::Text("Interaction: %s",target.node.c_str());
        if(ImGui::Button("Locate interaction"))for(std::size_t i=0;i<context_->scene().nodes.size();++i)if(context_->scene().nodes[i].id==target.node){context_->selectNode(i);break;}
    }
    if(auto* scripts=session_->scripts())for(const auto& error:scripts->errors()){
        ImGui::TextWrapped("%s",error.c_str());
        for(std::size_t i=0;i<context_->scene().nodes.size();++i){const auto& node=context_->scene().nodes[i];const auto script=context_->componentData(node.id,"azure.script");
            if(!script.is_null() && error.find(script.at("asset").get<std::string>())!=std::string::npos){ImGui::PushID(node.id.c_str());if(ImGui::SmallButton(("Locate "+node.id).c_str()))context_->selectNode(i);ImGui::PopID();}}
    }
    ImGui::End();
}

void ImGuiEditorLayer::drawBuildPanel() {
    session_->pollBuild();
    if(!ImGui::Begin("Build Game###build",workspace_.open("build"))){ImGui::End();return;}
    static std::array<char,1024> install{}, output{}, project{};
    static bool replace = false;
    ImGui::InputText("New game directory", project.data(), project.size());
    ImGui::BeginDisabled(session_->building());
    if(ImGui::Button("Create game template")) {
        try { const std::filesystem::path path(project.data()); Project::createGame(path,path.filename().string()); context_->log("Game template created: "+path.string()); }
        catch(const std::exception& error) { context_->log("ERROR: "+std::string(error.what())); }
    }
    ImGui::EndDisabled();
    ImGui::InputText("Release engine directory", install.data(), install.size());
    ImGui::InputText("Game output directory", output.data(), output.size());
    ImGui::Checkbox("Replace existing game package", &replace);
    ImGui::BeginDisabled(!context_->isProject() || session_->building() || session_->playing());
    if(ImGui::Button("Build Windows game"))static_cast<void>(session_->startBuild(install.data(),output.data(),replace));
    ImGui::EndDisabled();
    if(session_->building())ImGui::TextUnformatted("Building...");
    else if(!session_->buildResult().message.empty())ImGui::TextWrapped("%s (%.0f ms)",session_->buildResult().message.c_str(),session_->buildResult().milliseconds);
    ImGui::End();
}
void ImGuiEditorLayer::drawConsolePanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.50F, 0.72F, 0.50F, 0.28F);
#endif
    if(!ImGui::Begin("Console###console",workspace_.open("console"))){ImGui::End();return;}
    if(auto* scripts=session_->scripts())for(const auto& error:scripts->errors())ImGui::TextWrapped("Script: %s",error.c_str());
    if(auto* levels=session_->levels())if(!levels->lastError().empty())ImGui::TextWrapped("Level: %s",levels->lastError().c_str());
    if(auto* presentation=session_->presentation())for(const auto& error:presentation->errors())ImGui::TextWrapped("Presentation: %s",error.c_str());
    consoleFilter_.Draw("Search logs",240*dpi_);observeWidget("console.search");ImGui::SameLine();
    ImGui::SetNextItemWidth(100*dpi_);ImGui::Combo("Level",&consoleLevel_,"All\0Warnings\0Errors\0");ImGui::SameLine();
    const auto messages=azurerender::RuntimeDiagnostics::instance().messages();
    auto accepts=[&](const std::string& text){
        if(!consoleFilter_.PassFilter(text.c_str()))return false;
        const bool error=text.find("ERROR")!=std::string::npos||text.find("error")!=std::string::npos;
        const bool warning=text.find("warn")!=std::string::npos||text.find("WARN")!=std::string::npos;
        return consoleLevel_==0||(consoleLevel_==1&&(warning||error))||(consoleLevel_==2&&error);
    };
    if(ImGui::Button("Copy visible")) {std::string text;for(const auto& message:messages)if(accepts(message))text+=message+"\n";ImGui::SetClipboardText(text.c_str());}
    for(const auto& message:messages)if(accepts(message)) {
        const bool error=message.find("ERROR")!=std::string::npos||message.find("error")!=std::string::npos;
        if(error)ImGui::PushStyleColor(ImGuiCol_Text,{1,.55F,.45F,1});
        ImGui::TextUnformatted(message.c_str());if(error)ImGui::PopStyleColor();
        if(ImGui::IsItemClicked())for(std::size_t i=0;i<context_->scene().nodes.size();++i)
            if(message.find(context_->scene().nodes[i].id)!=std::string::npos){context_->selectNode(i);break;}
    }
    ImGui::End();
}

}  // namespace azurerender
#else

ImGuiEditorLayer::ImGuiEditorLayer(std::shared_ptr<EditorSession> session)
    : session_(std::move(session)) {
    if (session_ != nullptr) {
        context_ = &session_->context();
    }
}

ImGuiEditorLayer::~ImGuiEditorLayer() = default;

void ImGuiEditorLayer::initialize(
    GLFWwindow*, VkInstance, VkPhysicalDevice, VkDevice, std::uint32_t,
    VkQueue, VkRenderPass, std::uint32_t, VkFormat) {}

void ImGuiEditorLayer::shutdownVulkan() {}
void ImGuiEditorLayer::newFrame() {}
void ImGuiEditorLayer::drawPanels() {}
void ImGuiEditorLayer::render(VkCommandBuffer) {}
void ImGuiEditorLayer::setViewportImages(
    VkSampler, const std::vector<VkImageView>&, std::uint32_t, std::uint32_t) {}
void ImGuiEditorLayer::clearViewportImages() {}
void ImGuiEditorLayer::setViewportImageIndex(std::uint32_t) {}
EditorViewportInput ImGuiEditorLayer::consumeViewportInput() noexcept {
    return {};
}
bool ImGuiEditorLayer::consumeViewportResizeRequest(
    std::uint32_t&, std::uint32_t&) noexcept {
    return false;
}

}  // namespace azurerender
#endif
