//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalRHIContext.h"

#include "MetalBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalDevice.h"
#include "MetalLayerBridge.h"
#include "MetalSwapchain.h"
#include "Pipelines/MetalTexturedQuadPipeline.h"
#include "MetalTexture.h"
#include "Window/Window.h"

namespace carrot::rhi::metal {
    namespace {
        [[nodiscard]] MTL::PixelFormat to_metal_pixel_format(const texture_format_t format) noexcept
        {
            switch (format)
            {
                case texture_format_t::rgba8_unorm: return MTL::PixelFormatRGBA8Unorm;
                case texture_format_t::rgba8_srgb:  return MTL::PixelFormatRGBA8Unorm_sRGB;
                default:                            return MTL::PixelFormatRGBA8Unorm_sRGB;
            }
        }
    } // namespace

    size_t metal_rhi_context_t::align_up(const size_t value, const size_t alignment) noexcept
    {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    metal_rhi_context_t::metal_rhi_context_t(const rhi_desc_t& desc)
    {
        core::platform::native_window_handle_t window_handle{ window::get_primary_window().get_native_handle() };
        _metal_layer = window_handle.cocoa_t.metal_layer;

        if (!_metal_layer)
        {
            LOG_GRAPHICS_FATAL("No CAMetalLayer provided by window");
            return;
        }

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
            MTL::PixelFormatBGRA8Unorm_sRGB
        );

        if (!_textured_quad_pipeline || !_textured_quad_pipeline->is_valid())
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal textured quad pipeline");
            return;
        }

        MTL::SamplerDescriptor* sampler_desc{ MTL::SamplerDescriptor::alloc()->init() };
        if (!sampler_desc)
        {
            LOG_GRAPHICS_FATAL("Failed to allocate Metal sampler descriptor");
            return;
        }

        sampler_desc->setMinFilter(MTL::SamplerMinMagFilterNearest);
        sampler_desc->setMagFilter(MTL::SamplerMinMagFilterNearest);
        sampler_desc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
        sampler_desc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
        sampler_desc->setSupportArgumentBuffers(true);

        _textured_quad_sampler = mtl_device->newSamplerState(sampler_desc);
        sampler_desc->release();

        if (!_textured_quad_sampler)
        {
            LOG_GRAPHICS_FATAL("Failed to create Metal sampler state");
            return;
        }

        _textured_quad_root_stride =
            align_up(sizeof(textured_quad_root_argument_buffer_t), 8);
        _textured_quad_srv_stride =
            align_up(sizeof(descriptor_table_entry_t), 8);

        ensure_textured_quad_argument_capacity(16);
        initialize_textured_quad_sampler_table();

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

        if (_textured_quad_sampler)
        {
            _textured_quad_sampler->release();
            _textured_quad_sampler = nullptr;
        }

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

    void metal_rhi_context_t::ensure_textured_quad_argument_capacity(const size_t batch_count)
    {
        if (batch_count <= _textured_quad_argument_capacity)
            return;

        const size_t new_capacity{ std::max<size_t>(batch_count, _textured_quad_argument_capacity == 0 ? 16 : _textured_quad_argument_capacity * 2) };

        const size_t root_buffer_size{ _textured_quad_root_stride * new_capacity };
        const size_t srv_buffer_size{ _textured_quad_srv_stride * new_capacity };
        const size_t sampler_buffer_size{ sizeof(descriptor_table_entry_t) };

        MTL::Buffer* root_ab{ _device->mtl_device()->newBuffer(root_buffer_size, MTL::ResourceStorageModeShared) };
        MTL::Buffer* srv_table{ _device->mtl_device()->newBuffer(srv_buffer_size, MTL::ResourceStorageModeShared) };
        MTL::Buffer* sampler_table{ nullptr };

        if (_textured_quad_sampler_descriptor_table)
        {
            sampler_table = _textured_quad_sampler_descriptor_table->mtl_buffer();
            sampler_table->retain();
        }
        else
        {
            sampler_table = _device->mtl_device()->newBuffer(sampler_buffer_size, MTL::ResourceStorageModeShared);
        }

        if (!root_ab || !srv_table || !sampler_table)
        {
            if (root_ab) root_ab->release();
            if (srv_table) srv_table->release();
            if (sampler_table) sampler_table->release();

            LOG_GRAPHICS_FATAL("Failed to allocate Metal textured quad argument buffers");
            return;
        }

        _textured_quad_root_argument_buffer = std::make_unique<metal_buffer_t>(
            root_ab, root_buffer_size, buffer_usage_t::uniform
        );

        _textured_quad_srv_descriptor_table = std::make_unique<metal_buffer_t>(
            srv_table, srv_buffer_size, buffer_usage_t::uniform
        );

        _textured_quad_sampler_descriptor_table = std::make_unique<metal_buffer_t>(
            sampler_table, sampler_buffer_size, buffer_usage_t::uniform
        );

        std::memset(_textured_quad_root_argument_buffer->mtl_buffer()->contents(), 0, root_buffer_size);
        std::memset(_textured_quad_srv_descriptor_table->mtl_buffer()->contents(), 0, srv_buffer_size);
        std::memset(_textured_quad_sampler_descriptor_table->mtl_buffer()->contents(), 0, sampler_buffer_size);

        _textured_quad_argument_capacity = new_capacity;

        initialize_textured_quad_sampler_table();
    }

