#include "AzureRenderApp.hpp"
#include "platform/GlfwFrontend.hpp"
#include "editor/EditorWorkspace.hpp"
#include "editor/EditorTheme.hpp"
#include "editor/projects/ProjectCenterController.hpp"
#include "editor/projects/ProjectCenterView.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stdexcept>
#include <algorithm>
namespace {
void check(VkResult result) { if(result!=VK_SUCCESS)throw std::runtime_error("Project center Vulkan error: "+std::to_string(result)); }
}
std::shared_ptr<azurerender::EditorContext> AzureRenderApp::showProjectCenter(const AzureRenderOptions& options,const std::string& initialProject) {
    runOptions_=options; runOptions_.captureDirectory=(azurerender::EditorWorkspace::configDirectory()/"logs").u8string();
    azurerender::GlfwFrontendConfig config;config.width=options.width;config.height=options.height;config.title="AzureRender - Project Center";
    frontend_=std::make_unique<azurerender::GlfwFrontend>(config);
    createInstance();setupDebugMessenger();createSurface();pickPhysicalDevice();createLogicalDevice();
    if(findQueueFamilies(physicalDevice_).present!=findQueueFamilies(physicalDevice_).graphics)
        throw std::runtime_error("Project center requires a graphics queue supporting presentation");
    ImGui_ImplVulkanH_Window window;window.Surface=surface_;
    const VkFormat formats[]={VK_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_R8G8B8A8_UNORM};
    window.SurfaceFormat=ImGui_ImplVulkanH_SelectSurfaceFormat(physicalDevice_,surface_,formats,2,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    const VkPresentModeKHR modes[]={VK_PRESENT_MODE_FIFO_KHR};
    window.PresentMode=ImGui_ImplVulkanH_SelectPresentMode(physicalDevice_,surface_,modes,1);
    VkDescriptorPool pool=VK_NULL_HANDLE;bool context=false,glfw=false,vulkan=false;
    const auto release=[&] {
        vkDeviceWaitIdle(device_);
        if(vulkan)ImGui_ImplVulkan_Shutdown();if(glfw)ImGui_ImplGlfw_Shutdown();if(context)ImGui::DestroyContext();
        if(pool)vkDestroyDescriptorPool(device_,pool,nullptr);
        ImGui_ImplVulkanH_DestroyWindow(instance_,device_,&window,nullptr);
    };
    try {
        const auto [width,height]=frontend_->framebufferSize();
        ImGui_ImplVulkanH_CreateOrResizeWindow(instance_,physicalDevice_,device_,&window,graphicsQueueFamily_,nullptr,width,height,2,0);
        VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_SAMPLER,128},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,128},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,128}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};poolInfo.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;poolInfo.maxSets=384;poolInfo.poolSizeCount=3;poolInfo.pPoolSizes=sizes;
        check(vkCreateDescriptorPool(device_,&poolInfo,nullptr,&pool));
        IMGUI_CHECKVERSION();ImGui::CreateContext();context=true;
        auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
        float scale=1,y=1;glfwGetWindowContentScale(frontend_->nativeHandle(),&scale,&y);azurerender::EditorTheme::apply(scale);
        const auto font=azurerender::ResourceLocator(std::filesystem::u8path(options.resourceRoot)).publicAsset("fonts/NotoSansCJKsc-Regular.otf");
        if(!io.Fonts->AddFontFromFileTTF(font.u8string().c_str(),14))throw std::runtime_error("Project center font is unavailable");
        if(!ImGui_ImplGlfw_InitForVulkan(frontend_->nativeHandle(),true))throw std::runtime_error("Project center GLFW backend failed");glfw=true;
        ImGui_ImplVulkan_InitInfo info{};info.ApiVersion=VK_API_VERSION_1_3;info.Instance=instance_;info.PhysicalDevice=physicalDevice_;info.Device=device_;info.QueueFamily=graphicsQueueFamily_;info.Queue=graphicsQueue_;info.DescriptorPool=pool;info.MinImageCount=2;info.ImageCount=window.ImageCount;info.PipelineInfoMain.RenderPass=window.RenderPass;info.PipelineInfoMain.MSAASamples=VK_SAMPLE_COUNT_1_BIT;
        if(!ImGui_ImplVulkan_Init(&info))throw std::runtime_error("Project center Vulkan backend failed");vulkan=true;
        azurerender::ProjectCenterController controller(azurerender::EditorWorkspace::configDirectory()/"recent-projects.json");
        azurerender::ProjectCenterView view;if(!initialProject.empty())controller.open(std::filesystem::u8path(initialProject));
        std::uint64_t frames=0;
        while(!frontend_->shouldClose() || controller.busy()) {
            frontend_->pollEvents();controller.poll();if(frontend_->shouldClose()&&!controller.busy())break;
            if(auto ready=controller.takeReady()){release();return ready;}
            if(frontend_->shouldClose()){glfwWaitEventsTimeout(.01);continue;}
            const auto [w,h]=frontend_->framebufferSize();if(w<=0||h<=0){glfwWaitEventsTimeout(.05);continue;}
            if(w!=window.Width||h!=window.Height) {vkDeviceWaitIdle(device_);ImGui_ImplVulkanH_CreateOrResizeWindow(instance_,physicalDevice_,device_,&window,graphicsQueueFamily_,nullptr,w,h,2,0);}
            float dpi=1,dy=1;glfwGetWindowContentScale(frontend_->nativeHandle(),&dpi,&dy);if(std::abs(dpi-scale)>.01F){scale=dpi;azurerender::EditorTheme::apply(scale);}
            ImGui_ImplVulkan_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();view.draw(controller);ImGui::Render();
            const auto& semaphores=window.FrameSemaphores[window.SemaphoreIndex];
            auto acquired=vkAcquireNextImageKHR(device_,window.Swapchain,UINT64_MAX,semaphores.ImageAcquiredSemaphore,VK_NULL_HANDLE,&window.FrameIndex);
            if(acquired==VK_ERROR_OUT_OF_DATE_KHR){window.Width=0;continue;}if(acquired!=VK_SUBOPTIMAL_KHR)check(acquired);
            const auto renderComplete=window.FrameSemaphores[window.FrameIndex].RenderCompleteSemaphore;
            auto& frame=window.Frames[window.FrameIndex];check(vkWaitForFences(device_,1,&frame.Fence,VK_TRUE,UINT64_MAX));check(vkResetFences(device_,1,&frame.Fence));check(vkResetCommandPool(device_,frame.CommandPool,0));
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(frame.CommandBuffer,&begin));
            VkClearValue clear{};clear.color={{.06F,.07F,.09F,1}};VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=window.RenderPass;pass.framebuffer=frame.Framebuffer;pass.renderArea.extent={static_cast<uint32_t>(w),static_cast<uint32_t>(h)};pass.clearValueCount=1;pass.pClearValues=&clear;
            vkCmdBeginRenderPass(frame.CommandBuffer,&pass,VK_SUBPASS_CONTENTS_INLINE);ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),frame.CommandBuffer);vkCmdEndRenderPass(frame.CommandBuffer);check(vkEndCommandBuffer(frame.CommandBuffer));
            VkPipelineStageFlags stage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&semaphores.ImageAcquiredSemaphore;submit.pWaitDstStageMask=&stage;submit.commandBufferCount=1;submit.pCommandBuffers=&frame.CommandBuffer;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&renderComplete;check(vkQueueSubmit(graphicsQueue_,1,&submit,frame.Fence));
            VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&renderComplete;present.swapchainCount=1;present.pSwapchains=&window.Swapchain;present.pImageIndices=&window.FrameIndex;
            const auto result=vkQueuePresentKHR(presentQueue_,&present);if(result==VK_ERROR_OUT_OF_DATE_KHR||result==VK_SUBOPTIMAL_KHR)window.Width=0;else check(result);
            window.SemaphoreIndex=(window.SemaphoreIndex+1)%window.SemaphoreCount;
            if(options.smokeFrameLimit>0&&++frames>=options.smokeFrameLimit)break;
        }
        release();return {};
    }catch(...){release();throw;}
}
