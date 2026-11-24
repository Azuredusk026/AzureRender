#pragma once
#include "rhi/GpuAllocator.hpp"
#include "rhi/VulkanRhi.hpp"
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace azurerender::test {
inline void vkRequire(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(result));
}

// Standalone compute host. Production RHI and allocation implementations are
// exercised without constructing a window, world or application renderer.
class VulkanComputeFixture {
public:
    VkInstance instance=VK_NULL_HANDLE;
    VkPhysicalDevice physical=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    std::uint32_t family=0,timestampBits=0;
    std::atomic<unsigned> errors{0};
    rhi::GpuAllocator allocator;
    std::unique_ptr<rhi::VulkanRhi> backend;

    VulkanComputeFixture() {
        try {
            VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            application.pApplicationName="AzureRender Compute Contract";application.apiVersion=VK_API_VERSION_1_3;
            const char* layer="VK_LAYER_KHRONOS_validation";const char* extension=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            VkValidationFeatureEnableEXT enabled=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
            VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
            validation.enabledValidationFeatureCount=1;validation.pEnabledValidationFeatures=&enabled;
            VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            create.pApplicationInfo=&application;create.enabledLayerCount=1;create.ppEnabledLayerNames=&layer;
            create.enabledExtensionCount=1;create.ppEnabledExtensionNames=&extension;create.pNext=&validation;
            vkRequire(vkCreateInstance(&create,nullptr,&instance),"vkCreateInstance");
            VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug.pUserData=this;
            debug.pfnUserCallback=[](VkDebugUtilsMessageSeverityFlagBitsEXT,VkDebugUtilsMessageTypeFlagsEXT,
                const VkDebugUtilsMessengerCallbackDataEXT* data,void* user)->VkBool32 {
                ++static_cast<VulkanComputeFixture*>(user)->errors;std::cerr<<data->pMessage<<'\n';return VK_FALSE;
            };
            const auto make=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));
            if(!make) throw std::runtime_error("Validation debug messenger is unavailable");
            vkRequire(make(instance,&debug,nullptr,&messenger_),"vkCreateDebugUtilsMessengerEXT");
            std::uint32_t count=0;vkRequire(vkEnumeratePhysicalDevices(instance,&count,nullptr),"vkEnumeratePhysicalDevices");
            std::vector<VkPhysicalDevice> devices(count);vkRequire(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"vkEnumeratePhysicalDevices");
            for(auto candidate:devices) {
                VkPhysicalDeviceProperties value{};vkGetPhysicalDeviceProperties(candidate,&value);
                if(physical==VK_NULL_HANDLE || value.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) physical=candidate;
                if(value.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) break;
            }
            if(physical==VK_NULL_HANDLE) throw std::runtime_error("No Vulkan compute device");
            vkGetPhysicalDeviceProperties(physical,&properties);
            vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
            std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
            bool found=false;
            for(std::uint32_t index=0;index<count;++index) if((families[index].queueFlags&VK_QUEUE_COMPUTE_BIT)!=0) {
                family=index;timestampBits=families[index].timestampValidBits;found=true;break;
            }
            if(!found || !timestampBits) throw std::runtime_error("Compute queue with timestamps is required");
            const float priority=1;
            VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueInfo.queueFamilyIndex=family;queueInfo.queueCount=1;queueInfo.pQueuePriorities=&priority;
            VkPhysicalDeviceFeatures features{};vkGetPhysicalDeviceFeatures(physical,&features);
            VkPhysicalDeviceFeatures used{};used.shaderStorageImageExtendedFormats=features.shaderStorageImageExtendedFormats;
            VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            deviceInfo.queueCreateInfoCount=1;deviceInfo.pQueueCreateInfos=&queueInfo;deviceInfo.pEnabledFeatures=&used;
            vkRequire(vkCreateDevice(physical,&deviceInfo,nullptr,&device),"vkCreateDevice");
            vkGetDeviceQueue(device,family,0,&queue);
            VkCommandPoolCreateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};commandInfo.queueFamilyIndex=family;
            vkRequire(vkCreateCommandPool(device,&commandInfo,nullptr,&pool),"vkCreateCommandPool");
            allocator.initialize(instance,physical,device,VK_API_VERSION_1_3);
            backend=std::make_unique<rhi::VulkanRhi>(device,physical,queue,pool,allocator);
        } catch(...) {close();throw;}
    }
    ~VulkanComputeFixture() {close();}
    VulkanComputeFixture(const VulkanComputeFixture&)=delete;
    VulkanComputeFixture& operator=(const VulkanComputeFixture&)=delete;

    void submit(const std::function<void(rhi::ICommandRecorder&,VkCommandBuffer)>& record) {
        VkCommandBuffer command=VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool=pool;allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocate.commandBufferCount=1;
        vkRequire(vkAllocateCommandBuffers(device,&allocate,&command),"vkAllocateCommandBuffers");
        try {
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkRequire(vkBeginCommandBuffer(command,&begin),"vkBeginCommandBuffer");
            rhi::VulkanCommandRecorder recorder(command);record(recorder,command);
            vkRequire(vkEndCommandBuffer(command),"vkEndCommandBuffer");
            VkSubmitInfo submission{VK_STRUCTURE_TYPE_SUBMIT_INFO};submission.commandBufferCount=1;submission.pCommandBuffers=&command;
            vkRequire(vkQueueSubmit(queue,1,&submission,VK_NULL_HANDLE),"vkQueueSubmit");
            vkRequire(vkQueueWaitIdle(queue),"vkQueueWaitIdle");
        } catch(...) {vkDeviceWaitIdle(device);vkFreeCommandBuffers(device,pool,1,&command);throw;}
        vkFreeCommandBuffers(device,pool,1,&command);
    }
    void close() noexcept {
        if(device) vkDeviceWaitIdle(device);
        backend.reset();allocator.shutdown();
        if(pool) vkDestroyCommandPool(device,pool,nullptr);
        if(device) vkDestroyDevice(device,nullptr);
        if(messenger_) {
            const auto destroy=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"));
            if(destroy) destroy(instance,messenger_,nullptr);
        }
        if(instance) vkDestroyInstance(instance,nullptr);
        instance=VK_NULL_HANDLE;device=VK_NULL_HANDLE;pool=VK_NULL_HANDLE;messenger_=VK_NULL_HANDLE;
    }
private:
    VkDebugUtilsMessengerEXT messenger_=VK_NULL_HANDLE;
};
}
