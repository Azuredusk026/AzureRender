#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <stdexcept>
#include <optional>
#include <string>
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
// Only the primary-UV surface and coverage families participate. Resource and
// lighting policies keep their data ownership; the reference family contains
// the typed lighting implementation for every selected surface.
inline std::optional<std::string> materialUvGradientProgram(std::string path){
    const auto slash=path.find_last_of("/\\");const auto start=slash==std::string::npos?0:slash+1;
    const auto name=path.substr(start);
    if(name.size()<9||name.compare(name.size()-9,9,".frag.spv")!=0)return std::nullopt;
    const bool mesh=name.rfind("mesh",0)==0||name.rfind("character_snapshot",0)==0;
    const bool coverage=name.rfind("character_depth",0)==0||name.rfind("shadow",0)==0
        ||name.rfind("outline_mask",0)==0||name.rfind("outline_depth",0)==0||name.rfind("outline_view_scaled",0)==0;
    if((!mesh&&!coverage)||name.find("_early")!=std::string::npos||name.find("_integral")!=std::string::npos
        ||name.find("_fog")!=std::string::npos||name.find("_radiance")!=std::string::npos)return std::nullopt;
    const auto lighting=path.find("_lighting",start);if(lighting!=std::string::npos)path.erase(lighting,9);
    path.insert(path.size()-9,"_uvquad");return path;
}

}
