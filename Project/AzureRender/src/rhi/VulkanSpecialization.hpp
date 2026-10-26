#pragma once
#include "rhi/Rhi.hpp"
#include <array>

namespace azurerender::rhi {
// Lives through vkCreate*Pipelines; neither descriptor nor pointers escape.
class VulkanSpecialization final {
public:
    explicit VulkanSpecialization(const ShaderSpecialization& constants) {
        validateShaderSpecialization(constants);
        for(std::size_t i=0;i<constants.size();++i){
            entries_[i]={constants[i].id,static_cast<std::uint32_t>(i*sizeof(std::uint32_t)),sizeof(std::uint32_t)};
            values_[i]=constants[i].value;
        }
        info_={static_cast<std::uint32_t>(constants.size()),entries_.data(),constants.size()*sizeof(std::uint32_t),values_.data()};
    }
    VulkanSpecialization(const VulkanSpecialization&)=delete;
    VulkanSpecialization& operator=(const VulkanSpecialization&)=delete;
    const VkSpecializationInfo* get() const {return info_.mapEntryCount?&info_:nullptr;}
private:
    std::array<VkSpecializationMapEntry,kMaxShaderSpecializationConstants> entries_{};
    std::array<std::uint32_t,kMaxShaderSpecializationConstants> values_{};
    VkSpecializationInfo info_{};
};
}
