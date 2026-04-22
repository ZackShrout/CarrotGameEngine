//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalTexturedQuadPipeline.h"

#include "Assets/Shaders/VFSShaderFileProvider.h"
#include "RHI/Backends/Metal/MetalDevice.h"
#include "Renderer/Draw/QuadInstanceData.h"
#include "Renderer/Primitives/QuadVertex.h"
#include "Utils/File/FileUtils.h"

namespace carrot::rhi::metal {
    namespace {
        template<typename T>
        std::shared_ptr<T> make_mtl_shared(T* obj)
        {
            return std::shared_ptr<T>(obj, [](T* p) {
                if (p) p->release();
            });
        }

        void configure_attribute(MTL::VertexDescriptor* desc,
                                 const NS::UInteger attribute_index,
                                 const MTL::VertexFormat format,
                                 const NS::UInteger offset,
                                 const NS::UInteger buffer_index)
        {
            MTL::VertexAttributeDescriptor* attribute{ desc->attributes()->object(attribute_index) };
            attribute->setFormat(format);
            attribute->setOffset(offset);
            attribute->setBufferIndex(buffer_index);
        }
    } // anonymous namespace

    // PUBLIC

    metal_textured_quad_pipeline_t::metal_textured_quad_pipeline_t(const metal_device_t& device,
                                                                   const assets::shader_file_provider_t& shader_files,
                                                                   const MTL::PixelFormat color_format,
                                                                   const std::string_view vertex_shader_path,
                                                                   const std::string_view fragment_shader_path,
                                                                   const std::string_view debug_name,
                                                                   const bool instanced,
                                                                   const blend_mode_t blend_mode)
    {
        MTL::Device* mtl_device{ device.mtl_device() };
        if (!mtl_device)
        {
            LOG_GRAPHICS_FATAL("Metal pipeline received null device");
            return;
        }

        _vertex_descriptor = make_mtl_shared(create_vertex_descriptor(instanced));
        if (!_vertex_descriptor)
        {
            LOG_GRAPHICS_FATAL("Failed to create vertex descriptor");
            return;
        }

        MTL::Library* vs_lib{
            load_library(mtl_device, shader_files, vertex_shader_path)
        };
        MTL::Library* fs_lib{
            load_library(mtl_device, shader_files, fragment_shader_path)
        };

        if (!vs_lib || !fs_lib)
        {
            LOG_GRAPHICS_FATAL("Failed to load Metal shader libraries");
            return;
        }

        const std::shared_ptr<MTL::Library> vs_library{ make_mtl_shared(vs_lib) };
        const std::shared_ptr<MTL::Library> fs_library{ make_mtl_shared(fs_lib) };

        const std::shared_ptr<MTL::Function> vs_func{ make_mtl_shared(load_function(vs_library.get())) };
        const std::shared_ptr<MTL::Function> fs_func{ make_mtl_shared(load_function(fs_library.get())) };

        if (!vs_func || !fs_func)
        {
            LOG_GRAPHICS_FATAL("Failed to load Metal shader functions");
            return;
        }

        MTL::RenderPipelineDescriptor* desc{ MTL::RenderPipelineDescriptor::alloc()->init() };
        if (!desc)
        {
            LOG_GRAPHICS_FATAL("Failed to allocate pipeline descriptor");
            return;
        }

        desc->setVertexFunction(vs_func.get());
        desc->setFragmentFunction(fs_func.get());
        desc->setVertexDescriptor(_vertex_descriptor.get());

        MTL::RenderPipelineColorAttachmentDescriptor* color{ desc->colorAttachments()->object(0) };
        color->setPixelFormat(color_format);

        color->setBlendingEnabled(true);
        if (blend_mode == blend_mode_t::additive)
        {
            color->setSourceRGBBlendFactor(MTL::BlendFactorOne);
            color->setDestinationRGBBlendFactor(MTL::BlendFactorOne);
            color->setRgbBlendOperation(MTL::BlendOperationAdd);
            color->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
            color->setDestinationAlphaBlendFactor(MTL::BlendFactorOne);
            color->setAlphaBlendOperation(MTL::BlendOperationAdd);
        }
        else
        {
            color->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
            color->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
            color->setRgbBlendOperation(MTL::BlendOperationAdd);
            color->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
            color->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
            color->setAlphaBlendOperation(MTL::BlendOperationAdd);
        }

        NS::Error* error{ nullptr };
        MTL::RenderPipelineState* state{ mtl_device->newRenderPipelineState(desc, &error) };

        desc->release();

        if (!state)
        {
            const char* msg = error ? error->localizedDescription()->utf8String() : "Unknown error";
            LOG_GRAPHICS_FATAL("Failed to create pipeline: {}", msg);
            return;
        }

        _state = make_mtl_shared(state);

        LOG_GRAPHICS_INFO("Metal {} pipeline created", debug_name);
    }

    // PRIVATE

