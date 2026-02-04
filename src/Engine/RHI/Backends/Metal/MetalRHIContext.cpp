//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "MetalRHIContext.h"

#include "MetalDevice.h"
#include "MetalCommandQueue.h"
#include "MetalLayerBridge.h"
#include "MetalSwapchain.h"
#include "Window/Window.h"

namespace carrot::rhi::metal {
    namespace {
        struct push_constants
        {
            uint32_t frame_count;
        };
    } // anonymous nanmespace

    metal_rhi_context_t::metal_rhi_context_t(const rhi_desc_t& desc)
    {
        core::platform::native_window_handle_t window_handle{ window::get_primary_window().get_native_handle() };
        _metal_layer = window_handle.cocoa_t.metal_layer;

        if (!_metal_layer)
        {
            LOG_GRAPHICS_FATAL("No CAMetalLayer provided by window");
            return;
        }

        MTL::Device* mtl_device{ static_cast<MTL::Device*>(metal_layer_get_device(_metal_layer)) };
        _device = std::make_unique<metal_device_t>(mtl_device);
        MTL_CHECK_FATAL(_device->mtl_device());

        if (desc.enable_debug_layers)
        {
            setenv("MTL_API_VALIDATION", "1", 1);           // Enables Metal API validation (errors on misuse)
            setenv("MTL_SHADER_VALIDATION", "1", 1);        // Validates shaders at runtime
            setenv("METAL_DEVICE_WRAPPER_TYPE", "1", 1);    // Enables extra debug checks on device objects
            setenv("MTL_DEBUG_LAYER", "1", 1);              // General debug layer (sometimes needed)

            LOG_GRAPHICS_INFO("Metal validation layers enabled via environment variables");
        }

        for (uint32_t i{ 0 }; i < k_push_buffer_count; ++i)
        {
            _push_buffers[i] = MTL_CHECK_FATAL(
                _device->mtl_device()->newBuffer(sizeof(push_constants), MTL::ResourceStorageModeShared)
            );
        }

        _frame_semaphore = dispatch_semaphore_create(k_max_frames_in_flight);

        _swapchain = std::make_unique<metal_swapchain_t>(_device->mtl_device(), _metal_layer, desc.width, desc.height);

        _command_queue = std::make_unique<metal_command_queue_t>(
            MTL_CHECK_FATAL(_device->mtl_device()->newCommandQueue()));

        NS::String* vert_shader{
            NS::String::string("shaders/triangle.vert.metallib", NS::StringEncoding::UTF8StringEncoding)
        };
        NS::String* frag_shader{
            NS::String::string("shaders/triangle.frag.metallib", NS::StringEncoding::UTF8StringEncoding)
        };

        NS::Error* error{ nullptr };

        MTL::Library* vertex_lib{ MTL_CHECKED_FATAL(_device->mtl_device()->newLibrary(vert_shader, &error), &error) };
        MTL::Library* fragment_lib{ MTL_CHECKED_FATAL(_device->mtl_device()->newLibrary(frag_shader, &error), &error) };

        MTL::Function* vertex_fn{
            MTL_CHECK_FATAL(vertex_lib->newFunction(NS::String::string("main", NS::UTF8StringEncoding)))
        };
        MTL::Function* fragment_fn{
            MTL_CHECK_FATAL(fragment_lib->newFunction(NS::String::string("main", NS::UTF8StringEncoding)))
        };

        if (!vertex_fn || !fragment_fn)
        {
            LOG_GRAPHICS_FATAL("Failed to find 'main' entry points in shaders");
            return;
        }

        MTL::VertexDescriptor* vertex_desc{ MTL::VertexDescriptor::alloc()->init() };

        MTL::VertexBufferLayoutDescriptor* layout{ vertex_desc->layouts()->object(0) };
        layout->setStride(sizeof(push_constants));
        layout->setStepFunction(MTL::VertexStepFunctionConstant);
        layout->setStepRate(1);

        MTL::RenderPipelineDescriptor* pipeline_desc{ MTL::RenderPipelineDescriptor::alloc()->init() };
        pipeline_desc->setVertexFunction(vertex_fn);
        pipeline_desc->setFragmentFunction(fragment_fn);
        pipeline_desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
        pipeline_desc->setVertexDescriptor(vertex_desc);

        _triangle_pipeline.state = MTL_CHECKED_FATAL(
            _device->mtl_device()->newRenderPipelineState(pipeline_desc, &error), &error);

        if (!_triangle_pipeline) return;

        // Clean-up
        vertex_fn->release();
        fragment_fn->release();
        vertex_desc->release();
        pipeline_desc->release();
        vertex_lib->release();
        fragment_lib->release();

        LOG_GRAPHICS_INFO("Metal RHI context created successfully");
        LOG_GRAPHICS_INFO("Device: {}", _device->mtl_device()->name()->utf8String());
    }

