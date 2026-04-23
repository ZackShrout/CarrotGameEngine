//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "DirectX12TexturedQuadPipeline.h"

#include "Assets/Shaders/ShaderFileProvider.h"
#include "RHI/Backends/DirectX12/DirectX12Buffer.h"
#include "RHI/Backends/DirectX12/DirectX12Core.h"
#include "RHI/Backends/DirectX12/DirectX12Sampler.h"
#include "RHI/Backends/DirectX12/DirectX12Texture.h"
#include "RHI/SamplerPresets.h"
#include "Renderer/Draw/QuadInstanceData.h"
#include "Renderer/Primitives/QuadVertex.h"
#include "Utils/File/FileUtils.h"

namespace carrot::rhi::dx12 {
    namespace {
        constexpr uint32_t k_srv_descriptors_per_batch{ 5u };

        bool prepare_graphics_srv_buffer(ID3D12GraphicsCommandList* const command_list,
                                         const dx12_buffer_t& buffer,
                                         const char* const label)
        {
            if (!command_list)
                return false;

            if (!buffer.flush_pending_upload(command_list))
            {
                LOG_GRAPHICS_ERROR("DX12 textured quad pipeline failed to flush pending upload for {}", label);
                return false;
            }

            if (!buffer.transition_to(command_list, D3D12_RESOURCE_STATE_GENERIC_READ))
            {
                LOG_GRAPHICS_ERROR("DX12 textured quad pipeline failed to transition {} to generic-read state", label);
                return false;
            }

            return true;
        }

