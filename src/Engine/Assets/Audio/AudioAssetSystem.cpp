//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetSystem.h"

#include "Assets/AssetID.h"
#include "Assets/Audio/AudioAssetLoader.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] asset_iteration_status_t base_audio_status(const audio_asset_record_t& record) noexcept
        {
            asset_iteration_status_t status;
            status.kind = asset_kind_t::audio;
            status.id = record.id;
            status.logical_id = record.logical_id;
            status.source_uri = record.source_uri;
            status.manifest_uri = record.manifest_uri;
            status.dependency_shape = asset_dependency_shape_t::leaf_runtime_data;
            status.watch_mode = asset_iteration_watch_mode_t::source_and_manifest_timestamps;
            status.dependency_summary = record.streamed
                ? "Depends on audio source and manifest playback settings; currently streamed from source and kept manual-refresh-oriented."
                : "Depends on audio source and manifest playback settings; safe leaf runtime data for live reload.";
            status.reload_policy = record.streamed ? asset_reload_policy_t::manual_refresh_only
                                                   : asset_reload_policy_t::reloadable_live;
            return status;
        }
    }

    const loaded_audio_asset_t* audio_asset_system_t::get(asset_id_t id)
    {
        if (const auto it{ _loaded.find(id) }; it != _loaded.end())
            return &it->second;

        const audio_asset_record_t* record{ _registry.find(id) };
        if (!record)
        {
            LOG_ASSET_ERROR("Audio asset id '{}' is not registered", id);
            return nullptr;
        }

        audio_asset_load_result_t result{ load_audio_asset(*record, _vfs) };
        if (!result.success())
        {
            record_load_result(*record, result, to_string(result.error));
            LOG_ASSET_ERROR("Failed to load audio asset '{}' from '{}': {}",
                            record->logical_id,
                            record->source_uri,
                            to_string(result.error));
            return nullptr;
        }

        const auto [it, inserted]{ _loaded.emplace(id, std::move(result.asset)) };
        (void)inserted;
        record_load_result(*record, result, {});

        return &it->second;
    }

    const loaded_audio_asset_t* audio_asset_system_t::get(std::string_view logical_id)
    {
        return get(make_asset_id(logical_id));
    }

    void audio_asset_system_t::clear_runtime_cache()
    {
        _loaded.clear();
    }

    void audio_asset_system_t::clear_all()
    {
        _loaded.clear();
        _registry.clear();
        _statuses.clear();
    }

    std::vector<asset_iteration_status_t> audio_asset_system_t::collect_iteration_statuses() const
    {
        std::vector<asset_iteration_status_t> out;
        out.reserve(_registry.records().size());

        for (const auto& [id, record] : _registry.records())
        {
            auto status{ find_iteration_status(id).value_or(base_audio_status(record)) };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            out.push_back(std::move(status));
        }

        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> audio_asset_system_t::find_iteration_status(const asset_id_t id) const
    {
        const audio_asset_record_t* record{ _registry.find(id) };
        if (!record)
            return std::nullopt;

        if (const auto it{ _statuses.find(id) }; it != _statuses.end())
        {
            auto status{ it->second };
            status.loaded_in_runtime_cache = _loaded.contains(id);
            return status;
        }

        auto status{ base_audio_status(*record) };
        status.loaded_in_runtime_cache = _loaded.contains(id);
        return status;
    }

    bool audio_asset_system_t::reload(const asset_id_t id)
    {
        _loaded.erase(id);
        return get(id) != nullptr;
    }

    bool audio_asset_system_t::reload(const std::string_view logical_id)
    {
        return reload(make_asset_id(logical_id));
    }

    asset_iteration_status_t audio_asset_system_t::make_status(const audio_asset_record_t& record) const
    {
        auto status{ base_audio_status(record) };
        status.loaded_in_runtime_cache = _loaded.contains(record.id);
        return status;
    }

    void audio_asset_system_t::record_load_result(const audio_asset_record_t& record,
                                                  const audio_asset_load_result_t& result,
                                                  const std::string_view error_message)
    {
        asset_iteration_status_t status{ make_status(record) };
        if (const auto it{ _statuses.find(record.id) }; it != _statuses.end())
            status = it->second;

        status.kind = asset_kind_t::audio;
        status.id = record.id;
        status.logical_id = record.logical_id;
        status.source_uri = record.source_uri;
        status.manifest_uri = record.manifest_uri;
        status.reload_policy = record.streamed ? asset_reload_policy_t::manual_refresh_only
                                               : asset_reload_policy_t::reloadable_live;
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
}
