//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextureAssetSystem.h"

#include "Assets/AssetID.h"
#include "TextureAssetLoader.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] asset_iteration_status_t base_texture_status(const texture_asset_record_t& record) noexcept
        {
            asset_iteration_status_t status;
            status.kind = asset_kind_t::texture;
            status.id = record.id;
            status.logical_id = record.logical_id;
            status.source_uri = record.source_uri;
            status.manifest_uri = record.manifest_uri;
            status.dependency_shape = asset_dependency_shape_t::leaf_runtime_data;
            status.watch_mode = asset_iteration_watch_mode_t::source_and_manifest_timestamps;
            status.dependency_summary =
                "Depends on texture source pixels and manifest import settings; safe leaf runtime data for live reload.";
            status.reload_policy = asset_reload_policy_t::reloadable_live;
            return status;
        }
    }

    const loaded_texture_asset_t* texture_asset_system_t::get(asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return it->second.get();

        const texture_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Texture asset id '{}' is not registered", id);
            return nullptr;
        }

        texture_asset_load_result_t result{ load_texture_asset(*record, _vfs, _rhi) };
        if (!result.success())
        {
            record_load_result(*record, result, to_string(result.error));
            LOG_ASSET_ERROR(
                "Failed to load texture asset '{}' from '{}': {}",
                record->logical_id,
                record->source_uri,
                to_string(result.error)
            );
            return nullptr;
        }

        auto loaded_asset{ std::make_unique<loaded_texture_asset_t>(std::move(result.asset)) };
        const loaded_texture_asset_t* loaded_asset_ptr{ loaded_asset.get() };
        _loaded.emplace(id, std::move(loaded_asset));
        record_load_result(*record, result, {});
        return loaded_asset_ptr;
    }

    const loaded_texture_asset_t* texture_asset_system_t::get(std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void texture_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void texture_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
        _statuses.clear();
    }

    std::vector<asset_iteration_status_t> texture_asset_system_t::collect_iteration_statuses() const
    {
        std::vector<asset_iteration_status_t> out;
        out.reserve(_registry.records().size());

        for (const auto& [id, record] : _registry.records())
        {
            auto status{ find_iteration_status(id).value_or(base_texture_status(record)) };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            out.push_back(std::move(status));
        }

        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> texture_asset_system_t::find_iteration_status(const asset_id_t id) const
    {
        const texture_asset_record_t* record{ _registry.find(id) };
        if (!record)
            return std::nullopt;

        if (const auto it{ _statuses.find(id) }; it != _statuses.end())
        {
            auto status{ it->second };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            return status;
        }

        auto status{ base_texture_status(*record) };
        status.loaded_in_runtime_cache = _loaded.contains(id);
        return status;
    }

    bool texture_asset_system_t::reload(const asset_id_t id)
    {
        _loaded.erase(id);
        return get(id) != nullptr;
    }

    bool texture_asset_system_t::reload(const std::string_view logical_id)
    {
        return reload(make_asset_id(logical_id));
    }

    asset_iteration_status_t texture_asset_system_t::make_status(const texture_asset_record_t& record) const
    {
        auto status{ base_texture_status(record) };
        status.loaded_in_runtime_cache = _loaded.contains(record.id);
        return status;
    }

    void texture_asset_system_t::record_load_result(const texture_asset_record_t& record,
                                                    const texture_asset_load_result_t& result,
                                                    const std::string_view error_message)
    {
        asset_iteration_status_t status{ make_status(record) };
        if (const auto it{ _statuses.find(record.id) }; it != _statuses.end())
            status = it->second;

        status.kind = asset_kind_t::texture;
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