        void write_raw_buffer_srv(ID3D12Device* device,
                                  const dx12_buffer_t& buffer,
                                  const D3D12_CPU_DESCRIPTOR_HANDLE handle)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{ };
            srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Buffer.FirstElement = 0;
            srv_desc.Buffer.NumElements = static_cast<UINT>((buffer.size_bytes() + 3u) / 4u);
            srv_desc.Buffer.StructureByteStride = 0;
            srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            device->CreateShaderResourceView(buffer.resource(), &srv_desc, handle);
        }
    }

    dx12_textured_quad_pipeline_t::dx12_textured_quad_pipeline_t(ID3D12Device* device,
                                                                 assets::shader_file_provider_t& shader_files,
                                                                 const std::string_view vertex_shader_path,
                                                                 const std::string_view fragment_shader_path,
                                                                 const blend_mode_t blend_mode,
                                                                 const DXGI_FORMAT render_target_format)
        : _device{ device }
    {
        if (!_device)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad pipeline created with null device");
            return;
        }

        D3D12_DESCRIPTOR_RANGE srv_range{ };
        srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srv_range.NumDescriptors = k_srv_descriptors_per_batch;
        srv_range.BaseShaderRegister = 0;
        srv_range.RegisterSpace = 0;
        srv_range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_DESCRIPTOR_RANGE cbv_range{ };
        cbv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        cbv_range.NumDescriptors = 1;
        cbv_range.BaseShaderRegister = 0;
        cbv_range.RegisterSpace = 0;
        cbv_range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_DESCRIPTOR_RANGE sampler_range{ };
        sampler_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        sampler_range.NumDescriptors = 1;
        sampler_range.BaseShaderRegister = 0;
        sampler_range.RegisterSpace = 0;
        sampler_range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER root_params[3]{ };

        root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        root_params[0].DescriptorTable.NumDescriptorRanges = 1;
        root_params[0].DescriptorTable.pDescriptorRanges = &cbv_range;

        root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        root_params[1].DescriptorTable.NumDescriptorRanges = 1;
        root_params[1].DescriptorTable.pDescriptorRanges = &srv_range;

        root_params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        root_params[2].DescriptorTable.NumDescriptorRanges = 1;
        root_params[2].DescriptorTable.pDescriptorRanges = &sampler_range;

        D3D12_ROOT_SIGNATURE_DESC root_sig_desc{ };
        root_sig_desc.NumParameters = 3;
        root_sig_desc.pParameters = root_params;
        root_sig_desc.NumStaticSamplers = 0;
        root_sig_desc.pStaticSamplers = nullptr;
        root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* sig_blob{ nullptr };
        ID3DBlob* error_blob{ nullptr };

        const HRESULT hr{
            D3D12SerializeRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob)
        };

        if (FAILED(hr))
        {
            if (error_blob)
            {
                LOG_GRAPHICS_ERROR("DX12 textured quad root signature error: {}",
                                   static_cast<const char*>(error_blob->GetBufferPointer()));
                error_blob->Release();
                error_blob = nullptr;
            }

            if (sig_blob)
            {
                sig_blob->Release();
                sig_blob = nullptr;
            }

            LOG_GRAPHICS_FATAL("Failed to serialize DX12 textured quad root signature");
        }

        DX12_CHECK(
            _device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(), IID_PPV_ARGS(&
                _root_signature)));

        DX12_NAME(_root_signature, L"DX12 Textured Quad Root Signature");

        sig_blob->Release();
        sig_blob = nullptr;

        if (error_blob)
        {
            error_blob->Release();
            error_blob = nullptr;
        }

        const auto vs_path{ shader_files.resolve(vertex_shader_path) };
        const auto ps_path{ shader_files.resolve(fragment_shader_path) };

        if (!vs_path || !ps_path)
            LOG_GRAPHICS_FATAL("Failed to resolve DX12 textured quad shader paths");

        const auto vs_bytes{ utils::file::load_binary_file(*vs_path) };
        const auto ps_bytes{ utils::file::load_binary_file(*ps_path) };

        if (!vs_bytes || !ps_bytes)
            LOG_GRAPHICS_FATAL("Failed to load DX12 textured quad shader bytecode");

        D3D12_SHADER_BYTECODE vs_bc{ };
        vs_bc.pShaderBytecode = vs_bytes->data();
        vs_bc.BytecodeLength = vs_bytes->size();

        D3D12_SHADER_BYTECODE ps_bc{ };
        ps_bc.pShaderBytecode = ps_bytes->data();
        ps_bc.BytecodeLength = ps_bytes->size();

        constexpr D3D12_INPUT_ELEMENT_DESC instanced_input_layout[]{
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        };

        D3D12_RASTERIZER_DESC rasterizer{ };
        rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer.CullMode = D3D12_CULL_MODE_NONE;
        rasterizer.FrontCounterClockwise = FALSE;
        rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizer.DepthClipEnable = TRUE;
        rasterizer.MultisampleEnable = FALSE;
        rasterizer.AntialiasedLineEnable = FALSE;
        rasterizer.ForcedSampleCount = 0;
        rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_BLEND_DESC blend{ };
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;

        D3D12_RENDER_TARGET_BLEND_DESC& rt0{ blend.RenderTarget[0] };
        rt0.BlendEnable = TRUE;
        rt0.LogicOpEnable = FALSE;
        rt0.SrcBlend = blend_mode == blend_mode_t::additive ? D3D12_BLEND_ONE : D3D12_BLEND_SRC_ALPHA;
        rt0.DestBlend = blend_mode == blend_mode_t::additive ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA;
        rt0.BlendOp = D3D12_BLEND_OP_ADD;
        rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt0.DestBlendAlpha = blend_mode == blend_mode_t::additive ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA;
        rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt0.LogicOp = D3D12_LOGIC_OP_NOOP;
        rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_DEPTH_STENCIL_DESC depth_stencil{ };
        depth_stencil.DepthEnable = FALSE;
        depth_stencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depth_stencil.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        depth_stencil.StencilEnable = FALSE;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{ };
        pso_desc.pRootSignature = _root_signature;
        pso_desc.VS = vs_bc;
        pso_desc.PS = ps_bc;
        pso_desc.BlendState = blend;
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.RasterizerState = rasterizer;
        pso_desc.DepthStencilState = depth_stencil;
        pso_desc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ instanced_input_layout, _countof(instanced_input_layout) };
        pso_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = render_target_format;
        pso_desc.SampleDesc.Count = 1;
        pso_desc.SampleDesc.Quality = 0;

        DX12_CHECK(_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&_pipeline_state)));
        DX12_NAME(_pipeline_state, L"DX12 Textured Quad Pipeline State");
    }

    dx12_textured_quad_pipeline_t::~dx12_textured_quad_pipeline_t()
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

    void dx12_textured_quad_pipeline_t::draw(const draw_context_t& draw_context,
                                             const descriptor_context_t& descriptor_context) const
    {
        if (!draw_context.command_list ||
            !draw_context.vertex_buffer ||
            !draw_context.index_buffer ||
            draw_context.batches.empty() ||
            !descriptor_context.tables.srv_heap ||
            !descriptor_context.tables.sampler_heap ||
            descriptor_context.tables.camera_cbv_handle.ptr == 0 ||
            !descriptor_context.sampler_provider)
        {
            return;
        }

        ID3D12GraphicsCommandList* cmd{ draw_context.command_list };

        const dx12_buffer_t& dx_vertex_buffer{ dynamic_cast<const dx12_buffer_t&>(*draw_context.vertex_buffer) };
        const dx12_buffer_t& dx_index_buffer{ dynamic_cast<const dx12_buffer_t&>(*draw_context.index_buffer) };
        const dx12_buffer_t* dx_instance_buffer{ dynamic_cast<const dx12_buffer_t*>(draw_context.instance_buffer) };
        if (!dx_instance_buffer)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad pipeline requires an instance buffer");
            return;
        }
        const dx12_buffer_t* light_input_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_light_input_buffer)
        };
        const dx12_buffer_t* output_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_output_buffer)
        };
        const dx12_buffer_t* world_item_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.world_item_buffer)
        };
        const dx12_buffer_t* visible_item_index_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.visible_item_index_buffer)
        };
        if (!light_input_buffer || !output_buffer || !world_item_buffer || !visible_item_index_buffer)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad pipeline received non-DX12 forward+ buffers");
            return;
        }

        if (!prepare_graphics_srv_buffer(cmd, *light_input_buffer, "forward+ light input buffer") ||
            !prepare_graphics_srv_buffer(cmd, *output_buffer, "forward+ output buffer") ||
            !prepare_graphics_srv_buffer(cmd, *world_item_buffer, "world item buffer") ||
            !prepare_graphics_srv_buffer(cmd, *visible_item_index_buffer, "visible item index buffer"))
        {
            return;
        }

        D3D12_VIEWPORT viewport{ };
        viewport.TopLeftX = static_cast<float>(draw_context.viewport.rect_px.position.x);
        viewport.TopLeftY = static_cast<float>(draw_context.viewport.rect_px.position.y);
        viewport.Width = static_cast<float>(draw_context.viewport.rect_px.size.x);
        viewport.Height = static_cast<float>(draw_context.viewport.rect_px.size.y);
        viewport.MinDepth = 0.f;
        viewport.MaxDepth = 1.f;

        D3D12_RECT scissor{ };
        scissor.left = static_cast<LONG>(draw_context.viewport.rect_px.position.x);
        scissor.top = static_cast<LONG>(draw_context.viewport.rect_px.position.y);
        scissor.right = static_cast<LONG>(draw_context.viewport.rect_px.position.x + draw_context.viewport.rect_px.size.x);
        scissor.bottom = static_cast<LONG>(draw_context.viewport.rect_px.position.y + draw_context.viewport.rect_px.size.y);

        D3D12_VERTEX_BUFFER_VIEW vbv{ };
        vbv.BufferLocation = dx_vertex_buffer.resource()->GetGPUVirtualAddress();
        vbv.SizeInBytes = static_cast<UINT>(dx_vertex_buffer.size_bytes());
        vbv.StrideInBytes = sizeof(renderer::quad_vertex_t);

        D3D12_VERTEX_BUFFER_VIEW instance_vbv{ };
        instance_vbv.BufferLocation =
            dx_instance_buffer->resource()->GetGPUVirtualAddress() + draw_context.stage.instance_buffer_offset_bytes;
        instance_vbv.SizeInBytes = static_cast<UINT>(dx_instance_buffer->size_bytes() -
                                                     draw_context.stage.instance_buffer_offset_bytes);
        instance_vbv.StrideInBytes = sizeof(renderer::gpu_quad_instance_t);

        D3D12_INDEX_BUFFER_VIEW ibv{ };
        ibv.BufferLocation = dx_index_buffer.resource()->GetGPUVirtualAddress();
        ibv.SizeInBytes = static_cast<UINT>(dx_index_buffer.size_bytes());
        ibv.Format = DXGI_FORMAT_R32_UINT;

        ID3D12DescriptorHeap* descriptor_heaps[] = {
            descriptor_context.tables.srv_heap,
            descriptor_context.tables.sampler_heap
        };

        cmd->SetGraphicsRootSignature(_root_signature);
        cmd->SetPipelineState(_pipeline_state);
        cmd->SetDescriptorHeaps(_countof(descriptor_heaps), descriptor_heaps);
        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const D3D12_VERTEX_BUFFER_VIEW vertex_views[]{ vbv, instance_vbv };
        cmd->IASetVertexBuffers(0, _countof(vertex_views), vertex_views);
        cmd->IASetIndexBuffer(&ibv);
        cmd->SetGraphicsRootDescriptorTable(0, descriptor_context.tables.camera_cbv_handle);

        const D3D12_GPU_DESCRIPTOR_HANDLE srv_heap_start{
            descriptor_context.tables.srv_heap->GetGPUDescriptorHandleForHeapStart()
        };

        const D3D12_GPU_DESCRIPTOR_HANDLE sampler_heap_start{
            descriptor_context.tables.sampler_heap->GetGPUDescriptorHandleForHeapStart()
        };

        for (uint32_t i{ 0 }; i < static_cast<uint32_t>(draw_context.batches.size()); ++i)
        {
            const auto& batch{ draw_context.batches[i] };
            const auto* dx_texture{ dynamic_cast<const dx12_texture_t*>(batch.texture) };
            if (!dx_texture)
            {
                LOG_GRAPHICS_FATAL("DX12 textured quad batch texture is not a dx12_texture_t");
                return;
            }

            if (!dx_texture->transition_to(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
            {
                LOG_GRAPHICS_FATAL("DX12 textured quad pipeline failed to transition batch texture to shader-read state");
                return;
            }

            write_batch_descriptors(i, batch, descriptor_context);

            D3D12_GPU_DESCRIPTOR_HANDLE srv_handle{ srv_heap_start };
            srv_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_srv_index +
                              i * k_srv_descriptors_per_batch) *
                              descriptor_context.tables.srv_descriptor_size;

            D3D12_GPU_DESCRIPTOR_HANDLE sampler_handle{ sampler_heap_start };
            sampler_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_sampler_index + i) *
                                  descriptor_context.tables.sampler_descriptor_size;

            cmd->SetGraphicsRootDescriptorTable(1, srv_handle);
            cmd->SetGraphicsRootDescriptorTable(2, sampler_handle);
            cmd->DrawIndexedInstanced(batch.index_count,
                                      batch.instance_count,
                                      batch.first_index,
                                      0,
                                      batch.first_instance);
        }
    }

    void dx12_textured_quad_pipeline_t::draw_indirect(const indirect_draw_context_t& draw_context,
                                                      const descriptor_context_t& descriptor_context) const
    {
        if (!draw_context.command_list ||
            !draw_context.draw_indexed_indirect_signature ||
            !draw_context.vertex_buffer ||
            !draw_context.index_buffer ||
            !draw_context.indirect_buffer ||
            !draw_context.texture ||
            !draw_context.sampler ||
            !descriptor_context.tables.srv_heap ||
            !descriptor_context.tables.sampler_heap ||
            descriptor_context.tables.camera_cbv_handle.ptr == 0 ||
            !descriptor_context.sampler_provider)
        {
            return;
        }

        ID3D12GraphicsCommandList* cmd{ draw_context.command_list };

        const dx12_buffer_t& dx_vertex_buffer{ dynamic_cast<const dx12_buffer_t&>(*draw_context.vertex_buffer) };
        const dx12_buffer_t& dx_index_buffer{ dynamic_cast<const dx12_buffer_t&>(*draw_context.index_buffer) };
        const dx12_buffer_t& dx_indirect_buffer{ dynamic_cast<const dx12_buffer_t&>(*draw_context.indirect_buffer) };
        const dx12_buffer_t* light_input_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_light_input_buffer)
        };
        const dx12_buffer_t* output_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_output_buffer)
        };
        const dx12_buffer_t* world_item_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.world_item_buffer)
        };
        const dx12_buffer_t* visible_item_index_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.visible_item_index_buffer)
        };
        if (!light_input_buffer || !output_buffer || !world_item_buffer || !visible_item_index_buffer)
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad received non-DX12 forward+ buffers");
            return;
        }

        if (!prepare_graphics_srv_buffer(cmd, *light_input_buffer, "forward+ light input buffer") ||
            !prepare_graphics_srv_buffer(cmd, *output_buffer, "forward+ output buffer") ||
            !prepare_graphics_srv_buffer(cmd, *world_item_buffer, "world item buffer") ||
            !prepare_graphics_srv_buffer(cmd, *visible_item_index_buffer, "visible item index buffer"))
        {
            return;
        }

        D3D12_VIEWPORT viewport{ };
        viewport.TopLeftX = static_cast<float>(draw_context.viewport.rect_px.position.x);
        viewport.TopLeftY = static_cast<float>(draw_context.viewport.rect_px.position.y);
        viewport.Width = static_cast<float>(draw_context.viewport.rect_px.size.x);
        viewport.Height = static_cast<float>(draw_context.viewport.rect_px.size.y);
        viewport.MinDepth = 0.f;
        viewport.MaxDepth = 1.f;

        D3D12_RECT scissor{ };
        scissor.left = static_cast<LONG>(draw_context.viewport.rect_px.position.x);
        scissor.top = static_cast<LONG>(draw_context.viewport.rect_px.position.y);
        scissor.right = static_cast<LONG>(draw_context.viewport.rect_px.position.x + draw_context.viewport.rect_px.size.x);
        scissor.bottom = static_cast<LONG>(draw_context.viewport.rect_px.position.y + draw_context.viewport.rect_px.size.y);

        D3D12_VERTEX_BUFFER_VIEW vbv{ };
        vbv.BufferLocation = dx_vertex_buffer.resource()->GetGPUVirtualAddress();
        vbv.SizeInBytes = static_cast<UINT>(dx_vertex_buffer.size_bytes());
        vbv.StrideInBytes = sizeof(renderer::quad_vertex_t);

        D3D12_INDEX_BUFFER_VIEW ibv{ };
        ibv.BufferLocation = dx_index_buffer.resource()->GetGPUVirtualAddress();
        ibv.SizeInBytes = static_cast<UINT>(dx_index_buffer.size_bytes());
        ibv.Format = DXGI_FORMAT_R32_UINT;

        ID3D12DescriptorHeap* descriptor_heaps[] = {
            descriptor_context.tables.srv_heap,
            descriptor_context.tables.sampler_heap
        };

        cmd->SetGraphicsRootSignature(_root_signature);
        cmd->SetPipelineState(_pipeline_state);
        cmd->SetDescriptorHeaps(_countof(descriptor_heaps), descriptor_heaps);
        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->IASetVertexBuffers(0, 1, &vbv);
        cmd->IASetIndexBuffer(&ibv);
        cmd->SetGraphicsRootDescriptorTable(0, descriptor_context.tables.camera_cbv_handle);
        const auto* dx_texture{ dynamic_cast<const dx12_texture_t*>(draw_context.texture) };
        if (!dx_texture)
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad texture is not a dx12_texture_t");
            return;
        }
        if (!dx_texture->transition_to(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad failed to transition texture to shader-read state");
            return;
        }
        write_indirect_descriptors(*draw_context.texture, *draw_context.sampler, descriptor_context);

        const D3D12_GPU_DESCRIPTOR_HANDLE srv_heap_start{
            descriptor_context.tables.srv_heap->GetGPUDescriptorHandleForHeapStart()
        };
        const D3D12_GPU_DESCRIPTOR_HANDLE sampler_heap_start{
            descriptor_context.tables.sampler_heap->GetGPUDescriptorHandleForHeapStart()
        };

        D3D12_GPU_DESCRIPTOR_HANDLE srv_handle{ srv_heap_start };
        srv_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_srv_index) *
                          descriptor_context.tables.srv_descriptor_size;

        D3D12_GPU_DESCRIPTOR_HANDLE sampler_handle{ sampler_heap_start };
        sampler_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_sampler_index) *
                              descriptor_context.tables.sampler_descriptor_size;

        cmd->SetGraphicsRootDescriptorTable(1, srv_handle);
        cmd->SetGraphicsRootDescriptorTable(2, sampler_handle);
        cmd->ExecuteIndirect(draw_context.draw_indexed_indirect_signature,
                             1,
                             dx_indirect_buffer.resource(),
                             draw_context.indirect_buffer_offset_bytes,
                             nullptr,
                             0);
    }

    void dx12_textured_quad_pipeline_t::write_batch_descriptors(const uint32_t batch_index,
                                                                const renderer::textured_quad_batch_t& batch,
                                                                const descriptor_context_t& descriptor_context) const
    {
        if (!batch.texture)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad batch has null texture");
            return;
        }
        const dx12_buffer_t* light_input_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_light_input_buffer)
        };
        const dx12_buffer_t* output_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_output_buffer)
        };
        const dx12_buffer_t* world_item_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.world_item_buffer)
        };
        const dx12_buffer_t* visible_item_index_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.visible_item_index_buffer)
        };
        if (!light_input_buffer || !output_buffer || !world_item_buffer || !visible_item_index_buffer)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad pipeline received non-DX12 forward+ buffers");
            return;
        }

        const dx12_texture_t* dx_texture{ dynamic_cast<const dx12_texture_t*>(batch.texture) };
        if (!dx_texture)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad batch texture is not a dx12_texture_t");
            return;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{ };
        srv_desc.Format = dx_texture->srv_format();
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.PlaneSlice = 0;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.f;

        D3D12_CPU_DESCRIPTOR_HANDLE srv_handle{
            descriptor_context.tables.srv_heap->GetCPUDescriptorHandleForHeapStart()
        };
        srv_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_srv_index +
                          batch_index * k_srv_descriptors_per_batch) *
                          descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *light_input_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *output_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *world_item_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *visible_item_index_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        _device->CreateShaderResourceView(dx_texture->resource(), &srv_desc, srv_handle);

        const sampler_desc_t sampler_desc{ sampler_desc_from_preset(batch.sampler_preset) };
        if (!descriptor_context.sampler_provider->get_or_create_sampler(sampler_desc))
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad pipeline failed to retrieve sampler");
            return;
        }
        const auto* dx_sampler = dynamic_cast<const dx12_sampler_t*>(descriptor_context.sampler_provider->get_or_create_sampler(sampler_desc));
        if (!dx_sampler)
        {
            LOG_GRAPHICS_FATAL("DX12 textured quad pipeline received non-DX12 sampler");
            return;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle{
            descriptor_context.tables.sampler_heap->GetCPUDescriptorHandleForHeapStart()
        };
        sampler_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_sampler_index + batch_index) *
                              descriptor_context.tables.sampler_descriptor_size;
        _device->CopyDescriptorsSimple(1u, sampler_handle, dx_sampler->cpu_handle(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    void dx12_textured_quad_pipeline_t::write_indirect_descriptors(const rhi_texture_t& texture,
                                                                   const rhi_sampler_t& sampler,
                                                                   const descriptor_context_t& descriptor_context) const
    {
        const dx12_buffer_t* light_input_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_light_input_buffer)
        };
        const dx12_buffer_t* output_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.forward_plus_output_buffer)
        };
        const dx12_buffer_t* world_item_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.world_item_buffer)
        };
        const dx12_buffer_t* visible_item_index_buffer{
            dynamic_cast<const dx12_buffer_t*>(descriptor_context.visible_item_index_buffer)
        };
        if (!light_input_buffer || !output_buffer || !world_item_buffer || !visible_item_index_buffer)
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad received non-DX12 forward+ buffers");
            return;
        }

        const dx12_texture_t* dx_texture{ dynamic_cast<const dx12_texture_t*>(&texture) };
        if (!dx_texture)
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad texture is not a dx12_texture_t");
            return;
        }

        const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{
            .Format = dx_texture->srv_format(),
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {
                .MostDetailedMip = 0,
                .MipLevels = 1,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.f
            }
        };

        D3D12_CPU_DESCRIPTOR_HANDLE srv_handle{
            descriptor_context.tables.srv_heap->GetCPUDescriptorHandleForHeapStart()
        };
        srv_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_srv_index) *
                          descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *light_input_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *output_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *world_item_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        write_raw_buffer_srv(_device, *visible_item_index_buffer, srv_handle);
        srv_handle.ptr += descriptor_context.tables.srv_descriptor_size;
        _device->CreateShaderResourceView(dx_texture->resource(), &srv_desc, srv_handle);

        if (!descriptor_context.sampler_provider->get_or_create_sampler(sampler.desc()))
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad failed to retrieve sampler");
            return;
        }
        const auto* dx_sampler = dynamic_cast<const dx12_sampler_t*>(&sampler);
        if (!dx_sampler)
        {
            LOG_GRAPHICS_FATAL("DX12 indirect textured quad received non-DX12 sampler");
            return;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle{
            descriptor_context.tables.sampler_heap->GetCPUDescriptorHandleForHeapStart()
        };
        sampler_handle.ptr += static_cast<SIZE_T>(descriptor_context.tables.first_batch_sampler_index) *
                              descriptor_context.tables.sampler_descriptor_size;
        _device->CopyDescriptorsSimple(1u, sampler_handle, dx_sampler->cpu_handle(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
} // namespace carrot::rhi::dx12
