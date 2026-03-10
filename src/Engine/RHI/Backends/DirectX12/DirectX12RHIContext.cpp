//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12RHIContext.h"

#include "DirectX12CommandList.h"
#include "DirectX12CommandQueue.h"
#include "DirectX12Device.h"
#include "DirectX12Fence.h"
#include "DirectX12Swapchain.h"
#include "RHI/RHI.h"
#include "Utils/File/FileUtils.h"
#include "Window/Window.h"

namespace carrot::rhi::dx12 {
    dx12_rhi_context_t::dx12_rhi_context_t(const rhi_desc_t& desc)
    {
        if (core::platform::current_platform() != core::platform::platform_type::win32)
            LOG_GRAPHICS_FATAL("DX12 backend requires Win32 platform");

        core::platform::native_window_handle_t window{ window::get_native_handle() };

        HWND hwnd{ static_cast<HWND>(window.win32_t.hwnd) };
        if (!hwnd)
            LOG_GRAPHICS_FATAL("Invalid HWND passed to DX12 context");

        _device = std::make_unique<dx12_device_t>(desc);
        _graphics_queue = std::make_unique<dx12_command_queue_t>(_device->id3d12_device());
        _swapchain = std::make_unique<dx12_swapchain_t>(_device->id3d12_device(),
                                                        _graphics_queue->id3d12_command_queue(), hwnd, desc.width,
                                                        desc.height);

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            dx12_frame_t& frame{ _frames[i] };

            HRESULT hr{
                _device->id3d12_device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                 IID_PPV_ARGS(&frame.allocator))
            };

            if (FAILED(hr))
                LOG_GRAPHICS_FATAL("Failed to create DX12 command allocator");