    void metal_rhi_context_t::initialize_textured_quad_sampler_table()
    {
        if (!_textured_quad_sampler_descriptor_table || !_textured_quad_sampler)
            return;

        descriptor_table_entry_t sampler_entry{};
        sampler_entry.gpu_address_or_resource_id = metal_sampler_resource_id(_textured_quad_sampler);
        sampler_entry.texture_or_sampler_resource_id = 0;
        sampler_entry.metadata = 0;

        std::memcpy(_textured_quad_sampler_descriptor_table->mtl_buffer()->contents(),
                    &sampler_entry,
                    sizeof(sampler_entry));
    }

    void metal_rhi_context_t::begin_frame()
    {
        if (!_device || !_swapchain || !_command_queue)
            return;

        dispatch_semaphore_wait(_frame_semaphore, DISPATCH_TIME_FOREVER);

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

        _render_encoder.begin(
            _active_command_buffer,
            _active_drawable,
            MTL::ClearColor(0.02, 0.02, 0.04, 1.0)
        );
    }

    void metal_rhi_context_t::record_frame()
    {
        if (!is_frame_active())
            return;

        MTL::RenderCommandEncoder* encoder{ _render_encoder.encoder() };
        if (!encoder)
            return;

        if (!_textured_quad_pipeline || !_textured_quad_pipeline->is_valid())
            return;

        if (!_textured_quad_vertex_buffer || !_textured_quad_index_buffer || _textured_quad_batches.empty())
            return;

        ensure_textured_quad_argument_capacity(_textured_quad_batches.size());

        MTL::Viewport viewport{};
        viewport.originX = 0.0;
        viewport.originY = 0.0;
        viewport.width = static_cast<double>(_swapchain->get_width());
        viewport.height = static_cast<double>(_swapchain->get_height());
        viewport.znear = 0.0;
        viewport.zfar = 1.0;
        encoder->setViewport(viewport);

        encoder->setRenderPipelineState(_textured_quad_pipeline->state());
        encoder->setVertexBuffer(_textured_quad_vertex_buffer->mtl_buffer(), 0, 0);

        for (size_t i = 0; i < _textured_quad_batches.size(); ++i)
        {
            const renderer::textured_quad_batch_t& batch{ _textured_quad_batches[i] };

            const metal_texture_t* texture{ dynamic_cast<const metal_texture_t*>(batch.texture) };
            if (!texture)
            {
                LOG_GRAPHICS_WARN("Skipping Metal textured quad batch with non-Metal texture");
                continue;
            }

            size_t root_ab_offset{ 0 };
            encode_textured_quad_argument_buffers(*texture, i, root_ab_offset);

            encoder->setFragmentBuffer(
                _textured_quad_root_argument_buffer->mtl_buffer(),
                static_cast<NS::UInteger>(root_ab_offset),
                2
            );

            encoder->useResource(
                _textured_quad_srv_descriptor_table->mtl_buffer(),
                MTL::ResourceUsageRead,
                MTL::RenderStageFragment
            );

            encoder->useResource(
                _textured_quad_sampler_descriptor_table->mtl_buffer(),
                MTL::ResourceUsageRead,
                MTL::RenderStageFragment
            );

            encoder->useResource(texture->mtl_texture(), MTL::ResourceUsageRead, MTL::RenderStageFragment);

            encoder->drawIndexedPrimitives(
                MTL::PrimitiveTypeTriangle,
                static_cast<NS::UInteger>(batch.index_count),
                MTL::IndexTypeUInt32,
                _textured_quad_index_buffer->mtl_buffer(),
                static_cast<NS::UInteger>(batch.first_index * sizeof(uint32_t))
            );
        }
    }

