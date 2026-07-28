#include "VulkanApp.hpp"

#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

using Matrix4 = std::array<float, 16>;
using Vector3 = std::array<float, 3>;

Matrix4 multiply(const Matrix4& left, const Matrix4& right) {
    Matrix4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result[column * 4 + row] +=
                    left[inner * 4 + row] * right[column * 4 + inner];
            }
        }
    }
    return result;
}

Vector3 subtract(const Vector3& left, const Vector3& right) {
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
}

float dot(const Vector3& left, const Vector3& right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

Vector3 cross(const Vector3& left, const Vector3& right) {
    return {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    };
}

Vector3 transformPosition(const Matrix4& transform, const Vector3& position) {
    return {
        transform[0] * position[0] + transform[4] * position[1]
            + transform[8] * position[2] + transform[12],
        transform[1] * position[0] + transform[5] * position[1]
            + transform[9] * position[2] + transform[13],
        transform[2] * position[0] + transform[6] * position[1]
            + transform[10] * position[2] + transform[14],
    };
}

Vector3 normalize(const Vector3& value) {
    const float length = std::sqrt(dot(value, value));
    return {value[0] / length, value[1] / length, value[2] / length};
}

Matrix4 rotationY(const float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        cosine, 0.0F, -sine, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        sine, 0.0F, cosine, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

Matrix4 translation(const float x, const float y, const float z) {
    return {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        x, y, z, 1.0F,
    };
}

Matrix4 uniformScale(const float scale) {
    return {
        scale, 0.0F, 0.0F, 0.0F,
        0.0F, scale, 0.0F, 0.0F,
        0.0F, 0.0F, scale, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

Matrix4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
    const Vector3 forward = normalize(subtract(target, eye));
    const Vector3 right = normalize(cross(forward, up));
    const Vector3 correctedUp = cross(right, forward);
    return {
        right[0], correctedUp[0], -forward[0], 0.0F,
        right[1], correctedUp[1], -forward[1], 0.0F,
        right[2], correctedUp[2], -forward[2], 0.0F,
        -dot(right, eye), -dot(correctedUp, eye), dot(forward, eye), 1.0F,
    };
}

Matrix4 perspective(const float verticalFov, const float aspect, const float nearPlane, const float farPlane) {
    const float focalLength = 1.0F / std::tan(verticalFov * 0.5F);
    return {
        focalLength / aspect, 0.0F, 0.0F, 0.0F,
        0.0F, -focalLength, 0.0F, 0.0F,
        0.0F, 0.0F, farPlane / (nearPlane - farPlane), -1.0F,
        0.0F, 0.0F, (nearPlane * farPlane) / (nearPlane - farPlane), 0.0F,
    };
}

Matrix4 orthographic(
    const float left,
    const float right,
    const float bottom,
    const float top,
    const float nearPlane,
    const float farPlane) {
    return {
        2.0F / (right - left), 0.0F, 0.0F, 0.0F,
        0.0F, -2.0F / (top - bottom), 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F / (nearPlane - farPlane), 0.0F,
        -(right + left) / (right - left),
        (top + bottom) / (top - bottom),
        nearPlane / (nearPlane - farPlane),
        1.0F,
    };
}

void appendShowcasePlatform(LoadedAsset& asset) {
    constexpr std::uint32_t kSegments = 96;
    const Vector3 boundsCenter = {
        (asset.boundsMin[0] + asset.boundsMax[0]) * 0.5F,
        (asset.boundsMin[1] + asset.boundsMax[1]) * 0.5F,
        (asset.boundsMin[2] + asset.boundsMax[2]) * 0.5F,
    };
    const float largestExtent = std::max({
        asset.boundsMax[0] - asset.boundsMin[0],
        asset.boundsMax[1] - asset.boundsMin[1],
        asset.boundsMax[2] - asset.boundsMin[2],
    });
    const float radius = largestExtent * 0.40F;
    const float topY = asset.boundsMin[1] - largestExtent * 0.004F;
    const float bottomY = topY - largestExtent * 0.050F;

    AssetMaterial platformMaterial;
    platformMaterial.baseColorWidth = 2;
    platformMaterial.baseColorHeight = 2;
    platformMaterial.baseColorPixels = {
        38, 50, 66, 255, 38, 50, 66, 255,
        38, 50, 66, 255, 38, 50, 66, 255,
    };
    platformMaterial.normalWidth = 2;
    platformMaterial.normalHeight = 2;
    platformMaterial.normalPixels = {
        128, 128, 255, 255, 128, 128, 255, 255,
        128, 128, 255, 255, 128, 128, 255, 255,
    };
    platformMaterial.metallicRoughnessWidth = 2;
    platformMaterial.metallicRoughnessHeight = 2;
    platformMaterial.metallicRoughnessPixels = {
        255, 210, 28, 255, 255, 210, 28, 255,
        255, 210, 28, 255, 255, 210, 28, 255,
    };
    platformMaterial.specularEmissiveWidth = 2;
    platformMaterial.specularEmissiveHeight = 2;
    platformMaterial.specularEmissivePixels = {
        0, 0, 0, 96, 0, 0, 0, 96,
        0, 0, 0, 96, 0, 0, 0, 96,
    };
    platformMaterial.styleMaskWidth = 2;
    platformMaterial.styleMaskHeight = 2;
    platformMaterial.styleMaskPixels.assign(16, 0);
    for (std::size_t alpha = 3; alpha < 16; alpha += 4) {
        platformMaterial.styleMaskPixels[alpha] = 255;
    }
    platformMaterial.matcapWidth = 2;
    platformMaterial.matcapHeight = 2;
    platformMaterial.matcapPixels = platformMaterial.styleMaskPixels;
    platformMaterial.hairDataWidth = 2;
    platformMaterial.hairDataHeight = 2;
    platformMaterial.hairDataPixels.assign(16, 128);
    platformMaterial.showcasePlatform = 1.0F;
    platformMaterial.doubleSided = true;

    const std::uint32_t materialIndex =
        static_cast<std::uint32_t>(asset.materials.size());
    asset.materials.push_back(std::move(platformMaterial));
    const std::uint32_t firstVertex =
        static_cast<std::uint32_t>(asset.vertices.size());
    const std::uint32_t firstIndex =
        static_cast<std::uint32_t>(asset.indices.size());

    asset.vertices.push_back({
        {boundsCenter[0], topY, boundsCenter[2]},
        {0.0F, 1.0F, 0.0F},
        {1.0F, 0.0F, 0.0F, 1.0F},
        {0.5F, 0.5F},
    });
    constexpr float kTwoPi = 6.28318530717958647692F;
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const float angle =
            kTwoPi * static_cast<float>(segment)
            / static_cast<float>(kSegments);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        asset.vertices.push_back({
            {
                boundsCenter[0] + cosine * radius,
                topY,
                boundsCenter[2] + sine * radius,
            },
            {0.0F, 1.0F, 0.0F},
            {1.0F, 0.0F, 0.0F, 1.0F},
            {cosine * 0.5F + 0.5F, sine * 0.5F + 0.5F},
        });
    }
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const std::uint32_t next = (segment + 1) % kSegments;
        asset.indices.push_back(firstVertex);
        asset.indices.push_back(firstVertex + 1 + next);
        asset.indices.push_back(firstVertex + 1 + segment);
    }

    const std::uint32_t sideFirst =
        static_cast<std::uint32_t>(asset.vertices.size());
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const float angle =
            kTwoPi * static_cast<float>(segment)
            / static_cast<float>(kSegments);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const float u =
            static_cast<float>(segment) / static_cast<float>(kSegments);
        for (const float y : {topY, bottomY}) {
            asset.vertices.push_back({
                {
                    boundsCenter[0] + cosine * radius,
                    y,
                    boundsCenter[2] + sine * radius,
                },
                {cosine, 0.0F, sine},
                {-sine, 0.0F, cosine, 1.0F},
                {u, y == topY ? 0.0F : 1.0F},
            });
        }
    }
    for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
        const std::uint32_t next = (segment + 1) % kSegments;
        const std::uint32_t top = sideFirst + segment * 2;
        const std::uint32_t bottom = top + 1;
        const std::uint32_t nextTop = sideFirst + next * 2;
        const std::uint32_t nextBottom = nextTop + 1;
        asset.indices.insert(
            asset.indices.end(),
            {top, nextTop, bottom, bottom, nextTop, nextBottom});
    }

    asset.primitives.push_back({
        firstIndex,
        static_cast<std::uint32_t>(asset.indices.size()) - firstIndex,
        materialIndex,
        {boundsCenter[0], (topY + bottomY) * 0.5F, boundsCenter[2]},
    });
}

void vkCheck(const VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            std::string(operation) + " failed with VkResult " + std::to_string(result));
    }
}

VkResult createDebugUtilsMessenger(
    const VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger) {
    const auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    return function != nullptr
        ? function(instance, createInfo, nullptr, messenger)
        : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void destroyDebugUtilsMessenger(
    const VkInstance instance,
    const VkDebugUtilsMessengerEXT messenger) {
    const auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (function != nullptr) {
        function(instance, messenger, nullptr);
    }
}

}  // namespace

VulkanApp::~VulkanApp() {
    cleanup();
}

void VulkanApp::run(
    const std::string& assetPath,
    const std::uint64_t smokeFrameLimit) {
    initWindow();
    initVulkan(assetPath);
    mainLoop(smokeFrameLimit);
}

