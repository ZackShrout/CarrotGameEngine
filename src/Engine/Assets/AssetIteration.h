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

    enum class asset_runtime_refresh_action_t
    {
        none,
        reload_now,
        reload_on_next_use,
        manual_refresh,
        rebuild_current_scene,
        restart_runtime,
    };

    enum class asset_load_origin_t
    {
        never_loaded,
        cooked_cache,
        regenerated_from_source,
        source_without_cooked_cache,
        streamed_direct,
    };

    enum class asset_dependency_shape_t
    {
        leaf_runtime_data,
        referenced_runtime_assets,
        layout_or_presentation_contract,
        scene_or_world_structure,
    };

    enum class asset_iteration_watch_mode_t
    {
        not_polled,
        source_and_manifest_timestamps,
    };

    enum class asset_iteration_watch_change_t
    {
        none,
        source_timestamp_changed,
        manifest_timestamp_changed,
        source_and_manifest_timestamps_changed,
    };

    enum class asset_iteration_request_origin_t
    {
        none,
        manual_request,
        automatic_watch_poll,
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
        asset_dependency_shape_t dependency_shape{ asset_dependency_shape_t::leaf_runtime_data };
        asset_iteration_watch_mode_t watch_mode{ asset_iteration_watch_mode_t::not_polled };
        std::string dependency_summary;
        asset_reload_policy_t reload_policy{ asset_reload_policy_t::manual_refresh_only };
        bool loaded_in_runtime_cache{ false };
        asset_iteration_watch_change_t last_watch_change{ asset_iteration_watch_change_t::none };
        asset_iteration_request_origin_t last_refresh_request_origin{ asset_iteration_request_origin_t::none };
        asset_runtime_refresh_action_t last_requested_action{ asset_runtime_refresh_action_t::none };
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

    [[nodiscard]] constexpr std::string_view to_string(const asset_dependency_shape_t shape) noexcept
    {
        switch (shape)
        {
            case asset_dependency_shape_t::leaf_runtime_data: return "leaf_runtime_data";
            case asset_dependency_shape_t::referenced_runtime_assets: return "referenced_runtime_assets";
            case asset_dependency_shape_t::layout_or_presentation_contract: return "layout_or_presentation_contract";
            case asset_dependency_shape_t::scene_or_world_structure: return "scene_or_world_structure";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view to_string(const asset_iteration_watch_mode_t mode) noexcept
    {
        switch (mode)
        {
            case asset_iteration_watch_mode_t::not_polled: return "not_polled";
            case asset_iteration_watch_mode_t::source_and_manifest_timestamps: return "source_and_manifest_timestamps";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view to_string(const asset_iteration_watch_change_t change) noexcept
    {
        switch (change)
        {
            case asset_iteration_watch_change_t::none: return "none";
            case asset_iteration_watch_change_t::source_timestamp_changed: return "source_timestamp_changed";
            case asset_iteration_watch_change_t::manifest_timestamp_changed: return "manifest_timestamp_changed";
            case asset_iteration_watch_change_t::source_and_manifest_timestamps_changed:
                return "source_and_manifest_timestamps_changed";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view to_string(const asset_iteration_request_origin_t origin) noexcept
    {
        switch (origin)
        {
            case asset_iteration_request_origin_t::none: return "none";
            case asset_iteration_request_origin_t::manual_request: return "manual_request";
            case asset_iteration_request_origin_t::automatic_watch_poll: return "automatic_watch_poll";
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

    [[nodiscard]] constexpr std::string_view to_string(const asset_runtime_refresh_action_t action) noexcept
    {
        switch (action)
        {
            case asset_runtime_refresh_action_t::none: return "none";
            case asset_runtime_refresh_action_t::reload_now: return "reload_now";
            case asset_runtime_refresh_action_t::reload_on_next_use: return "reload_on_next_use";
            case asset_runtime_refresh_action_t::manual_refresh: return "manual_refresh";
            case asset_runtime_refresh_action_t::rebuild_current_scene: return "rebuild_current_scene";
            case asset_runtime_refresh_action_t::restart_runtime: return "restart_runtime";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr asset_runtime_refresh_action_t recommended_runtime_refresh_action(
        const asset_reload_policy_t policy,
        const bool has_active_scene) noexcept
    {
        switch (policy)
        {
            case asset_reload_policy_t::reloadable_live:
                return asset_runtime_refresh_action_t::reload_now;
            case asset_reload_policy_t::reloadable_on_next_use:
                return asset_runtime_refresh_action_t::reload_on_next_use;
            case asset_reload_policy_t::manual_refresh_only:
                return asset_runtime_refresh_action_t::manual_refresh;
            case asset_reload_policy_t::restart_or_scene_rebuild_required:
                return has_active_scene
                    ? asset_runtime_refresh_action_t::rebuild_current_scene
                    : asset_runtime_refresh_action_t::restart_runtime;
            default:
                return asset_runtime_refresh_action_t::none;
        }
    }

    [[nodiscard]] constexpr asset_runtime_refresh_action_t recommended_runtime_refresh_action(
        const asset_iteration_status_t& status,
        const bool has_active_scene) noexcept
    {
        switch (status.reload_policy)
        {
            case asset_reload_policy_t::reloadable_live:
                return status.loaded_in_runtime_cache
                    ? asset_runtime_refresh_action_t::reload_now
                    : asset_runtime_refresh_action_t::reload_on_next_use;
            case asset_reload_policy_t::reloadable_on_next_use:
                return asset_runtime_refresh_action_t::reload_on_next_use;
            case asset_reload_policy_t::manual_refresh_only:
                return status.loaded_in_runtime_cache
                    ? asset_runtime_refresh_action_t::manual_refresh
                    : asset_runtime_refresh_action_t::reload_on_next_use;
            case asset_reload_policy_t::restart_or_scene_rebuild_required:
                switch (status.dependency_shape)
                {
                    case asset_dependency_shape_t::scene_or_world_structure:
                    case asset_dependency_shape_t::layout_or_presentation_contract:
                        return has_active_scene
                            ? asset_runtime_refresh_action_t::rebuild_current_scene
                            : asset_runtime_refresh_action_t::restart_runtime;
                    case asset_dependency_shape_t::referenced_runtime_assets:
                    case asset_dependency_shape_t::leaf_runtime_data:
                        return status.loaded_in_runtime_cache
                            ? asset_runtime_refresh_action_t::manual_refresh
                            : asset_runtime_refresh_action_t::reload_on_next_use;
                    default:
                        return has_active_scene
                            ? asset_runtime_refresh_action_t::rebuild_current_scene
                            : asset_runtime_refresh_action_t::restart_runtime;
                }
            default:
                return asset_runtime_refresh_action_t::none;
        }
    }

    [[nodiscard]] constexpr asset_iteration_watch_change_t detect_watch_change(const bool source_changed,
                                                                               const bool manifest_changed) noexcept
    {
        if (source_changed && manifest_changed)
            return asset_iteration_watch_change_t::source_and_manifest_timestamps_changed;
        if (source_changed)
            return asset_iteration_watch_change_t::source_timestamp_changed;
        if (manifest_changed)
            return asset_iteration_watch_change_t::manifest_timestamp_changed;
        return asset_iteration_watch_change_t::none;
    }

    [[nodiscard]] constexpr std::string_view describe_watch_change(const asset_iteration_watch_change_t change) noexcept
    {
        switch (change)
        {
            case asset_iteration_watch_change_t::none:
                return "No watched source or manifest timestamp change has been observed.";
            case asset_iteration_watch_change_t::source_timestamp_changed:
                return "The authored source changed since the previous watch snapshot.";
            case asset_iteration_watch_change_t::manifest_timestamp_changed:
                return "The asset manifest or definition changed since the previous watch snapshot.";
            case asset_iteration_watch_change_t::source_and_manifest_timestamps_changed:
                return "Both the authored source and manifest changed since the previous watch snapshot.";
            default:
                return "The watch change state is unknown.";
        }
    }

    [[nodiscard]] constexpr std::string_view describe_runtime_refresh_action_reason(
        const asset_iteration_status_t& status,
        const bool has_active_scene) noexcept
    {
        switch (recommended_runtime_refresh_action(status, has_active_scene))
        {
            case asset_runtime_refresh_action_t::reload_now:
                return "This asset is live-reloadable and currently cached, so the engine can refresh it immediately.";
            case asset_runtime_refresh_action_t::reload_on_next_use:
                return "This asset is not currently cached in runtime, so the latest version will be picked up on next use.";
            case asset_runtime_refresh_action_t::manual_refresh:
                return "This asset currently requires an explicit refresh request instead of automatic live reload.";
            case asset_runtime_refresh_action_t::rebuild_current_scene:
                return "This asset affects scene/world structure or presentation contracts, so rebuilding the current scene is the safest refresh path.";
            case asset_runtime_refresh_action_t::restart_runtime:
                return "This asset affects scene/world structure or presentation contracts and no active scene rebuild path is available, so restarting runtime is the safest refresh path.";
            case asset_runtime_refresh_action_t::none:
            default:
                return "No runtime refresh action is currently recommended.";
        }
    }

    [[nodiscard]] constexpr std::string_view describe_invalidation_reason(const imported_artifact_issue_t issue) noexcept
    {
        switch (issue)
        {
            case imported_artifact_issue_t::none:
                return "No cooked artifact invalidation reason is currently recorded.";
            case imported_artifact_issue_t::missing_artifact:
                return "No cooked artifact was found, so the runtime load had to rebuild or fail.";
            case imported_artifact_issue_t::unreadable_artifact:
                return "The cooked artifact exists but could not be read as a valid imported asset.";
            case imported_artifact_issue_t::importer_version_changed:
                return "The importer version changed, so previously cooked output is no longer trusted.";
            case imported_artifact_issue_t::source_changed:
                return "The authored source content changed, so previously cooked output is stale.";
            case imported_artifact_issue_t::asset_definition_changed:
                return "The asset definition changed, so previously cooked output is stale.";
            case imported_artifact_issue_t::import_settings_changed:
                return "Import settings changed, so previously cooked output is stale.";
            case imported_artifact_issue_t::reserved_changed:
                return "A reserved invalidation field changed, so previously cooked output is treated as stale.";
            default:
                return "The cooked artifact invalidation reason is unknown.";
        }
    }

    [[nodiscard]] constexpr std::string_view describe_last_attempt_summary(const asset_iteration_status_t& status) noexcept
    {
        if (!status.has_last_attempt)
            return "No runtime load or refresh attempt has been recorded yet.";

        if (status.last_result == asset_iteration_result_t::failed)
            return "The most recent runtime load or refresh attempt failed.";

        switch (status.last_load_origin)
        {
            case asset_load_origin_t::never_loaded:
                return "A runtime attempt was recorded without a resolved load origin.";
            case asset_load_origin_t::cooked_cache:
                return "The most recent runtime attempt loaded the asset from cooked cache.";
            case asset_load_origin_t::regenerated_from_source:
                return "The most recent runtime attempt regenerated or refreshed the asset from source data.";
            case asset_load_origin_t::source_without_cooked_cache:
                return "The most recent runtime attempt loaded directly from source because cooked cache was unavailable.";
            case asset_load_origin_t::streamed_direct:
                return "The most recent runtime attempt bound the asset through direct streaming.";
            default:
                return "The most recent runtime load or refresh attempt succeeded.";
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
