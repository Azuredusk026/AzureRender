#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <stdexcept>
namespace azurerender {
enum class MaterialUvGradientPolicy : std::uint32_t {Device,MirroredCoarseY};
struct MaterialUvGradientSupport {VkShaderStageFlags stages=0;VkSubgroupFeatureFlags operations=0;};
inline void validateMaterialUvGradientPolicy(MaterialUvGradientPolicy policy,MaterialUvGradientSupport support){
    if(policy!=MaterialUvGradientPolicy::Device&&policy!=MaterialUvGradientPolicy::MirroredCoarseY)
        throw std::invalid_argument("Unknown material UV gradient policy");
    if(policy==MaterialUvGradientPolicy::MirroredCoarseY
        &&((support.stages&VK_SHADER_STAGE_FRAGMENT_BIT)==0||(support.operations&VK_SUBGROUP_FEATURE_QUAD_BIT)==0))
        throw std::invalid_argument("Mirrored coarse UV gradients require fragment subgroup quad operations");
}
inline MaterialUvGradientSupport queryMaterialUvGradientSupport(VkPhysicalDevice physical){
    if(physical==VK_NULL_HANDLE)throw std::invalid_argument("UV gradient capability query requires a physical device");
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};properties.pNext=&subgroup;
    vkGetPhysicalDeviceProperties2(physical,&properties);
    return {subgroup.supportedStages,subgroup.supportedOperations};
}
}
