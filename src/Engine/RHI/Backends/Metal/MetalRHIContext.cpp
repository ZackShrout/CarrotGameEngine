//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalRHIContext.h"

#include "MetalBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalDescriptorEncoding.h"
#include "MetalDevice.h"
#include "MetalLayerBridge.h"
#include "MetalSampler.h"
#include "MetalSwapchain.h"
#include "MetalTexture.h"
#include "Pipelines/MetalTexturedQuadPipeline.h"
#include "RHI/SamplerPresets.h"
#include "Renderer/Draw/TexturedQuadCameraUniform.h"
#include "Window/Window.h"

#include <algorithm>

namespace carrot::rhi::metal {
    namespace {
        constexpr NS::UInteger k_textured_quad_root_argument_buffer_index{ 2 };

        [[nodiscard]] MTL::PixelFormat to_metal_pixel_format(const texture_format_t format) noexcept
        {
            switch (format)
            {
                case texture_format_t::rgba8_unorm: return MTL::PixelFormatRGBA8Unorm;
                case texture_format_t::rgba8_srgb: return MTL::PixelFormatRGBA8Unorm_sRGB;
                default: return MTL::PixelFormatRGBA8Unorm_sRGB;
            }
        }

        [[nodiscard]] MTL::SamplerMinMagFilter to_metal_filter(const sampler_filter_t filter) noexcept
        {
            switch (filter)
            {
                case sampler_filter_t::nearest: return MTL::SamplerMinMagFilterNearest;
                case sampler_filter_t::linear: return MTL::SamplerMinMagFilterLinear;
                default: return MTL::SamplerMinMagFilterNearest;
            }
        }

        [[nodiscard]] MTL::SamplerMipFilter to_metal_mip_filter(const sampler_mip_filter_t filter) noexcept
        {
            switch (filter)
            {
                case sampler_mip_filter_t::nearest: return MTL::SamplerMipFilterNearest;
                case sampler_mip_filter_t::linear: return MTL::SamplerMipFilterLinear;
                default: return MTL::SamplerMipFilterNearest;
            }
        }

        [[nodiscard]] MTL::SamplerAddressMode to_metal_address_mode(const sampler_address_mode_t mode) noexcept
        {
            switch (mode)
            {
                case sampler_address_mode_t::clamp_to_edge: return MTL::SamplerAddressModeClampToEdge;
                case sampler_address_mode_t::repeat: return MTL::SamplerAddressModeRepeat;
                case sampler_address_mode_t::mirrored_repeat: return MTL::SamplerAddressModeMirrorRepeat;
                default: return MTL::SamplerAddressModeClampToEdge;
            }
        }
    } // namespace
    // PUBLIC

    metal_rhi_context_t::metal_rhi_context_t(const rhi_desc_t& desc)
    {
        const window::window_id_t presentation_window_id{
            window::has_window(desc.presentation_window_id)
                ? desc.presentation_window_id
                : window::get_main_window_id()
        };
        _presentation_window_id = presentation_window_id;
        core::platform::native_window_handle_t window_handle{ window::get_native_handle(presentation_window_id) };
        _metal_layer = window_handle.cocoa_t.metal_layer;

        if (!_metal_layer)
        {
            LOG_GRAPHICS_FATAL("No CAMetalLayer provided by window");
            return;
        }

        metal_set_layer_pixel_format_srgb(_metal_layer);

        MTL::Device* mtl_device{ MTL::CreateSystemDefaultDevice() };
        if (!mtl_device)
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal device");
            return;
        }

        _device = std::make_unique<metal_device_t>(mtl_device);

