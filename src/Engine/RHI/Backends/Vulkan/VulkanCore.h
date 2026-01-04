//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"

#include <array>

namespace carrot::rhi::vulkan {
    constexpr uint32_t k_max_frames_in_flight{ 2 };

    struct frame_resources_t
    {
        VkCommandBuffer command_buffer{ VK_NULL_HANDLE };
        VkSemaphore image_available{ VK_NULL_HANDLE };
        VkSemaphore render_finished{ VK_NULL_HANDLE };
        VkFence in_flight{ VK_NULL_HANDLE };

        // Note: potential future per-frame additions...
        // VkDescriptorSet descriptor_set{ VK_NULL_HANDLE };
        // void*           mapped_uniform_buffer{ nullptr };
    };

    // ─────────────────────────────────────────────────────────────────────────────────────────────────
    // Note: the next several structs are RAII wrappers designed to destroy their resource on scope exit
    // ─────────────────────────────────────────────────────────────────────────────────────────────────
    struct pipeline_t
    {
        VkDevice device{ VK_NULL_HANDLE };
        VkPipeline pipeline{ VK_NULL_HANDLE };

        pipeline_t() = default;
        pipeline_t(VkDevice dev, VkPipeline pipe) : device{ dev }, pipeline{ pipe } {}

        ~pipeline_t() { if (pipeline) vkDestroyPipeline(device, pipeline, nullptr); }

        // Move allowed, copy disallowed
        DISABLE_COPY(pipeline_t)
        pipeline_t(pipeline_t&& other) noexcept { *this = std::move(other); }

        pipeline_t& operator=(pipeline_t&& other) noexcept
        {
            if (this != &other)
            {
                if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
                device = other.device;
                pipeline = other.pipeline;
                other.device = VK_NULL_HANDLE;
                other.pipeline = VK_NULL_HANDLE;
            }
            return *this;
        }

        explicit operator VkPipeline() const { return pipeline; }
    };

    struct pipeline_layout_t
    {
        VkDevice device = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;

        pipeline_layout_t() = default;
        pipeline_layout_t(VkDevice dev, VkPipelineLayout pipe_layout) : device{ dev }, layout{ pipe_layout } {}

        ~pipeline_layout_t() { if (layout) vkDestroyPipelineLayout(device, layout, nullptr); }

        // Move allowed, copy disallowed
        DISABLE_COPY(pipeline_layout_t)
        pipeline_layout_t(pipeline_layout_t&& other) noexcept { *this = std::move(other); }

        pipeline_layout_t& operator=(pipeline_layout_t&& other) noexcept
        {
            if (this != &other)
            {
                if (layout) vkDestroyPipelineLayout(device, layout, nullptr);
                device = other.device;
                layout = other.layout;
                other.device = VK_NULL_HANDLE;
                other.layout = VK_NULL_HANDLE;
            }
            return *this;
        }

        explicit operator VkPipelineLayout() const { return layout; }
    };

    struct render_pass_t
    {
        VkDevice device = VK_NULL_HANDLE;
        VkRenderPass pass = VK_NULL_HANDLE;

        render_pass_t() = default;
        render_pass_t(VkDevice dev, VkRenderPass render_pass) : device{ dev }, pass{ render_pass } {}

        ~render_pass_t() { if (pass) vkDestroyRenderPass(device, pass, nullptr); }

        // Move allowed, copy disallowed
        DISABLE_COPY(render_pass_t)
        render_pass_t(render_pass_t&& other) noexcept { *this = std::move(other); }

        render_pass_t& operator=(render_pass_t&& other) noexcept
        {
            if (this != &other)
            {
                if (pass) vkDestroyRenderPass(device, pass, nullptr);
                device = other.device;
                pass = other.pass;
                other.device = VK_NULL_HANDLE;
                other.pass = VK_NULL_HANDLE;
            }
            return *this;
        }

        explicit operator VkRenderPass() const { return pass; }
    };

    struct command_pool_t
    {
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;

        command_pool_t() = default;
        command_pool_t(VkDevice dev, VkCommandPool cmd_pool) : device{ dev }, pool{ cmd_pool } {}

        ~command_pool_t() { if (pool) vkDestroyCommandPool(device, pool, nullptr); }

        // Move allowed, copy disallowed
        DISABLE_COPY(command_pool_t)
        command_pool_t(command_pool_t&& other) noexcept { *this = std::move(other); }

        command_pool_t& operator=(command_pool_t&& other) noexcept
        {
            if (this != &other)
            {
                if (pool) vkDestroyCommandPool(device, pool, nullptr);
                device = other.device;
                pool = other.pool;
                other.device = VK_NULL_HANDLE;
                other.pool = VK_NULL_HANDLE;
            }
            return *this;
        }

