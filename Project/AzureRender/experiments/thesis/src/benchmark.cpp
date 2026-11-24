#include "Benchmark.hpp"
#include "Workload.hpp"
#include "Capture.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace thesis {
static void check(VkResult result, const char* name) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(name)+": "+std::to_string(result));
}
VKAPI_ATTR VkBool32 VKAPI_CALL Benchmark::debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void* user) {
    auto self=static_cast<Benchmark*>(user);
    if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++self->errors;
    else if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ++self->warnings;
    std::cerr << message->pMessage << '\n';
    return VK_FALSE;
}

void Benchmark::initialize() {
    if(options.present) window=std::make_unique<Window>(options.width,options.height);
    std::vector<const char*> instanceExtensions=window?window->instanceExtensions():std::vector<const char*>{};
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="AzureThesisBenchmark";
    uint32_t loaderVersion=VK_API_VERSION_1_0;
    auto enumerateVersion=reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr,"vkEnumerateInstanceVersion"));
    if(enumerateVersion) check(enumerateVersion(&loaderVersion),"loader version");
    if(loaderVersion<VK_API_VERSION_1_3) throw std::runtime_error("Vulkan 1.3 loader required");
    app.apiVersion=std::min(loaderVersion,options.path=="drlr"?VK_API_VERSION_1_4:VK_API_VERSION_1_3);
    const char* layer="VK_LAYER_KHRONOS_validation";
    const char* extension=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkValidationFeatureEnableEXT sync=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validation.enabledValidationFeatureCount=1; validation.pEnabledValidationFeatures=&sync;
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugInfo.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debugInfo.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback=debug; debugInfo.pUserData=this;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo=&app;
    if(options.validation) {
        ci.enabledLayerCount=1; ci.ppEnabledLayerNames=&layer;
        instanceExtensions.push_back(extension);
        ci.pNext=&debugInfo; debugInfo.pNext=&validation;
    }
    ci.enabledExtensionCount=uint32_t(instanceExtensions.size());ci.ppEnabledExtensionNames=instanceExtensions.data();
    check(vkCreateInstance(&ci,nullptr,&instance),"create instance");
    if(window) window->createSurface(instance);
    if(options.validation) {
        debugInfo.pNext=nullptr;
        auto create=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));
        if(!create) throw std::runtime_error("debug messenger unavailable");
        check(create(instance,&debugInfo,nullptr,&messenger),"create debug messenger");
    }
    uint32_t count=0; check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"device count");
    std::vector<VkPhysicalDevice> devices(count); check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"devices");
    for(auto candidate:devices) {
        VkPhysicalDeviceProperties p{}; vkGetPhysicalDeviceProperties(candidate,&p);
        if(std::string(p.deviceName).find(options.gpu)!=std::string::npos) { physical=candidate; properties=p; break; }
    }
    if(!physical) throw std::runtime_error("requested GPU unavailable");
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);
    std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
    bool found=false;
    for(uint32_t i=0;i<count;++i) if((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && families[i].timestampValidBits && (!window || window->supports(physical,i))) {
        queueFamily=i; timestampBits=families[i].timestampValidBits; found=true; break;
    }
    if(!found) throw std::runtime_error("graphics timestamp queue unavailable");
    float priority=1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=queueFamily; qi.queueCount=1; qi.pQueuePriorities=&priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi;
    VkPhysicalDeviceDynamicRenderingFeatures dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR local{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR};
    VkPhysicalDeviceFeatures enabled{};
    const char* localExtension=VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME;
    std::vector<const char*> deviceExtensions;
    if(window) deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(options.path=="drlr") {
        uint32_t n=0; check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&n,nullptr),"extension count");
        std::vector<VkExtensionProperties> exts(n); check(vkEnumerateDeviceExtensionProperties(physical,nullptr,&n,exts.data()),"extensions");
        bool hasExtension=std::any_of(exts.begin(),exts.end(),[&](auto& e) { return std::strcmp(e.extensionName,localExtension)==0; });
        if(properties.apiVersion<VK_API_VERSION_1_4 && !hasExtension) throw std::runtime_error("DRLR extension unavailable on selected device");
        dynamic.pNext=&local; VkPhysicalDeviceFeatures2 f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; f.pNext=&dynamic;
        vkGetPhysicalDeviceFeatures2(physical,&f);
        if(!dynamic.dynamicRendering || !local.dynamicRenderingLocalRead || !f.features.independentBlend)
            throw std::runtime_error("DRLR or independent color write masks unavailable");
        enabled.independentBlend=VK_TRUE; di.pEnabledFeatures=&enabled; di.pNext=&dynamic;
        if(hasExtension) deviceExtensions.push_back(localExtension);
    }
    di.enabledExtensionCount=uint32_t(deviceExtensions.size());di.ppEnabledExtensionNames=deviceExtensions.data();
    check(vkCreateDevice(physical,&di,nullptr,&device),"create device");
    if(options.path=="drlr") {
        beginRendering=reinterpret_cast<PFN_vkCmdBeginRendering>(vkGetDeviceProcAddr(device,"vkCmdBeginRendering"));
        endRendering=reinterpret_cast<PFN_vkCmdEndRendering>(vkGetDeviceProcAddr(device,"vkCmdEndRendering"));
        setLocations=reinterpret_cast<PFN_vkCmdSetRenderingAttachmentLocationsKHR>(vkGetDeviceProcAddr(device,"vkCmdSetRenderingAttachmentLocations"));
        if(!setLocations) setLocations=reinterpret_cast<PFN_vkCmdSetRenderingAttachmentLocationsKHR>(vkGetDeviceProcAddr(device,"vkCmdSetRenderingAttachmentLocationsKHR"));
        setInputIndices=reinterpret_cast<PFN_vkCmdSetRenderingInputAttachmentIndicesKHR>(vkGetDeviceProcAddr(device,"vkCmdSetRenderingInputAttachmentIndices"));
        if(!setInputIndices) setInputIndices=reinterpret_cast<PFN_vkCmdSetRenderingInputAttachmentIndicesKHR>(vkGetDeviceProcAddr(device,"vkCmdSetRenderingInputAttachmentIndicesKHR"));
        if(!beginRendering || !endRendering || !setLocations || !setInputIndices) throw std::runtime_error("DRLR command entry points unavailable");
    }
    vkGetDeviceQueue(device,queueFamily,0,&queue);
    if(window) window->initialize(physical,device,queueFamily);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.queueFamilyIndex=queueFamily; pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    check(vkCreateCommandPool(device,&pool,nullptr,&commandPool),"command pool");
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool=commandPool; alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount=1;
    check(vkAllocateCommandBuffers(device,&alloc,&command),"command buffer");
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; check(vkCreateFence(device,&fi,nullptr,&fence),"fence");
    VkQueryPoolCreateInfo qp{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO}; qp.queryType=VK_QUERY_TYPE_TIMESTAMP; qp.queryCount=2;
    check(vkCreateQueryPool(device,&qp,nullptr,&queries),"query pool");
    auto color=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    auto sampled=VK_IMAGE_USAGE_SAMPLED_BIT;
    auto input=VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    makeImage(albedo,VK_FORMAT_R8G8B8A8_UNORM,color|sampled|input,VK_IMAGE_ASPECT_COLOR_BIT);
    makeImage(normal,VK_FORMAT_R16G16B16A16_SFLOAT,color|sampled|input,VK_IMAGE_ASPECT_COLOR_BIT);
    makeImage(depth,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
    makeImage(toon,VK_FORMAT_R8G8B8A8_UNORM,color|sampled,VK_IMAGE_ASPECT_COLOR_BIT);
    makeImage(output,VK_FORMAT_R8G8B8A8_UNORM,color|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
    auto mesh=options.complexity=="low"?sphere(32,16):sphere(128,64);
    vertexCount=uint32_t(mesh.size());
    makeBuffer(vertices,mesh.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    void* mapping{}; check(vkMapMemory(device,vertices.memory,0,vertices.size,0,&mapping),"map vertices");
    std::memcpy(mapping,mesh.data(),size_t(vertices.size)); vkUnmapMemory(device,vertices.memory);
    makeBuffer(readback,VkDeviceSize(options.width)*options.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    descriptors(); createPaths();
}

uint32_t Benchmark::memoryType(uint32_t bits,VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties m{}; vkGetPhysicalDeviceMemoryProperties(physical,&m);
    for(uint32_t i=0;i<m.memoryTypeCount;++i) if((bits & (1u<<i)) && (m.memoryTypes[i].propertyFlags & flags)==flags) return i;
    throw std::runtime_error("memory type unavailable");
}
void Benchmark::makeBuffer(Buffer& b,VkDeviceSize size,VkBufferUsageFlags usage) {
    b.size=size;
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; ci.size=size; ci.usage=usage; ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device,&ci,nullptr,&b.buffer),"buffer");
    VkMemoryRequirements r{}; vkGetBufferMemoryRequirements(device,b.buffer,&r);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; a.allocationSize=r.size;
    a.memoryTypeIndex=memoryType(r.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vkAllocateMemory(device,&a,nullptr,&b.memory),"buffer memory"); check(vkBindBufferMemory(device,b.buffer,b.memory,0),"bind buffer");
}
void Benchmark::makeImage(Image& image,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect) {
    image.format=format;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ci.imageType=VK_IMAGE_TYPE_2D; ci.format=format;
    ci.extent={options.width,options.height,1}; ci.mipLevels=1; ci.arrayLayers=1; ci.samples=VK_SAMPLE_COUNT_1_BIT;
    ci.tiling=VK_IMAGE_TILING_OPTIMAL; ci.usage=usage; ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VkImageFormatProperties capabilities{};
    check(vkGetPhysicalDeviceImageFormatProperties(physical,format,ci.imageType,ci.tiling,usage,0,&capabilities),"image capabilities");
    if(options.width>capabilities.maxExtent.width || options.height>capabilities.maxExtent.height) throw std::runtime_error("extent unsupported");
    check(vkCreateImage(device,&ci,nullptr,&image.image),"image");
    VkMemoryRequirements r{}; vkGetImageMemoryRequirements(device,image.image,&r);
    VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; a.allocationSize=r.size; a.memoryTypeIndex=memoryType(r.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device,&a,nullptr,&image.memory),"image memory"); check(vkBindImageMemory(device,image.image,image.memory,0),"bind image");
    VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; v.image=image.image; v.viewType=VK_IMAGE_VIEW_TYPE_2D; v.format=format;
    v.subresourceRange={aspect,0,1,0,1}; check(vkCreateImageView(device,&v,nullptr,&image.view),"image view");
}

