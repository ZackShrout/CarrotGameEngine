//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Logger.h"

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace carrot::rhi::dx12 {
    inline void dx12_check(const HRESULT hr, const char* expr, const char* file, const int line)
    {
        if (SUCCEEDED(hr))
            return;

        LOG_GRAPHICS_FATAL("DX12 call failed: {} (hr={:#010x}) at {}:{}",
                           expr,
                           static_cast<uint32_t>(hr),
                           file,
                           line);
    }

#if defined(_DEBUG)
    inline void dx12_set_debug_name(ID3D12Object* object, const wchar_t* name)
    {
        if (object && name)
            object->SetName(name);
    }
#else
    inline void dx12_set_debug_name(ID3D12Object*, const wchar_t*) {}
#endif

#if defined(_DEBUG)
    inline void dx12_set_debug_name_indexed(ID3D12Object* object, const uint32_t index, const wchar_t* base_name)
    {
        if (!object || !base_name)
            return;

        wchar_t full_name[128]{ };
        swprintf_s(full_name, L"%s [%u]", base_name, index);
        object->SetName(full_name);
    }
#else
    inline void dx12_set_debug_name_indexed(ID3D12Object*, uint32_t, const wchar_t*) {}
#endif
} // namespace carrot::rhi::dx12

#define DX12_CHECK(expr) ::carrot::rhi::dx12::dx12_check((expr), #expr, __FILE__, __LINE__)
#define DX12_NAME(obj, name) ::carrot::rhi::dx12::dx12_set_debug_name((obj), (name))
#define DX12_NAME_INDEXED(obj, index, name) \
    ::carrot::rhi::dx12::dx12_set_debug_name_indexed((obj), (index), (name))