            frame.command_list = std::make_unique<dx12_command_list_t>(_device->id3d12_device(), frame.allocator);
            frame.fence = std::make_unique<dx12_fence_t>(_device->id3d12_device());
            frame.fence_value = 0;
        }

        _rtv_descriptor_stride = _device->id3d12_device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_ROOT_PARAMETER root_param{ };
        root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        root_param.Constants.Num32BitValues = 1;
        root_param.Constants.ShaderRegister = 0; // b0
        root_param.Constants.RegisterSpace = 0;

        D3D12_ROOT_SIGNATURE_DESC rs_desc{ };
        rs_desc.NumParameters = 1;
        rs_desc.pParameters = &root_param;
        rs_desc.NumStaticSamplers = 0;
        rs_desc.pStaticSamplers = nullptr;
        rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* sig_blob{ nullptr };
        ID3DBlob* error_blob{ nullptr };
        HRESULT hr = D3D12SerializeRootSignature(
            &rs_desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &sig_blob,
            &error_blob
        );

        if (FAILED(hr))
        {
            if (error_blob)
            {
                LOG_GRAPHICS_ERROR("DX12 root signature error: {}",
                                   static_cast<const char*>(error_blob->GetBufferPointer()));
                error_blob->Release();
            }
            LOG_GRAPHICS_FATAL("Failed to serialize DX12 root signature");
        }

        hr = _device->id3d12_device()->CreateRootSignature(
            0,
            sig_blob->GetBufferPointer(),
            sig_blob->GetBufferSize(),
            IID_PPV_ARGS(&_root_signature)
        );

        sig_blob->Release();
        if (error_blob) error_blob->Release();

        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DX12 root signature");

        const std::string vs_path = "shaders/triangle.vert.dxil";
        const std::string ps_path = "shaders/triangle.frag.dxil";

        auto vs_bytes = utils::file::load_binary_file(vs_path);
        auto ps_bytes = utils::file::load_binary_file(ps_path);

        D3D12_SHADER_BYTECODE vs_bc{ };
        vs_bc.pShaderBytecode = vs_bytes.data();
        vs_bc.BytecodeLength = vs_bytes.size();

        D3D12_SHADER_BYTECODE ps_bc{ };
        ps_bc.pShaderBytecode = ps_bytes.data();
        ps_bc.BytecodeLength = ps_bytes.size();

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{ };
        pso_desc.pRootSignature = _root_signature;
        pso_desc.VS = vs_bc;
        pso_desc.PS = ps_bc;

        // No input layout (SV_VertexID only)
        pso_desc.InputLayout = { nullptr, 0 };

        // Rasterizer
        D3D12_RASTERIZER_DESC rast{ };
        rast.FillMode = D3D12_FILL_MODE_SOLID;
        rast.CullMode = D3D12_CULL_MODE_BACK;
        rast.FrontCounterClockwise = FALSE;
        rast.DepthClipEnable = TRUE;
        pso_desc.RasterizerState = rast;

        // Blend
        D3D12_BLEND_DESC blend{ };
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;
        auto& rt0 = blend.RenderTarget[0];
        rt0.BlendEnable = FALSE;
        rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso_desc.BlendState = blend;

        // Depth-stencil off
        D3D12_DEPTH_STENCIL_DESC ds{ };
        ds.DepthEnable = FALSE;
        ds.StencilEnable = FALSE;
        pso_desc.DepthStencilState = ds;

        pso_desc.SampleMask = UINT_MAX;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.SampleDesc.Count = 1;
        pso_desc.SampleDesc.Quality = 0;

        hr = _device->id3d12_device()->CreateGraphicsPipelineState(
            &pso_desc,
            IID_PPV_ARGS(&_pipeline_state)
        );

        if (FAILED(hr))
            LOG_GRAPHICS_FATAL("Failed to create DX12 pipeline state for triangle");
    }

    dx12_rhi_context_t::~dx12_rhi_context_t()
    {
        wait_idle();

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

        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
        {
            dx12_frame_t& frame{ _frames[i] };

            frame.command_list.reset();
            frame.fence.reset();

            if (frame.allocator)
            {
                frame.allocator->Release();
                frame.allocator = nullptr;
            }
        }

        _swapchain.reset();
        _graphics_queue.reset();
        _device.reset();
    }

    void dx12_rhi_context_t::begin_frame()
    {
        dx12_frame_t& frame{ _frames[_frame_index] };
        frame.fence->wait(frame.fence_value);

        frame.allocator->Reset();
        frame.command_list->set_allocator(frame.allocator);
        frame.command_list->reset();
        frame.command_list->begin_recording();

        _swapchain->acquire_next_image(nullptr);
    }

    void dx12_rhi_context_t::record_frame()
    {
        ID3D12GraphicsCommandList* cmd{ _frames[_frame_index].command_list->id3d12_graphics_command_list() };
        const dx12_swapchain_t* sc{ _swapchain.get() };

        const UINT stride{
            _device->id3d12_device()->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
        };

        float clear[]{ 0.02f, 0.02f, 0.04f, 1.0f };

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv{ sc->get_current_rtv(stride) };
        ID3D12Resource* backbuffer{ sc->get_backbuffer(sc->get_current_image_index()) };

        D3D12_RESOURCE_BARRIER to_rtv{ };
        to_rtv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_rtv.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        to_rtv.Transition.pResource = backbuffer;
        to_rtv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        to_rtv.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        to_rtv.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        cmd->ResourceBarrier(1, &to_rtv);

        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmd->ClearRenderTargetView(sc->get_current_rtv(stride), clear, 0, nullptr);

        D3D12_VIEWPORT viewport{ };
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(sc->get_width());
        viewport.Height = static_cast<float>(sc->get_height());
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissor{ };
        scissor.left = 0;
        scissor.top = 0;
        scissor.right = static_cast<LONG>(sc->get_width());
        scissor.bottom = static_cast<LONG>(sc->get_height());

        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor);

        // ── Bind pipeline + root constants ────────
        cmd->SetGraphicsRootSignature(_root_signature);
        cmd->SetPipelineState(_pipeline_state);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // frameCount for rotation
        cmd->SetGraphicsRoot32BitConstant(0, _frame_counter, 0);

        // No vertex buffer needed (SV_VertexID)
        cmd->DrawInstanced(3, 1, 0, 0);

        _frame_counter++;

        std::swap(to_rtv.Transition.StateBefore, to_rtv.Transition.StateAfter);
        cmd->ResourceBarrier(1, &to_rtv);
    }

    void dx12_rhi_context_t::end_frame()
    {
        auto& f{ _frames[_frame_index] };
        f.command_list->end_recording();

        _graphics_queue->submit(f.command_list.get(), f.fence.get(), nullptr, nullptr);
        f.fence_value = f.fence->current_value();
        _swapchain->present(nullptr);

        _frame_index = (_frame_index + 1) % k_max_frames_in_flight;
    }

    void dx12_rhi_context_t::resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        // Make sure GPU is not touching old backbuffers/RTVs.
        wait_idle();

        if (_swapchain)
        {
            _swapchain->resize(width, height);
        }
    }

    rhi_device_t* dx12_rhi_context_t::get_device() const noexcept
    {
        return _device.get();
    }

    rhi_swapchain_t* dx12_rhi_context_t::get_swapchain() const noexcept
    {
        return _swapchain.get();
    }

    rhi_command_queue_t* dx12_rhi_context_t::get_command_queue() const noexcept
    {
        return _graphics_queue.get();
    }

    std::unique_ptr<rhi_texture_t> dx12_rhi_context_t::create_texture_2d(const texture_create_info_t& info)
    {
        return nullptr;
    }

    void dx12_rhi_context_t::wait_idle()
    {
        if (_graphics_queue)
            _graphics_queue->wait_idle();

        // These should all be completed now, but we are being safe
        for (uint32_t i{ 0 }; i < k_max_frames_in_flight; ++i)
            _frames[i].fence->wait(_frames[i].fence_value);
    }
} // namespace carrot::rhi::dx12