void VulkanApp::initWindow() {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("GLFW initialization failed");
    }
    glfwInitialized_ = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window_ = glfwCreateWindow(
        static_cast<int>(kInitialWidth),
        static_cast<int>(kInitialHeight),
        "Vulkan Stylized Renderer - glTF Textured Mesh",
        nullptr,
        nullptr);

    if (window_ == nullptr) {
        throw std::runtime_error("GLFW window creation failed");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
    lastRotationTime_ = glfwGetTime();
    std::cout
        << "Controls: Space pause/resume, R auto rotate, "
        << "1/2/3/4 full-body angles, 5 face close-up, "
        << "Left/Right fine rotate, "
        << "F1/F2/F3 showcase presets, "
        << "F10 inner outlines, "
        << "F9 style toggle, F7/F8 mask strength, "
        << "F5/F6 band threshold, "
        << "F12 screenshot\n";
}

void VulkanApp::initVulkan(const std::string& assetPath) {
    std::cout << "Validation layer: " << (kEnableValidation ? "enabled" : "disabled") << '\n';
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createDescriptorSetLayout();
    createPostProcessDescriptorSetLayout();
    const std::string resolvedAssetPath = assetPath.empty()
        ? std::string(MYVULKANAPP_ASSET_DIR) + "/test_model.gltf"
        : assetPath;
    asset_ = loadGltfAsset(resolvedAssetPath);
    appendShowcasePlatform(asset_);
    std::cout << "Asset path: " << resolvedAssetPath << '\n';
    std::cout << "Loaded asset: " << asset_.vertices.size() << " vertices, "
              << asset_.indices.size() << " indices, "
              << asset_.primitives.size() << " primitives, "
              << asset_.materials.size() << " materials\n";
    std::cout << "Skinning: "
              << (asset_.hasSkin ? "enabled" : "static fallback")
              << ", " << asset_.jointMatrices.size() << " joint matrices\n";
    createVertexBuffer();
    createIndexBuffer();
    createTexture();
    createUniformBuffers();
    createJointBuffers();
    createShadowResources();
    createDescriptorPool();
    createDescriptorSets();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createNormalResources();
    createRenderPass();
    createPostProcessRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createPostProcessFramebuffers();
    createPostProcessDescriptorSets();
    createSwapchainSemaphores();
    createCommandBuffers();
    createSyncObjects();
}

void VulkanApp::mainLoop(const std::uint64_t smokeFrameLimit) {
    std::uint64_t renderedFrames = 0;
    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        drawFrame();
        ++renderedFrames;
        if (smokeFrameLimit > 0 && renderedFrames >= smokeFrameLimit) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    std::cout << "Rendered frames: " << renderedFrames << '\n';
}

void VulkanApp::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        cleanupSwapchain();

        for (const auto semaphore : imageAvailableSemaphores_) {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }
        for (const auto fence : inFlightFences_) {
            vkDestroyFence(device_, fence, nullptr);
        }

        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        }
        if (screenAttachmentSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, screenAttachmentSampler_, nullptr);
        }
        if (postProcessDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(
                device_,
                postProcessDescriptorSetLayout_,
                nullptr);
        }
        for (std::size_t index = 0; index < uniformBuffers_.size(); ++index) {
            if (uniformBufferMapped_[index] != nullptr) {
                vkUnmapMemory(device_, uniformBufferMemories_[index]);
            }
            vkDestroyBuffer(device_, uniformBuffers_[index], nullptr);
            vkFreeMemory(device_, uniformBufferMemories_[index], nullptr);
        }
        for (std::size_t index = 0; index < jointBuffers_.size(); ++index) {
            if (jointBufferMapped_[index] != nullptr) {
                vkUnmapMemory(device_, jointBufferMemories_[index]);
            }
            vkDestroyBuffer(device_, jointBuffers_[index], nullptr);
            vkFreeMemory(device_, jointBufferMemories_[index], nullptr);
        }
        if (indexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, indexBuffer_, nullptr);
            vkFreeMemory(device_, indexBufferMemory_, nullptr);
        }
        if (vertexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, vertexBuffer_, nullptr);
            vkFreeMemory(device_, vertexBufferMemory_, nullptr);
        }
        for (const auto& material : gpuMaterials_) {
            for (const GpuTexture* texture : {
                     &material.baseColor,
                     &material.normal,
                     &material.metallicRoughness,
                     &material.specularEmissive,
                     &material.styleMask,
                     &material.matcap,
                     &material.hairData}) {
                vkDestroySampler(device_, texture->sampler, nullptr);
                vkDestroyImageView(device_, texture->view, nullptr);
                vkDestroyImage(device_, texture->image, nullptr);
                vkFreeMemory(device_, texture->memory, nullptr);
            }
        }
        vkDestroySampler(device_, environmentTexture_.sampler, nullptr);
        vkDestroyImageView(device_, environmentTexture_.view, nullptr);
        vkDestroyImage(device_, environmentTexture_.image, nullptr);
        vkFreeMemory(device_, environmentTexture_.memory, nullptr);
        if (shadowFramebuffer_ != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, shadowFramebuffer_, nullptr);
        }
        if (shadowRenderPass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, shadowRenderPass_, nullptr);
        }
        if (shadowSampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_, shadowSampler_, nullptr);
        }
        if (shadowImageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, shadowImageView_, nullptr);
        }
        if (shadowImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, shadowImage_, nullptr);
        }
        if (shadowImageMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, shadowImageMemory_, nullptr);
        }
        if (descriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        }
        if (commandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (debugMessenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        destroyDebugUtilsMessenger(instance_, debugMessenger_);
        debugMessenger_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfwInitialized_) {
        glfwTerminate();
        glfwInitialized_ = false;
    }
}

void VulkanApp::createInstance() {
    if (kEnableValidation && !checkValidationLayerSupport()) {
        throw std::runtime_error("VK_LAYER_KHRONOS_validation is unavailable");
    }

    VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "Vulkan Stylized Renderer";
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.pEngineName = "MyVulkanApp";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_3;

    const auto extensions = requiredInstanceExtensions();
    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (kEnableValidation) {
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    vkCheck(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
}

void VulkanApp::setupDebugMessenger() {
    if (!kEnableValidation) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    vkCheck(
        createDebugUtilsMessenger(instance_, &createInfo, &debugMessenger_),
        "vkCreateDebugUtilsMessengerEXT");
}

void VulkanApp::createSurface() {
    vkCheck(
        glfwCreateWindowSurface(instance_, window_, nullptr, &surface_),
        "glfwCreateWindowSurface");
}

void VulkanApp::pickPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    vkCheck(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr), "vkEnumeratePhysicalDevices");
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU was found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkCheck(
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()),
        "vkEnumeratePhysicalDevices");

    for (const auto device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && isDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        const auto iterator = std::find_if(
            devices.begin(),
            devices.end(),
            [this](const VkPhysicalDevice device) { return isDeviceSuitable(device); });
        if (iterator != devices.end()) {
            physicalDevice_ = *iterator;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("No GPU supports the required graphics/present queues and swapchain");
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    std::cout << "Selected GPU: " << properties.deviceName << '\n';
}

void VulkanApp::createLogicalDevice() {
    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    const std::set uniqueFamilies = {*indices.graphics, *indices.present};
    const float priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueFamilies.size());

    for (const std::uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    vkCheck(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, *indices.graphics, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, *indices.present, 0, &presentQueue_);
}

void VulkanApp::createSwapchain() {
    const SwapchainSupport support = querySwapchainSupport(physicalDevice_);
    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    const VkExtent2D extent = chooseExtent(support.capabilities);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    if ((support.capabilities.supportedUsageFlags
         & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
        throw std::runtime_error(
            "Surface swapchain images do not support screenshot transfer source");
    }
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    const std::array queueFamilies = {*indices.graphics, *indices.present};
    if (indices.graphics != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilies.size());
        createInfo.pQueueFamilyIndices = queueFamilies.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    vkCheck(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_), "vkCreateSwapchainKHR");
    vkCheck(
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr),
        "vkGetSwapchainImagesKHR");
    swapchainImages_.resize(imageCount);
    vkCheck(
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data()),
        "vkGetSwapchainImagesKHR");

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void VulkanApp::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());
    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = swapchainImages_[index];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainFormat_;
        createInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        };
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.layerCount = 1;

        vkCheck(
            vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[index]),
            "vkCreateImageView");
    }
}

