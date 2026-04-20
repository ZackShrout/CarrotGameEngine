//
// Created by Zack Shrout on 4/18/2026.
//

#include "Core/Pch.h"

#include "DirectX12ComputePipeline.h"

#include "Assets/Shaders/ShaderFileProvider.h"
#include "Utils/File/FileUtils.h"

namespace carrot::rhi::dx12 {
    dx12_compute_pipeline_t::dx12_compute_pipeline_t(ID3D12Device* device,
                                                     assets::shader_file_provider_t& shader_files,
                                                     const compute_pipeline_create_info_t& info)
        : rhi_compute_pipeline_t{ info }, _device{ device }
    {
        if (!_device)
        {
            LOG_GRAPHICS_FATAL("DX12 compute pipeline created with null device");
            return;
        }

        constexpr std::uint32_t compute_buffer_root_parameter_count{
            k_max_compute_buffer_bindings * 2u
        };
        std::array<D3D12_ROOT_PARAMETER, compute_buffer_root_parameter_count + 1u> root_parameters{ };

        for (std::uint32_t slot{ 0u }; slot < k_max_compute_buffer_bindings; ++slot)
        {
            root_parameters[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            root_parameters[slot].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            root_parameters[slot].Descriptor.ShaderRegister = slot;
            root_parameters[slot].Descriptor.RegisterSpace = 0;
        }

        for (std::uint32_t slot{ 0u }; slot < k_max_compute_buffer_bindings; ++slot)
        {
            const std::uint32_t root_index{ k_max_compute_buffer_bindings + slot };
            root_parameters[root_index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
            root_parameters[root_index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            root_parameters[root_index].Descriptor.ShaderRegister = slot;
            root_parameters[root_index].Descriptor.RegisterSpace = 0;
        }

        std::uint32_t root_parameter_count{ compute_buffer_root_parameter_count };
        if (info.max_constant_size_bytes > 0u)
        {
            root_parameters[compute_buffer_root_parameter_count].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            root_parameters[compute_buffer_root_parameter_count].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            root_parameters[compute_buffer_root_parameter_count].Descriptor.ShaderRegister = k_compute_constant_register;
            root_parameters[compute_buffer_root_parameter_count].Descriptor.RegisterSpace = 0;
            root_parameter_count = compute_buffer_root_parameter_count + 1u;
        }

        D3D12_ROOT_SIGNATURE_DESC root_signature_desc{ };
        root_signature_desc.NumParameters = root_parameter_count;
        root_signature_desc.pParameters = root_parameters.data();
        root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* signature_blob{ nullptr };
        ID3DBlob* error_blob{ nullptr };
        const HRESULT serialize_result{
            D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob,
                                        &error_blob)
        };

        if (FAILED(serialize_result))
        {
            if (error_blob)
            {
                LOG_GRAPHICS_ERROR("DX12 compute root signature error: {}",
                                   static_cast<const char*>(error_blob->GetBufferPointer()));
                error_blob->Release();
            }
            LOG_GRAPHICS_FATAL("Failed to serialize DX12 compute root signature");
            return;
        }

        DX12_CHECK(_device->CreateRootSignature(0,
                                                signature_blob->GetBufferPointer(),
                                                signature_blob->GetBufferSize(),
                                                IID_PPV_ARGS(&_root_signature)));
        DX12_NAME(_root_signature, L"DX12 Compute Root Signature");
        signature_blob->Release();
        if (error_blob)
            error_blob->Release();

        const auto shader_path{ shader_files.resolve(info.shader_path) };
        if (!shader_path)
            LOG_GRAPHICS_FATAL("Failed to resolve DX12 compute shader path");

        const auto shader_bytes{ utils::file::load_binary_file(*shader_path) };
        if (!shader_bytes || shader_bytes->empty())
            LOG_GRAPHICS_FATAL("Failed to load DX12 compute shader bytecode");

        D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{ };
        pso_desc.pRootSignature = _root_signature;
        pso_desc.CS.pShaderBytecode = shader_bytes->data();
        pso_desc.CS.BytecodeLength = shader_bytes->size();

        DX12_CHECK(_device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&_pipeline_state)));
        DX12_NAME(_pipeline_state, L"DX12 Compute Pipeline State");
    }

    dx12_compute_pipeline_t::~dx12_compute_pipeline_t()
    {
        if (_pipeline_state)
        {
            _pipeline_state->Release();
            _pipeline_state = nullptr;
        }

        if (_root_signature)
        {
            _root_signature->Release();
            _root_signature = nullptr;
        }
    }
} // namespace carrot::rhi::dx12
