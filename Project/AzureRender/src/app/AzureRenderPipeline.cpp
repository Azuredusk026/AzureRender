#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

using namespace azurerender::internal;

void AzureRenderApp::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = kHdrSceneColorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

void AzureRenderApp::createPostProcessRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = editorUiEnabled_
        ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo createInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount =
        static_cast<std::uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    vkCheck(
        vkCreateRenderPass(
            device_,
            &createInfo,
            nullptr,
            &postProcessRenderPass_),
        "vkCreateRenderPass(post process)");
}

void AzureRenderApp::createEditorUiRenderPass() {
    if (!editorUiEnabled_) {
        return;
    }
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
            device_, &createInfo, nullptr, &editorUiRenderPass_),
        "vkCreateRenderPass(editor UI)");
}



void AzureRenderApp::createGraphicsPipeline() {
    const std::string shaderDirectory = resourceLocator_.shaderDirectory().string();
    const auto innerOutlineVertexCode =
        readBinaryFile(shaderDirectory + "/inner_outline.vert.spv");
    const auto innerOutlineFragmentCode =
        readBinaryFile(shaderDirectory + "/inner_outline.frag.spv");
    const auto hudVertexCode =
        readBinaryFile(shaderDirectory + "/hud.vert.spv");
    const auto hudFragmentCode =
        readBinaryFile(shaderDirectory + "/hud.frag.spv");
    const VkShaderModule innerOutlineVertexModule =
        createShaderModule(innerOutlineVertexCode);
    const VkShaderModule innerOutlineFragmentModule =
        createShaderModule(innerOutlineFragmentCode);
    const VkShaderModule hudVertexModule =
        createShaderModule(hudVertexCode);
    const VkShaderModule hudFragmentModule =
        createShaderModule(hudFragmentCode);

    try {
        VkPipelineShaderStageCreateInfo vertexStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = innerOutlineVertexModule;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragmentStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = innerOutlineFragmentModule;
        fragmentStage.pName = "main";
        const std::array innerOutlineShaderStages = {
            vertexStage,
            fragmentStage,
        };
        VkPipelineShaderStageCreateInfo hudVertexStage = vertexStage;
        hudVertexStage.module = hudVertexModule;
        VkPipelineShaderStageCreateInfo hudFragmentStage = fragmentStage;
        hudFragmentStage.module = hudFragmentModule;
        const std::array hudShaderStages = {
            hudVertexStage,
            hudFragmentStage,
        };

        VkPipelineVertexInputStateCreateInfo emptyVertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth = 1.0F;
        VkPipelineMultisampleStateCreateInfo multisampling{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depthStencil{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        VkPipelineColorBlendAttachmentState postBlendAttachment{};
        postBlendAttachment.blendEnable = VK_FALSE;
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
        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &postBlendAttachment;
        const std::array dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount =
            static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

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
        VkPipelineLayoutCreateInfo hudLayoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        vkCheck(
            vkCreatePipelineLayout(
                device_,
                &hudLayoutInfo,
                nullptr,
                &hudPipelineLayout_),
            "vkCreatePipelineLayout(HUD)");

        VkGraphicsPipelineCreateInfo pipelineInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = static_cast<std::uint32_t>(
            innerOutlineShaderStages.size());
        pipelineInfo.pStages = innerOutlineShaderStages.data();
        pipelineInfo.pVertexInputState = &emptyVertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = postProcessPipelineLayout_;
        pipelineInfo.renderPass = postProcessRenderPass_;
        pipelineInfo.subpass = 0;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &innerOutlinePipeline_),
            "vkCreateGraphicsPipelines(inner outline)");

        VkVertexInputBindingDescription hudBinding{};
        hudBinding.binding = 0;
        hudBinding.stride = sizeof(HudVertex);
        hudBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        const std::array<VkVertexInputAttributeDescription, 2>
            hudAttributes = {{
                {
                    0,
                    0,
                    VK_FORMAT_R32G32_SFLOAT,
                    offsetof(HudVertex, position),
                },
                {
                    1,
                    0,
                    VK_FORMAT_R8G8B8A8_UNORM,
                    offsetof(HudVertex, color),
                },
            }};
        VkPipelineVertexInputStateCreateInfo hudVertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        hudVertexInput.vertexBindingDescriptionCount = 1;
        hudVertexInput.pVertexBindingDescriptions = &hudBinding;
        hudVertexInput.vertexAttributeDescriptionCount =
            static_cast<std::uint32_t>(hudAttributes.size());
        hudVertexInput.pVertexAttributeDescriptions =
            hudAttributes.data();
        postBlendAttachment.blendEnable = VK_TRUE;
        pipelineInfo.pStages = hudShaderStages.data();
        pipelineInfo.pVertexInputState = &hudVertexInput;
        pipelineInfo.layout = hudPipelineLayout_;
        vkCheck(
            vkCreateGraphicsPipelines(
                device_,
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &hudPipeline_),
            "vkCreateGraphicsPipelines(HUD)");
    } catch (...) {
        vkDestroyShaderModule(device_, hudFragmentModule, nullptr);
        vkDestroyShaderModule(device_, hudVertexModule, nullptr);
        vkDestroyShaderModule(
            device_,
            innerOutlineFragmentModule,
            nullptr);
        vkDestroyShaderModule(
            device_,
            innerOutlineVertexModule,
            nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, hudFragmentModule, nullptr);
    vkDestroyShaderModule(device_, hudVertexModule, nullptr);
    vkDestroyShaderModule(
        device_,
        innerOutlineFragmentModule,
        nullptr);
    vkDestroyShaderModule(
        device_,
        innerOutlineVertexModule,
        nullptr);
}

void AzureRenderApp::createFramebuffers() {
    swapchainFramebuffers_.resize(swapchainImageViews_.size());
    for (std::size_t index = 0; index < swapchainImageViews_.size(); ++index) {
        const std::array attachments = {
            sceneColorImageViews_[index],
            depthImageViews_[index],
            normalImageViews_[index],
        };
        VkFramebufferCreateInfo createInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = renderPass_;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = renderExtent_.width;
        createInfo.height = renderExtent_.height;
        createInfo.layers = 1;
        vkCheck(
            vkCreateFramebuffer(device_, &createInfo, nullptr, &swapchainFramebuffers_[index]),
            "vkCreateFramebuffer");
    }
}

void AzureRenderApp::createPostProcessFramebuffers() {
    const std::vector<VkImageView>& targetViews = editorUiEnabled_
        ? editorViewportImageViews_
        : swapchainImageViews_;
    postProcessFramebuffers_.resize(targetViews.size());
    for (std::size_t index = 0;
         index < targetViews.size();
         ++index) {
        VkFramebufferCreateInfo createInfo{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = postProcessRenderPass_;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &targetViews[index];
        createInfo.width = renderExtent_.width;
        createInfo.height = renderExtent_.height;
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

void AzureRenderApp::createEditorUiFramebuffers() {
    if (!editorUiEnabled_) {
        return;
    }
    editorUiFramebuffers_.resize(swapchainImageViews_.size());
    for (std::size_t index = 0;
         index < swapchainImageViews_.size(); ++index) {
        VkFramebufferCreateInfo createInfo{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        createInfo.renderPass = editorUiRenderPass_;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &swapchainImageViews_[index];
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        vkCheck(
            vkCreateFramebuffer(
                device_, &createInfo, nullptr, &editorUiFramebuffers_[index]),
            "vkCreateFramebuffer(editor UI)");
    }
}