void VulkanApp::createDepthResources() {
    depthFormat_ = findDepthFormat();
    depthImages_.resize(swapchainImages_.size());
    depthImageMemories_.resize(swapchainImages_.size());
    depthImageViews_.resize(swapchainImages_.size());

    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        createImage(
            swapchainExtent_.width,
            swapchainExtent_.height,
            depthFormat_,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT,
            depthImages_[index],
            depthImageMemories_[index]);
        depthImageViews_[index] = createImageView(
            depthImages_[index], depthFormat_, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

void VulkanApp::createNormalResources() {
    normalImages_.resize(swapchainImages_.size());
    normalImageMemories_.resize(swapchainImages_.size());
    normalImageViews_.resize(swapchainImages_.size());
    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        createImage(
            swapchainExtent_.width,
            swapchainExtent_.height,
            normalFormat_,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT,
            normalImages_[index],
            normalImageMemories_[index]);
        normalImageViews_[index] = createImageView(
            normalImages_[index],
            normalFormat_,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanApp::createShadowResources() {
    shadowFormat_ = findDepthFormat();
    createImage(
        kShadowMapSize,
        kShadowMapSize,
        shadowFormat_,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT,
        shadowImage_,
        shadowImageMemory_);
    shadowImageView_ = createImageView(
        shadowImage_,
        shadowFormat_,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxLod = 1.0F;
    vkCheck(
        vkCreateSampler(device_, &samplerInfo, nullptr, &shadowSampler_),
        "vkCreateSampler(shadow)");

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = shadowFormat_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 0;
    depthReference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount =
        static_cast<std::uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();
    vkCheck(
        vkCreateRenderPass(
            device_,
            &renderPassInfo,
            nullptr,
            &shadowRenderPass_),
        "vkCreateRenderPass(shadow)");

    VkFramebufferCreateInfo framebufferInfo{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = shadowRenderPass_;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &shadowImageView_;
    framebufferInfo.width = kShadowMapSize;
    framebufferInfo.height = kShadowMapSize;
    framebufferInfo.layers = 1;
    vkCheck(
        vkCreateFramebuffer(
            device_,
            &framebufferInfo,
            nullptr,
            &shadowFramebuffer_),
        "vkCreateFramebuffer(shadow)");
}

void VulkanApp::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = normalFormat_;
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference normalReference{};
    normalReference.attachment = 2;
    normalReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const std::array colorReferences = {
        colorReference,
        normalReference,
    };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount =
        static_cast<std::uint32_t>(colorReferences.size());
    subpass.pColorAttachments = colorReferences.data();
    subpass.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = dependencies[0].srcStageMask;
    dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    const std::array attachments = {
        colorAttachment,
        depthAttachment,
        normalAttachment,
    };
    VkRenderPassCreateInfo createInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount =
        static_cast<std::uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();

    vkCheck(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_), "vkCreateRenderPass");
}

void VulkanApp::createPostProcessRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo createInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;
    vkCheck(
        vkCreateRenderPass(
            device_,
            &createInfo,
            nullptr,
            &postProcessRenderPass_),
        "vkCreateRenderPass(post process)");
}

void VulkanApp::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.binding = 0;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 1;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding normalBinding = textureBinding;
    normalBinding.binding = 2;

    VkDescriptorSetLayoutBinding metallicRoughnessBinding = textureBinding;
    metallicRoughnessBinding.binding = 3;

    VkDescriptorSetLayoutBinding environmentBinding = textureBinding;
    environmentBinding.binding = 4;

    VkDescriptorSetLayoutBinding specularEmissiveBinding = textureBinding;
    specularEmissiveBinding.binding = 5;
    VkDescriptorSetLayoutBinding styleMaskBinding = textureBinding;
    styleMaskBinding.binding = 6;
    VkDescriptorSetLayoutBinding matcapBinding = textureBinding;
    matcapBinding.binding = 7;
    VkDescriptorSetLayoutBinding hairDataBinding = textureBinding;
    hairDataBinding.binding = 8;
    VkDescriptorSetLayoutBinding shadowBinding = textureBinding;
    shadowBinding.binding = 9;
    VkDescriptorSetLayoutBinding jointBinding{};
    jointBinding.binding = 10;
    jointBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    jointBinding.descriptorCount = 1;
    jointBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    const std::array bindings = {
        uniformBinding,
        textureBinding,
        normalBinding,
        metallicRoughnessBinding,
        environmentBinding,
        specularEmissiveBinding,
        styleMaskBinding,
        matcapBinding,
        hairDataBinding,
        shadowBinding,
        jointBinding,
    };
    VkDescriptorSetLayoutCreateInfo createInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();
    vkCheck(
        vkCreateDescriptorSetLayout(device_, &createInfo, nullptr, &descriptorSetLayout_),
        "vkCreateDescriptorSetLayout");
}

void VulkanApp::createPostProcessDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding normalBinding{};
    normalBinding.binding = 0;
    normalBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalBinding.descriptorCount = 1;
    normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding depthBinding = normalBinding;
    depthBinding.binding = 1;
    const std::array bindings = {normalBinding, depthBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount =
        static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    vkCheck(
        vkCreateDescriptorSetLayout(
            device_,
            &layoutInfo,
            nullptr,
            &postProcessDescriptorSetLayout_),
        "vkCreateDescriptorSetLayout(post process)");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0F;
    vkCheck(
        vkCreateSampler(
            device_,
            &samplerInfo,
            nullptr,
            &screenAttachmentSampler_),
        "vkCreateSampler(screen attachments)");
}

void VulkanApp::createGraphicsPipeline() {
    const std::string shaderDirectory = MYVULKANAPP_SHADER_DIR;
    const auto vertexCode = readBinaryFile(shaderDirectory + "/mesh.vert.spv");
    const auto fragmentCode = readBinaryFile(shaderDirectory + "/mesh.frag.spv");
    const auto outlineVertexCode =
        readBinaryFile(shaderDirectory + "/outline.vert.spv");
    const auto outlineFragmentCode =
        readBinaryFile(shaderDirectory + "/outline.frag.spv");
    const auto backgroundVertexCode =
        readBinaryFile(shaderDirectory + "/background.vert.spv");
    const auto backgroundFragmentCode =
        readBinaryFile(shaderDirectory + "/background.frag.spv");
    const auto shadowVertexCode =
        readBinaryFile(shaderDirectory + "/shadow.vert.spv");
    const auto shadowFragmentCode =
        readBinaryFile(shaderDirectory + "/shadow.frag.spv");
    const auto innerOutlineVertexCode =
        readBinaryFile(shaderDirectory + "/inner_outline.vert.spv");
    const auto innerOutlineFragmentCode =
        readBinaryFile(shaderDirectory + "/inner_outline.frag.spv");
    const VkShaderModule vertexModule = createShaderModule(vertexCode);
    const VkShaderModule fragmentModule = createShaderModule(fragmentCode);
    const VkShaderModule outlineVertexModule =
        createShaderModule(outlineVertexCode);
    const VkShaderModule outlineFragmentModule =
        createShaderModule(outlineFragmentCode);
    const VkShaderModule backgroundVertexModule =
        createShaderModule(backgroundVertexCode);
    const VkShaderModule backgroundFragmentModule =
        createShaderModule(backgroundFragmentCode);
    const VkShaderModule shadowVertexModule =
        createShaderModule(shadowVertexCode);
    const VkShaderModule shadowFragmentModule =
        createShaderModule(shadowFragmentCode);
    const VkShaderModule innerOutlineVertexModule =
        createShaderModule(innerOutlineVertexCode);
    const VkShaderModule innerOutlineFragmentModule =
        createShaderModule(innerOutlineFragmentCode);

    try {
        VkPipelineShaderStageCreateInfo vertexStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexModule;
        vertexStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragmentStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentModule;
        fragmentStage.pName = "main";
        const std::array shaderStages = {vertexStage, fragmentStage};
        VkPipelineShaderStageCreateInfo outlineVertexStage = vertexStage;
        outlineVertexStage.module = outlineVertexModule;
        VkPipelineShaderStageCreateInfo outlineFragmentStage = fragmentStage;
        outlineFragmentStage.module = outlineFragmentModule;
        const std::array outlineShaderStages = {
            outlineVertexStage,
            outlineFragmentStage,
        };
        VkPipelineShaderStageCreateInfo backgroundVertexStage = vertexStage;
        backgroundVertexStage.module = backgroundVertexModule;
        VkPipelineShaderStageCreateInfo backgroundFragmentStage = fragmentStage;
        backgroundFragmentStage.module = backgroundFragmentModule;
        const std::array backgroundShaderStages = {
            backgroundVertexStage,
            backgroundFragmentStage,
        };
        VkPipelineShaderStageCreateInfo shadowVertexStage = vertexStage;
        shadowVertexStage.module = shadowVertexModule;
        VkPipelineShaderStageCreateInfo shadowFragmentStage = fragmentStage;
        shadowFragmentStage.module = shadowFragmentModule;
        const std::array shadowShaderStages = {
            shadowVertexStage,
            shadowFragmentStage,
        };
        VkPipelineShaderStageCreateInfo innerOutlineVertexStage =
            vertexStage;
        innerOutlineVertexStage.module = innerOutlineVertexModule;
        VkPipelineShaderStageCreateInfo innerOutlineFragmentStage =
            fragmentStage;
        innerOutlineFragmentStage.module = innerOutlineFragmentModule;
        const std::array innerOutlineShaderStages = {
            innerOutlineVertexStage,
            innerOutlineFragmentStage,
        };

        VkPipelineVertexInputStateCreateInfo vertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(AssetVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        std::array<VkVertexInputAttributeDescription, 6> attributeDescriptions{};
        attributeDescriptions[0] = {
            0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(AssetVertex, position)};
        attributeDescriptions[1] = {
            1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(AssetVertex, normal)};
        attributeDescriptions[2] = {
            2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(AssetVertex, tangent)};
        attributeDescriptions[3] = {
            3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(AssetVertex, texcoord)};
        attributeDescriptions[4] = {
            4, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(AssetVertex, joints)};
        attributeDescriptions[5] = {
            5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(AssetVertex, weights)};
        const std::array shadowAttributeDescriptions = {
            attributeDescriptions[0],
            attributeDescriptions[3],
            attributeDescriptions[4],
            attributeDescriptions[5],
        };
        const std::array outlineAttributeDescriptions = {
            attributeDescriptions[0],
            attributeDescriptions[1],
            attributeDescriptions[4],
            attributeDescriptions[5],
        };
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(attributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisampling{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        std::array<VkPipelineColorBlendAttachmentState, 2>
            colorBlendAttachments{};
        for (auto& attachment : colorBlendAttachments) {
            attachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount =
            static_cast<std::uint32_t>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();

        const std::array dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo layoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorSetLayout_;
        VkPushConstantRange materialPushRange{};
        materialPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        materialPushRange.size = sizeof(MaterialPushConstants);
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &materialPushRange;
        vkCheck(
            vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_),
            "vkCreatePipelineLayout");
        VkPushConstantRange postProcessPushRange{};
        postProcessPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        postProcessPushRange.size = sizeof(PostProcessPushConstants);
        VkPipelineLayoutCreateInfo postProcessLayoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        postProcessLayoutInfo.setLayoutCount = 1;
        postProcessLayoutInfo.pSetLayouts =
            &postProcessDescriptorSetLayout_;
        postProcessLayoutInfo.pushConstantRangeCount = 1;
        postProcessLayoutInfo.pPushConstantRanges =
            &postProcessPushRange;
        vkCheck(
            vkCreatePipelineLayout(
                device_,
                &postProcessLayoutInfo,
                nullptr,
                &postProcessPipelineLayout_),
            "vkCreatePipelineLayout(post process)");

        VkGraphicsPipelineCreateInfo pipelineInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = static_cast<std::uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        pipelineInfo.subpass = 0;

        const auto createVariant = [&](
            const VkCullModeFlags cullMode,
            const bool blend,
            VkPipeline& pipeline) {
            rasterizer.cullMode = cullMode;
            depthStencil.depthWriteEnable = blend ? VK_FALSE : VK_TRUE;
            auto& colorAttachment = colorBlendAttachments[0];
            colorAttachment.blendEnable = blend ? VK_TRUE : VK_FALSE;
            colorAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorAttachment.dstColorBlendFactor =
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorAttachment.dstAlphaBlendFactor =
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachments[1] = colorAttachment;
            vkCheck(
                vkCreateGraphicsPipelines(
                    device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
                "vkCreateGraphicsPipelines(material variant)");
        };
        createVariant(VK_CULL_MODE_BACK_BIT, false, opaquePipeline_);
        createVariant(VK_CULL_MODE_NONE, false, opaqueDoubleSidedPipeline_);
        createVariant(VK_CULL_MODE_BACK_BIT, true, blendPipeline_);
        createVariant(VK_CULL_MODE_NONE, true, blendDoubleSidedPipeline_);
        pipelineInfo.pStages = outlineShaderStages.data();
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(
                outlineAttributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions =
            outlineAttributeDescriptions.data();
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        depthStencil.depthWriteEnable = VK_FALSE;
        colorBlendAttachments[0].blendEnable = VK_FALSE;
        colorBlendAttachments[1].blendEnable = VK_FALSE;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &outlinePipeline_),
            "vkCreateGraphicsPipelines(outline)");

        VkPipelineVertexInputStateCreateInfo emptyVertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        pipelineInfo.pStages = backgroundShaderStages.data();
        pipelineInfo.pVertexInputState = &emptyVertexInput;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &backgroundPipeline_),
            "vkCreateGraphicsPipelines(background)");

        pipelineInfo.pStages = shadowShaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(shadowAttributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions =
            shadowAttributeDescriptions.data();
        pipelineInfo.renderPass = shadowRenderPass_;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.depthBiasEnable = VK_TRUE;
        rasterizer.depthBiasConstantFactor = 1.25F;
        rasterizer.depthBiasSlopeFactor = 1.75F;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &shadowPipeline_),
            "vkCreateGraphicsPipelines(shadow)");

        VkPipelineColorBlendAttachmentState postBlendAttachment{};
        postBlendAttachment.blendEnable = VK_TRUE;
        postBlendAttachment.srcColorBlendFactor =
            VK_BLEND_FACTOR_SRC_ALPHA;
        postBlendAttachment.dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        postBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        postBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        postBlendAttachment.dstAlphaBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        postBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        postBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &postBlendAttachment;
        pipelineInfo.pStages = innerOutlineShaderStages.data();
        pipelineInfo.pVertexInputState = &emptyVertexInput;
        pipelineInfo.layout = postProcessPipelineLayout_;
        pipelineInfo.renderPass = postProcessRenderPass_;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &innerOutlinePipeline_),
            "vkCreateGraphicsPipelines(inner outline)");
    } catch (...) {
        vkDestroyShaderModule(
            device_,
            innerOutlineFragmentModule,
            nullptr);
        vkDestroyShaderModule(
            device_,
            innerOutlineVertexModule,
            nullptr);
        vkDestroyShaderModule(device_, shadowFragmentModule, nullptr);
        vkDestroyShaderModule(device_, shadowVertexModule, nullptr);
        vkDestroyShaderModule(device_, backgroundFragmentModule, nullptr);
        vkDestroyShaderModule(device_, backgroundVertexModule, nullptr);
        vkDestroyShaderModule(device_, outlineFragmentModule, nullptr);
        vkDestroyShaderModule(device_, outlineVertexModule, nullptr);
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        throw;
    }

    vkDestroyShaderModule(
        device_,
        innerOutlineFragmentModule,
        nullptr);
    vkDestroyShaderModule(
        device_,
        innerOutlineVertexModule,
        nullptr);
    vkDestroyShaderModule(device_, shadowFragmentModule, nullptr);
    vkDestroyShaderModule(device_, shadowVertexModule, nullptr);
    vkDestroyShaderModule(device_, backgroundFragmentModule, nullptr);
    vkDestroyShaderModule(device_, backgroundVertexModule, nullptr);
    vkDestroyShaderModule(device_, outlineFragmentModule, nullptr);
    vkDestroyShaderModule(device_, outlineVertexModule, nullptr);
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
}

