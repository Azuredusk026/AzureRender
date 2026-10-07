#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include <string>
#include <nlohmann/json.hpp>
namespace azurerender {
class EditRegistry;
class EditorSession;
struct PreviewImage {
    std::uint64_t handle=0;
    VkImageView image=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    std::uint32_t width=0,height=0;
};
struct DeveloperServices {
    std::function<PreviewImage(std::uint64_t)> image;
    std::function<nlohmann::json()> report;
    std::function<nlohmann::json(const std::string&,const nlohmann::json&)> operation;
};
void registerDeveloperOperations(EditRegistry&,EditorSession&);
}
