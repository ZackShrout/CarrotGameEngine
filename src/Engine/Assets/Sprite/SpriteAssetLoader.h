//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetIteration.h"
#include "LoadedSpriteAsset.h"
#include "SpriteAsset.h"

#include <filesystem>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::assets {
    struct sprite_asset_record_t;
    class texture_asset_system_t;

    enum class sprite_asset_load_error_t
    {
        none = 0,
        invalid_record,
        missing_texture_asset,
        source_not_found,
        manifest_not_found,
        cooked_write_failed
    };

    struct sprite_asset_load_result_t
    {
        loaded_sprite_asset_t asset;
        sprite_asset_load_error_t error{ sprite_asset_load_error_t::none };
        asset_load_origin_t load_origin{ asset_load_origin_t::never_loaded };
        imported_artifact_state_t cooked_artifact_state{ imported_artifact_state_t::missing };
        imported_artifact_issue_t invalidation_reason{ imported_artifact_issue_t::none };

        [[nodiscard]] bool success() const noexcept
        {
            return error == sprite_asset_load_error_t::none;
        }
    };

    [[nodiscard]] sprite_asset_load_result_t load_sprite_asset(
        const sprite_asset_record_t& record,
        const io::virtual_file_system_t& vfs,
        texture_asset_system_t& textures
    );

    [[nodiscard]] std::filesystem::path cooked_sprite_cache_path(std::string_view logical_id,
                                                                 const io::virtual_file_system_t& vfs) noexcept;
} // namespace carrot::assets