void VulkanApp::createFramebuffers() {
    swapchainFramebuffers_.resize(swapchainImageViews_.size());
    for (std::size_t index = 0; index < swapchainImageViews_.size(); ++index) {
        const std::array attachments = {
            swapchainImageViews_[index],
            depthImageViews_[index],
            normalImageViews_[index],
        };
        VkFramebufferCreateInfo createInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = renderPass_;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        vkCheck(
            vkCreateFramebuffer(device_, &createInfo, nullptr, &swapchainFramebuffers_[index]),
            "vkCreateFramebuffer");
    }
}

void VulkanApp::createPostProcessFramebuffers() {
    postProcessFramebuffers_.resize(swapchainImageViews_.size());
    for (std::size_t index = 0;
         index < swapchainImageViews_.size();
         ++index) {
        VkFramebufferCreateInfo createInfo{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = postProcessRenderPass_;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &swapchainImageViews_[index];
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        vkCheck(
            vkCreateFramebuffer(
                device_,
                &createInfo,
                nullptr,
                &postProcessFramebuffers_[index]),
            "vkCreateFramebuffer(post process)");
    }
}

void VulkanApp::createSwapchainSemaphores() {
    renderFinishedSemaphores_.resize(swapchainImages_.size());
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (auto& semaphore : renderFinishedSemaphores_) {
        vkCheck(
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore),
            "vkCreateSemaphore(render finished)");
    }
}

void VulkanApp::createCommandPool() {
    const QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    VkCommandPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = *indices.graphics;
    vkCheck(vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_), "vkCreateCommandPool");
}

void VulkanApp::createVertexBuffer() {
    const VkDeviceSize size = sizeof(AssetVertex) * asset_.vertices.size();

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);

    void* mapped = nullptr;
    vkCheck(vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped), "vkMapMemory(vertex)");
    std::memcpy(mapped, asset_.vertices.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(device_, stagingMemory);

    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer_,
        vertexBufferMemory_);
    copyBuffer(stagingBuffer, vertexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void VulkanApp::createIndexBuffer() {
    const VkDeviceSize size = sizeof(std::uint32_t) * asset_.indices.size();

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory);

    void* mapped = nullptr;
    vkCheck(vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped), "vkMapMemory(index)");
    std::memcpy(mapped, asset_.indices.data(), static_cast<std::size_t>(size));
    vkUnmapMemory(device_, stagingMemory);

    createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer_,
        indexBufferMemory_);
    copyBuffer(stagingBuffer, indexBuffer_, size);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void VulkanApp::createTexture() {
    const auto uploadTexture = [this](
        const std::vector<std::uint8_t>& pixels,
        const std::uint32_t width,
        const std::uint32_t height,
        const VkFormat format,
        const bool clampVertical,
        GpuTexture& texture) {
        const VkDeviceSize size = pixels.size();
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory);
        void* mapped = nullptr;
        vkCheck(
            vkMapMemory(device_, stagingMemory, 0, size, 0, &mapped),
            "vkMapMemory(texture)");
        std::memcpy(mapped, pixels.data(), static_cast<std::size_t>(size));
        vkUnmapMemory(device_, stagingMemory);
        createImage(
            width,
            height,
            format,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            texture.image,
            texture.memory);
        transitionImageLayout(
            texture.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, texture.image, width, height);
        transitionImageLayout(
            texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        texture.view = createImageView(
            texture.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = clampVertical
            ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
            : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.maxLod = 0.0F;
        vkCheck(
            vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler),
            "vkCreateSampler");
    };

    gpuMaterials_.resize(asset_.materials.size());
    for (std::size_t index = 0; index < asset_.materials.size(); ++index) {
        const AssetMaterial& material = asset_.materials[index];
        uploadTexture(
            material.baseColorPixels,
            material.baseColorWidth,
            material.baseColorHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterials_[index].baseColor);
        uploadTexture(
            material.normalPixels,
            material.normalWidth,
            material.normalHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].normal);
        uploadTexture(
            material.metallicRoughnessPixels,
            material.metallicRoughnessWidth,
            material.metallicRoughnessHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].metallicRoughness);
        uploadTexture(
            material.specularEmissivePixels,
            material.specularEmissiveWidth,
            material.specularEmissiveHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterials_[index].specularEmissive);
        uploadTexture(
            material.styleMaskPixels,
            material.styleMaskWidth,
            material.styleMaskHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].styleMask);
        uploadTexture(
            material.matcapPixels,
            material.matcapWidth,
            material.matcapHeight,
            VK_FORMAT_R8G8B8A8_SRGB,
            false,
            gpuMaterials_[index].matcap);
        uploadTexture(
            material.hairDataPixels,
            material.hairDataWidth,
            material.hairDataHeight,
            VK_FORMAT_R8G8B8A8_UNORM,
            false,
            gpuMaterials_[index].hairData);
    }

    constexpr std::uint32_t kEnvironmentWidth = 512;
    constexpr std::uint32_t kEnvironmentHeight = 256;
    constexpr float kPi = 3.14159265358979323846F;
    std::vector<std::uint8_t> environmentPixels(
        static_cast<std::size_t>(kEnvironmentWidth)
        * kEnvironmentHeight
        * 4);
    const Vector3 sunDirection = {0.45F, 0.85F, 0.35F};
    const float sunLength = std::sqrt(dot(sunDirection, sunDirection));
    const Vector3 normalizedSun = {
        sunDirection[0] / sunLength,
        sunDirection[1] / sunLength,
        sunDirection[2] / sunLength,
    };
    for (std::uint32_t y = 0; y < kEnvironmentHeight; ++y) {
        const float v =
            (static_cast<float>(y) + 0.5F)
            / static_cast<float>(kEnvironmentHeight);
        const float theta = v * kPi;
        const float directionY = std::cos(theta);
        const float ringRadius = std::sin(theta);
        for (std::uint32_t x = 0; x < kEnvironmentWidth; ++x) {
            const float u =
                (static_cast<float>(x) + 0.5F)
                / static_cast<float>(kEnvironmentWidth);
            const float phi = (u - 0.5F) * 2.0F * kPi;
            const Vector3 direction = {
                ringRadius * std::cos(phi),
                directionY,
                ringRadius * std::sin(phi),
            };
            const float skyAmount = std::clamp(directionY * 0.5F + 0.5F, 0.0F, 1.0F);
            const float horizon = std::exp(-std::abs(directionY) * 9.0F);
            const float sun = std::pow(
                std::max(dot(direction, normalizedSun), 0.0F),
                320.0F);
            const std::array<float, 3> ground = {0.055F, 0.075F, 0.090F};
            const std::array<float, 3> zenith = {0.20F, 0.34F, 0.46F};
            const std::array<float, 3> horizonColor = {0.38F, 0.43F, 0.46F};
            const std::size_t pixel =
                (static_cast<std::size_t>(y) * kEnvironmentWidth + x) * 4;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                float color =
                    ground[channel] * (1.0F - skyAmount)
                    + zenith[channel] * skyAmount;
                color = color * (1.0F - horizon * 0.55F)
                    + horizonColor[channel] * horizon * 0.55F;
                color += sun * (channel == 2 ? 0.70F : 1.0F);
                environmentPixels[pixel + channel] =
                    static_cast<std::uint8_t>(
                        std::clamp(color, 0.0F, 1.0F) * 255.0F);
            }
            environmentPixels[pixel + 3] = 255;
        }
    }
    uploadTexture(
        environmentPixels,
        kEnvironmentWidth,
        kEnvironmentHeight,
        VK_FORMAT_R8G8B8A8_UNORM,
        true,
        environmentTexture_);
}

