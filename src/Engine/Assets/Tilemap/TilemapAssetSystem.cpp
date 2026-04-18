//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapAssetSystem.h"

#include "Assets/AssetID.h"
#include "TilemapAssetLoader.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] asset_iteration_status_t base_tilemap_status(const tilemap_asset_record_t& record) noexcept
        {
            asset_iteration_status_t status;
            status.kind = asset_kind_t::tilemap;
            status.id = record.id;
            status.logical_id = record.logical_id;
            status.source_uri = record.source_uri;
            status.manifest_uri = record.manifest_uri;
            status.dependency_shape = asset_dependency_shape_t::scene_or_world_structure;
            status.watch_mode = asset_iteration_watch_mode_t::not_polled;
            status.dependency_summary =
                "Depends on tilemap authored data, imported tileset/image references, and manifest import settings; changes affect world structure, collision, layering, and authored light data.";
            status.reload_policy = asset_reload_policy_t::restart_or_scene_rebuild_required;
            return status;
        }

        [[nodiscard]] std::string_view to_string(const tilemap_asset_load_error_t error) noexcept
        {
            switch (error)
            {
                case tilemap_asset_load_error_t::none: return "none";
                case tilemap_asset_load_error_t::invalid_record: return "invalid_record";
                case tilemap_asset_load_error_t::resolve_failed: return "resolve_failed";
                case tilemap_asset_load_error_t::source_not_found: return "source_not_found";
                case tilemap_asset_load_error_t::manifest_not_found: return "manifest_not_found";
                case tilemap_asset_load_error_t::decode_failed: return "decode_failed";
                case tilemap_asset_load_error_t::texture_create_failed: return "texture_create_failed";
                case tilemap_asset_load_error_t::cooked_write_failed: return "cooked_write_failed";
                default: return "unknown";
            }
        }
    }

    const loaded_tilemap_asset_t* tilemap_asset_system_t::get(const asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return &it->second;

        const tilemap_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Tilemap asset id '{}' is not registered", id);
            return nullptr;
        }

        tilemap_asset_load_result_t result{ load_tilemap_asset(*record, _vfs, _rhi) };
        if (!result.success())
        {
            record_load_result(*record, result, to_string(result.error));
            LOG_ASSET_ERROR("Failed to load tilemap asset '{}': {}", record->logical_id, to_string(result.error));
            return nullptr;
        }

        const auto [it, inserted]{ _loaded.emplace(id, std::move(result.asset)) };
        record_load_result(*record, result, {});
        return &it->second;
    }

    const loaded_tilemap_asset_t* tilemap_asset_system_t::get(const std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void tilemap_asset_system_t::cache_loaded(const asset_id_t id, loaded_tilemap_asset_t asset)
    {
        _loaded.insert_or_assign(id, std::move(asset));
    }

    std::vector<asset_iteration_status_t> tilemap_asset_system_t::collect_iteration_statuses() const
    {
        std::vector<asset_iteration_status_t> out;
        out.reserve(_registry.records().size());

        for (const auto& [id, record] : _registry.records())
        {
            auto status{ find_iteration_status(id).value_or(base_tilemap_status(record)) };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            out.push_back(std::move(status));
        }

        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> tilemap_asset_system_t::find_iteration_status(const asset_id_t id) const
    {
        const tilemap_asset_record_t* record{ _registry.find(id) };
        if (!record)
            return std::nullopt;

        if (const auto it{ _statuses.find(id) }; it != _statuses.end())
        {
            auto status{ it->second };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            return status;
        }

        auto status{ base_tilemap_status(*record) };
        status.loaded_in_runtime_cache = _loaded.contains(id);
        return status;
    }

    void tilemap_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void tilemap_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
        _statuses.clear();
    }

    asset_iteration_status_t tilemap_asset_system_t::make_status(const tilemap_asset_record_t& record) const
    {
        auto status{ base_tilemap_status(record) };
        status.loaded_in_runtime_cache = _loaded.contains(record.id);
        return status;
    }

    void tilemap_asset_system_t::record_load_result(const tilemap_asset_record_t& record,
                                                    const tilemap_asset_load_result_t& result,
                                                    const std::string_view error_message)
    {
        asset_iteration_status_t status{ make_status(record) };
        if (const auto it{ _statuses.find(record.id) }; it != _statuses.end())
            status = it->second;

        status.kind = asset_kind_t::tilemap;
        status.id = record.id;
        status.logical_id = record.logical_id;
        status.source_uri = record.source_uri;
        status.manifest_uri = record.manifest_uri;
        status.reload_policy = asset_reload_policy_t::restart_or_scene_rebuild_required;
        status.loaded_in_runtime_cache = result.success();
        status.has_last_attempt = true;
        status.last_attempt_at = std::chrono::system_clock::now();
        status.last_result = result.success() ? asset_iteration_result_t::success : asset_iteration_result_t::failed;
        status.last_load_origin = result.load_origin;
        status.last_cooked_artifact_state = result.cooked_artifact_state;
        status.last_invalidation_reason = result.invalidation_reason;
        status.last_error = std::string{ error_message };
        _statuses[record.id] = std::move(status);
    }
} // namespace carrot::assets
