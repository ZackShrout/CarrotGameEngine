//
// Created by zshrout on 1/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Renderer.h"

#include "IO/VirtualFileSystem.h"
#include "Window/Window.h"
#include "RHI/Buffer.h"

namespace carrot::renderer {
    // PUBLIC

    renderer_t::renderer_t(io::virtual_file_system_t& vfs, const engine_graphics_config_t& config)
        : _vfs{ vfs }, _config{ config }
    {
        init();
    }

    void renderer_t::init()
    {
        if (_is_initialized) return;

        LOG_GRAPHICS_INFO("Initializing Renderer...");

        _shader_provider = std::make_unique<assets::vfs_shader_file_provider_t>(_vfs);

        rhi::rhi_desc_t desc{ };
        desc.api = _config.api;
        desc.enable_debug_layers = _config.enable_debug_layers;
        desc.width = window::get_width();
        desc.height = window::get_height();
        desc.shader_files = _shader_provider.get();

        _rhi = rhi::create_rhi_context(desc);
        if (!_rhi)
        {
            LOG_GRAPHICS_FATAL("Failed to create RHI context!");
            return;
        }

        _is_initialized = true;
        LOG_GRAPHICS_INFO("Renderer initialized successfully (backend: {})",
                          carrot::rhi::graphics_api_to_string(desc.api));
    }

    void renderer_t::shutdown()
    {
        if (!_is_initialized)
            return;

        LOG_GRAPHICS_INFO("Shutting down Renderer...");

        if (_rhi)
            _rhi->wait_idle();

        release_frame_resources();

        _shader_provider.reset();
        _rhi.reset();

        _is_initialized = false;
        LOG_GRAPHICS_INFO("Renderer shutdown complete");
    }

    void renderer_t::begin_frame()
    {
        _frame_index++;

        _textured_quad.vertices_cpu.clear();
        _textured_quad.indices_cpu.clear();
        _textured_quad.batches.clear();

        _stats = { };

        _rhi->begin_frame();
    }

    void renderer_t::end_frame()
    {
        if (!_textured_quad.batches.empty())
        {
            ensure_textured_quad_frame_buffers();
            upload_textured_quad_frame_data();

            if (_textured_quad.frame_vertex_buffer && _textured_quad.frame_index_buffer)
            {
                _rhi->set_textured_quad_geometry(*_textured_quad.frame_vertex_buffer,
                                                 *_textured_quad.frame_index_buffer);

                _rhi->set_textured_quad_batches(_textured_quad.batches);
            }
        }

        _rhi->record_frame();

        _stats.vertex_count = static_cast<uint32_t>(_textured_quad.vertices_cpu.size());
        _stats.index_count = static_cast<uint32_t>(_textured_quad.indices_cpu.size());
        _stats.textured_quad_batch_count = static_cast<uint32_t>(_textured_quad.batches.size());
        _stats.draw_calls = _stats.textured_quad_batch_count;

        _rhi->end_frame();
    }