void VulkanApp::createUniformBuffers() {
    const VkDeviceSize size = sizeof(UniformBufferObject);
    uniformBuffers_.resize(kMaxFramesInFlight);
    uniformBufferMemories_.resize(kMaxFramesInFlight);
    uniformBufferMapped_.resize(kMaxFramesInFlight);

    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        createBuffer(
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers_[index],
            uniformBufferMemories_[index]);
        vkCheck(
            vkMapMemory(
                device_,
                uniformBufferMemories_[index],
                0,
                size,
                0,
                &uniformBufferMapped_[index]),
            "vkMapMemory(uniform)");
    }
}

void VulkanApp::createJointBuffers() {
    if (asset_.jointMatrices.empty()) {
        throw std::runtime_error("Asset has no joint-matrix fallback");
    }
    const VkDeviceSize size =
        sizeof(asset_.jointMatrices.front()) * asset_.jointMatrices.size();
    jointBuffers_.resize(kMaxFramesInFlight);
    jointBufferMemories_.resize(kMaxFramesInFlight);
    jointBufferMapped_.resize(kMaxFramesInFlight);

    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        createBuffer(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            jointBuffers_[index],
            jointBufferMemories_[index]);
        vkCheck(
            vkMapMemory(
                device_,
                jointBufferMemories_[index],
                0,
                size,
                0,
                &jointBufferMapped_[index]),
            "vkMapMemory(joints)");
        std::memcpy(
            jointBufferMapped_[index],
            asset_.jointMatrices.data(),
            static_cast<std::size_t>(size));
    }
}

void VulkanApp::createDescriptorPool() {
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(kMaxFramesInFlight * asset_.materials.size());
    const std::array<VkDescriptorPoolSize, 3> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * 9},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount},
    }};

    VkDescriptorPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.maxSets = descriptorCount;
    vkCheck(
        vkCreateDescriptorPool(device_, &createInfo, nullptr, &descriptorPool_),
        "vkCreateDescriptorPool");
}

void VulkanApp::createDescriptorSets() {
    const std::size_t descriptorCount = kMaxFramesInFlight * asset_.materials.size();
    const std::vector<VkDescriptorSetLayout> layouts(
        descriptorCount, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = descriptorPool_;
    allocateInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
    allocateInfo.pSetLayouts = layouts.data();
    descriptorSets_.resize(descriptorCount);
    vkCheck(
        vkAllocateDescriptorSets(device_, &allocateInfo, descriptorSets_.data()),
        "vkAllocateDescriptorSets");

    for (std::size_t frame = 0; frame < kMaxFramesInFlight; ++frame) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[frame];
        bufferInfo.range = sizeof(UniformBufferObject);
        VkDescriptorBufferInfo jointBufferInfo{};
        jointBufferInfo.buffer = jointBuffers_[frame];
        jointBufferInfo.range =
            sizeof(asset_.jointMatrices.front())
            * asset_.jointMatrices.size();

        for (std::size_t material = 0; material < asset_.materials.size(); ++material) {
            const std::size_t descriptorIndex =
                frame * asset_.materials.size() + material;
            VkDescriptorImageInfo baseColorInfo{};
            baseColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            baseColorInfo.imageView = gpuMaterials_[material].baseColor.view;
            baseColorInfo.sampler = gpuMaterials_[material].baseColor.sampler;
            VkDescriptorImageInfo normalInfo{};
            normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            normalInfo.imageView = gpuMaterials_[material].normal.view;
            normalInfo.sampler = gpuMaterials_[material].normal.sampler;
            VkDescriptorImageInfo metallicRoughnessInfo{};
            metallicRoughnessInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            metallicRoughnessInfo.imageView =
                gpuMaterials_[material].metallicRoughness.view;
            metallicRoughnessInfo.sampler =
                gpuMaterials_[material].metallicRoughness.sampler;
            VkDescriptorImageInfo environmentInfo{};
            environmentInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            environmentInfo.imageView = environmentTexture_.view;
            environmentInfo.sampler = environmentTexture_.sampler;
            VkDescriptorImageInfo specularEmissiveInfo{};
            specularEmissiveInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            specularEmissiveInfo.imageView =
                gpuMaterials_[material].specularEmissive.view;
            specularEmissiveInfo.sampler =
                gpuMaterials_[material].specularEmissive.sampler;
            VkDescriptorImageInfo styleMaskInfo{};
            styleMaskInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            styleMaskInfo.imageView =
                gpuMaterials_[material].styleMask.view;
            styleMaskInfo.sampler =
                gpuMaterials_[material].styleMask.sampler;
            VkDescriptorImageInfo matcapInfo{};
            matcapInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            matcapInfo.imageView =
                gpuMaterials_[material].matcap.view;
            matcapInfo.sampler =
                gpuMaterials_[material].matcap.sampler;
            VkDescriptorImageInfo hairDataInfo{};
            hairDataInfo.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            hairDataInfo.imageView =
                gpuMaterials_[material].hairData.view;
            hairDataInfo.sampler =
                gpuMaterials_[material].hairData.sampler;
            VkDescriptorImageInfo shadowInfo{};
            shadowInfo.imageLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowInfo.imageView = shadowImageView_;
            shadowInfo.sampler = shadowSampler_;

            std::array<VkWriteDescriptorSet, 11> writes{};
            writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[0].dstSet = descriptorSets_[descriptorIndex];
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &bufferInfo;
            writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[1].dstSet = descriptorSets_[descriptorIndex];
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].pImageInfo = &baseColorInfo;
            writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[2].dstSet = descriptorSets_[descriptorIndex];
            writes[2].dstBinding = 2;
            writes[2].descriptorCount = 1;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].pImageInfo = &normalInfo;
            writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[3].dstSet = descriptorSets_[descriptorIndex];
            writes[3].dstBinding = 3;
            writes[3].descriptorCount = 1;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[3].pImageInfo = &metallicRoughnessInfo;
            writes[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[4].dstSet = descriptorSets_[descriptorIndex];
            writes[4].dstBinding = 4;
            writes[4].descriptorCount = 1;
            writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[4].pImageInfo = &environmentInfo;
            writes[5] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[5].dstSet = descriptorSets_[descriptorIndex];
            writes[5].dstBinding = 5;
            writes[5].descriptorCount = 1;
            writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[5].pImageInfo = &specularEmissiveInfo;
            writes[6] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[6].dstSet = descriptorSets_[descriptorIndex];
            writes[6].dstBinding = 6;
            writes[6].descriptorCount = 1;
            writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[6].pImageInfo = &styleMaskInfo;
            writes[7] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[7].dstSet = descriptorSets_[descriptorIndex];
            writes[7].dstBinding = 7;
            writes[7].descriptorCount = 1;
            writes[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[7].pImageInfo = &matcapInfo;
            writes[8] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[8].dstSet = descriptorSets_[descriptorIndex];
            writes[8].dstBinding = 8;
            writes[8].descriptorCount = 1;
            writes[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[8].pImageInfo = &hairDataInfo;
            writes[9] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[9].dstSet = descriptorSets_[descriptorIndex];
            writes[9].dstBinding = 9;
            writes[9].descriptorCount = 1;
            writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[9].pImageInfo = &shadowInfo;
            writes[10] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[10].dstSet = descriptorSets_[descriptorIndex];
            writes[10].dstBinding = 10;
            writes[10].descriptorCount = 1;
            writes[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[10].pBufferInfo = &jointBufferInfo;
            vkUpdateDescriptorSets(
                device_,
                static_cast<std::uint32_t>(writes.size()),
                writes.data(),
                0,
                nullptr);
        }
    }
}

void VulkanApp::createPostProcessDescriptorSets() {
    const std::uint32_t descriptorCount =
        static_cast<std::uint32_t>(swapchainImages_.size());
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = descriptorCount * 2;
    VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = descriptorCount;
    vkCheck(
        vkCreateDescriptorPool(
            device_,
            &poolInfo,
            nullptr,
            &postProcessDescriptorPool_),
        "vkCreateDescriptorPool(post process)");

    const std::vector<VkDescriptorSetLayout> layouts(
        descriptorCount,
        postProcessDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = postProcessDescriptorPool_;
    allocateInfo.descriptorSetCount = descriptorCount;
    allocateInfo.pSetLayouts = layouts.data();
    postProcessDescriptorSets_.resize(descriptorCount);
    vkCheck(
        vkAllocateDescriptorSets(
            device_,
            &allocateInfo,
            postProcessDescriptorSets_.data()),
        "vkAllocateDescriptorSets(post process)");

    for (std::size_t index = 0; index < swapchainImages_.size(); ++index) {
        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = normalImageViews_[index];
        normalInfo.sampler = screenAttachmentSampler_;
        VkDescriptorImageInfo depthInfo{};
        depthInfo.imageLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthInfo.imageView = depthImageViews_[index];
        depthInfo.sampler = screenAttachmentSampler_;
        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[0].dstSet = postProcessDescriptorSets_[index];
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &normalInfo;
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[1].dstSet = postProcessDescriptorSets_[index];
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &depthInfo;
        vkUpdateDescriptorSets(
            device_,
            static_cast<std::uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr);
    }
}

void VulkanApp::createCommandBuffers() {
    commandBuffers_.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers_.data()),
        "vkAllocateCommandBuffers");
}

void VulkanApp::createSyncObjects() {
    imageAvailableSemaphores_.resize(kMaxFramesInFlight);
    inFlightFences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::size_t index = 0; index < kMaxFramesInFlight; ++index) {
        vkCheck(
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[index]),
            "vkCreateSemaphore");
        vkCheck(
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[index]),
            "vkCreateFence");
    }
}

