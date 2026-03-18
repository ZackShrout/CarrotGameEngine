//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanTexturedQuadPipeline.h"

#include "Assets/Shaders/ShaderFileProvider.h"
#include "Renderer/Primitives/QuadVertex.h"
#include "Renderer/Primitives/TexturedQuadPushConstants.h"
#include "Utils/File/FileUtils.h"

#include <fstream>

namespace carrot::rhi::vulkan {
    namespace {
        std::vector<uint32_t> load_spv(const std::filesystem::path& path)
        {
            std::ifstream file{ path, std::ios::ate | std::ios::binary };
            if (!file.is_open())
            {
                LOG_GRAPHICS_FATAL("Failed to open SPIR-V file: {}", utils::file::to_log_string(path));
                throw std::runtime_error("Failed to open shader file");
            }

            const std::streampos file_size{ file.tellg() };
            if (file_size < 0)
            {
                LOG_GRAPHICS_FATAL("Invalid file size for {}", utils::file::to_log_string(path));
                throw std::runtime_error("Invalid shader file size");
            }

            if ((file_size % 4) != 0)
            {
                LOG_GRAPHICS_FATAL("SPIR-V file size not multiple of 4 bytes: {} bytes ({})",
                                   static_cast<uint64_t>(file_size), utils::file::to_log_string(path));
                throw std::runtime_error("Invalid SPIR-V size");
            }

            std::vector<uint32_t> buffer(static_cast<size_t>(file_size / 4));

            file.seekg(0);
            file.read(reinterpret_cast<char*>(buffer.data()), file_size);

            if (!file)
            {
                LOG_GRAPHICS_FATAL("Failed to read SPIR-V file: {}", utils::file::to_log_string(path));
                throw std::runtime_error("Failed to read shader file");
            }

            LOG_GRAPHICS_INFO("Loaded SPIR-V {}: {} words ({} bytes)",
                              utils::file::to_log_string(path), buffer.size(), static_cast<uint64_t>(file_size));

            return buffer;
        }

        [[nodiscard]] VkVertexInputBindingDescription make_vertex_binding_description() noexcept
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(carrot::renderer::quad_vertex_t);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        [[nodiscard]] std::array<VkVertexInputAttributeDescription, 3> make_vertex_attribute_descriptions() noexcept
        {
            std::array<VkVertexInputAttributeDescription, 3> attributes{};

            // float x, y
            attributes[0].location = 0;
            attributes[0].binding = 0;
            attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[0].offset = offsetof(carrot::renderer::quad_vertex_t, x);

            // float u, v
            attributes[1].location = 1;
            attributes[1].binding = 0;
            attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[1].offset = offsetof(carrot::renderer::quad_vertex_t, u);

            // packed ABGR color
            attributes[2].location = 2;
            attributes[2].binding = 0;
            attributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
            attributes[2].offset = offsetof(carrot::renderer::quad_vertex_t, color);

            return attributes;
        }
    } // namespace

    vulkan_textured_quad_pipeline_t::vulkan_textured_quad_pipeline_t(const vulkan_device_t* device, VkRenderPass render_pass, assets::shader_file_provider_t* shader_files) : _device{ device }
    {
        const auto vert_path{ shader_files->resolve("engine://shaders/vulkan/textured_quad.vert.spv") };
        const auto frag_path{ shader_files->resolve("engine://shaders/vulkan/textured_quad.frag.spv") };

        if (!vert_path || !frag_path)
        {
            LOG_GRAPHICS_FATAL("Failed to resolve shader paths");
            return;
        }

        std::vector<uint32_t> vert_code{ load_spv(*vert_path) };
        std::vector<uint32_t> frag_code{ load_spv(*frag_path) };

        const VkShaderModule vert_module = create_shader_module(vert_code);
        const VkShaderModule frag_module = create_shader_module(frag_code);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_module;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag_module;
        stages[1].pName = "main";

        const VkVertexInputBindingDescription binding_desc = make_vertex_binding_description();
        const auto attribute_descs = make_vertex_attribute_descriptions();

        VkPipelineVertexInputStateCreateInfo vertex_input{};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.vertexBindingDescriptionCount = 1;
        vertex_input.pVertexBindingDescriptions = &binding_desc;
        vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descs.size());
        vertex_input.pVertexAttributeDescriptions = attribute_descs.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = 2;
        dynamic_state.pDynamicStates = dynamic_states;

        VkDescriptorSetLayoutBinding texture_binding{};
        texture_binding.binding = 0;
        texture_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texture_binding.descriptorCount = 1;
        texture_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        texture_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
        descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_info.bindingCount = 1;
        descriptor_set_layout_info.pBindings = &texture_binding;

        VK_CHECK_FATAL(vkCreateDescriptorSetLayout(
            _device->vk_device(),
            &descriptor_set_layout_info,
            nullptr,
            &_descriptor_set_layout
        ));

        VkPushConstantRange push_range{};
        push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_range.offset = 0;
        push_range.size = sizeof(textured_quad_push_constants_t);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &_descriptor_set_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_range;

        VK_CHECK_FATAL(vkCreatePipelineLayout(
            _device->vk_device(),
            &layout_info,
            nullptr,
            &_layout
        ));

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = nullptr;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = _layout;
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

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

        LOG_GRAPHICS_INFO("Textured quad pipeline created successfully");
    }

    vulkan_textured_quad_pipeline_t::~vulkan_textured_quad_pipeline_t()
    {
        if (_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(_device->vk_device(), _pipeline, nullptr);

        if (_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(_device->vk_device(), _layout, nullptr);

        if (_descriptor_set_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(_device->vk_device(), _descriptor_set_layout, nullptr);
    }

    VkShaderModule vulkan_textured_quad_pipeline_t::create_shader_module(const std::vector<uint32_t>& code) const
    {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size() * sizeof(uint32_t);
        create_info.pCode = code.data();

        VkShaderModule module{ VK_NULL_HANDLE };
        VK_CHECK_FATAL(vkCreateShaderModule(_device->vk_device(), &create_info, nullptr, &module));
        return module;
    }
} // namespace carrot::rhi::vulkan