    MTL::VertexDescriptor* metal_textured_quad_pipeline_t::create_vertex_descriptor(const bool instanced)
    {
        MTL::VertexDescriptor* desc{ MTL::VertexDescriptor::alloc()->init() };
        if (!desc)
            return nullptr;

        MTL::VertexBufferLayoutDescriptor* vertex_layout{ desc->layouts()->object(0) };
        vertex_layout->setStride(sizeof(renderer::quad_vertex_t));
        vertex_layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
        vertex_layout->setStepRate(1);

        if (instanced)
        {
            MTL::VertexBufferLayoutDescriptor* instance_layout{ desc->layouts()->object(1) };
            instance_layout->setStride(sizeof(renderer::gpu_quad_instance_t));
            instance_layout->setStepFunction(MTL::VertexStepFunctionPerInstance);
            instance_layout->setStepRate(1);

            // Metal shaderconverter metadata reports raw input indices 0..5, but the
            // compiled pipeline validation still refers to the semantic-driven slots
            // used by the older working path (11..16). Advertise both mappings so the
            // compiled library can bind whichever convention it emitted.
            for (const NS::UInteger position_slot : { NS::UInteger(0), NS::UInteger(11) })
            {
                configure_attribute(desc,
                                    position_slot,
                                    MTL::VertexFormatFloat2,
                                    offsetof(renderer::quad_vertex_t, x),
                                    0);
            }
            for (const NS::UInteger uv_slot : { NS::UInteger(1), NS::UInteger(12) })
            {
                configure_attribute(desc,
                                    uv_slot,
                                    MTL::VertexFormatFloat2,
                                    offsetof(renderer::quad_vertex_t, u),
                                    0);
            }
            for (const NS::UInteger quad_rect_slot : { NS::UInteger(2), NS::UInteger(13) })
            {
                configure_attribute(desc,
                                    quad_rect_slot,
                                    MTL::VertexFormatFloat4,
                                    offsetof(renderer::gpu_quad_instance_t, quad_rect_px),
                                    1);
            }
            for (const NS::UInteger uv_rect_slot : { NS::UInteger(3), NS::UInteger(14) })
            {
                configure_attribute(desc,
                                    uv_rect_slot,
                                    MTL::VertexFormatFloat4,
                                    offsetof(renderer::gpu_quad_instance_t, uv_rect),
                                    1);
            }
            for (const NS::UInteger color_slot : { NS::UInteger(4), NS::UInteger(15) })
            {
                configure_attribute(desc,
                                    color_slot,
                                    MTL::VertexFormatFloat4,
                                    offsetof(renderer::gpu_quad_instance_t, color),
                                    1);
            }
            for (const NS::UInteger draw_params_slot : { NS::UInteger(5), NS::UInteger(16) })
            {
                configure_attribute(desc,
                                    draw_params_slot,
                                    MTL::VertexFormatFloat4,
                                    offsetof(renderer::gpu_quad_instance_t, draw_params),
                                    1);
            }
        }
        else
        {
            configure_attribute(desc,
                                11,
                                MTL::VertexFormatFloat2,
                                offsetof(renderer::quad_vertex_t, x),
                                0);
            configure_attribute(desc,
                                12,
                                MTL::VertexFormatFloat2,
                                offsetof(renderer::quad_vertex_t, u),
                                0);
            configure_attribute(desc,
                                13,
                                MTL::VertexFormatUChar4Normalized,
                                offsetof(renderer::quad_vertex_t, color),
                                0);
            configure_attribute(desc,
                                14,
                                MTL::VertexFormatFloat,
                                offsetof(renderer::quad_vertex_t, effect_mode),
                                0);
            configure_attribute(desc,
                                15,
                                MTL::VertexFormatFloat,
                                offsetof(renderer::quad_vertex_t, effect_param0),
                                0);
        }

        return desc;
    }

    MTL::Library* metal_textured_quad_pipeline_t::load_library(MTL::Device* device,
                                                               const assets::shader_file_provider_t& shader_files,
                                                               const std::string_view virtual_path)
    {
        const auto native{ shader_files.resolve(virtual_path) };
        if (!native)
        {
            LOG_GRAPHICS_FATAL("Failed to resolve shader path: {}", virtual_path);
            return nullptr;
        }

        const auto bytes{ utils::file::load_binary_file(*native) };
        if (!bytes || bytes->empty())
        {
            LOG_GRAPHICS_FATAL("Failed to read shader file: {}", utils::file::to_log_string(*native));
            return nullptr;
        }

        dispatch_data_t data{
            dispatch_data_create(bytes->data(), bytes->size(), dispatch_get_main_queue(),
                                 DISPATCH_DATA_DESTRUCTOR_DEFAULT)
        };

        if (!data)
        {
            LOG_GRAPHICS_FATAL("Failed to create dispatch_data");
            return nullptr;
        }

        NS::Error* error{ nullptr };
        MTL::Library* lib{ device->newLibrary(data, &error) };

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
        const NS::Array* names{ lib->functionNames() };
        if (!names || names->count() == 0)
        {
            LOG_GRAPHICS_FATAL("Shader library has no functions");
            return nullptr;
        }

        const NS::String* name{ reinterpret_cast<NS::String*>(names->object(0)) };
        if (!name)
        {
            LOG_GRAPHICS_FATAL("Null function name in shader library");
            return nullptr;
        }

        MTL::Function* func{ lib->newFunction(name) };
        if (!func)
        {
            LOG_GRAPHICS_FATAL("Failed to create function '{}'", name->utf8String());
            return nullptr;
        }

        return func;
    }
}