void VulkanApp::drawFrame() {
    vkCheck(
        vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX),
        "vkWaitForFences");

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        vkCheck(acquireResult, "vkAcquireNextImageKHR");
    }

    const bool captureThisFrame = screenshotRequested_;
    screenshotRequested_ = false;
    VkBuffer screenshotBuffer = VK_NULL_HANDLE;
    VkDeviceMemory screenshotMemory = VK_NULL_HANDLE;
    if (captureThisFrame) {
        const VkDeviceSize screenshotSize =
            static_cast<VkDeviceSize>(swapchainExtent_.width)
            * swapchainExtent_.height
            * 4;
        createBuffer(
            screenshotSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            screenshotBuffer,
            screenshotMemory);
    }

    updateUniformBuffer(currentFrame_);
    vkCheck(vkResetFences(device_, 1, &inFlightFences_[currentFrame_]), "vkResetFences");
    vkCheck(vkResetCommandBuffer(commandBuffers_[currentFrame_], 0), "vkResetCommandBuffer");
    recordCommandBuffer(
        commandBuffers_[currentFrame_],
        imageIndex,
        screenshotBuffer);

    const VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[imageIndex]};

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]),
        "vkQueueSubmit");
    if (captureThisFrame) {
        vkCheck(
            vkWaitForFences(
                device_,
                1,
                &inFlightFences_[currentFrame_],
                VK_TRUE,
                UINT64_MAX),
            "vkWaitForFences(screenshot)");
        try {
            saveScreenshot(
                screenshotMemory,
                swapchainExtent_.width,
                swapchainExtent_.height);
        } catch (...) {
            vkDestroyBuffer(device_, screenshotBuffer, nullptr);
            vkFreeMemory(device_, screenshotMemory, nullptr);
            throw;
        }
        vkDestroyBuffer(device_, screenshotBuffer, nullptr);
        vkFreeMemory(device_, screenshotMemory, nullptr);
    }

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR
        || presentResult == VK_SUBOPTIMAL_KHR
        || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        vkCheck(presentResult, "vkQueuePresentKHR");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

void VulkanApp::updateUniformBuffer(const std::size_t frameIndex) {
    const double currentTime = glfwGetTime();
    const float deltaSeconds = static_cast<float>(
        std::max(currentTime - lastRotationTime_, 0.0));
    lastRotationTime_ = currentTime;
    if (autoRotate_) {
        rotationAngle_ += deltaSeconds * 0.65F;
    }

    const Vector3 center = {
        (asset_.boundsMin[0] + asset_.boundsMax[0]) * 0.5F,
        (asset_.boundsMin[1] + asset_.boundsMax[1]) * 0.5F,
        (asset_.boundsMin[2] + asset_.boundsMax[2]) * 0.5F,
    };
    const float largestExtent = std::max({
        asset_.boundsMax[0] - asset_.boundsMin[0],
        asset_.boundsMax[1] - asset_.boundsMin[1],
        asset_.boundsMax[2] - asset_.boundsMin[2],
    });
    const float fitScale = largestExtent > 0.0F ? 2.5F / largestExtent : 1.0F;
    const Matrix4 model = multiply(
        rotationY(rotationAngle_),
        multiply(
            uniformScale(fitScale),
            translation(-center[0], -center[1], -center[2])));
    currentModel_ = model;
    const Matrix4 view = lookAt(
        cameraPosition_,
        cameraTarget_,
        {0.0F, 1.0F, 0.0F});
    const float aspect =
        static_cast<float>(swapchainExtent_.width)
        / static_cast<float>(swapchainExtent_.height);
    constexpr float kPi = 3.14159265358979323846F;
    const Matrix4 projection = perspective(kPi / 3.0F, aspect, 0.1F, 100.0F);
    const Vector3 lightDirection = normalize({0.48F, 0.82F, 0.32F});
    const Vector3 lightTarget = {0.0F, -0.10F, 0.0F};
    const Vector3 lightPosition = {
        lightTarget[0] + lightDirection[0] * 4.5F,
        lightTarget[1] + lightDirection[1] * 4.5F,
        lightTarget[2] + lightDirection[2] * 4.5F,
    };
    const Matrix4 lightView = lookAt(
        lightPosition,
        lightTarget,
        {0.0F, 1.0F, 0.0F});
    const Matrix4 lightProjection = orthographic(
        -1.90F,
        1.90F,
        -1.90F,
        1.90F,
        0.10F,
        8.0F);

    UniformBufferObject uniform{};
    uniform.model = model;
    uniform.modelViewProjection = multiply(projection, multiply(view, model));
    uniform.lightModelViewProjection =
        multiply(lightProjection, multiply(lightView, model));
    uniform.cameraPosition = {
        cameraPosition_[0],
        cameraPosition_[1],
        cameraPosition_[2],
        1.0F,
    };
    uniform.renderingParameters = {
        largestExtent * 0.004F,
        stylizedLightingEnabled_ ? styleMaskStrength_ : 0.0F,
        stylizedLightingEnabled_ ? diffuseBandThreshold_ : -1.0F,
        0.12F,
    };
    constexpr std::array<std::array<float, 4>, 3> kShowcasePresets = {{
        {0.0F, 1.00F, 0.13F, 0.12F},
        {1.0F, 0.92F, 0.16F, 0.16F},
        {2.0F, 0.95F, 0.08F, 0.05F},
    }};
    uniform.showcaseParameters =
        kShowcasePresets[std::min<std::size_t>(
            showcasePreset_,
            kShowcasePresets.size() - 1)];
    std::memcpy(
        uniformBufferMapped_[frameIndex],
        &uniform,
        sizeof(uniform));
}

