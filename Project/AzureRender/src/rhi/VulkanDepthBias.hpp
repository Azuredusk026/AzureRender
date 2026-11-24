#pragma once
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace azurerender::rhi {
// Device creation owns negotiation. Only the two enabled features are returned;
// optional representations remain disabled and the returned chain is detached.
inline VkPhysicalDeviceDepthBiasControlFeaturesEXT queryExactDepthBias(VkPhysicalDevice physical) {
    VkPhysicalDeviceDepthBiasControlFeaturesEXT enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT};
    std::uint32_t count=0;
    if(vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,nullptr)!=VK_SUCCESS)
        throw std::runtime_error("Cannot query depth-bias device extensions");
    std::vector<VkExtensionProperties> extensions(count);
    if(vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,extensions.data())!=VK_SUCCESS)
        throw std::runtime_error("Cannot read depth-bias device extensions");
    extensions.resize(count);
    if(std::none_of(extensions.begin(),extensions.end(),[](const auto& value){
        return std::strcmp(value.extensionName,VK_EXT_DEPTH_BIAS_CONTROL_EXTENSION_NAME)==0;}))return enabled;
    VkPhysicalDeviceDepthBiasControlFeaturesEXT available{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&available;
    vkGetPhysicalDeviceFeatures2(physical,&features);
    enabled.depthBiasControl=enabled.depthBiasExact=available.depthBiasControl&&available.depthBiasExact;
    return enabled;
}
}
