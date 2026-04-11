//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/RHI.h"
#include "Renderer/Draw/TexturedQuadBatch.h"
#include "VulkanCommandQueue.h"
#include "VulkanCommon.h"
#include "VulkanCore.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"

#include <array>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace carrot::rhi::vulkan {
    constexpr uint32_t k_max_textured_quad_stage_records_per_frame{ 8 };

    class vulkan_buffer_t;
    class vulkan_textured_quad_pipeline_t;
    class vulkan_pipeline_t;
    class vulkan_render_pass_t;

    struct textured_quad_state_t
    {
        const vulkan_buffer_t* vertex_buffer{ nullptr };
        const vulkan_buffer_t* index_buffer{ nullptr };

        chlm::float4x4 view_projection{ chlm::float4x4::identity() };
        render_viewport_t viewport{ };

        std::array<std::array<std::unique_ptr<rhi_buffer_t>, k_max_textured_quad_stage_records_per_frame>,
                   k_max_frames_in_flight> camera_uniform_buffers;
        std::array<std::array<VkDescriptorSet, k_max_textured_quad_stage_records_per_frame>,
                   k_max_frames_in_flight> camera_descriptor_sets{ };

        std::vector<renderer::textured_quad_batch_t> batches;
        std::array<std::vector<VkDescriptorSet>, k_max_frames_in_flight> descriptor_sets;
    };

    class vulkan_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit vulkan_rhi_context_t(const rhi_desc_t& desc);
        ~vulkan_rhi_context_t() override;

        void begin_frame() override;
        void record_textured_quad_stage(const textured_quad_stage_record_t& stage) override;
        void record_text_quad_stage(const textured_quad_stage_record_t& stage) override;
        void end_frame() override;

        void release_asset_references() override;

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override { return _device.get(); }
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override { return _swapchain.get(); }
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override { return _graphics_queue.get(); }
        [[nodiscard]] graphics_api get_graphics_api() const noexcept override { return graphics_api::vulkan; }

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const override;

        [[nodiscard]] rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) override;
        void bind_textured_quad_resources([[maybe_unused]] const rhi_texture_t& texture,
                                          [[maybe_unused]] const rhi_sampler_t& sampler) override {}
        bool add_presentation_window(window::window_id_t window_id,
                                     uint32_t presentation_channel_mask = presentation_channel_gameplay) override;
        bool remove_presentation_window(window::window_id_t window_id) override;

        void wait_idle() override;

    private:
        enum class quad_pipeline_kind_t : uint8_t
        {
            textured = 0,
            text
        };

        struct auxiliary_surface_t
        {
            window::window_id_t id{ window::invalid_window_id };
            uint32_t presentation_channel_mask{ presentation_channel_gameplay };
            VkSurfaceKHR surface{ VK_NULL_HANDLE };
            std::unique_ptr<vulkan_swapchain_t> swapchain;
            framebuffer_array_t framebuffers;
            uint32_t current_image_index{ 0 };
            std::array<VkSemaphore, k_max_frames_in_flight> image_acquire{ };
            std::vector<VkSemaphore> render_finished;
            std::vector<VkSemaphore> retired_render_finished;
            uint32_t last_width{ 0 };
            uint32_t last_height{ 0 };
            bool swapchain_dirty{ false };
        };

        struct recorded_stage_t
        {
            textured_quad_stage_record_t stage;
            uint32_t descriptor_set_offset{ 0 };
            uint32_t descriptor_set_count{ 0 };
            quad_pipeline_kind_t pipeline_kind{ quad_pipeline_kind_t::textured };
        };

        void init(const rhi_desc_t& desc);
        void recreate_swapchain_dependent_resources();
        void recreate_render_finished_semaphores();
        void collect_retired_present_semaphores() noexcept;
        bool create_surface_for_window(window::window_id_t window_id, VkSurfaceKHR& out_surface) const;
        bool create_auxiliary_surface(window::window_id_t window_id, uint32_t presentation_channel_mask);
        void destroy_auxiliary_surface(auxiliary_surface_t& surface) noexcept;
        void destroy_all_auxiliary_surfaces() noexcept;
        void sync_auxiliary_surface_sizes();
        uint32_t prepare_quad_stage_descriptors(const textured_quad_stage_record_t& stage, uint32_t& out_batch_count);
        void encode_quad_stage_to_command_buffer(VkCommandBuffer command_buffer,
                                                 const textured_quad_stage_record_t& stage,
                                                 uint32_t descriptor_set_offset,
                                                 uint32_t batch_count,
                                                 quad_pipeline_kind_t pipeline_kind);
        [[nodiscard]] uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
        [[nodiscard]] VkCommandBuffer begin_single_time_commands() const;
        void end_single_time_commands(VkCommandBuffer cmd) const;

        void create_vk_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                              VkBuffer& out_buffer, VkDeviceMemory& out_memory) const;

        void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
        void create_descriptor_pool();
        void destroy_descriptor_pool() noexcept;

        void release_textured_quad_resources() noexcept;

        void allocate_textured_quad_descriptor_sets(uint32_t frame_index, uint32_t count);
        void ensure_textured_quad_descriptor_sets_for_frame(uint32_t frame_index, uint32_t batch_count);
        void write_textured_quad_descriptor_set(VkDescriptorSet descriptor_set, const rhi_texture_t& texture, const rhi_sampler_t& sampler) const;

        void allocate_textured_quad_camera_descriptor_sets();
        void write_textured_quad_camera_descriptor_set(uint32_t frame_index, uint32_t stage_slot) const;

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
        std::unique_ptr<vulkan_textured_quad_pipeline_t>    _text_quad_pipeline;
        framebuffer_array_t                                 _framebuffers;

        // ── Per-frame GPU resources ──
        std::array<frame_resources_t, k_max_frames_in_flight>   _frames;
        std::vector<VkSemaphore>                                _render_finished_semaphores;
        std::vector<VkSemaphore>                                _retired_render_finished_semaphores;

        // ── Textured quad GPU state ──
        textured_quad_state_t                                                                   _textured_quad;
        std::unordered_map<sampler_desc_t, std::unique_ptr<rhi_sampler_t>, sampler_desc_hash_t> _sampler_cache;

        // ── Frame progression / swapchain bookkeeping ──
        uint32_t _current_frame{ 0 };
        uint32_t _current_image_index{ 0 };
        uint32_t _pending_resize_width{ 0 };
        uint32_t _pending_resize_height{ 0 };
        std::array<uint32_t, k_max_frames_in_flight> _textured_quad_descriptor_set_cursor{ };

        // ── Deferred actions and lifecycle flags ──
        bool _pending_pipeline_reload{ false };
        bool _frame_active{ false };
        bool _render_pass_active{ false };
        bool _skip_frame{ false };
        bool _swapchain_dirty{ false };

        // ── Multi-window presentation surfaces (optional) ──
        window::window_id_t _presentation_window_id{ window::invalid_window_id };
        std::vector<auxiliary_surface_t> _auxiliary_surfaces;
        std::vector<recorded_stage_t> _recorded_stages;
    };
} // namespace carrot::rhi::vulkan