void VulkanApp::recordCommandBuffer(
    const VkCommandBuffer commandBuffer,
    const std::uint32_t imageIndex,
    const VkBuffer screenshotBuffer) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0F, 0};
    VkRenderPassBeginInfo shadowPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    shadowPassInfo.renderPass = shadowRenderPass_;
    shadowPassInfo.framebuffer = shadowFramebuffer_;
    shadowPassInfo.renderArea.extent = {
        kShadowMapSize,
        kShadowMapSize,
    };
    shadowPassInfo.clearValueCount = 1;
    shadowPassInfo.pClearValues = &shadowClear;
    vkCmdBeginRenderPass(
        commandBuffer,
        &shadowPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    VkViewport shadowViewport{};
    shadowViewport.width = static_cast<float>(kShadowMapSize);
    shadowViewport.height = static_cast<float>(kShadowMapSize);
    shadowViewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
    VkRect2D shadowScissor{};
    shadowScissor.extent = {kShadowMapSize, kShadowMapSize};
    vkCmdSetScissor(commandBuffer, 0, 1, &shadowScissor);
    const VkDeviceSize shadowOffsets[] = {0};
    vkCmdBindVertexBuffers(
        commandBuffer,
        0,
        1,
        &vertexBuffer_,
        shadowOffsets);
    vkCmdBindIndexBuffer(
        commandBuffer,
        indexBuffer_,
        0,
        VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shadowPipeline_);
    for (const AssetPrimitive& primitive : asset_.primitives) {
        const AssetMaterial& material =
            asset_.materials[primitive.materialIndex];
        if (material.showcasePlatform > 0.5F) {
            continue;
        }
        const std::size_t descriptorIndex =
            currentFrame_ * asset_.materials.size()
            + primitive.materialIndex;
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[descriptorIndex],
            0,
            nullptr);
        const MaterialPushConstants materialConstants{
            material.alphaCutoff,
            static_cast<std::uint32_t>(material.alphaMode),
            material.emissiveStrength,
            material.showcasePlatform,
            material.aoColor,
            material.lamShadowColor,
            material.matcapColor,
            material.hairParameters,
        };
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(materialConstants),
            &materialConstants);
        vkCmdDrawIndexed(
            commandBuffer,
            primitive.indexCount,
            1,
            primitive.firstIndex,
            0,
            0);
    }
    vkCmdEndRenderPass(commandBuffer);

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color.float32[0] = 0.035F;
    clearValues[0].color.float32[1] = 0.055F;
    clearValues[0].color.float32[2] = 0.075F;
    clearValues[0].color.float32[3] = 1.0F;
    clearValues[1].depthStencil = {1.0F, 0};
    clearValues[2].color.float32[0] = 0.5F;
    clearValues[2].color.float32[1] = 0.5F;
    clearValues[2].color.float32[2] = 1.0F;
    clearValues[2].color.float32[3] = 0.0F;
    VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapchainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = swapchainExtent_;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        backgroundPipeline_);
    const std::size_t backgroundDescriptorIndex =
        currentFrame_ * asset_.materials.size();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &descriptorSets_[backgroundDescriptorIndex],
        0,
        nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        outlinePipeline_);
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode
            == AssetAlphaMode::Blend) {
            continue;
        }
        const std::size_t descriptorIndex =
            currentFrame_ * asset_.materials.size()
            + primitive.materialIndex;
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[descriptorIndex],
            0,
            nullptr);
        vkCmdDrawIndexed(
            commandBuffer,
            primitive.indexCount,
            1,
            primitive.firstIndex,
            0,
            0);
    }
    const auto drawPrimitive = [&](const AssetPrimitive& primitive) {
        const AssetMaterial& material = asset_.materials[primitive.materialIndex];
        const bool blend = material.alphaMode == AssetAlphaMode::Blend;
        const VkPipeline pipeline = blend
            ? (material.doubleSided ? blendDoubleSidedPipeline_ : blendPipeline_)
            : (material.doubleSided ? opaqueDoubleSidedPipeline_ : opaquePipeline_);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        const std::size_t descriptorIndex =
            currentFrame_ * asset_.materials.size() + primitive.materialIndex;
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[descriptorIndex],
            0,
            nullptr);
        const MaterialPushConstants materialConstants{
            material.alphaCutoff,
            static_cast<std::uint32_t>(material.alphaMode),
            material.emissiveStrength,
            material.showcasePlatform,
            material.aoColor,
            material.lamShadowColor,
            material.matcapColor,
            material.hairParameters,
        };
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(materialConstants),
            &materialConstants);
        vkCmdDrawIndexed(
            commandBuffer,
            primitive.indexCount,
            1,
            primitive.firstIndex,
            0,
            0);
    };
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode != AssetAlphaMode::Blend) {
            drawPrimitive(primitive);
        }
    }
    std::vector<const AssetPrimitive*> transparentPrimitives;
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode
            == AssetAlphaMode::Blend) {
            transparentPrimitives.push_back(&primitive);
        }
    }
    const Vector3 cameraForward = normalize({
        -cameraPosition_[0],
        -cameraPosition_[1],
        -cameraPosition_[2],
    });
    const auto viewDepth = [&](const AssetPrimitive& primitive) {
        const Vector3 worldCenter = transformPosition(
            currentModel_,
            primitive.center);
        const Vector3 cameraOffset = subtract(worldCenter, cameraPosition_);
        return dot(cameraOffset, cameraForward);
    };
    std::stable_sort(
        transparentPrimitives.begin(),
        transparentPrimitives.end(),
        [&](const AssetPrimitive* left, const AssetPrimitive* right) {
            return viewDepth(*left) > viewDepth(*right);
        });
    for (const AssetPrimitive* primitive : transparentPrimitives) {
        drawPrimitive(*primitive);
    }
    vkCmdEndRenderPass(commandBuffer);

    VkRenderPassBeginInfo postProcessPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    postProcessPassInfo.renderPass = postProcessRenderPass_;
    postProcessPassInfo.framebuffer =
        postProcessFramebuffers_[imageIndex];
    postProcessPassInfo.renderArea.extent = swapchainExtent_;
    vkCmdBeginRenderPass(
        commandBuffer,
        &postProcessPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        innerOutlinePipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        postProcessPipelineLayout_,
        0,
        1,
        &postProcessDescriptorSets_[imageIndex],
        0,
        nullptr);
    const PostProcessPushConstants postProcessConstants{
        innerOutlineEnabled_ ? 0.40F : 0.0F,
        0.18F,
        0.20F,
        0.0F,
    };
    vkCmdPushConstants(
        commandBuffer,
        postProcessPipelineLayout_,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(postProcessConstants),
        &postProcessConstants);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    if (screenshotBuffer != VK_NULL_HANDLE) {
        VkImageMemoryBarrier toTransfer{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = swapchainImages_[imageIndex];
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {
            swapchainExtent_.width,
            swapchainExtent_.height,
            1,
        };
        vkCmdCopyImageToBuffer(
            commandBuffer,
            swapchainImages_[imageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            screenshotBuffer,
            1,
            &copy);

        VkImageMemoryBarrier toPresent = toTransfer;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toPresent);
    }

    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

void VulkanApp::saveScreenshot(
    const VkDeviceMemory screenshotMemory,
    const std::uint32_t width,
    const std::uint32_t height) const {
    const std::size_t byteCount =
        static_cast<std::size_t>(width) * height * 4;
    void* mapped = nullptr;
    vkCheck(
        vkMapMemory(
            device_,
            screenshotMemory,
            0,
            static_cast<VkDeviceSize>(byteCount),
            0,
            &mapped),
        "vkMapMemory(screenshot)");
    std::vector<std::uint8_t> rgba(byteCount);
    std::memcpy(rgba.data(), mapped, byteCount);
    vkUnmapMemory(device_, screenshotMemory);

    const bool bgra =
        swapchainFormat_ == VK_FORMAT_B8G8R8A8_SRGB
        || swapchainFormat_ == VK_FORMAT_B8G8R8A8_UNORM;
    const bool rgbaFormat =
        swapchainFormat_ == VK_FORMAT_R8G8B8A8_SRGB
        || swapchainFormat_ == VK_FORMAT_R8G8B8A8_UNORM;
    if (!bgra && !rgbaFormat) {
        throw std::runtime_error(
            "Screenshot only supports 8-bit RGBA/BGRA swapchain formats");
    }
    if (bgra) {
        for (std::size_t pixel = 0; pixel < byteCount; pixel += 4) {
            std::swap(rgba[pixel], rgba[pixel + 2]);
        }
    }

    const std::filesystem::path captureDirectory =
        MYVULKANAPP_CAPTURE_DIR;
    std::filesystem::create_directories(captureDirectory);
    const auto timestamp = std::chrono::duration_cast<
        std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::filesystem::path outputPath =
        captureDirectory
        / ("capture_" + std::to_string(timestamp) + ".png");
    if (stbi_write_png(
            outputPath.string().c_str(),
            static_cast<int>(width),
            static_cast<int>(height),
            4,
            rgba.data(),
            static_cast<int>(width * 4)) == 0) {
        throw std::runtime_error(
            "Failed to write screenshot: " + outputPath.string());
    }
    std::cout << "Screenshot saved: " << outputPath.string() << '\n';
}

void VulkanApp::recreateSwapchain() {
    int width = 0;
    int height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createNormalResources();
    createRenderPass();
    createPostProcessRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createPostProcessFramebuffers();
    createPostProcessDescriptorSets();
    createSwapchainSemaphores();
}

void VulkanApp::cleanupSwapchain() {
    if (postProcessDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            device_,
            postProcessDescriptorPool_,
            nullptr);
        postProcessDescriptorPool_ = VK_NULL_HANDLE;
    }
    postProcessDescriptorSets_.clear();

    for (const auto semaphore : renderFinishedSemaphores_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    renderFinishedSemaphores_.clear();

    for (const auto framebuffer : swapchainFramebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    swapchainFramebuffers_.clear();
    for (const auto framebuffer : postProcessFramebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    postProcessFramebuffers_.clear();

    for (const auto imageView : depthImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    for (const auto image : depthImages_) {
        vkDestroyImage(device_, image, nullptr);
    }
    for (const auto memory : depthImageMemories_) {
        vkFreeMemory(device_, memory, nullptr);
    }
    depthImageViews_.clear();
    depthImages_.clear();
    depthImageMemories_.clear();

    for (const auto imageView : normalImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    for (const auto image : normalImages_) {
        vkDestroyImage(device_, image, nullptr);
    }
    for (const auto memory : normalImageMemories_) {
        vkFreeMemory(device_, memory, nullptr);
    }
    normalImageViews_.clear();
    normalImages_.clear();
    normalImageMemories_.clear();

    for (VkPipeline* pipeline : {
             &opaquePipeline_,
             &opaqueDoubleSidedPipeline_,
             &blendPipeline_,
             &blendDoubleSidedPipeline_,
             &outlinePipeline_,
             &backgroundPipeline_,
             &shadowPipeline_}) {
        if (*pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, *pipeline, nullptr);
            *pipeline = VK_NULL_HANDLE;
        }
    }
    if (innerOutlinePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, innerOutlinePipeline_, nullptr);
        innerOutlinePipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (postProcessPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(
            device_,
            postProcessPipelineLayout_,
            nullptr);
        postProcessPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (postProcessRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(
            device_,
            postProcessRenderPass_,
            nullptr);
        postProcessRenderPass_ = VK_NULL_HANDLE;
    }
    for (const auto imageView : swapchainImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

VulkanApp::QueueFamilyIndices VulkanApp::findQueueFamilies(
    const VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (std::uint32_t index = 0; index < queueFamilyCount; ++index) {
        if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphics = index;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.present = index;
        }
        if (indices.complete()) {
            break;
        }
    }
    return indices;
}

VulkanApp::SwapchainSupport VulkanApp::querySwapchainSupport(
    const VkPhysicalDevice device) const {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
    support.formats.resize(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface_, &formatCount, support.formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
    support.presentModes.resize(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface_, &presentModeCount, support.presentModes.data());
    }
    return support;
}

bool VulkanApp::isDeviceSuitable(const VkPhysicalDevice device) const {
    const QueueFamilyIndices indices = findQueueFamilies(device);
    const bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapchainAdequate = false;
    if (extensionsSupported) {
        const SwapchainSupport support = querySwapchainSupport(device);
        swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }
    return indices.complete() && extensionsSupported && swapchainAdequate;
}

bool VulkanApp::checkDeviceExtensionSupport(const VkPhysicalDevice device) const {
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());
    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& extension : available) {
        required.erase(extension.extensionName);
    }
    return required.empty();
}

bool VulkanApp::checkValidationLayerSupport() const {
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    return std::all_of(
        kValidationLayers.begin(),
        kValidationLayers.end(),
        [&available](const char* requiredLayer) {
            return std::any_of(
                available.begin(),
                available.end(),
                [requiredLayer](const VkLayerProperties& layer) {
                    return std::strcmp(requiredLayer, layer.layerName) == 0;
                });
        });
}

std::vector<const char*> VulkanApp::requiredInstanceExtensions() const {
    std::uint32_t extensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (glfwExtensions == nullptr) {
        throw std::runtime_error("GLFW did not provide Vulkan instance extensions");
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);
    if (kEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VkSurfaceFormatKHR VulkanApp::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
    const auto preferred = std::find_if(
        formats.begin(),
        formats.end(),
        [](const VkSurfaceFormatKHR& format) {
            return format.format == VK_FORMAT_B8G8R8A8_SRGB
                && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
    return preferred != formats.end() ? *preferred : formats.front();
}

VkPresentModeKHR VulkanApp::choosePresentMode(
    const std::vector<VkPresentModeKHR>& presentModes) {
    return std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR)
            != presentModes.end()
        ? VK_PRESENT_MODE_MAILBOX_KHR
        : VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanApp::chooseExtent(
    const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    VkExtent2D extent{
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height),
    };
    extent.width = std::clamp(
        extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(
        extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);
    return extent;
}

std::uint32_t VulkanApp::findMemoryType(
    const std::uint32_t typeFilter,
    const VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        if ((typeFilter & (1U << index)) != 0U
            && (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }
    throw std::runtime_error("No suitable Vulkan memory type was found");
}

VkFormat VulkanApp::findDepthFormat() const {
    constexpr std::array candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);
        constexpr VkFormatFeatureFlags requiredFeatures =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & requiredFeatures)
            == requiredFeatures) {
            return format;
        }
    }
    throw std::runtime_error("No supported depth buffer format was found");
}

void VulkanApp::createBuffer(
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory) const {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);
    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);
    vkCheck(vkAllocateMemory(device_, &allocateInfo, nullptr, &memory), "vkAllocateMemory(buffer)");
    vkCheck(vkBindBufferMemory(device_, buffer, memory, 0), "vkBindBufferMemory");
}

void VulkanApp::copyBuffer(
    const VkBuffer source,
    const VkBuffer destination,
    const VkDeviceSize size) const {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(copy)");
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(copy)");

    const VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(commandBuffer, source, destination, 1, &region);
    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(copy)");

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit(copy)");
    vkCheck(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(copy)");
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void VulkanApp::transitionImageLayout(
    const VkImage image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout) const {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(image transition)");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(
        vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "vkBeginCommandBuffer(image transition)");

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
        && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (
        oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);
    vkCheck(
        vkEndCommandBuffer(commandBuffer),
        "vkEndCommandBuffer(image transition)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(image transition)");
    vkCheck(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(image transition)");
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void VulkanApp::copyBufferToImage(
    const VkBuffer source,
    const VkImage destination,
    const std::uint32_t width,
    const std::uint32_t height) const {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(image copy)");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(image copy)");
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(
        commandBuffer,
        source,
        destination,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(image copy)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(image copy)");
    vkCheck(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(image copy)");
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void VulkanApp::createImage(
    const std::uint32_t width,
    const std::uint32_t height,
    const VkFormat format,
    const VkImageUsageFlags usage,
    VkImage& image,
    VkDeviceMemory& memory) const {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateImage(device_, &imageInfo, nullptr, &image), "vkCreateImage");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, image, &requirements);
    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex =
        findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkCheck(vkAllocateMemory(device_, &allocateInfo, nullptr, &memory), "vkAllocateMemory(image)");
    vkCheck(vkBindImageMemory(device_, image, memory, 0), "vkBindImageMemory");
}

VkImageView VulkanApp::createImageView(
    const VkImage image,
    const VkFormat format,
    const VkImageAspectFlags aspect) const {
    VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspect;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    vkCheck(vkCreateImageView(device_, &createInfo, nullptr, &imageView), "vkCreateImageView");
    return imageView;
}

std::vector<char> VulkanApp::readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open shader: " + path);
    }
    const auto fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

VkShaderModule VulkanApp::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    vkCheck(
        vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule),
        "vkCreateShaderModule");
    return shaderModule;
}

void VulkanApp::framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto* application = static_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
    application->framebufferResized_ = true;
}

void VulkanApp::keyCallback(
    GLFWwindow* window,
    const int key,
    const int scancode,
    const int action,
    const int modifiers) {
    (void)scancode;
    (void)modifiers;
    auto* application = static_cast<VulkanApp*>(
        glfwGetWindowUserPointer(window));
    if (application == nullptr) {
        return;
    }

    constexpr float kPi = 3.14159265358979323846F;
    constexpr float kFineStep = kPi / 36.0F;
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_SPACE) {
            application->autoRotate_ = !application->autoRotate_;
            std::cout << "Auto rotate: "
                      << (application->autoRotate_ ? "on" : "paused")
                      << '\n';
        } else if (key == GLFW_KEY_R) {
            application->autoRotate_ = true;
            std::cout << "Auto rotate: on\n";
        } else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) {
            application->rotationAngle_ =
                kPi * 0.25F
                + static_cast<float>(key - GLFW_KEY_1) * kPi * 0.5F;
            application->cameraPosition_ = {2.8F, 2.1F, 3.2F};
            application->cameraTarget_ = {0.0F, 0.0F, 0.0F};
            application->autoRotate_ = false;
            std::cout << "Angle preset: " << key - GLFW_KEY_0 << '\n';
        } else if (key == GLFW_KEY_5) {
            application->rotationAngle_ = kPi * 0.25F;
            application->cameraPosition_ = {0.915F, 1.507F, 1.046F};
            application->cameraTarget_ = {0.0F, 0.82F, 0.0F};
            application->autoRotate_ = false;
            std::cout << "View preset: 5 (face close-up)\n";
        } else if (key == GLFW_KEY_F12) {
            application->screenshotRequested_ = true;
            std::cout << "Screenshot requested\n";
        } else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F3) {
            application->showcasePreset_ =
                static_cast<std::uint32_t>(key - GLFW_KEY_F1);
            constexpr std::array<const char*, 3> kPresetNames = {
                "Afterglow Gallery",
                "Endfield Industrial",
                "Neutral Material Check",
            };
            std::cout
                << "Showcase preset: "
                << key - GLFW_KEY_F1 + 1
                << " ("
                << kPresetNames[application->showcasePreset_]
                << ")\n";
        } else if (key == GLFW_KEY_F10) {
            application->innerOutlineEnabled_ =
                !application->innerOutlineEnabled_;
            std::cout
                << "Inner outlines: "
                << (application->innerOutlineEnabled_ ? "on" : "off")
                << '\n';
        } else if (key == GLFW_KEY_F9) {
            application->stylizedLightingEnabled_ =
                !application->stylizedLightingEnabled_;
            std::cout
                << "Stylized lighting: "
                << (application->stylizedLightingEnabled_ ? "on" : "off")
                << '\n';
        }
    }
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_LEFT) {
            application->rotationAngle_ -= kFineStep;
            application->autoRotate_ = false;
        } else if (key == GLFW_KEY_RIGHT) {
            application->rotationAngle_ += kFineStep;
            application->autoRotate_ = false;
        } else if (key == GLFW_KEY_F7) {
            application->styleMaskStrength_ = std::max(
                application->styleMaskStrength_ - 0.10F,
                0.0F);
            std::cout
                << "Style mask strength: "
                << application->styleMaskStrength_
                << '\n';
        } else if (key == GLFW_KEY_F8) {
            application->styleMaskStrength_ = std::min(
                application->styleMaskStrength_ + 0.10F,
                2.0F);
            std::cout
                << "Style mask strength: "
                << application->styleMaskStrength_
                << '\n';
        } else if (key == GLFW_KEY_F5) {
            application->diffuseBandThreshold_ = std::max(
                application->diffuseBandThreshold_ - 0.05F,
                0.05F);
            std::cout
                << "Diffuse band threshold: "
                << application->diffuseBandThreshold_
                << '\n';
        } else if (key == GLFW_KEY_F6) {
            application->diffuseBandThreshold_ = std::min(
                application->diffuseBandThreshold_ + 0.05F,
                0.95F);
            std::cout
                << "Diffuse band threshold: "
                << application->diffuseBandThreshold_
                << '\n';
        }
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanApp::debugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*) {
    const char* prefix = severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        ? "validation error"
        : "validation warning";
    std::cerr << '[' << prefix << "] " << callbackData->pMessage << '\n';
    return VK_FALSE;
}

void VulkanApp::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}
