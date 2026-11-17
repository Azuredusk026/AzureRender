#pragma once

#include "assets/image/ImageCodec.hpp"
#include "render/SampledTextureCache.hpp"

#include <filesystem>
#include <stdexcept>

namespace azurerender {

inline SampledTextureBinding loadScreenEffectTexture(rhi::IRhi& backend, SampledTextureCache& textures,
                                                     const std::filesystem::path& path, VkExtent2D extent,
                                                     VkFilter filter) {
    const auto source = decodeImageFile(path);
    if (source.width != extent.width || source.height != extent.height)
        throw std::invalid_argument("Unexpected opening effect texture dimensions");
    SampledTextureUpload upload;
    upload.width = extent.width;
    upload.height = extent.height;
    upload.format = VK_FORMAT_R8G8B8A8_SRGB;
    upload.mipLevels = 1;
    upload.sampler.filter = filter;
    upload.sampler.maxLod = 0;
    upload.sampler.mipmapLinear = false;
    if (!backend.supportsSampledImage(upload.format, filter == VK_FILTER_LINEAR))
        throw std::runtime_error("The device cannot sample the opening effect texture format");
    return textures.acquire(convertImageToRgba8(source).pixels, upload);
}

} // namespace azurerender
