//
// Created by Codex on 4/14/26.
//

#pragma once

#include "RHI/CommandList.h"
#include "RHI/CommandQueue.h"
#include "RHI/Fence.h"
#include "RHI/RHI.h"
#include "RHI/Semaphore.h"
#include "RHI/Swapchain.h"

#include <unordered_map>
#include <vector>

namespace carrot::rhi::null {
    class null_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit null_rhi_context_t(const rhi_desc_t& desc) noexcept;

        void begin_frame() override;
        void record_textured_quad_stage(const textured_quad_stage_record_t& stage) override;
        void record_text_quad_stage(const textured_quad_stage_record_t& stage) override;
        void end_frame() override;
        void release_asset_references() override;
        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override { return nullptr; }
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override;
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override;
        [[nodiscard]] graphics_api get_graphics_api() const noexcept override { return graphics_api::null_backend; }

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const override;

        [[nodiscard]] rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) override;
        void bind_textured_quad_resources(const rhi_texture_t& texture, const rhi_sampler_t& sampler) override;

        bool add_presentation_window(window::window_id_t window_id,
                                     uint32_t presentation_channel_mask = presentation_channel_gameplay) override;
        bool remove_presentation_window(window::window_id_t window_id) override;

        void wait_idle() override;

    private:
        class null_texture_t final : public rhi_texture_t
        {
        public:
            explicit null_texture_t(const texture_create_info_t& info) noexcept
                : _width{ info.width }, _height{ info.height }, _format{ info.format } {}

            [[nodiscard]] uint32_t width() const noexcept override { return _width; }
            [[nodiscard]] uint32_t height() const noexcept override { return _height; }
            [[nodiscard]] texture_format_t format() const noexcept override { return _format; }

        private:
            uint32_t _width{ 0u };
            uint32_t _height{ 0u };
            texture_format_t _format{ texture_format_t::rgba8_srgb };
        };

        class null_buffer_t final : public rhi_buffer_t
        {
        public:
            explicit null_buffer_t(const buffer_create_info_t& info) noexcept;
            [[nodiscard]] bool write(const void* data, size_t size_bytes, size_t offset_bytes = 0) override;

        private:
            std::vector<std::byte> _storage;
        };

        class null_sampler_t final : public rhi_sampler_t
        {
        public:
            explicit null_sampler_t(const sampler_desc_t& desc) noexcept
                : rhi_sampler_t{ desc } {}
        };

        class null_command_queue_t final : public rhi_command_queue_t
        {
        public:
            void submit(rhi_command_list_t* cmd_list,
                        rhi_fence_t* fence_to_signal = nullptr,
                        rhi_semaphore_t* wait_semaphore = nullptr,
                        rhi_semaphore_t* signal_semaphore = nullptr) override;
            void wait_idle() override;
        };

        class null_swapchain_t final : public rhi_swapchain_t
        {
        public:
            null_swapchain_t(uint32_t width, uint32_t height) noexcept;

            void resize(uint32_t width, uint32_t height) override;
            uint32_t acquire_next_image(rhi_semaphore_t* signal_semaphore) override;
            void present(rhi_semaphore_t* wait_semaphore) override;

            [[nodiscard]] rhi_texture_t* get_current_backbuffer() const override;
            [[nodiscard]] uint32_t get_current_image_index() const override { return 0u; }
            [[nodiscard]] uint32_t get_image_count() const override { return 1u; }
            [[nodiscard]] uint32_t get_width() const override { return _width; }
            [[nodiscard]] uint32_t get_height() const override { return _height; }

        private:
            uint32_t _width{ 0u };
            uint32_t _height{ 0u };
            std::unique_ptr<null_texture_t> _backbuffer;
        };

        null_command_queue_t _command_queue;
        null_swapchain_t _swapchain;
        std::unordered_map<sampler_desc_t, std::unique_ptr<null_sampler_t>, sampler_desc_hash_t> _samplers;
    };
} // namespace carrot::rhi::null
