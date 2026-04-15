//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AssetID.h"
#include "ImportedAssetCache.h"

#include <chrono>
#include <string>
#include <string_view>

namespace carrot::assets {
    enum class asset_kind_t
    {
        font,
        texture,
        sprite,
        audio,
        tilemap,
        scene,
    };

    enum class asset_reload_policy_t
    {
        reloadable_live,
        reloadable_on_next_use,
        manual_refresh_only,
        restart_or_scene_rebuild_required,
    };

    enum class asset_load_origin_t
    {
        never_loaded,
        cooked_cache,
        regenerated_from_source,
        source_without_cooked_cache,
        streamed_direct,
    };

    enum class asset_iteration_result_t
    {
        never_attempted,
        success,
        failed,
    };

    struct asset_iteration_status_t
    {
        asset_kind_t kind{ asset_kind_t::texture };
        asset_id_t id{ 0 };
        std::string logical_id;
        std::string source_uri;
        std::string manifest_uri;
        asset_reload_policy_t reload_policy{ asset_reload_policy_t::manual_refresh_only };
        bool loaded_in_runtime_cache{ false };
        bool has_last_attempt{ false };
        std::chrono::system_clock::time_point last_attempt_at{ };
        asset_iteration_result_t last_result{ asset_iteration_result_t::never_attempted };
        asset_load_origin_t last_load_origin{ asset_load_origin_t::never_loaded };
        imported_artifact_state_t last_cooked_artifact_state{ imported_artifact_state_t::missing };
        imported_artifact_issue_t last_invalidation_reason{ imported_artifact_issue_t::none };
        std::string last_error;
    };

    [[nodiscard]] constexpr std::string_view to_string(const asset_kind_t kind) noexcept
    {
        switch (kind)
        {
            case asset_kind_t::font: return "font";
            case asset_kind_t::texture: return "texture";
            case asset_kind_t::sprite: return "sprite";
            case asset_kind_t::audio: return "audio";
            case asset_kind_t::tilemap: return "tilemap";
            case asset_kind_t::scene: return "scene";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view to_string(const asset_reload_policy_t policy) noexcept
    {
        switch (policy)
        {
            case asset_reload_policy_t::reloadable_live: return "reloadable_live";
            case asset_reload_policy_t::reloadable_on_next_use: return "reloadable_on_next_use";
            case asset_reload_policy_t::manual_refresh_only: return "manual_refresh_only";
            case asset_reload_policy_t::restart_or_scene_rebuild_required: return "restart_or_scene_rebuild_required";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view to_string(const asset_load_origin_t origin) noexcept
    {
        switch (origin)
        {
            case asset_load_origin_t::never_loaded: return "never_loaded";
            case asset_load_origin_t::cooked_cache: return "cooked_cache";
            case asset_load_origin_t::regenerated_from_source: return "regenerated_from_source";
            case asset_load_origin_t::source_without_cooked_cache: return "source_without_cooked_cache";
            case asset_load_origin_t::streamed_direct: return "streamed_direct";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view to_string(const asset_iteration_result_t result) noexcept
    {
        switch (result)
        {
            case asset_iteration_result_t::never_attempted: return "never_attempted";
            case asset_iteration_result_t::success: return "success";
            case asset_iteration_result_t::failed: return "failed";
            default: return "unknown";
        }
    }
} // namespace carrot::assets
