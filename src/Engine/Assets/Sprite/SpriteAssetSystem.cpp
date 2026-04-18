//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SpriteAssetSystem.h"

#include "Assets/AssetID.h"
#include "SpriteAssetLoader.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] asset_iteration_status_t base_sprite_status(const sprite_asset_record_t& record) noexcept
        {
            asset_iteration_status_t status;
            status.kind = asset_kind_t::sprite;
            status.id = record.id;
            status.logical_id = record.logical_id;
            status.source_uri = record.source_uri;
            status.manifest_uri = record.manifest_uri;
            status.dependency_shape = asset_dependency_shape_t::referenced_runtime_assets;
            status.watch_mode = asset_iteration_watch_mode_t::source_and_manifest_timestamps;
            status.dependency_summary = std::string{
                "Depends on sprite authored data and referenced texture asset '"
            } + std::string{ record.sprite.texture_id() } + "'.";
            status.reload_policy = asset_reload_policy_t::reloadable_live;
            return status;
        }
    }

    const loaded_sprite_asset_t* sprite_asset_system_t::get(const asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return it->second.get();

        const sprite_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Sprite asset id '{}' is not registered", id);
            return nullptr;
        }

        sprite_asset_load_result_t result{ load_sprite_asset(*record, _vfs, _textures) };
        if (!result.success())
        {
            record_load_result(*record, result, result.error == sprite_asset_load_error_t::missing_texture_asset
                                                 ? "missing_texture_asset"
                                                 : result.error == sprite_asset_load_error_t::invalid_record
                                                     ? "invalid_record"
                                                     : result.error == sprite_asset_load_error_t::source_not_found
                                                         ? "source_not_found"
                                                         : result.error == sprite_asset_load_error_t::manifest_not_found
                                                             ? "manifest_not_found"
                                                             : "cooked_write_failed");
            LOG_ASSET_ERROR(
                "Failed to load sprite asset '{}': missing texture asset '{}'",
                record->logical_id,
                record->sprite.texture_id()
            );
            return nullptr;
        }

        auto loaded_asset{ std::make_unique<loaded_sprite_asset_t>(std::move(result.asset)) };
        const loaded_sprite_asset_t* loaded_asset_ptr{ loaded_asset.get() };
        _loaded.emplace(id, std::move(loaded_asset));
        record_load_result(*record, result, {});
        return loaded_asset_ptr;
    }

    const loaded_sprite_asset_t* sprite_asset_system_t::get(const std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void sprite_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void sprite_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
        _statuses.clear();
    }

    std::vector<asset_iteration_status_t> sprite_asset_system_t::collect_iteration_statuses() const
    {
        std::vector<asset_iteration_status_t> out;
        out.reserve(_registry.records().size());

        for (const auto& [id, record] : _registry.records())
        {
            auto status{ find_iteration_status(id).value_or(base_sprite_status(record)) };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            out.push_back(std::move(status));
        }

        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> sprite_asset_system_t::find_iteration_status(const asset_id_t id) const
    {
        const sprite_asset_record_t* record{ _registry.find(id) };
        if (!record)
            return std::nullopt;

        if (const auto it{ _statuses.find(id) }; it != _statuses.end())
        {
            auto status{ it->second };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            return status;
        }

        auto status{ base_sprite_status(*record) };
        status.loaded_in_runtime_cache = _loaded.contains(id);
        return status;
    }

    bool sprite_asset_system_t::reload(const asset_id_t id)
    {
        _loaded.erase(id);
        return get(id) != nullptr;
    }

    bool sprite_asset_system_t::reload(const std::string_view logical_id)
    {
        return reload(make_asset_id(logical_id));
    }

    asset_iteration_status_t sprite_asset_system_t::make_status(const sprite_asset_record_t& record) const
    {
        auto status{ base_sprite_status(record) };
        status.loaded_in_runtime_cache = _loaded.contains(record.id);
        return status;
    }

    void sprite_asset_system_t::record_load_result(const sprite_asset_record_t& record,
                                                   const sprite_asset_load_result_t& result,
                                                   const std::string_view error_message)
    {
        asset_iteration_status_t status{ make_status(record) };
        if (const auto it{ _statuses.find(record.id) }; it != _statuses.end())
            status = it->second;

        status.kind = asset_kind_t::sprite;
        status.id = record.id;
        status.logical_id = record.logical_id;
        status.source_uri = record.source_uri;
        status.manifest_uri = record.manifest_uri;
        status.reload_policy = asset_reload_policy_t::reloadable_live;
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
