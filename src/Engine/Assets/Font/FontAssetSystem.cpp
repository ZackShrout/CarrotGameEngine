//
// Created by Zack Shrout on 4/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FontAssetSystem.h"

#include "Assets/AssetID.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] asset_iteration_status_t base_font_status(const font_asset_record_t& record) noexcept
        {
            asset_iteration_status_t status;
            status.kind = asset_kind_t::font;
            status.id = record.id;
            status.logical_id = record.logical_id;
            status.source_uri = record.source_uri;
            status.manifest_uri = record.manifest_uri;
            status.reload_policy = asset_reload_policy_t::restart_or_scene_rebuild_required;
            return status;
        }
    }

    const loaded_font_asset_t* font_asset_system_t::get(const asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return it->second.get();

        const font_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Font asset id '{}' is not registered", id);
            return nullptr;
        }

        font_asset_load_result_t result{ load_font_asset(*record, _vfs, _rhi) };
        if (!result.success())
        {
            record_load_result(*record, result, to_string(result.error));
            LOG_ASSET_ERROR("Failed to load font asset '{}' from '{}': {}",
                            record->logical_id,
                            record->source_uri,
                            to_string(result.error));
            return nullptr;
        }

        auto loaded_asset{ std::make_unique<loaded_font_asset_t>(std::move(result.asset)) };
        const loaded_font_asset_t* loaded_asset_ptr{ loaded_asset.get() };
        _loaded.emplace(id, std::move(loaded_asset));
        record_load_result(*record, result, {});
        return loaded_asset_ptr;
    }

    const loaded_font_asset_t* font_asset_system_t::get(const std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    std::vector<asset_iteration_status_t> font_asset_system_t::collect_iteration_statuses() const
    {
        std::vector<asset_iteration_status_t> out;
        out.reserve(_registry.records().size());

        for (const auto& [id, record] : _registry.records())
        {
            auto status{ find_iteration_status(id).value_or(base_font_status(record)) };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            out.push_back(std::move(status));
        }

        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> font_asset_system_t::find_iteration_status(const asset_id_t id) const
    {
        const font_asset_record_t* record{ _registry.find(id) };
        if (!record)
            return std::nullopt;

        if (const auto it{ _statuses.find(id) }; it != _statuses.end())
        {
            auto status{ it->second };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            return status;
        }

        auto status{ base_font_status(*record) };
        status.loaded_in_runtime_cache = _loaded.contains(id);
        return status;
    }

    void font_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void font_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
        _statuses.clear();
    }

    asset_iteration_status_t font_asset_system_t::make_status(const font_asset_record_t& record) const
    {
        auto status{ base_font_status(record) };
        status.loaded_in_runtime_cache = _loaded.contains(record.id);
        return status;
    }

    void font_asset_system_t::record_load_result(const font_asset_record_t& record,
                                                 const font_asset_load_result_t& result,
                                                 const std::string_view error_message)
    {
        asset_iteration_status_t status{ make_status(record) };
        if (const auto it{ _statuses.find(record.id) }; it != _statuses.end())
            status = it->second;

        status.kind = asset_kind_t::font;
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