    void metal_rhi_context_t::encode_textured_quad_argument_buffers(const metal_texture_t& texture,
                                                                    const size_t batch_index,
                                                                    size_t& out_root_ab_offset)
    {
        using descriptor_table_entry_t = descriptor_table_entry_t;
        using root_ab_t = textured_quad_root_argument_buffer_t;

        const size_t srv_offset{ batch_index * _textured_quad_srv_stride };
        const size_t root_offset{ batch_index * _textured_quad_root_stride };

        auto* const srv_ptr{
            reinterpret_cast<descriptor_table_entry_t*>(
                static_cast<uint8_t*>(_textured_quad_srv_descriptor_table->mtl_buffer()->contents()) + srv_offset
            )
        };

        auto* const root_ptr{
            reinterpret_cast<root_ab_t*>(
                static_cast<uint8_t*>(_textured_quad_root_argument_buffer->mtl_buffer()->contents()) + root_offset
            )
        };

        srv_ptr->gpu_address_or_resource_id = metal_texture_resource_id(texture.mtl_texture());
        srv_ptr->texture_or_sampler_resource_id = metal_texture_resource_id(texture.mtl_texture());
        srv_ptr->metadata = 0;

        root_ptr->srv_table_gpu_address = metal_buffer_gpu_address(_textured_quad_srv_descriptor_table->mtl_buffer()) + srv_offset;
        root_ptr->sampler_table_gpu_address = metal_buffer_gpu_address(_textured_quad_sampler_descriptor_table->mtl_buffer());

        out_root_ab_offset = root_offset;
    }

    void metal_rhi_context_t::end_frame()
    {
        if (!is_frame_active())
            return;

        _render_encoder.end();

        _active_command_buffer->presentDrawable(_active_drawable);
        _active_command_buffer->commit();
        _active_command_buffer->release();
        _active_command_buffer = nullptr;

        _active_drawable = nullptr;

        _textured_quad_vertex_buffer = nullptr;
        _textured_quad_index_buffer = nullptr;
        _textured_quad_batches.clear();
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
                info.initial_data_stride_bytes ? info.initial_data_stride_bytes : (info.width * 4)
            };

            std::vector<uint8_t> flipped(static_cast<size_t>(bytes_per_row) * info.height);
            const auto* src = static_cast<const uint8_t*>(info.initial_data);

            for (uint32_t y = 0; y < info.height; ++y)
            {
                const uint32_t src_y = info.height - 1 - y;

                std::memcpy(
                    flipped.data() + static_cast<size_t>(y) * bytes_per_row,
                    src + static_cast<size_t>(src_y) * bytes_per_row,
                    bytes_per_row
                );
            }

            texture->replaceRegion(
                MTL::Region(0, 0, 0, info.width, info.height, 1),
                0,
                flipped.data(),
                bytes_per_row
            );
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

        const MTL::ResourceOptions options{ MTL::ResourceStorageModeShared };
        MTL::Buffer* buffer{ _device->mtl_device()->newBuffer(info.size_bytes, options) };
        if (!buffer)
        {
            LOG_GRAPHICS_ERROR("Failed to create Metal buffer ({} bytes)", info.size_bytes);
            return nullptr;
        }

        auto result{ std::make_unique<metal_buffer_t>(buffer, info.size_bytes, info.usage) };

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

    void metal_rhi_context_t::set_textured_quad_geometry(const rhi_buffer_t& vertex_buffer,
                                                         const rhi_buffer_t& index_buffer)
    {
        _textured_quad_vertex_buffer = dynamic_cast<const metal_buffer_t*>(&vertex_buffer);
        _textured_quad_index_buffer = dynamic_cast<const metal_buffer_t*>(&index_buffer);

        if (!_textured_quad_vertex_buffer || !_textured_quad_index_buffer)
            LOG_GRAPHICS_FATAL("Metal textured quad geometry received non-Metal buffers");
    }

    void metal_rhi_context_t::set_textured_quad_batches(std::span<const renderer::textured_quad_batch_t> batches)
    {
        _textured_quad_batches.assign(batches.begin(), batches.end());
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

    void metal_rhi_context_t::reset_frame_state() noexcept
    {
        if (_active_command_buffer)
        {
            _active_command_buffer->release();
            _active_command_buffer = nullptr;
        }

        _active_drawable = nullptr;

        _textured_quad_vertex_buffer = nullptr;
        _textured_quad_index_buffer = nullptr;
        _textured_quad_batches.clear();
    }
} // namespace carrot::rhi::metal
