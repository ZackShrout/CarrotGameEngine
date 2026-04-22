//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Backends/Vulkan/VulkanCommon.h"
#include "RHI/Backends/Vulkan/VulkanDevice.h"

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::rhi::vulkan {
    class vulkan_textured_quad_pipeline_t final
    {
    public:
        enum class blend_mode_t : std::uint8_t
        {
            alpha = 0,
            additive
        };

        vulkan_textured_quad_pipeline_t(const vulkan_device_t* device, VkRenderPass render_pass,
                                        assets::shader_file_provider_t* shader_files,
                                        std::string_view vertex_shader_path,
                                        std::string_view fragment_shader_path,
                                        std::string_view debug_name,
                                        bool instanced = false,
                                        blend_mode_t blend_mode = blend_mode_t::alpha);
        ~vulkan_textured_quad_pipeline_t();

        [[nodiscard]] VkPipeline vk_pipeline() const noexcept { return _pipeline; }
        [[nodiscard]] VkPipelineLayout vk_layout() const noexcept { return _layout; }
        [[nodiscard]] VkDescriptorSetLayout vk_camera_descriptor_set_layout() const noexcept;
        [[nodiscard]] VkDescriptorSetLayout vk_texture_descriptor_set_layout() const noexcept;

    private:
        [[nodiscard]] VkShaderModule create_shader_module(const std::vector<uint32_t>& code) const;

        const vulkan_device_t* _device{ nullptr };
        VkDescriptorSetLayout _camera_descriptor_set_layout{ VK_NULL_HANDLE };
        VkDescriptorSetLayout _texture_descriptor_set_layout{ VK_NULL_HANDLE };
        VkPipelineLayout _layout{ VK_NULL_HANDLE };
        VkPipeline _pipeline{ VK_NULL_HANDLE };
    };
}
