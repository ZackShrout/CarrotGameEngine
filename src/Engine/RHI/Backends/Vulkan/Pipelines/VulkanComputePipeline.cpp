//
// Created by Zack Shrout on 4/18/2026.
//

#include "Core/Pch.h"

#include "VulkanComputePipeline.h"

#include "Assets/Shaders/ShaderFileProvider.h"
#include "RHI/Backends/Vulkan/VulkanDevice.h"
#include "RHI/Backends/Vulkan/VulkanUtils.h"

namespace carrot::rhi::vulkan {
    vulkan_compute_pipeline_t::vulkan_compute_pipeline_t(const vulkan_device_t* device,
                                                         assets::shader_file_provider_t& shader_files,
                                                         const compute_pipeline_create_info_t& info)
        : rhi_compute_pipeline_t{ info }, _device{ device }
    {
        if (!_device)
        {
            LOG_GRAPHICS_FATAL("Vulkan compute pipeline created with null device");
            return;
        }

        std::array<VkDescriptorSetLayoutBinding, k_max_compute_storage_buffer_bindings> bindings{ };
        for (std::uint32_t i{ 0u }; i < k_max_compute_storage_buffer_bindings; ++i)
        {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layout_info{ };
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();
        VK_CHECK_FATAL(vkCreateDescriptorSetLayout(_device->vk_device(), &layout_info, nullptr, &_descriptor_set_layout));

        VkPushConstantRange push_constant_range{ };
        push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant_range.offset = 0u;
        push_constant_range.size = info.max_constant_size_bytes;

        VkPipelineLayoutCreateInfo pipeline_layout_info{ };
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &_descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = info.max_constant_size_bytes > 0u ? 1u : 0u;
        pipeline_layout_info.pPushConstantRanges =
            info.max_constant_size_bytes > 0u ? &push_constant_range : nullptr;
        VK_CHECK_FATAL(vkCreatePipelineLayout(_device->vk_device(), &pipeline_layout_info, nullptr, &_layout));

        const auto shader_path{ shader_files.resolve(info.shader_path) };
        if (!shader_path)
            LOG_GRAPHICS_FATAL("Failed to resolve Vulkan compute shader path");

        const std::vector<std::uint32_t> shader_code{ *load_spv_file(*shader_path) };
        const VkShaderModule shader_module{ create_shader_module(shader_code) };

        VkPipelineShaderStageCreateInfo stage_info{ };
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = "main";

        VkComputePipelineCreateInfo pipeline_info{ };
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = _layout;

        VK_CHECK_FATAL(vkCreateComputePipelines(_device->vk_device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                                &_pipeline));
        vkDestroyShaderModule(_device->vk_device(), shader_module, nullptr);
    }

    vulkan_compute_pipeline_t::~vulkan_compute_pipeline_t()
    {
        if (_pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(_device->vk_device(), _pipeline, nullptr);
        if (_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(_device->vk_device(), _layout, nullptr);
        if (_descriptor_set_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(_device->vk_device(), _descriptor_set_layout, nullptr);
    }

    VkShaderModule vulkan_compute_pipeline_t::create_shader_module(const std::vector<std::uint32_t>& code) const
    {
        VkShaderModuleCreateInfo create_info{ };
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size() * sizeof(std::uint32_t);
        create_info.pCode = code.data();

        VkShaderModule module{ VK_NULL_HANDLE };
        VK_CHECK_FATAL(vkCreateShaderModule(_device->vk_device(), &create_info, nullptr, &module));
        return module;
    }
} // namespace carrot::rhi::vulkan
