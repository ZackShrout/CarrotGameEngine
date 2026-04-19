//
// Created by Zack Shrout on 4/18/2026.
//

#include "Core/Pch.h"

#include "MetalComputePipeline.h"

#include "Assets/Shaders/ShaderFileProvider.h"
#include "RHI/Backends/Metal/MetalDevice.h"
#include "Utils/File/FileUtils.h"

namespace carrot::rhi::metal {
    namespace {
        template<typename T>
        std::shared_ptr<T> make_mtl_shared(T* obj)
        {
            return std::shared_ptr<T>(obj, [](T* p) {
                if (p)
                    p->release();
            });
        }
    } // namespace

    metal_compute_pipeline_t::metal_compute_pipeline_t(const metal_device_t& device,
                                                       const assets::shader_file_provider_t& shader_files,
                                                       const compute_pipeline_create_info_t& info)
        : rhi_compute_pipeline_t{ info }
    {
        MTL::Device* mtl_device{ device.mtl_device() };
        if (!mtl_device)
        {
            LOG_GRAPHICS_FATAL("Metal compute pipeline received null device");
            return;
        }

        const std::shared_ptr<MTL::Library> library{ make_mtl_shared(load_library(mtl_device, shader_files,
                                                                                  info.shader_path)) };
        if (!library)
            return;

        const std::shared_ptr<MTL::Function> function{ make_mtl_shared(load_function(library.get())) };
        if (!function)
            return;

        NS::Error* error{ nullptr };
        MTL::ComputePipelineState* state{ mtl_device->newComputePipelineState(function.get(), &error) };
        if (!state)
        {
            const char* msg = error ? error->localizedDescription()->utf8String() : "Unknown error";
            LOG_GRAPHICS_FATAL("Failed to create Metal compute pipeline '{}': {}", info.debug_name, msg);
            return;
        }

        _state = make_mtl_shared(state);
    }

    MTL::Library* metal_compute_pipeline_t::load_library(MTL::Device* device,
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
            LOG_GRAPHICS_FATAL("Failed to create dispatch_data for compute pipeline");
            return nullptr;
        }

        NS::Error* error{ nullptr };
        MTL::Library* library{ device->newLibrary(data, &error) };
        dispatch_release(data);

        if (!library)
        {
            const char* msg = error ? error->localizedDescription()->utf8String() : "Unknown error";
            LOG_GRAPHICS_FATAL("Failed to create Metal library '{}': {}", virtual_path, msg);
            return nullptr;
        }

        return library;
    }

    MTL::Function* metal_compute_pipeline_t::load_function(MTL::Library* library)
    {
        const NS::Array* names{ library->functionNames() };
        if (!names || names->count() == 0)
        {
            LOG_GRAPHICS_FATAL("Compute shader library has no functions");
            return nullptr;
        }

        const NS::String* name{ reinterpret_cast<NS::String*>(names->object(0)) };
        if (!name)
        {
            LOG_GRAPHICS_FATAL("Null compute function name in shader library");
            return nullptr;
        }

        MTL::Function* function{ library->newFunction(name) };
        if (!function)
        {
            LOG_GRAPHICS_FATAL("Failed to create compute function '{}'", name->utf8String());
            return nullptr;
        }

        return function;
    }
} // namespace carrot::rhi::metal