        explicit operator VkCommandPool() const { return pool; }
    };

    struct device_t
    {
        VkDevice device{ VK_NULL_HANDLE };

        device_t() = default;
        device_t(VkDevice dev) : device(dev) {}

        ~device_t() { if (device) vkDestroyDevice(device, nullptr); }

        DISABLE_COPY(device_t)
        device_t(device_t&& other) noexcept : device(other.device) { other.device = VK_NULL_HANDLE; }

        device_t& operator=(device_t&& other) noexcept
        {
            if (this != &other)
            {
                if (device) vkDestroyDevice(device, nullptr);
                device = other.device;
                other.device = VK_NULL_HANDLE;
            }
            return *this;
        }

        operator VkDevice() const { return device; }
    };

    struct swapchain_t
    {
        VkDevice device{ VK_NULL_HANDLE };
        VkSwapchainKHR swapchain{ VK_NULL_HANDLE };

        swapchain_t() = default;
        swapchain_t(VkDevice dev, VkSwapchainKHR sw) : device(dev), swapchain(sw) {}

        ~swapchain_t() { if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr); }

        DISABLE_COPY(swapchain_t)
        swapchain_t(swapchain_t&& other) noexcept { *this = std::move(other); }

        swapchain_t& operator=(swapchain_t&& other) noexcept
        {
            if (this != &other)
            {
                if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
                device = other.device;
                swapchain = other.swapchain;
                other.device = VK_NULL_HANDLE;
                other.swapchain = VK_NULL_HANDLE;
            }
            return *this;
        }

        operator VkSwapchainKHR() const { return swapchain; }
    };

    // Custom framebuffer container with auto-cleanup
    struct framebuffer_array_t : std::vector<VkFramebuffer>
    {
        VkDevice device{ VK_NULL_HANDLE };

        framebuffer_array_t() = default;
        explicit framebuffer_array_t(VkDevice dev) : device{ dev } {}

        ~framebuffer_array_t()
        {
            for (auto fb: *this)
                if (fb) vkDestroyFramebuffer(device, fb, nullptr);
            clear();
        }

        // Move allowed, copy disallowed
        DISABLE_COPY(framebuffer_array_t)

        framebuffer_array_t(framebuffer_array_t&& other) noexcept
            : std::vector<VkFramebuffer>(std::move(static_cast<std::vector<VkFramebuffer> &>(other))),
              device(other.device)
        {
            other.device = VK_NULL_HANDLE;
            other.clear(); // optional: prevent double destroy
        }

        framebuffer_array_t& operator=(framebuffer_array_t&& other) noexcept
        {
            if (this != &other)
            {
                // Destroy current contents
                if (device != VK_NULL_HANDLE)
                {
                    for (auto fb: *this)
                        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
                }
                clear();

                // Move from other
                static_cast<std::vector<VkFramebuffer> &>(*this) = std::move(
                    static_cast<std::vector<VkFramebuffer> &>(other));
                device = other.device;
                other.device = VK_NULL_HANDLE;
                other.clear();
            }
            return *this;
        }
    };

    // Reusable pattern for an array of destroyable objects
    template<typename T, void(*DestroyFunc)(VkDevice, T, const VkAllocationCallbacks*)>
    struct vk_array_t : std::vector<T>
    {
        VkDevice device{ VK_NULL_HANDLE };

        explicit vk_array_t(VkDevice dev = VK_NULL_HANDLE) : device(dev) {}

        ~vk_array_t()
        {
            for (auto& item: *this)
                if (item) DestroyFunc(device, item, nullptr);
            this->clear();
        }

        // Move allowed, copy disallowed
        DISABLE_COPY(vk_array_t)

        vk_array_t(vk_array_t&& other) noexcept
            : std::vector<T>(std::move(other)), device(other.device)
        {
            other.device = VK_NULL_HANDLE;
        }

        vk_array_t& operator=(vk_array_t&& other) noexcept
        {
            if (this != &other)
            {
                // Destroy existing resources
                for (auto& item: *this)
                    if (item) DestroyFunc(device, item, nullptr);
                this->clear();

                // Move the vector data
                static_cast<std::vector<T> &>(*this) = std::move(static_cast<std::vector<T> &>(other));

                // Move device
                device = other.device;
                other.device = VK_NULL_HANDLE;
            }
            return *this;
        }

        void reset()
        {
            for (auto& item: *this)
                if (item) DestroyFunc(device, item, nullptr);
            this->clear();
        }
    };

    using frame_data_t = std::array<frame_resources_t, k_max_frames_in_flight>;
    using image_view_array_t = vk_array_t<VkImageView, vkDestroyImageView>;
} // namespace carrot::rhi::vulkan