void Benchmark::descriptors() {
    VkSamplerCreateInfo s{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO}; s.magFilter=VK_FILTER_NEAREST; s.minFilter=VK_FILTER_NEAREST;
    s.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST; s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    check(vkCreateSampler(device,&s,nullptr,&sampler),"sampler");
    VkDescriptorSetLayoutBinding bindings[2]{};
    for(uint32_t i=0;i<2;++i) { bindings[i].binding=i; bindings[i].descriptorCount=1; bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT; bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; }
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; li.bindingCount=2; li.pBindings=bindings;
    check(vkCreateDescriptorSetLayout(device,&li,nullptr,&sampledLayout),"sampled layout");
    for(auto& binding:bindings) binding.descriptorType=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    check(vkCreateDescriptorSetLayout(device,&li,nullptr,&inputLayout),"input layout");
    VkPushConstantRange pc{VK_SHADER_STAGE_VERTEX_BIT,0,4};
    VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pi.setLayoutCount=1; pi.pSetLayouts=&sampledLayout; pi.pushConstantRangeCount=1; pi.pPushConstantRanges=&pc;
    check(vkCreatePipelineLayout(device,&pi,nullptr,&sampledPipelineLayout),"sampled pipeline layout"); pi.pSetLayouts=&inputLayout;
    check(vkCreatePipelineLayout(device,&pi,nullptr,&inputPipelineLayout),"input pipeline layout");
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4},{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,2}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pool.maxSets=3; pool.poolSizeCount=2; pool.pPoolSizes=sizes;
    check(vkCreateDescriptorPool(device,&pool,nullptr,&descriptorPool),"descriptor pool");
    VkDescriptorSetLayout layouts[]={sampledLayout,inputLayout,sampledLayout}; VkDescriptorSet sets[3]{};
    VkDescriptorSetAllocateInfo a{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; a.descriptorPool=descriptorPool; a.descriptorSetCount=3; a.pSetLayouts=layouts;
    check(vkAllocateDescriptorSets(device,&a,sets),"descriptor sets"); sampledSet=sets[0]; inputSet=sets[1]; contourSet=sets[2];
    for(int j=0;j<3;++j) for(uint32_t i=0;i<2;++i) {
        Image* image=i==1?&normal:(j==2?&toon:&albedo);
        VkDescriptorImageInfo info{j==1?VK_NULL_HANDLE:sampler,image->view,
            j==1 && options.path=="drlr"?VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; w.dstSet=sets[j]; w.dstBinding=i; w.descriptorCount=1;
        w.descriptorType=j==1?VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w.pImageInfo=&info;
        vkUpdateDescriptorSets(device,1,&w,0,nullptr);
    }
}

VkRenderPass Benchmark::makePass(const std::vector<Image*>& colors,Image* depthImage,VkImageLayout finalLayout) {
    std::vector<VkAttachmentDescription> descriptions;
    std::vector<VkAttachmentReference> references;
    for(auto image:colors) {
        VkAttachmentDescription d{}; d.format=image->format; d.samples=VK_SAMPLE_COUNT_1_BIT;
        d.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; d.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        d.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; d.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        d.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; d.finalLayout=finalLayout;
        references.push_back({uint32_t(descriptions.size()),VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}); descriptions.push_back(d);
    }
    VkAttachmentReference depthReference{uint32_t(descriptions.size()),VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    if(depthImage) {
        VkAttachmentDescription d{}; d.format=depthImage->format; d.samples=VK_SAMPLE_COUNT_1_BIT;
        d.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; d.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        d.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; d.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        d.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; descriptions.push_back(d);
    }
    VkSubpassDescription sub{}; sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount=uint32_t(references.size()); sub.pColorAttachments=references.data(); sub.pDepthStencilAttachment=depthImage?&depthReference:nullptr;
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass=VK_SUBPASS_EXTERNAL; deps[0].dstSubpass=0;
    deps[0].srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; deps[0].srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass=0; deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstStageMask=finalLayout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL?VK_PIPELINE_STAGE_TRANSFER_BIT:VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].dstAccessMask=finalLayout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; ci.attachmentCount=uint32_t(descriptions.size()); ci.pAttachments=descriptions.data();
    ci.subpassCount=1; ci.pSubpasses=&sub; ci.dependencyCount=2; ci.pDependencies=deps;
    VkRenderPass pass{}; check(vkCreateRenderPass(device,&ci,nullptr,&pass),"render pass"); return pass;
}
VkFramebuffer Benchmark::makeFramebuffer(VkRenderPass pass,const std::vector<Image*>& images) {
    std::vector<VkImageView> views; for(auto image:images) views.push_back(image->view);
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; ci.renderPass=pass; ci.attachmentCount=uint32_t(views.size()); ci.pAttachments=views.data();
    ci.width=options.width; ci.height=options.height; ci.layers=1;
    VkFramebuffer framebuffer{}; check(vkCreateFramebuffer(device,&ci,nullptr,&framebuffer),"framebuffer"); return framebuffer;
}
VkShaderModule Benchmark::shader(const std::string& name) {
    std::ifstream file(options.shaders/(name+".spv"),std::ios::binary|std::ios::ate);
    if(!file) throw std::runtime_error("shader missing: "+name);
    auto bytes=file.tellg(); if(bytes<=0 || size_t(bytes)%4) throw std::runtime_error("invalid shader");
    std::vector<uint32_t> words(size_t(bytes)/4); file.seekg(0); file.read(reinterpret_cast<char*>(words.data()),bytes);
    if(!file) throw std::runtime_error("shader read failed");
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; ci.codeSize=size_t(bytes); ci.pCode=words.data();
    VkShaderModule module{}; check(vkCreateShaderModule(device,&ci,nullptr,&module),"shader module"); modules.push_back(module); return module;
}
VkPipeline Benchmark::makePipeline(VkRenderPass pass,uint32_t subpass,const std::string& fragment,bool geometry,uint32_t colorCount,bool dynamic) {
    VkPipelineShaderStageCreateInfo stages[2]{};
    for(auto& stage:stages) { stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName="main"; }
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=shader(geometry?"geometry.vert":"fullscreen.vert");
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=shader(fragment);
    VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attribute{0,0,VK_FORMAT_R32G32B32_SFLOAT,0};
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if(geometry) { vi.vertexBindingDescriptionCount=1; vi.pVertexBindingDescriptions=&binding; vi.vertexAttributeDescriptionCount=1; vi.pVertexAttributeDescriptions=&attribute; }
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0,0,float(options.width),float(options.height),0,1}; VkRect2D scissor{{0,0},{options.width,options.height}};
    VkPipelineViewportStateCreateInfo vs{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vs.viewportCount=1; vs.pViewports=&viewport; vs.scissorCount=1; vs.pScissors=&scissor;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; rs.polygonMode=VK_POLYGON_MODE_FILL; rs.cullMode=VK_CULL_MODE_NONE; rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth=1;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; ds.depthTestEnable=geometry; ds.depthWriteEnable=geometry; ds.depthCompareOp=VK_COMPARE_OP_LESS;
    std::vector<VkPipelineColorBlendAttachmentState> blends(colorCount); for(auto& blend:blends) blend.colorWriteMask=15;
    if(dynamic) for(uint32_t i=0;i<colorCount;++i) blends[i].colorWriteMask=(geometry?(i<2):(i==2))?15:0;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; cb.attachmentCount=colorCount; cb.pAttachments=blends.data();
    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; ci.stageCount=2; ci.pStages=stages; ci.pVertexInputState=&vi; ci.pInputAssemblyState=&ia; ci.pViewportState=&vs;
    ci.pRasterizationState=&rs; ci.pMultisampleState=&ms; ci.pDepthStencilState=&ds; ci.pColorBlendState=&cb;
    ci.layout=(!geometry && (fragment.find("local")!=std::string::npos || fragment.find("dynamic")!=std::string::npos))?inputPipelineLayout:sampledPipelineLayout;
    ci.renderPass=pass; ci.subpass=subpass;
    VkFormat formats[]={albedo.format,normal.format,toon.format};
    uint32_t locations[]={0,1,2}, indices[]={0,1,VK_ATTACHMENT_UNUSED}, unused=VK_ATTACHMENT_UNUSED;
    VkRenderingInputAttachmentIndexInfoKHR inputInfo{VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR};
    inputInfo.colorAttachmentCount=3; inputInfo.pColorAttachmentInputIndices=indices;
    inputInfo.pDepthInputAttachmentIndex=&unused; inputInfo.pStencilInputAttachmentIndex=&unused;
    VkRenderingAttachmentLocationInfoKHR locationInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR};
    locationInfo.colorAttachmentCount=3; locationInfo.pColorAttachmentLocations=locations; locationInfo.pNext=&inputInfo;
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount=3; rendering.pColorAttachmentFormats=formats; rendering.depthAttachmentFormat=depth.format;
    rendering.pNext=&locationInfo;
    if(dynamic) ci.pNext=&rendering;
    VkPipeline pipeline{}; check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline),"graphics pipeline"); return pipeline;
}

