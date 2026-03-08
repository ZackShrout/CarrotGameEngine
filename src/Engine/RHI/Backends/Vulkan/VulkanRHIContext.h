//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"
#include "VulkanCore.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanCommandQueue.h"
#include "RHI/RHI.h"

namespace carrot::rhi::vulkan {
    class vulkan_pipeline_t;
    class vulkan_render_pass_t;

    class vulkan_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit vulkan_rhi_context_t(const rhi_desc_t& desc);
        ~vulkan_rhi_context_t() override;

        void begin_frame() override;
        void record_frame() override;
        void end_frame() override;

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override { return _device.get(); }
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override { return _swapchain.get();}
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override { return _graphics_queue.get(); }

        void wait_idle() override;

    private:
        void init(const rhi_desc_t& desc);
        void recreate_swapchain_dependent_resources();
        void recreate_render_finished_semaphores();

        VkInstance                                              _vk_instance{ VK_NULL_HANDLE };
        VkSurfaceKHR                                            _vk_surface{ VK_NULL_HANDLE };
        VkCommandPool                                           _command_pool{ VK_NULL_HANDLE };
        std::unique_ptr<vulkan_device_t>                        _device;
        std::unique_ptr<vulkan_swapchain_t>                     _swapchain;
        std::unique_ptr<vulkan_command_queue_t>                 _graphics_queue;
        std::unique_ptr<vulkan_render_pass_t>                   _render_pass;
        std::unique_ptr<vulkan_pipeline_t>                      _graphics_pipeline;
        framebuffer_array_t                                     _framebuffers;
        std::array<frame_resources_t, k_max_frames_in_flight>   _frames;
        // std::vector<VkSemaphore>                                _image_available_semaphores;
        std::vector<VkSemaphore>                                _render_finished_semaphores;
        uint32_t                                                _frame_counter{ 0 };
        uint32_t                                                _current_frame{ 0 };
        uint32_t                                                _current_image_index{ 0 };
        bool                                                    _pending_pipeline_reload{ false };

        bool _frame_active{ false };
        bool _render_pass_active{ false };
        bool _skip_frame{ false };
        bool _swapchain_dirty{ false };
        uint32_t _pending_resize_width{ 0 };
        uint32_t _pending_resize_height{ 0 };
    };

} // namespace carrot::rhi::vulkan
