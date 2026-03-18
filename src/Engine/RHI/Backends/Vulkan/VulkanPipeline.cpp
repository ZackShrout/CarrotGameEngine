//
// Created by zshrout on 1/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanPipeline.h"

#include "VulkanUtils.h"
#include "Assets/Shaders/ShaderFileProvider.h"
#include "Utils/File/FileUtils.h"

#include <fstream>

namespace carrot::rhi::vulkan {
    // PUBLIC
    vulkan_pipeline_t::vulkan_pipeline_t(const vulkan_device_t* device, VkRenderPass render_pass,
                                         assets::shader_file_provider_t* shader_files) : _device{ device }
    {
        const auto vert_path{ shader_files->resolve("engine://shaders/vulkan/triangle.vert.spv") };
        const auto frag_path{ shader_files->resolve("engine://shaders/vulkan/triangle.frag.spv") };

        if (!vert_path || !frag_path)
        {
            LOG_GRAPHICS_FATAL("Failed to resolve shader paths");
            return;
        }

        std::vector<uint32_t> vert_code{ *load_spv_file(*vert_path) };
        std::vector<uint32_t> frag_code{ *load_spv_file(*frag_path) };

        VkShaderModule vert_module = create_shader_module(vert_code);
        VkShaderModule frag_module = create_shader_module(frag_code);

        VkPipelineShaderStageCreateInfo stages[2]{ };
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_module;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag_module;
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input{ };
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo input_assembly{ };
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport_state{ };
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = nullptr;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = nullptr;

        VkPipelineRasterizationStateCreateInfo rasterizer{ };
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{ };
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attachment{ };
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending{ };
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state{ };
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = 2;
        dynamic_state.pDynamicStates = dynamic_states;

        // Pipeline layout with push constant support
        VkPushConstantRange push_range{ };
        push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // only vertex uses it
        push_range.offset = 0;
        push_range.size = sizeof(uint32_t); // exactly 4 bytes for uint frameCount

        VkPipelineLayoutCreateInfo layout_info{ };
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_range;

        VK_CHECK_FATAL(vkCreatePipelineLayout(
            _device->vk_device(),
            &layout_info,
            nullptr,
            &_layout
        ));

        VkGraphicsPipelineCreateInfo pipeline_info{ };
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = _layout;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;

        VK_CHECK_FATAL(vkCreateGraphicsPipelines(
            _device->vk_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &_pipeline
        ));

        vkDestroyShaderModule(_device->vk_device(), vert_module, nullptr);
        vkDestroyShaderModule(_device->vk_device(), frag_module, nullptr);

        LOG_GRAPHICS_INFO("Triangle pipeline created successfully");
    }

    vulkan_pipeline_t::~vulkan_pipeline_t()
    {
        if (_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(_device->vk_device(), _pipeline, nullptr);
        if (_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(_device->vk_device(), _layout, nullptr);
    }

    // PRIVATE
    VkShaderModule vulkan_pipeline_t::create_shader_module(const std::vector<uint32_t>& code) const
    {
        VkShaderModuleCreateInfo create_info{ };
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size() * sizeof(uint32_t);
        create_info.pCode = code.data();

        VkShaderModule module;
        VK_CHECK_FATAL(vkCreateShaderModule(_device->vk_device(), &create_info, nullptr, &module));
        return module;
    }
} // namespace carrot::rhi::vulkan
