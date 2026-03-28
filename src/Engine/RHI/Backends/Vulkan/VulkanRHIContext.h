//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "VulkanCommandQueue.h"
#include "VulkanCommon.h"
#include "VulkanCore.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "Renderer/Draw/TexturedQuadBatch.h"
#include "RHI/RHI.h"

#include <array>
#include <memory>
#include <span>
#include <vector>
#include <unordered_map>

namespace carrot::rhi::vulkan {
    class vulkan_buffer_t;
    class vulkan_textured_quad_pipeline_t;
    class vulkan_pipeline_t;
    class vulkan_render_pass_t;

    struct textured_quad_state_t
    {
        const vulkan_buffer_t* vertex_buffer{ nullptr };
        const vulkan_buffer_t* index_buffer{ nullptr };
        std::vector<renderer::textured_quad_batch_t> batches;
        std::array<std::vector<VkDescriptorSet>, k_max_frames_in_flight> descriptor_sets;
    };

    class vulkan_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit vulkan_rhi_context_t(const rhi_desc_t& desc);
        ~vulkan_rhi_context_t() override;

        void begin_frame() override;
        void record_frame() override;
        void end_frame() override;

        void release_asset_references() override;

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override { return _device.get(); }
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override { return _swapchain.get(); }
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override { return _graphics_queue.get(); }

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const override;
        void set_textured_quad_geometry(const rhi_buffer_t& vertex_buffer, const rhi_buffer_t& index_buffer) override;
        void set_textured_quad_batches(std::span<const renderer::textured_quad_batch_t> batches) override;

        [[nodiscard]] rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) override;
        void bind_textured_quad_resources(const rhi_texture_t& texture, const rhi_sampler_t& sampler) override {}

        void wait_idle() override;

    private:
        void init(const rhi_desc_t& desc);
        void recreate_swapchain_dependent_resources();
        void recreate_render_finished_semaphores();
        [[nodiscard]] uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
        [[nodiscard]] VkCommandBuffer begin_single_time_commands() const;
        void end_single_time_commands(VkCommandBuffer cmd) const;

        void create_vk_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                              VkBuffer& out_buffer, VkDeviceMemory& out_memory) const;

        void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
        void create_descriptor_pool();
        void destroy_descriptor_pool() noexcept;

        void allocate_textured_quad_descriptor_sets(uint32_t frame_index, uint32_t count);
        void ensure_textured_quad_descriptor_sets_for_frame(uint32_t frame_index, uint32_t batch_count);
        void write_textured_quad_descriptor_set(VkDescriptorSet descriptor_set, const rhi_texture_t& texture, const rhi_sampler_t& sampler) const;

        // ── Core Vulkan handles ──
        VkInstance          _vk_instance{ VK_NULL_HANDLE };
        VkSurfaceKHR        _vk_surface{ VK_NULL_HANDLE };
        VkCommandPool       _command_pool{ VK_NULL_HANDLE };
        VkDescriptorPool    _descriptor_pool{ VK_NULL_HANDLE };

        // ── Backend-owned services and persistent GPU objects ──
        assets::shader_file_provider_t*                     _shader_files{ nullptr };
        std::unique_ptr<vulkan_device_t>                    _device;
        std::unique_ptr<vulkan_swapchain_t>                 _swapchain;
        std::unique_ptr<vulkan_command_queue_t>             _graphics_queue;
        std::unique_ptr<vulkan_render_pass_t>               _render_pass;
        std::unique_ptr<vulkan_textured_quad_pipeline_t>    _textured_quad_pipeline;
        framebuffer_array_t                                 _framebuffers;

        // ── Per-frame GPU resources ──
        std::array<frame_resources_t, k_max_frames_in_flight>   _frames;
        std::vector<VkSemaphore>                                _render_finished_semaphores;

        // ── Renderer submission state ──
        textured_quad_state_t                                                                   _textured_quad;
        std::unordered_map<sampler_desc_t, std::unique_ptr<rhi_sampler_t>, sampler_desc_hash_t> _sampler_cache;

        // ── Frame progression / swapchain bookkeeping ──
        uint32_t _frame_counter{ 0 };
        uint32_t _current_frame{ 0 };
        uint32_t _current_image_index{ 0 };
        uint32_t _pending_resize_width{ 0 };
        uint32_t _pending_resize_height{ 0 };

        // ── Deferred actions and lifecycle flags ──
        bool _pending_pipeline_reload{ false };
        bool _frame_active{ false };
        bool _render_pass_active{ false };
        bool _skip_frame{ false };
        bool _swapchain_dirty{ false };
    };
} // namespace carrot::rhi::vulkan
