//
// Created by Zack Shrout on 4/18/2026.
//

#pragma once

#include "RHI/Backends/Vulkan/VulkanCommon.h"
#include "RHI/Pipeline.h"

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::rhi::vulkan {
    class vulkan_device_t;

    class vulkan_compute_pipeline_t final : public rhi_compute_pipeline_t
    {
    public:
        vulkan_compute_pipeline_t(const vulkan_device_t* device,
                                  assets::shader_file_provider_t& shader_files,
                                  const compute_pipeline_create_info_t& info);
        ~vulkan_compute_pipeline_t() override;

        [[nodiscard]] bool is_valid() const noexcept
        {
            return _descriptor_set_layout != VK_NULL_HANDLE &&
                   _layout != VK_NULL_HANDLE &&
                   _pipeline != VK_NULL_HANDLE;
        }

        [[nodiscard]] VkDescriptorSetLayout descriptor_set_layout() const noexcept { return _descriptor_set_layout; }
        [[nodiscard]] VkPipelineLayout layout() const noexcept { return _layout; }
        [[nodiscard]] VkPipeline pipeline() const noexcept { return _pipeline; }

    private:
        [[nodiscard]] VkShaderModule create_shader_module(const std::vector<std::uint32_t>& code) const;

        const vulkan_device_t* _device{ nullptr };
        VkDescriptorSetLayout _descriptor_set_layout{ VK_NULL_HANDLE };
        VkPipelineLayout _layout{ VK_NULL_HANDLE };
        VkPipeline _pipeline{ VK_NULL_HANDLE };
    };
} // namespace carrot::rhi::vulkan
