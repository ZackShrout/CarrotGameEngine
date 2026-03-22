//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TextureAsset.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_context_t;
}

namespace carrot::assets {
    enum class texture_asset_load_error
    {
        ok,
        resolve_failed,
        source_not_found,
        decode_failed,
        texture_create_failed,
        invalid_record,
    };

    struct texture_asset_load_result_t
    {
        loaded_texture_asset_t asset;
        texture_asset_load_error error{ texture_asset_load_error::ok };

        [[nodiscard]] bool success() const noexcept
        {
            return error == texture_asset_load_error::ok;
        }
    };

    [[nodiscard]] texture_asset_load_result_t load_texture_asset(
        const texture_asset_record_t& record,
        const io::virtual_file_system_t& vfs,
        rhi::rhi_context_t& rhi
    ) noexcept;

    [[nodiscard]] constexpr std::string_view to_string(const texture_asset_load_error error) noexcept
    {
        switch (error)
        {
            case texture_asset_load_error::ok: return "ok";
            case texture_asset_load_error::resolve_failed: return "resolve_failed";
            case texture_asset_load_error::source_not_found: return "source_not_found";
            case texture_asset_load_error::decode_failed: return "decode_failed";
            case texture_asset_load_error::texture_create_failed: return "texture_create_failed";
            case texture_asset_load_error::invalid_record: return "invalid_record";
            default: return "unknown_error";
        }
    }
} // namespace carrot::assets