        MTL::CommandQueue* mtl_queue{ mtl_device->newCommandQueue() };
        if (!mtl_queue)
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal command queue");
            return;
        }

        _command_queue = std::make_unique<metal_command_queue_t>(mtl_queue);
        _swapchain = std::make_unique<metal_swapchain_t>(mtl_device, _metal_layer, desc.width, desc.height);

        _textured_quad_pipeline = std::make_unique<metal_textured_quad_pipeline_t>(
            *_device,
            *desc.shader_files,
            MTL::PixelFormatBGRA8Unorm_sRGB,
            "engine://shaders/metal/textured_quad.vert.metallib",
            "engine://shaders/metal/textured_quad.frag.metallib",
            "textured quad");

        _text_quad_pipeline = std::make_unique<metal_textured_quad_pipeline_t>(
            *_device,
            *desc.shader_files,
            MTL::PixelFormatBGRA8Unorm_sRGB,
            "engine://shaders/metal/text_quad.vert.metallib",
            "engine://shaders/metal/text_quad.frag.metallib",
            "text quad");

        if (!_textured_quad_pipeline || !_textured_quad_pipeline->is_valid())
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal textured quad pipeline");
            return;
        }
        if (!_text_quad_pipeline || !_text_quad_pipeline->is_valid())
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal text quad pipeline");
            return;
        }

        _textured_quad_root_stride = align_up(sizeof(textured_quad_root_argument_buffer_t));
        _textured_quad_cbv_stride = align_up(sizeof(descriptor_table_entry_t));
        _textured_quad_srv_stride = align_up(sizeof(descriptor_table_entry_t));
        _textured_quad_sampler_stride = align_up(sizeof(descriptor_table_entry_t));

        for (uint32_t stage_slot{ 0 }; stage_slot < k_max_textured_quad_stage_slots_per_frame; ++stage_slot)
        {
            ensure_textured_quad_argument_capacity(stage_slot, 16);

            _textured_quad_camera_uniform_buffers[stage_slot] = std::make_unique<metal_buffer_t>(
                _device->mtl_device()->newBuffer(sizeof(renderer::world_forward_plus_uniform_t),
                                                 MTL::ResourceStorageModeShared),
                sizeof(renderer::world_forward_plus_uniform_t),
                buffer_usage_t::uniform
            );

            if (!_textured_quad_camera_uniform_buffers[stage_slot] ||
                !_textured_quad_camera_uniform_buffers[stage_slot]->mtl_buffer())
            {
                LOG_GRAPHICS_FATAL("Failed to allocate Metal world forward+ uniform buffer for stage slot {}",
                                   stage_slot);
                return;
            }
        }

        _frame_semaphore = dispatch_semaphore_create(3);
        if (!_frame_semaphore)
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal frame semaphore");
            return;
        }

        LOG_GRAPHICS_INFO("Metal RHI initialized successfully");
    }

    metal_rhi_context_t::~metal_rhi_context_t()
    {
        wait_idle();
        reset_frame_state();

        _sampler_cache.clear();

#if !OS_OBJECT_USE_OBJC
        if (_frame_semaphore)
        {
            dispatch_release(_frame_semaphore);
            _frame_semaphore = nullptr;
        }
#else
        _frame_semaphore = nullptr;
#endif
    }

    void metal_rhi_context_t::begin_frame()
    {
        if (!_device || !_swapchain || !_command_queue)
            return;

        dispatch_semaphore_wait(_frame_semaphore, DISPATCH_TIME_FOREVER);
        _recorded_stages.clear();

        _swapchain->acquire_next_image(nullptr);
        _active_drawable = static_cast<const CA::MetalDrawable*>(_swapchain->get_current_drawable());

        if (!_active_drawable)
        {
            LOG_GRAPHICS_WARN("Metal begin_frame failed to acquire drawable");
            dispatch_semaphore_signal(_frame_semaphore);
            return;
        }

        _active_command_buffer = _command_queue->mtl_command_queue()->commandBuffer();
        if (!_active_command_buffer)
        {
            LOG_GRAPHICS_WARN("Metal begin_frame failed to create command buffer");
            _active_drawable = nullptr;
            dispatch_semaphore_signal(_frame_semaphore);
            return;
        }

        _active_command_buffer->retain();
        metal_add_command_buffer_completion_signal(_active_command_buffer, _frame_semaphore);
        if (!window::is_fullscreen(_presentation_window_id))
            acquire_auxiliary_drawables();
        else
            release_auxiliary_drawables();

        _render_encoder.begin(_active_command_buffer, _active_drawable, MTL::ClearColor(0.02, 0.02, 0.04, 1.0));
    }

    void metal_rhi_context_t::record_textured_quad_stage(const textured_quad_stage_record_t& stage)
    {
        if (!is_frame_active())
            return;

        _recorded_stages.push_back({
            .stage = stage,
            .stage_slot = static_cast<uint32_t>(_recorded_stages.size()),
            .pipeline_kind = quad_pipeline_kind_t::textured
        });
        if (_recorded_stages.back().stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("Metal textured quad stage slot {} exceeds max supported stage slots {}",
                               _recorded_stages.back().stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            _recorded_stages.pop_back();
            return;
        }

        MTL::RenderCommandEncoder* encoder{ _render_encoder.encoder() };
        if (!encoder)
            return;

        if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
            encode_quad_stage(encoder, stage, _recorded_stages.back().stage_slot, quad_pipeline_kind_t::textured);
    }

    void metal_rhi_context_t::record_text_quad_stage(const textured_quad_stage_record_t& stage)
    {
        if (!is_frame_active())
            return;

        _recorded_stages.push_back({
            .stage = stage,
            .stage_slot = static_cast<uint32_t>(_recorded_stages.size()),
            .pipeline_kind = quad_pipeline_kind_t::text
        });
        if (_recorded_stages.back().stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("Metal textured quad stage slot {} exceeds max supported stage slots {}",
                               _recorded_stages.back().stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            _recorded_stages.pop_back();
            return;
        }

        MTL::RenderCommandEncoder* encoder{ _render_encoder.encoder() };
        if (!encoder)
            return;

        if (presentation_mask_includes(stage.presentation_mask, presentation_channel_gameplay))
            encode_quad_stage(encoder, stage, _recorded_stages.back().stage_slot, quad_pipeline_kind_t::text);
    }

    void metal_rhi_context_t::end_frame()
    {
        if (!is_frame_active())
            return;

        _render_encoder.end();

        for (auxiliary_surface_t& surface : _auxiliary_surfaces)
        {
            if (!surface.drawable)
                continue;

            metal_render_encoder_t auxiliary_encoder;
            auxiliary_encoder.begin(_active_command_buffer, surface.drawable, MTL::ClearColor(0.02, 0.02, 0.04, 1.0));
            if (MTL::RenderCommandEncoder* encoder{ auxiliary_encoder.encoder() })
            {
                for (const recorded_stage_t& recorded_stage : _recorded_stages)
                {
                    const textured_quad_stage_record_t& stage{ recorded_stage.stage };
                    if (!presentation_mask_includes(stage.presentation_mask, surface.presentation_channel_mask))
                        continue;
                    encode_quad_stage(encoder, stage, recorded_stage.stage_slot, recorded_stage.pipeline_kind);
                }
            }
            auxiliary_encoder.end();
            _active_command_buffer->presentDrawable(surface.drawable);
        }

        _active_command_buffer->presentDrawable(_active_drawable);
        _active_command_buffer->commit();
        _active_command_buffer->release();
        _active_command_buffer = nullptr;

        _active_drawable = nullptr;
        release_auxiliary_drawables();
    }

    void metal_rhi_context_t::resize(const uint32_t width, const uint32_t height)
    {
        if (_swapchain)
            _swapchain->resize(width, height);
    }

    rhi_device_t* metal_rhi_context_t::get_device() const noexcept
    {
        return _device.get();
    }

    rhi_swapchain_t* metal_rhi_context_t::get_swapchain() const noexcept
    {
        return _swapchain.get();
    }

    rhi_command_queue_t* metal_rhi_context_t::get_command_queue() const noexcept
    {
        return _command_queue.get();
    }

    std::unique_ptr<rhi_texture_t> metal_rhi_context_t::create_texture_2d(const texture_create_info_t& info)
    {
        if (!_device || !_device->mtl_device())
            return nullptr;

        if (info.width == 0 || info.height == 0)
        {
            LOG_GRAPHICS_ERROR("Metal create_texture_2d called with invalid size {}x{}", info.width, info.height);
            return nullptr;
        }

        MTL::TextureDescriptor* desc{ MTL::TextureDescriptor::alloc()->init() };
        if (!desc)
        {
            LOG_GRAPHICS_ERROR("Failed to allocate Metal texture descriptor");
            return nullptr;
        }

        desc->setTextureType(MTL::TextureType2D);
        desc->setWidth(info.width);
        desc->setHeight(info.height);
        desc->setDepth(1);
        desc->setArrayLength(1);
        desc->setMipmapLevelCount(1);
        desc->setSampleCount(1);
        desc->setUsage(MTL::TextureUsageShaderRead);
        desc->setStorageMode(MTL::StorageModeShared);
        desc->setCpuCacheMode(MTL::CPUCacheModeDefaultCache);
        desc->setPixelFormat(to_metal_pixel_format(info.format));

        MTL::Texture* texture{ _device->mtl_device()->newTexture(desc) };
        desc->release();

        if (!texture)
        {
            LOG_GRAPHICS_ERROR("Failed to create Metal texture {}x{}", info.width, info.height);
            return nullptr;
        }

        if (info.initial_data)
        {
            const uint32_t bytes_per_row{
                info.initial_data_stride_bytes ? info.initial_data_stride_bytes : info.width * 4
            };

            texture->replaceRegion(MTL::Region(0, 0, 0, info.width, info.height, 1), 0, info.initial_data, bytes_per_row);
        }

        return std::make_unique<metal_texture_t>(texture, info.width, info.height, info.format);
    }

    std::unique_ptr<rhi_buffer_t> metal_rhi_context_t::create_buffer(const buffer_create_info_t& info)
    {
        if (!_device || !_device->mtl_device())
            return nullptr;

        if (info.size_bytes == 0)
        {
            LOG_GRAPHICS_ERROR("Metal create_buffer called with size 0");
            return nullptr;
        }

        constexpr MTL::ResourceOptions options{ MTL::ResourceStorageModeShared };
        MTL::Buffer* buffer{ _device->mtl_device()->newBuffer(info.size_bytes, options) };

        if (!buffer)
        {
            LOG_GRAPHICS_ERROR("Failed to create Metal buffer ({} bytes)", info.size_bytes);
            return nullptr;
        }

        std::unique_ptr<rhi_buffer_t> result{ std::make_unique<metal_buffer_t>(buffer, info.size_bytes, info.usage) };

        if (info.initial_data)
        {
            if (!result->write(info.initial_data, info.size_bytes, 0))
            {
                LOG_GRAPHICS_ERROR("Failed to initialize Metal buffer");
                return nullptr;
            }
        }

        return result;
    }

    std::unique_ptr<rhi_sampler_t> metal_rhi_context_t::create_sampler(const sampler_desc_t& desc) const
    {
        if (!_device || !_device->mtl_device())
            return nullptr;

        metal_sampler_create_info_t info{};

        // Filters
        info.min_filter = static_cast<uint32_t>(to_metal_filter(desc.min_filter));
        info.mag_filter = static_cast<uint32_t>(to_metal_filter(desc.mag_filter));
        info.mip_filter = static_cast<uint32_t>(to_metal_mip_filter(desc.mip_filter));

        // Address modes
        info.address_u = static_cast<uint32_t>(to_metal_address_mode(desc.address_u));
        info.address_v = static_cast<uint32_t>(to_metal_address_mode(desc.address_v));
        info.address_w = static_cast<uint32_t>(to_metal_address_mode(desc.address_w));

        // LOD controls
        info.mip_lod_bias = desc.mip_lod_bias;
        info.min_lod = desc.min_lod;
        info.max_lod = desc.max_lod;

        void* sampler_ptr{ metal_create_sampler_state(_device->mtl_device(), &info) };

        if (!sampler_ptr)
        {
            LOG_GRAPHICS_ERROR("Failed to create Metal sampler state via bridge");
            return nullptr;
        }

        // Wrap in RAII object
        MTL::SamplerState* sampler{ static_cast<MTL::SamplerState*>(sampler_ptr) };

        return std::make_unique<metal_sampler_t>(sampler, desc);
    }

    rhi_sampler_t* metal_rhi_context_t::get_or_create_sampler(const sampler_desc_t& desc)
    {
        if (const auto it{ _sampler_cache.find(desc) }; it != _sampler_cache.end())
            return it->second.get();

        std::unique_ptr<rhi_sampler_t> sampler{ create_sampler(desc) };
        if (!sampler)
            return nullptr;

        rhi_sampler_t* result{ sampler.get() };
        _sampler_cache.emplace(desc, std::move(sampler));

        return result;
    }

    void metal_rhi_context_t::wait_idle()
    {
        if (_active_command_buffer)
            _active_command_buffer->waitUntilCompleted();

        if (_command_queue)
            _command_queue->wait_idle();
    }

    bool metal_rhi_context_t::is_frame_active() const noexcept
    {
        return _active_command_buffer != nullptr && _active_drawable != nullptr;
    }

    bool metal_rhi_context_t::add_presentation_window(const window::window_id_t window_id,
                                                      const uint32_t presentation_channel_mask)
    {
        if (!_device || window_id == window::invalid_window_id || window_id == _presentation_window_id)
            return false;

        const bool already_registered{
            std::find_if(_auxiliary_surfaces.begin(),
                         _auxiliary_surfaces.end(),
                         [window_id](const auxiliary_surface_t& surface) {
                             return surface.id == window_id;
                         }) != _auxiliary_surfaces.end()
        };
        if (already_registered)
            return true;

        if (!window::has_window(window_id))
            return false;

        const core::platform::native_window_handle_t handle{ window::get_native_handle(window_id) };
        if (!handle.cocoa_t.metal_layer)
            return false;

        // Keep auxiliary window render-target format consistent with the primary pipeline target.
        metal_set_layer_pixel_format_srgb(handle.cocoa_t.metal_layer);

        _auxiliary_surfaces.push_back(auxiliary_surface_t{
            .id = window_id,
            .presentation_channel_mask = presentation_channel_mask,
            .swapchain = std::make_unique<metal_swapchain_t>(_device->mtl_device(),
                                                              handle.cocoa_t.metal_layer,
                                                              window::get_width(window_id),
                                                              window::get_height(window_id))
        });

        return true;
    }

    bool metal_rhi_context_t::remove_presentation_window(const window::window_id_t window_id)
    {
        if (window_id == window::invalid_window_id)
            return false;

        const auto old_size{ _auxiliary_surfaces.size() };
        std::erase_if(_auxiliary_surfaces, [window_id](const auxiliary_surface_t& surface) {
            return surface.id == window_id;
        });

        return _auxiliary_surfaces.size() != old_size;
    }

    void metal_rhi_context_t::acquire_auxiliary_drawables()
    {
        for (auto it = _auxiliary_surfaces.begin(); it != _auxiliary_surfaces.end();)
        {
            if (!window::has_window(it->id))
            {
                it = _auxiliary_surfaces.erase(it);
                continue;
            }

            it->swapchain->resize(window::get_width(it->id), window::get_height(it->id));
            it->swapchain->acquire_next_image(nullptr);
            it->drawable = static_cast<const CA::MetalDrawable*>(it->swapchain->get_current_drawable());
            ++it;
        }
    }

    void metal_rhi_context_t::release_auxiliary_drawables() noexcept
    {
        for (auxiliary_surface_t& surface : _auxiliary_surfaces)
            surface.drawable = nullptr;
    }

    void metal_rhi_context_t::encode_quad_stage(MTL::RenderCommandEncoder* encoder,
                                                const textured_quad_stage_record_t& stage,
                                                const uint32_t stage_slot,
                                                const quad_pipeline_kind_t pipeline_kind)
    {
        metal_textured_quad_pipeline_t* pipeline{
            pipeline_kind == quad_pipeline_kind_t::text ? _text_quad_pipeline.get() : _textured_quad_pipeline.get()
        };

        if (!encoder || !pipeline || !pipeline->is_valid())
            return;

        const metal_buffer_t* vertex_buffer{ dynamic_cast<const metal_buffer_t*>(stage.vertex_buffer) };
        const metal_buffer_t* index_buffer{ dynamic_cast<const metal_buffer_t*>(stage.index_buffer) };
        if (!vertex_buffer || !index_buffer || stage.batches.empty())
            return;

        if (stage_slot >= k_max_textured_quad_stage_slots_per_frame)
        {
            LOG_GRAPHICS_FATAL("Metal textured quad stage slot {} exceeds max supported stage slots {}",
                               stage_slot,
                               k_max_textured_quad_stage_slots_per_frame);
            return;
        }

        ensure_textured_quad_argument_capacity(stage_slot, stage.batches.size());

        const chlm::uint_rect viewport_rect{ stage.viewport.rect_px };

        MTL::Viewport viewport{ };
        viewport.originX = static_cast<double>(viewport_rect.position.x);
        viewport.originY = static_cast<double>(viewport_rect.position.y);
        viewport.width = static_cast<double>(viewport_rect.size.x);
        viewport.height = static_cast<double>(viewport_rect.size.y);
        viewport.znear = 0.0;
        viewport.zfar = 1.0;
        encoder->setViewport(viewport);

        MTL::ScissorRect scissor{ };
        scissor.x = viewport_rect.position.x;
        scissor.y = viewport_rect.position.y;
        scissor.width = viewport_rect.size.x;
        scissor.height = viewport_rect.size.y;
        encoder->setScissorRect(scissor);

        encoder->setRenderPipelineState(pipeline->state());
        encoder->setVertexBuffer(vertex_buffer->mtl_buffer(), 0, 0);

        renderer::world_forward_plus_uniform_t world_uniform{ };
        world_uniform.view_projection = stage.view_projection;
        world_uniform.ambient_color = stage.ambient_color;
        world_uniform.forward_plus_grid_params = stage.forward_plus_grid_params;
        world_uniform.forward_plus_tile_counts = stage.forward_plus_tile_counts;
        world_uniform.point_light_counts[0] = stage.point_light_count;
        world_uniform.point_lights = stage.point_lights;
        world_uniform.forward_plus_tiles = stage.forward_plus_tiles;
        world_uniform.forward_plus_light_indices = stage.forward_plus_light_indices;

        if (!_textured_quad_camera_uniform_buffers[stage_slot] ||
            !_textured_quad_camera_uniform_buffers[stage_slot]->write(&world_uniform, sizeof(world_uniform), 0))
        {
            LOG_GRAPHICS_WARN("Failed to upload Metal world forward+ uniform");
            return;
        }

        for (size_t i{ 0 }; i < stage.batches.size(); ++i)
        {
            const renderer::textured_quad_batch_t& batch{ stage.batches[i] };

            const metal_texture_t* texture{ dynamic_cast<const metal_texture_t*>(batch.texture) };
            if (!texture)
            {
                LOG_GRAPHICS_WARN("Skipping Metal textured quad batch with non-Metal texture");
                continue;
            }

            size_t root_ab_offset{ 0 };
            encode_textured_quad_argument_buffers(*texture, stage_slot, i, batch, root_ab_offset);
            encoder->setVertexBuffer(_textured_quad_root_argument_buffers[stage_slot]->mtl_buffer(),
                                     root_ab_offset,
                                     k_textured_quad_root_argument_buffer_index);
            encoder->setFragmentBuffer(_textured_quad_root_argument_buffers[stage_slot]->mtl_buffer(),
                                       root_ab_offset,
                                       k_textured_quad_root_argument_buffer_index);

            encoder->useResource(_textured_quad_cbv_descriptor_tables[stage_slot]->mtl_buffer(), MTL::ResourceUsageRead,
                                 MTL::RenderStageVertex);
            encoder->useResource(_textured_quad_camera_uniform_buffers[stage_slot]->mtl_buffer(), MTL::ResourceUsageRead,
                                 MTL::RenderStageVertex);
            encoder->useResource(_textured_quad_cbv_descriptor_tables[stage_slot]->mtl_buffer(), MTL::ResourceUsageRead,
                                 MTL::RenderStageFragment);
            encoder->useResource(_textured_quad_camera_uniform_buffers[stage_slot]->mtl_buffer(), MTL::ResourceUsageRead,
                                 MTL::RenderStageFragment);

            encoder->useResource(_textured_quad_srv_descriptor_tables[stage_slot]->mtl_buffer(), MTL::ResourceUsageRead,
                                 MTL::RenderStageFragment);

            encoder->useResource(_textured_quad_sampler_descriptor_tables[stage_slot]->mtl_buffer(), MTL::ResourceUsageRead,
                                 MTL::RenderStageFragment);

            encoder->useResource(texture->mtl_texture(), MTL::ResourceUsageRead, MTL::RenderStageFragment);

            encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, batch.index_count, MTL::IndexTypeUInt32,
                                           index_buffer->mtl_buffer(),
                                           batch.first_index * sizeof(uint32_t));
        }
    }

    void metal_rhi_context_t::reset_frame_state() noexcept
    {
        _render_encoder.end();

        if (_active_command_buffer)
        {
            _active_command_buffer->release();
            _active_command_buffer = nullptr;
        }

        _active_drawable = nullptr;
        release_auxiliary_drawables();
    }

    void metal_rhi_context_t::ensure_textured_quad_argument_capacity(const uint32_t stage_slot, const size_t batch_count)
    {
        if (batch_count <= _textured_quad_argument_capacities[stage_slot])
            return;

        const size_t new_capacity{
            std::max<size_t>(batch_count,
                             _textured_quad_argument_capacities[stage_slot] == 0 ? 16
                                                                                 : _textured_quad_argument_capacities[stage_slot] * 2)
        };

        const size_t root_buffer_size{ _textured_quad_root_stride * new_capacity };
        const size_t cbv_buffer_size{ _textured_quad_cbv_stride * new_capacity };
        const size_t srv_buffer_size{ _textured_quad_srv_stride * new_capacity };
        const size_t sampler_buffer_size{ _textured_quad_sampler_stride * new_capacity };

        MTL::Buffer* root_ab{ _device->mtl_device()->newBuffer(root_buffer_size, MTL::ResourceStorageModeShared) };
        MTL::Buffer* cbv_table{ _device->mtl_device()->newBuffer(cbv_buffer_size, MTL::ResourceStorageModeShared) };
        MTL::Buffer* srv_table{ _device->mtl_device()->newBuffer(srv_buffer_size, MTL::ResourceStorageModeShared) };
        MTL::Buffer* sampler_table{
            _device->mtl_device()->newBuffer(sampler_buffer_size, MTL::ResourceStorageModeShared)
        };

        if (!root_ab || !cbv_table || !srv_table || !sampler_table)
        {
            if (root_ab) root_ab->release();
            if (cbv_table) cbv_table->release();
            if (srv_table) srv_table->release();
            if (sampler_table) sampler_table->release();

            LOG_GRAPHICS_FATAL("Failed to allocate Metal textured quad argument buffers");
            return;
        }

        _textured_quad_root_argument_buffers[stage_slot] = std::make_unique<metal_buffer_t>(
            root_ab, root_buffer_size, buffer_usage_t::uniform
        );

        _textured_quad_cbv_descriptor_tables[stage_slot] = std::make_unique<metal_buffer_t>(
            cbv_table, cbv_buffer_size, buffer_usage_t::uniform
        );

        _textured_quad_srv_descriptor_tables[stage_slot] = std::make_unique<metal_buffer_t>(
            srv_table, srv_buffer_size, buffer_usage_t::uniform
        );

        _textured_quad_sampler_descriptor_tables[stage_slot] = std::make_unique<metal_buffer_t>(
            sampler_table, sampler_buffer_size, buffer_usage_t::uniform
        );

        std::memset(_textured_quad_root_argument_buffers[stage_slot]->mtl_buffer()->contents(), 0, root_buffer_size);
        std::memset(_textured_quad_cbv_descriptor_tables[stage_slot]->mtl_buffer()->contents(), 0, cbv_buffer_size);
        std::memset(_textured_quad_srv_descriptor_tables[stage_slot]->mtl_buffer()->contents(), 0, srv_buffer_size);
        std::memset(_textured_quad_sampler_descriptor_tables[stage_slot]->mtl_buffer()->contents(), 0, sampler_buffer_size);

        _textured_quad_argument_capacities[stage_slot] = new_capacity;
    }

    void metal_rhi_context_t::encode_textured_quad_argument_buffers(const metal_texture_t& texture,
                                                                    const uint32_t stage_slot,
                                                                    const size_t batch_index,
                                                                    const renderer::textured_quad_batch_t& batch,
                                                                    size_t& out_root_ab_offset)
    {
        const sampler_desc_t sampler_desc{ sampler_desc_from_preset(batch.sampler_preset) };
        const rhi_sampler_t* sampler_base{ get_or_create_sampler(sampler_desc) };
        if (!sampler_base)
        {
            LOG_GRAPHICS_WARN("Metal textured quad batch failed to resolve sampler");
            return;
        }

        const metal_sampler_t* metal_sampler{ dynamic_cast<const metal_sampler_t*>(sampler_base) };
        if (!metal_sampler || !metal_sampler->mtl_sampler())
        {
            LOG_GRAPHICS_WARN("Metal textured quad batch resolved non-Metal sampler");
            return;
        }

        MTL::SamplerState* sampler{ metal_sampler->mtl_sampler() };

        const size_t cbv_offset{ batch_index * _textured_quad_cbv_stride };
        const size_t srv_offset{ batch_index * _textured_quad_srv_stride };
        const size_t sampler_offset{ batch_index * _textured_quad_sampler_stride };
        const size_t root_offset{ batch_index * _textured_quad_root_stride };

        descriptor_table_entry_t* const cbv_ptr{
            reinterpret_cast<descriptor_table_entry_t*>(
                static_cast<uint8_t*>(_textured_quad_cbv_descriptor_tables[stage_slot]->mtl_buffer()->contents()) + cbv_offset
            )
        };

        descriptor_table_entry_t* const srv_ptr{
            reinterpret_cast<descriptor_table_entry_t*>(
                static_cast<uint8_t*>(_textured_quad_srv_descriptor_tables[stage_slot]->mtl_buffer()->contents()) + srv_offset
            )
        };

        descriptor_table_entry_t* const sampler_ptr{
            reinterpret_cast<descriptor_table_entry_t*>(
                static_cast<uint8_t*>(_textured_quad_sampler_descriptor_tables[stage_slot]->mtl_buffer()->contents()) +
                sampler_offset
            )
        };

        textured_quad_root_argument_buffer_t* const root_ptr{
            reinterpret_cast<textured_quad_root_argument_buffer_t*>(
                static_cast<uint8_t*>(_textured_quad_root_argument_buffers[stage_slot]->mtl_buffer()->contents()) + root_offset
            )
        };

        *cbv_ptr = encode_metal_constant_buffer_cbv_descriptor(_textured_quad_camera_uniform_buffers[stage_slot]->mtl_buffer(),
                                                               0,
                                                               sizeof(renderer::world_forward_plus_uniform_t));
        *srv_ptr = encode_metal_texture_srv_descriptor(texture.mtl_texture());
        *sampler_ptr = encode_metal_sampler_descriptor(sampler);

        *root_ptr = encode_textured_quad_root_argument_buffer(_textured_quad_cbv_descriptor_tables[stage_slot]->mtl_buffer(),
                                                              cbv_offset,
                                                              _textured_quad_srv_descriptor_tables[stage_slot]->mtl_buffer(),
                                                              srv_offset,
                                                              _textured_quad_sampler_descriptor_tables[stage_slot]->mtl_buffer(),
                                                              sampler_offset);

        out_root_ab_offset = root_offset;
    }

    size_t metal_rhi_context_t::align_up(const size_t value, const size_t alignment/* = 8*/) noexcept
    {
        return value + (alignment - 1) & ~(alignment - 1);
    }
} // namespace carrot::rhi::metal