    void renderer_t::draw_textured_quad(const textured_quad_draw_info_t& quad)
    {
        if (quad.texture == nullptr)
        {
            LOG_GRAPHICS_WARN("draw_textured_quad called with null texture");
            return;
        }

        if (_textured_quad.batches.empty() || _textured_quad.batches.back().texture != quad.texture)
        {
            _textured_quad.batches.push_back(textured_quad_batch_t{
                .texture = quad.texture,
                .first_index = static_cast<uint32_t>(_textured_quad.indices_cpu.size()),
                .index_count = 0
            });
        }

        const uint32_t base_vertex{ static_cast<uint32_t>(_textured_quad.vertices_cpu.size()) };

        _textured_quad.vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x,
            .y = quad.y,
            .u = quad.u0,
            .v = quad.v0,
            .color = quad.color
        });

        _textured_quad.vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x + quad.width,
            .y = quad.y,
            .u = quad.u1,
            .v = quad.v0,
            .color = quad.color
        });

        _textured_quad.vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x + quad.width,
            .y = quad.y + quad.height,
            .u = quad.u1,
            .v = quad.v1,
            .color = quad.color
        });

        _textured_quad.vertices_cpu.push_back(quad_vertex_t{
            .x = quad.x,
            .y = quad.y + quad.height,
            .u = quad.u0,
            .v = quad.v1,
            .color = quad.color
        });

        _textured_quad.indices_cpu.push_back(base_vertex + 0);
        _textured_quad.indices_cpu.push_back(base_vertex + 1);
        _textured_quad.indices_cpu.push_back(base_vertex + 2);
        _textured_quad.indices_cpu.push_back(base_vertex + 0);
        _textured_quad.indices_cpu.push_back(base_vertex + 2);
        _textured_quad.indices_cpu.push_back(base_vertex + 3);

        _textured_quad.batches.back().index_count += 6;

        _stats.textured_quad_count++;
    }

    void renderer_t::notify_shader_changed(std::string_view path)
    {
        LOG_GRAPHICS_INFO("Shader changed: {}, notifying RHI...", path);
        // For now just log — later forward to pipeline cache / reload system
    }

    // PRIVATE
    void renderer_t::release_frame_resources()
    {
        _textured_quad.vertices_cpu.clear();
        _textured_quad.indices_cpu.clear();
        _textured_quad.batches.clear();

        _textured_quad.frame_vertex_buffer.reset();
        _textured_quad.frame_index_buffer.reset();

        _textured_quad.vertex_capacity = 0;
        _textured_quad.index_capacity = 0;

        _stats = {};
    }

    void renderer_t::ensure_textured_quad_frame_buffers()
    {
        const size_t required_vertex_bytes{ _textured_quad.vertices_cpu.size() * sizeof(quad_vertex_t) };
        const size_t required_index_bytes{ _textured_quad.indices_cpu.size() * sizeof(uint32_t) };

        if (required_vertex_bytes == 0 || required_index_bytes == 0)
            return;

        if (_textured_quad.frame_vertex_buffer == nullptr || _textured_quad.vertex_capacity < required_vertex_bytes)
        {
            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_vertex_bytes;
            info.usage = rhi::buffer_usage_t::vertex;
            info.initial_data = nullptr;
            info.cpu_writable = true;

            _textured_quad.frame_vertex_buffer = _rhi->create_buffer(info);
            if (!_textured_quad.frame_vertex_buffer)
            {
                LOG_GRAPHICS_FATAL("Failed to create textured quad frame vertex buffer");
                _textured_quad.vertex_capacity = 0;
                return;
            }

            _textured_quad.vertex_capacity = required_vertex_bytes;
        }

        if (_textured_quad.frame_index_buffer == nullptr || _textured_quad.index_capacity < required_index_bytes)
        {
            rhi::buffer_create_info_t info{ };
            info.size_bytes = required_index_bytes;
            info.usage = rhi::buffer_usage_t::index;
            info.initial_data = nullptr;
            info.cpu_writable = true;

            _textured_quad.frame_index_buffer = _rhi->create_buffer(info);
            if (!_textured_quad.frame_index_buffer)
            {
                LOG_GRAPHICS_FATAL("Failed to create textured quad frame index buffer");
                _textured_quad.index_capacity = 0;
                return;
            }

            _textured_quad.index_capacity = required_index_bytes;
        }
    }

    void renderer_t::upload_textured_quad_frame_data() const
    {
        if (_textured_quad.vertices_cpu.empty() || _textured_quad.indices_cpu.empty())
            return;

        if (!_textured_quad.frame_vertex_buffer || !_textured_quad.frame_index_buffer)
        {
            LOG_GRAPHICS_FATAL("Textured quad frame buffers are not available for upload");
            return;
        }

        const size_t vertex_bytes{ _textured_quad.vertices_cpu.size() * sizeof(quad_vertex_t) };
        const size_t index_bytes{ _textured_quad.indices_cpu.size() * sizeof(uint32_t) };

        if (!_textured_quad.frame_vertex_buffer->write(_textured_quad.vertices_cpu.data(), vertex_bytes, 0))
        {
            LOG_GRAPHICS_FATAL("Failed to upload textured quad vertex data");
            return;
        }

        if (!_textured_quad.frame_index_buffer->write(_textured_quad.indices_cpu.data(), index_bytes, 0))
            LOG_GRAPHICS_FATAL("Failed to upload textured quad index data");
    }
} // namespace carrot::renderer