    metal_rhi_context_t::~metal_rhi_context_t()
    {
        for (const auto& buf: _push_buffers)
        {
            if (buf) buf->release();
        }

        if (_command_queue) _command_queue.reset();
        if (_swapchain) _swapchain.reset();
        if (_device) _device.reset();
    }

    void metal_rhi_context_t::begin_frame() {}

    void metal_rhi_context_t::record_frame() {}

    void metal_rhi_context_t::end_frame()
    {
        dispatch_semaphore_wait(_frame_semaphore, DISPATCH_TIME_FOREVER);

        NS::AutoreleasePool* pool{ NS::AutoreleasePool::alloc()->init() };

        uint32_t image_index{ _swapchain->acquire_next_image(nullptr) };

        const CA::MetalDrawable* drawable{ static_cast<CA::MetalDrawable *>(_swapchain->get_current_drawable()) };
        if (!drawable)
        {
            LOG_GRAPHICS_WARN("No drawable available - skipping frame!");
            dispatch_semaphore_signal(_frame_semaphore);
            pool->release();
            return;
        }

        MTL::RenderPassDescriptor* rpd{ MTL::RenderPassDescriptor::alloc()->init() };
        if (!rpd)
        {
            dispatch_semaphore_signal(_frame_semaphore);
            pool->release();
            return;
        }

        auto* color_attachment{ rpd->colorAttachments()->object(0) };
        color_attachment->setTexture(drawable->texture());
        color_attachment->setLoadAction(MTL::LoadActionClear);
        color_attachment->setStoreAction(MTL::StoreActionStore);
        color_attachment->setClearColor(MTL::ClearColor(0.02f, 0.02f, 0.04f, 1.0f));

        MTL::CommandBuffer* cmdBuf{ _command_queue->mtl_command_queue()->commandBuffer() };
        cmdBuf->addCompletedHandler([this](MTL::CommandBuffer* /*buffer*/) {
            dispatch_semaphore_signal(_frame_semaphore);
        });

        MTL::RenderCommandEncoder* encoder{ cmdBuf->renderCommandEncoder(rpd) };

        if (encoder && _triangle_pipeline)
        {
            encoder->setRenderPipelineState(_triangle_pipeline.state);

            MTL::Buffer* current_buf{ _push_buffers[_current_push_index] };

            push_constants* mapped{ static_cast<push_constants *>(current_buf->contents()) };
            mapped->frame_count = _frame_counter++;
            current_buf->didModifyRange(NS::Range(0, sizeof(push_constants)));

            encoder->setVertexBuffer(current_buf, 0, 2);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0),
                                    static_cast<NS::UInteger>(3));

            encoder->endEncoding();

            _current_push_index = (_current_push_index + 1) % k_push_buffer_count;
        }
        else if (encoder)
        {
            encoder->endEncoding();
        }

        cmdBuf->presentDrawable(drawable);
        cmdBuf->commit();

        rpd->release();
        pool->release();
    }

    void metal_rhi_context_t::resize(uint32_t width, uint32_t height) {}

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

    void metal_rhi_context_t::wait_idle()
    {
        // NOTE: Metal doesn't have a direct function like Vulkan's vkDeviceWaitIdle, but we
        //       can approximate it with a dummy command buffer that waits on completion
        if (!_command_queue) return;

        MTL::CommandQueue* native_queue{ _command_queue->mtl_command_queue() };
        if (!native_queue) return;

        NS::AutoreleasePool* pool{ NS::AutoreleasePool::alloc()->init() };

        MTL::CommandBuffer* cmd{ native_queue->commandBuffer() };
        cmd->commit();
        cmd->waitUntilCompleted(); // blocks until GPU finishes this buffer

        pool->release();
    }
} // namespace carrot::rhi::metal