void Benchmark::createPaths() {
    if(options.path=="drlr") {
        createDynamic();
        contourPass=makePass({&output},nullptr,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        contourFramebuffer=makeFramebuffer(contourPass,{&output});
        contourPipeline=makePipeline(contourPass,0,"contour.frag",false,1);
        return;
    }
    if(options.path=="subpasses") {
        createSubpasses();
        contourPass=makePass({&output},nullptr,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        contourFramebuffer=makeFramebuffer(contourPass,{&output});
        contourPipeline=makePipeline(contourPass,0,"contour.frag",false,1);
        return;
    }
    geometryPass=makePass({&albedo,&normal},&depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    toonPass=makePass({&toon},nullptr,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    contourPass=makePass({&output},nullptr,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    geometryFramebuffer=makeFramebuffer(geometryPass,{&albedo,&normal,&depth});
    toonFramebuffer=makeFramebuffer(toonPass,{&toon});
    contourFramebuffer=makeFramebuffer(contourPass,{&output});
    geometryPipeline=makePipeline(geometryPass,0,"geometry.frag",true,2);
    toonPipeline=makePipeline(toonPass,0,"toon.frag",false,1);
    contourPipeline=makePipeline(contourPass,0,"contour.frag",false,1);
}
void Benchmark::beginPass(VkRenderPass pass,VkFramebuffer framebuffer,uint32_t colorCount,bool hasDepth) {
    std::vector<VkClearValue> clears(colorCount+(hasDepth?1:0));
    if(pass==geometryPass) clears[1].color.float32[3]=1;
    if(hasDepth) clears.back().depthStencil.depth=1;
    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; info.renderPass=pass; info.framebuffer=framebuffer;
    info.renderArea.extent={options.width,options.height}; info.clearValueCount=uint32_t(clears.size()); info.pClearValues=clears.data();
    vkCmdBeginRenderPass(command,&info,VK_SUBPASS_CONTENTS_INLINE);
}
void Benchmark::geometryDraw() {
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,geometryPipeline);
    VkDeviceSize offset=0; vkCmdBindVertexBuffers(command,0,1,&vertices.buffer,&offset);
    float aspect=float(options.width)/float(options.height);
    vkCmdPushConstants(command,sampledPipelineLayout,VK_SHADER_STAGE_VERTEX_BIT,0,4,&aspect);
    vkCmdDraw(command,vertexCount,1,0,0); ++geometryDraws;
}
void Benchmark::fullscreenDraw(VkPipeline pipeline,VkPipelineLayout layout,VkDescriptorSet set) {
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&set,0,nullptr);
    vkCmdDraw(command,3,1,0,0);
}
void Benchmark::record() {
    if(options.path=="drlr") {
        recordDynamic();
        beginPass(contourPass,contourFramebuffer,1,false);
        fullscreenDraw(contourPipeline,sampledPipelineLayout,contourSet); ++contourDraws;
        vkCmdEndRenderPass(command);
        return;
    }
    if(options.path=="subpasses") {
        beginPass(geometryPass,geometryFramebuffer,3,true); geometryDraw();
        vkCmdNextSubpass(command,VK_SUBPASS_CONTENTS_INLINE); ++nextSubpassCalls;
        fullscreenDraw(toonPipeline,inputPipelineLayout,inputSet); ++toonDraws;
        vkCmdEndRenderPass(command);
        beginPass(contourPass,contourFramebuffer,1,false);
        fullscreenDraw(contourPipeline,sampledPipelineLayout,contourSet); ++contourDraws;
        vkCmdEndRenderPass(command);
        return;
    }
    beginPass(geometryPass,geometryFramebuffer,2,true); geometryDraw(); vkCmdEndRenderPass(command);
    beginPass(toonPass,toonFramebuffer,1,false); fullscreenDraw(toonPipeline,sampledPipelineLayout,sampledSet); ++toonDraws; vkCmdEndRenderPass(command);
    beginPass(contourPass,contourFramebuffer,1,false); fullscreenDraw(contourPipeline,sampledPipelineLayout,contourSet); ++contourDraws; vkCmdEndRenderPass(command);
}
void Benchmark::submit() {
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&command;
    check(vkQueueSubmit(queue,1,&si,fence),"queue submit");
    check(vkWaitForFences(device,1,&fence,VK_TRUE,30'000'000'000ull),"frame completion"); check(vkResetFences(device,1,&fence),"reset fence");
}
void Benchmark::run() {
    if(options.path!="traditional" && options.path!="subpasses" && options.path!="drlr") throw std::runtime_error("path not implemented");
    Capture capture(options.captureDll,options.capturePrefix);
    initialize();
    for(uint32_t frame=0;frame<options.frames+options.warmup;++frame) {
        if(frame==0) capture.begin();
        check(vkResetCommandPool(device,commandPool,0),"reset command pool");
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(command,&bi),"begin commands");
        vkCmdResetQueryPool(command,queries,0,2);
        vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,queries,0);
        auto start=std::chrono::steady_clock::now(); record(); auto end=std::chrono::steady_clock::now();
        vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,queries,1);
        check(vkEndCommandBuffer(command),"end commands"); submit();
        if(frame==0) capture.end();
        uint64_t values[2]{}; check(vkGetQueryPoolResults(device,queries,0,2,sizeof(values),values,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"timestamp results");
        uint64_t mask=timestampBits==64?~uint64_t(0):(uint64_t(1)<<timestampBits)-1;
        double gpu=double((values[1]-values[0])&mask)*double(properties.limits.timestampPeriod)/1e6;
        if(frame>=options.warmup) { gpuMs.push_back(gpu); cpuMs.push_back(std::chrono::duration<double,std::milli>(end-start).count()); }
        if(window) { double ms=window->present(queue,output.image,options.width,options.height); if(frame>=options.warmup)presentationMs.push_back(ms); }
    }
    save();
    if(errors) throw std::runtime_error("validation errors occurred");
}
void Benchmark::save() {
    check(vkResetCommandPool(device,commandPool,0),"reset for readback");
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(command,&bi),"begin readback");
    VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={options.width,options.height,1};
    vkCmdCopyImageToBuffer(command,output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; barrier.buffer=readback.buffer; barrier.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    check(vkEndCommandBuffer(command),"end readback"); submit();
    std::filesystem::create_directories(options.output);
    void* data{}; check(vkMapMemory(device,readback.memory,0,readback.size,0,&data),"map readback");
    std::ofstream image(options.output/"image.rgba",std::ios::binary); image.write(static_cast<const char*>(data),std::streamsize(readback.size));
    std::ofstream ppm(options.output/"image.ppm",std::ios::binary); ppm<<"P6\n"<<options.width<<' '<<options.height<<"\n255\n";
    for(size_t i=0;i<size_t(readback.size);i+=4) ppm.write(static_cast<const char*>(data)+i,3);
    vkUnmapMemory(device,readback.memory);
    std::ofstream json(options.output/"run.json"); json<<std::setprecision(12);
    json<<"{\"schemaVersion\":1,\"path\":"<<std::quoted(options.path)<<",\"device\":"<<std::quoted(properties.deviceName)
        <<",\"apiVersion\":"<<properties.apiVersion<<",\"driverVersion\":"<<properties.driverVersion
        <<",\"width\":"<<options.width<<",\"height\":"<<options.height<<",\"complexity\":"<<std::quoted(options.complexity)
        <<",\"triangles\":"<<vertexCount/3<<",\"warmup\":"<<options.warmup<<",\"queueFamily\":"<<queueFamily
        <<",\"timestampValidBits\":"<<timestampBits<<",\"timestampPeriodNs\":"<<properties.limits.timestampPeriod
        <<",\"validationEnabled\":"<<(options.validation?"true":"false")<<",\"synchronizationValidation\":"<<(options.validation?"true":"false")
        <<",\"validationErrors\":"<<errors.load()<<",\"validationWarnings\":"<<warnings.load()
        <<",\"geometryDraws\":"<<geometryDraws<<",\"toonDraws\":"<<toonDraws<<",\"contourDraws\":"<<contourDraws
        <<",\"nextSubpassCalls\":"<<nextSubpassCalls<<",\"beginRenderingCalls\":"<<beginRenderingCalls
        <<",\"dynamicRenderingLocalReadEnabled\":"<<(options.path=="drlr"?"true":"false")
        <<",\"captureEnabled\":"<<(options.capturePrefix.empty()?"false":"true")
        <<",\"measurementScope\":\"gbuffer-through-contour; readback-excluded\",\"status\":\"pilot-environment-not-qualified\",\"gpuMs\":[";
    for(size_t i=0;i<gpuMs.size();++i) { if(i) json<<','; json<<gpuMs[i]; }
    json<<"],\"cpuRecordingMs\":["; for(size_t i=0;i<cpuMs.size();++i) { if(i) json<<','; json<<cpuMs[i]; }
    json<<"],\"presentationWorkMs\":[";
    for(size_t i=0;i<presentationMs.size();++i) { if(i)json<<',';json<<presentationMs[i]; }
    json<<"]}\n";
    if(!image || !ppm || !json) throw std::runtime_error("result write failed");
}
Benchmark::~Benchmark() {
    if(device) {
        vkDeviceWaitIdle(device);
        window.reset();
        for(auto p:{geometryPipeline,toonPipeline,contourPipeline}) if(p) vkDestroyPipeline(device,p,nullptr);
        for(auto f:{geometryFramebuffer,toonFramebuffer,contourFramebuffer}) if(f) vkDestroyFramebuffer(device,f,nullptr);
        for(auto p:{geometryPass,toonPass,contourPass}) if(p) vkDestroyRenderPass(device,p,nullptr);
        for(auto m:modules) vkDestroyShaderModule(device,m,nullptr);
        if(descriptorPool) vkDestroyDescriptorPool(device,descriptorPool,nullptr);
        if(sampler) vkDestroySampler(device,sampler,nullptr);
        for(auto l:{sampledPipelineLayout,inputPipelineLayout}) if(l) vkDestroyPipelineLayout(device,l,nullptr);
        for(auto l:{sampledLayout,inputLayout}) if(l) vkDestroyDescriptorSetLayout(device,l,nullptr);
        for(auto b:{vertices,readback}) { if(b.buffer) vkDestroyBuffer(device,b.buffer,nullptr); if(b.memory) vkFreeMemory(device,b.memory,nullptr); }
        for(auto i:{albedo,normal,depth,toon,output}) { if(i.view) vkDestroyImageView(device,i.view,nullptr); if(i.image) vkDestroyImage(device,i.image,nullptr); if(i.memory) vkFreeMemory(device,i.memory,nullptr); }
        if(queries) vkDestroyQueryPool(device,queries,nullptr);
        if(fence) vkDestroyFence(device,fence,nullptr);
        if(commandPool) vkDestroyCommandPool(device,commandPool,nullptr);
        vkDestroyDevice(device,nullptr);
    }
    if(messenger) {
        auto destroy=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"));
        if(destroy) destroy(instance,messenger,nullptr);
    }
    window.reset();
    if(instance) vkDestroyInstance(instance,nullptr);
}
}

int main(int argc,char** argv) {
    try {
        thesis::Options o;
        for(int i=1;i<argc;++i) {
            std::string key=argv[i]; if(key=="--validation") { o.validation=true; continue; }
            if(key=="--present") { o.present=true;continue; }
            if(i+1==argc) throw std::invalid_argument("missing option value"); std::string value=argv[++i];
            auto number=[&]() { size_t used=0; auto n=std::stoul(value,&used); if(used!=value.size() || n>1000000) throw std::invalid_argument("invalid integer"); return uint32_t(n); };
            if(key=="--path") o.path=value; else if(key=="--gpu") o.gpu=value; else if(key=="--complexity") o.complexity=value;
            else if(key=="--capture") o.capturePrefix=value; else if(key=="--renderdoc") o.captureDll=value;
            else if(key=="--shaders") o.shaders=value; else if(key=="--output") o.output=value;
            else if(key=="--width") o.width=number(); else if(key=="--height") o.height=number(); else if(key=="--frames") o.frames=number(); else if(key=="--warmup") o.warmup=number();
            else throw std::invalid_argument("unknown option: "+key);
        }
        if(o.output.empty() || o.shaders.empty() || !o.width || !o.height || o.width>8192 || o.height>8192 || !o.frames || (o.complexity!="low" && o.complexity!="high")) throw std::invalid_argument("invalid workload/output configuration");
        thesis::Benchmark b(o); b.run(); return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
