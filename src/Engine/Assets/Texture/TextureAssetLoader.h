//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
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
        manifest_not_found,
        decode_failed,
        texture_create_failed,
        invalid_record,
        cooked_invalid_format,
        cooked_invalid_payload,
        cooked_serialize_failed,
        cooked_write_failed,
    };

    struct texture_asset_load_result_t
    {
        loaded_texture_asset_t asset;
        texture_asset_load_error error{ texture_asset_load_error::ok };
        asset_load_origin_t load_origin{ asset_load_origin_t::never_loaded };
        imported_artifact_state_t cooked_artifact_state{ imported_artifact_state_t::missing };
        imported_artifact_issue_t invalidation_reason{ imported_artifact_issue_t::none };

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

    [[nodiscard]] std::filesystem::path cooked_texture_cache_path(std::string_view logical_id,
                                                                  const io::virtual_file_system_t& vfs) noexcept;

    [[nodiscard]] constexpr std::string_view to_string(const texture_asset_load_error error) noexcept
    {
        switch (error)
        {
            case texture_asset_load_error::ok: return "ok";
            case texture_asset_load_error::resolve_failed: return "resolve_failed";
            case texture_asset_load_error::source_not_found: return "source_not_found";
            case texture_asset_load_error::manifest_not_found: return "manifest_not_found";
            case texture_asset_load_error::decode_failed: return "decode_failed";
            case texture_asset_load_error::texture_create_failed: return "texture_create_failed";
            case texture_asset_load_error::invalid_record: return "invalid_record";
            case texture_asset_load_error::cooked_invalid_format: return "cooked_invalid_format";
            case texture_asset_load_error::cooked_invalid_payload: return "cooked_invalid_payload";
            case texture_asset_load_error::cooked_serialize_failed: return "cooked_serialize_failed";
            case texture_asset_load_error::cooked_write_failed: return "cooked_write_failed";
            default: return "unknown_error";
        }
    }
} // namespace carrot::assets
