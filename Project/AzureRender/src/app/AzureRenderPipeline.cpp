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



void AzureRenderApp::createGraphicsPipeline() {
    const std::string shaderDirectory = AZURERENDER_SHADER_DIR;
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
    const auto hudVertexCode =
        readBinaryFile(shaderDirectory + "/hud.vert.spv");
    const auto hudFragmentCode =
        readBinaryFile(shaderDirectory + "/hud.frag.spv");
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
    const VkShaderModule hudVertexModule =
        createShaderModule(hudVertexCode);
    const VkShaderModule hudFragmentModule =
        createShaderModule(hudFragmentCode);

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
        VkPipelineShaderStageCreateInfo hudVertexStage = vertexStage;
        hudVertexStage.module = hudVertexModule;
        VkPipelineShaderStageCreateInfo hudFragmentStage = fragmentStage;
        hudFragmentStage.module = hudFragmentModule;
        const std::array hudShaderStages = {
            hudVertexStage,
            hudFragmentStage,
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
    vkDestroyShaderModule(device_, shadowFragmentModule, nullptr);
    vkDestroyShaderModule(device_, shadowVertexModule, nullptr);
    vkDestroyShaderModule(device_, backgroundFragmentModule, nullptr);
    vkDestroyShaderModule(device_, backgroundVertexModule, nullptr);
    vkDestroyShaderModule(device_, outlineFragmentModule, nullptr);
    vkDestroyShaderModule(device_, outlineVertexModule, nullptr);
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
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
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        vkCheck(
            vkCreateFramebuffer(device_, &createInfo, nullptr, &swapchainFramebuffers_[index]),
            "vkCreateFramebuffer");
    }
}

void AzureRenderApp::createPostProcessFramebuffers() {
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
