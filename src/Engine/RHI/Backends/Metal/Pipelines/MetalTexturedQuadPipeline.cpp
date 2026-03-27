//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalTexturedQuadPipeline.h"

#include "Assets/Shaders/VFSShaderFileProvider.h"
#include "RHI/Backends/Metal/MetalDevice.h"
#include "Renderer/Primitives/QuadVertex.h"
#include "Utils/File/FileUtils.h"

namespace carrot::rhi::metal {
    namespace {
        constexpr std::string_view k_vertex_shader_path{ "engine://shaders/metal/textured_quad.vert.metallib" };
        constexpr std::string_view k_fragment_shader_path{ "engine://shaders/metal/textured_quad.frag.metallib" };

        template<typename T>
        std::shared_ptr<T> make_mtl_shared(T* obj)
        {
            return std::shared_ptr<T>(obj, [](T* p) {
                if (p) p->release();
            });
        }
    }

    metal_textured_quad_pipeline_t::metal_textured_quad_pipeline_t(const metal_device_t& device,
                                                                   assets::shader_file_provider_t& shader_files,
                                                                   const MTL::PixelFormat color_format)
    {
        MTL::Device* mtl_device{ device.mtl_device() };
        if (!mtl_device)
        {
            LOG_GRAPHICS_FATAL("Metal pipeline received null device");
            return;
        }

        _vertex_descriptor = make_mtl_shared(create_vertex_descriptor());
        if (!_vertex_descriptor)
        {
            LOG_GRAPHICS_FATAL("Failed to create vertex descriptor");
            return;
        }

        MTL::Library* vs_lib{ load_library(mtl_device, shader_files, k_vertex_shader_path) };
        MTL::Library* fs_lib{ load_library(mtl_device, shader_files, k_fragment_shader_path) };

        if (!vs_lib || !fs_lib)
        {
            LOG_GRAPHICS_FATAL("Failed to load Metal shader libraries");
            return;
        }

        std::shared_ptr<MTL::Library> vs_library = make_mtl_shared(vs_lib);
        std::shared_ptr<MTL::Library> fs_library = make_mtl_shared(fs_lib);

        std::shared_ptr<MTL::Function> vs_func = make_mtl_shared(load_function(vs_library.get()));
        std::shared_ptr<MTL::Function> fs_func = make_mtl_shared(load_function(fs_library.get()));

        if (!vs_func || !fs_func)
        {
            LOG_GRAPHICS_FATAL("Failed to load Metal shader functions");
            return;
        }

        MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
        if (!desc)
        {
            LOG_GRAPHICS_FATAL("Failed to allocate pipeline descriptor");
            return;
        }

        desc->setVertexFunction(vs_func.get());
        desc->setFragmentFunction(fs_func.get());
        desc->setVertexDescriptor(_vertex_descriptor.get());

        auto* color = desc->colorAttachments()->object(0);
        color->setPixelFormat(color_format);

        // Alpha blending (very important for sprites)
        color->setBlendingEnabled(true);
        color->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
        color->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
        color->setRgbBlendOperation(MTL::BlendOperationAdd);
        color->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
        color->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
        color->setAlphaBlendOperation(MTL::BlendOperationAdd);

        NS::Error* error{ nullptr };
        MTL::RenderPipelineState* state = mtl_device->newRenderPipelineState(desc, &error);

        desc->release();

        if (!state)
        {
            const char* msg = error ? error->localizedDescription()->utf8String() : "Unknown error";
            LOG_GRAPHICS_FATAL("Failed to create pipeline: {}", msg);
            return;
        }

        _state = make_mtl_shared(state);

        LOG_GRAPHICS_INFO("Metal textured quad pipeline created");
    }

    metal_textured_quad_pipeline_t::~metal_textured_quad_pipeline_t() = default;

    bool metal_textured_quad_pipeline_t::is_valid() const noexcept
    {
        return _state != nullptr;
    }

    MTL::RenderPipelineState* metal_textured_quad_pipeline_t::state() const noexcept
    {
        return _state.get();
    }

    MTL::VertexDescriptor* metal_textured_quad_pipeline_t::vertex_descriptor() const noexcept
    {
        return _vertex_descriptor.get();
    }

    MTL::VertexDescriptor* metal_textured_quad_pipeline_t::create_vertex_descriptor()
    {
        MTL::VertexDescriptor* desc = MTL::VertexDescriptor::alloc()->init();
        if (!desc)
            return nullptr;

        constexpr NS::UInteger stride = sizeof(renderer::quad_vertex_t);

        auto* layout = desc->layouts()->object(0);
        layout->setStride(stride);
        layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
        layout->setStepRate(1);

        auto* pos = desc->attributes()->object(11);
        pos->setFormat(MTL::VertexFormatFloat2);
        pos->setOffset(offsetof(renderer::quad_vertex_t, x));
        pos->setBufferIndex(0);

        auto* uv = desc->attributes()->object(12);
        uv->setFormat(MTL::VertexFormatFloat2);
        uv->setOffset(offsetof(renderer::quad_vertex_t, u));
        uv->setBufferIndex(0);

        auto* col = desc->attributes()->object(13);
        col->setFormat(MTL::VertexFormatUChar4Normalized);
        col->setOffset(offsetof(renderer::quad_vertex_t, color));
        col->setBufferIndex(0);

        return desc;
    }

    MTL::Library* metal_textured_quad_pipeline_t::load_library(MTL::Device* device,
                                                               const assets::shader_file_provider_t& shader_files,
                                                               const std::string_view virtual_path)
    {
        const auto native = shader_files.resolve(virtual_path);
        if (!native)
        {
            LOG_GRAPHICS_FATAL("Failed to resolve shader path: {}", virtual_path);
            return nullptr;
        }

        auto bytes = utils::file::load_binary_file(*native);
        if (!bytes || bytes->empty())
        {
            LOG_GRAPHICS_FATAL("Failed to read shader file: {}", utils::file::to_log_string(*native));
            return nullptr;
        }

        dispatch_data_t data = dispatch_data_create(
            bytes->data(),
            bytes->size(),
            dispatch_get_main_queue(),
            DISPATCH_DATA_DESTRUCTOR_DEFAULT
        );

        if (!data)
        {
            LOG_GRAPHICS_FATAL("Failed to create dispatch_data");
            return nullptr;
        }

        NS::Error* error{ nullptr };
        MTL::Library* lib = device->newLibrary(data, &error);

        dispatch_release(data);

        if (!lib)
        {
            const char* msg = error ? error->localizedDescription()->utf8String() : "Unknown error";
            LOG_GRAPHICS_FATAL("Failed to create Metal library '{}': {}", virtual_path, msg);
            return nullptr;
        }

        return lib;
    }

    MTL::Function* metal_textured_quad_pipeline_t::load_function(MTL::Library* lib)
    {
        auto* names = lib->functionNames();
        if (!names || names->count() == 0)
        {
            LOG_GRAPHICS_FATAL("Shader library has no functions");
            return nullptr;
        }

        auto* name = static_cast<NS::String*>(names->object(0));
        if (!name)
        {
            LOG_GRAPHICS_FATAL("Null function name in shader library");
            return nullptr;
        }

        MTL::Function* func = lib->newFunction(name);
        if (!func)
        {
            LOG_GRAPHICS_FATAL("Failed to create function '{}'", name->utf8String());
            return nullptr;
        }

        return func;
    }
}
