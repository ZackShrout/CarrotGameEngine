//
// Created by Zack Shrout on 4/14/26.
//

#include "Core/Pch.h"

#include "NullRHIContext.h"

#include <cstring>

namespace carrot::rhi::null {
    null_rhi_context_t::null_buffer_t::null_buffer_t(const buffer_create_info_t& info) noexcept
        : rhi_buffer_t{ info.size_bytes, info.usage }, _storage(info.size_bytes)
    {
        if (info.initial_data && info.size_bytes > 0u)
            std::memcpy(_storage.data(), info.initial_data, info.size_bytes);
    }

    bool null_rhi_context_t::null_buffer_t::write(const void* data,
                                                  const size_t size_bytes,
                                                  const size_t offset_bytes)
    {
        if (!data || offset_bytes + size_bytes > _storage.size())
            return false;

        std::memcpy(_storage.data() + offset_bytes, data, size_bytes);
        return true;
    }

    null_rhi_context_t::null_swapchain_t::null_swapchain_t(const uint32_t width, const uint32_t height) noexcept
        : _width{ width }, _height{ height }
    {
        resize(width, height);
    }

    void null_rhi_context_t::null_swapchain_t::resize(const uint32_t width, const uint32_t height)
    {
        _width = width;
        _height = height;
        _backbuffer = std::make_unique<null_texture_t>(texture_create_info_t{
            .width = _width,
            .height = _height,
            .format = texture_format_t::rgba8_unorm
        });
    }

    uint32_t null_rhi_context_t::null_swapchain_t::acquire_next_image([[maybe_unused]] rhi_semaphore_t* signal_semaphore)
    {
        return 0u;
    }

    void null_rhi_context_t::null_swapchain_t::present([[maybe_unused]] rhi_semaphore_t* wait_semaphore) {}

    rhi_texture_t* null_rhi_context_t::null_swapchain_t::get_current_backbuffer() const
    {
        return _backbuffer.get();
    }

    void null_rhi_context_t::null_command_queue_t::submit([[maybe_unused]] rhi_command_list_t* cmd_list,
                                                          [[maybe_unused]] rhi_fence_t* fence_to_signal,
                                                          [[maybe_unused]] rhi_semaphore_t* wait_semaphore,
                                                          [[maybe_unused]] rhi_semaphore_t* signal_semaphore)
    {
    }

    void null_rhi_context_t::null_command_queue_t::wait_idle() {}

    null_rhi_context_t::null_rhi_context_t(const rhi_desc_t& desc) noexcept
        : _swapchain{ desc.width, desc.height }
    {
    }

    void null_rhi_context_t::begin_frame()
    {
        _recorded_textured_stages.clear();
        _recorded_text_stages.clear();
    }

    void null_rhi_context_t::record_textured_quad_stage(const textured_quad_stage_record_t& stage)
    {
        _recorded_textured_stages.push_back(recorded_stage_t{
            .batch_count = stage.batches.size(),
            .viewport = stage.viewport,
            .presentation_mask = stage.presentation_mask,
            .point_light_count = stage.point_light_count,
            .ambient_color = stage.ambient_color
        });
    }

    void null_rhi_context_t::record_text_quad_stage(const textured_quad_stage_record_t& stage)
    {
        _recorded_text_stages.push_back(recorded_stage_t{
            .batch_count = stage.batches.size(),
            .viewport = stage.viewport,
            .presentation_mask = stage.presentation_mask,
            .point_light_count = stage.point_light_count,
            .ambient_color = stage.ambient_color
        });
    }

    void null_rhi_context_t::end_frame() {}

    void null_rhi_context_t::release_asset_references() {}

    void null_rhi_context_t::resize(const uint32_t width, const uint32_t height)
    {
        _swapchain.resize(width, height);
    }

    rhi_swapchain_t* null_rhi_context_t::get_swapchain() const noexcept
    {
        return const_cast<null_swapchain_t*>(&_swapchain);
    }

    rhi_command_queue_t* null_rhi_context_t::get_command_queue() const noexcept
    {
        return const_cast<null_command_queue_t*>(&_command_queue);
    }

    std::unique_ptr<rhi_texture_t> null_rhi_context_t::create_texture_2d(const texture_create_info_t& info)
    {
        return std::make_unique<null_texture_t>(info);
    }

    std::unique_ptr<rhi_buffer_t> null_rhi_context_t::create_buffer(const buffer_create_info_t& info)
    {
        return std::make_unique<null_buffer_t>(info);
    }

    std::unique_ptr<rhi_sampler_t> null_rhi_context_t::create_sampler(const sampler_desc_t& desc) const
    {
        return std::make_unique<null_sampler_t>(desc);
    }

    rhi_sampler_t* null_rhi_context_t::get_or_create_sampler(const sampler_desc_t& desc)
    {
        const auto [it, inserted]{
            _samplers.try_emplace(desc, nullptr)
        };
        if (inserted)
            it->second = std::make_unique<null_sampler_t>(desc);

        return it->second.get();
    }

    void null_rhi_context_t::bind_textured_quad_resources([[maybe_unused]] const rhi_texture_t& texture,
                                                           [[maybe_unused]] const rhi_sampler_t& sampler)
    {
    }

    bool null_rhi_context_t::add_presentation_window([[maybe_unused]] window::window_id_t window_id,
                                                     [[maybe_unused]] uint32_t presentation_channel_mask)
    {
        return true;
    }

    bool null_rhi_context_t::remove_presentation_window([[maybe_unused]] window::window_id_t window_id)
    {
        return true;
    }

    void null_rhi_context_t::wait_idle()
    {
        _command_queue.wait_idle();
    }
} // namespace carrot::rhi::null
