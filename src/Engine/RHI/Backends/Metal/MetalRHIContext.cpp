//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "MetalRHIContext.h"

#include "MetalDevice.h"
#include "MetalCommandQueue.h"
#include "MetalLayerBridge.h"
#include "Window/Window.h"

namespace carrot::rhi::metal {
    namespace {
        struct PushConstants
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

        // if (desc.enable_debug_layers)
        // {
        //     setenv("MTL_API_VALIDATION", "1", 1);           // Enables Metal API validation (errors on misuse)
        //     setenv("MTL_SHADER_VALIDATION", "1", 1);        // Validates shaders at runtime
        //     setenv("METAL_DEVICE_WRAPPER_TYPE", "1", 1);    // Enables extra debug checks on device objects
        //     setenv("MTL_DEBUG_LAYER", "1", 1);              // General debug layer (sometimes needed)
        //
        //     LOG_GRAPHICS_INFO("Metal validation layers enabled via environment variables");
        // }

        for (uint32_t i = 0; i < k_push_buffer_count; ++i)
        {
            _push_buffers[i] = MTL_CHECK_FATAL(
                _device->mtl_device()->newBuffer(sizeof(PushConstants), MTL::ResourceStorageModeShared)
            );
        }

        _frame_semaphore = dispatch_semaphore_create(k_max_frames_in_flight);

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

        MTL::VertexDescriptor* vertex_desc = MTL::VertexDescriptor::alloc()->init();

        // Tell Metal we have a constant buffer at binding 0
        auto* layout = vertex_desc->layouts()->object(0);
        layout->setStride(sizeof(PushConstants)); // size of the struct
        layout->setStepFunction(MTL::VertexStepFunctionConstant); // per-vertex? No — constant across vertices
        layout->setStepRate(1);

        // Create pipeline descriptor
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
        if (_device) _device.reset();
    }

    void metal_rhi_context_t::begin_frame() {}

    void metal_rhi_context_t::record_frame() {}

    void metal_rhi_context_t::end_frame()
    {
        dispatch_semaphore_wait(_frame_semaphore, DISPATCH_TIME_FOREVER);

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        void* drawable_handle{ metal_next_drawable(_metal_layer) };
        if (!drawable_handle)
        {
            LOG_GRAPHICS_WARN("No drawable available - skipping frame!");
            dispatch_semaphore_signal(_frame_semaphore);
            pool->release();
            return;
        }

        CA::MetalDrawable* drawable{ static_cast<CA::MetalDrawable *>(drawable_handle) };

        MTL::RenderPassDescriptor* rpd{ MTL::RenderPassDescriptor::alloc()->init() };
        if (!rpd)
        {
            dispatch_semaphore_signal(_frame_semaphore);
            pool->release();
            return;
        }

        // Clear to cornflower blue
        auto* colorAttachment = rpd->colorAttachments()->object(0);
        colorAttachment->setTexture(drawable->texture());
        colorAttachment->setLoadAction(MTL::LoadActionClear);
        colorAttachment->setStoreAction(MTL::StoreActionStore);
        colorAttachment->setClearColor(MTL::ClearColor(
            100.0 / 255.0, 149.0 / 255.0, 237.0 / 255.0, 1.0
        ));

        MTL::CommandBuffer* cmdBuf = _command_queue->mtl_command_queue()->commandBuffer();
        cmdBuf->addCompletedHandler([this](MTL::CommandBuffer* /*buffer*/) {
            dispatch_semaphore_signal(_frame_semaphore);
        });

        MTL::RenderCommandEncoder* encoder = cmdBuf->renderCommandEncoder(rpd);

        if (encoder && _triangle_pipeline)
        {
            encoder->setRenderPipelineState(_triangle_pipeline.state);

            MTL::Buffer* current_buf = _push_buffers[_current_push_index];

            PushConstants* mapped = static_cast<PushConstants *>(current_buf->contents());
            mapped->frame_count = _frame_counter++;
            LOG_GRAPHICS_TRACE("Writing frame count {} to buffer {}", mapped->frame_count, _current_push_index);
            current_buf->didModifyRange(NS::Range(0, sizeof(PushConstants)));

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
        metal_release_drawable(drawable);
        pool->release();
    }

    void metal_rhi_context_t::resize(uint32_t width, uint32_t height) {}

    rhi_device_t* metal_rhi_context_t::get_device() const noexcept
    {
        return _device.get();
    }

    rhi_swapchain_t* metal_rhi_context_t::get_swapchain() const noexcept
    {
        return nullptr;
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
