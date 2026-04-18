//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AssetManager.h"

#include "IO/VirtualFileSystem.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] std::optional<std::filesystem::file_time_type> last_write_time_if_exists(
            const std::optional<std::filesystem::path>& path) noexcept
        {
            if (!path || !std::filesystem::exists(*path))
                return std::nullopt;

            std::error_code ec;
            const auto write_time{ std::filesystem::last_write_time(*path, ec) };
            if (ec)
                return std::nullopt;

            return write_time;
        }
    }

    std::uint64_t asset_manager_t::runtime_iteration_watch_key(const asset_kind_t kind, const asset_id_t id) noexcept
    {
        return (static_cast<std::uint64_t>(kind) << 32u) | static_cast<std::uint64_t>(id);
    }

    asset_manager_t::runtime_iteration_watch_snapshot_t asset_manager_t::capture_watch_snapshot(
        const std::string_view source_uri,
        const std::string_view manifest_uri) const
    {
        runtime_iteration_watch_snapshot_t snapshot;

        if (!source_uri.empty())
            snapshot.source_write_time = last_write_time_if_exists(_vfs.resolve_native_path(source_uri));
        if (!manifest_uri.empty())
            snapshot.manifest_write_time = last_write_time_if_exists(_vfs.resolve_native_path(manifest_uri));

        return snapshot;
    }

    std::vector<asset_iteration_status_t> asset_manager_t::collect_runtime_iteration_statuses() const
    {
        std::vector<asset_iteration_status_t> out;

        auto append = [&out](std::vector<asset_iteration_status_t> statuses)
        {
            out.insert(out.end(),
                       std::make_move_iterator(statuses.begin()),
                       std::make_move_iterator(statuses.end()));
        };

        append(_audio.collect_iteration_statuses());
        append(_fonts.collect_iteration_statuses());
        append(_textures.collect_iteration_statuses());
        append(_sprites.collect_iteration_statuses());
        append(_tilemaps.collect_iteration_statuses());
        append(_scenes.collect_iteration_statuses());
        std::ranges::transform(out, out.begin(), [this](asset_iteration_status_t status)
        {
            return enrich_runtime_iteration_status(std::move(status));
        });
        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> asset_manager_t::find_runtime_iteration_status(const asset_kind_t kind,
                                                                                            const asset_id_t id) const
    {
        std::optional<asset_iteration_status_t> status;
        switch (kind)
        {
            case asset_kind_t::font: status = _fonts.find_iteration_status(id); break;
            case asset_kind_t::audio: status = _audio.find_iteration_status(id); break;
            case asset_kind_t::texture: status = _textures.find_iteration_status(id); break;
            case asset_kind_t::sprite: status = _sprites.find_iteration_status(id); break;
            case asset_kind_t::tilemap: status = _tilemaps.find_iteration_status(id); break;
            case asset_kind_t::scene: status = _scenes.find_iteration_status(id); break;
            default: return std::nullopt;
        }

        if (!status.has_value())
            return std::nullopt;

        return enrich_runtime_iteration_status(std::move(*status));
    }

    bool asset_manager_t::reload_asset(const asset_kind_t kind, const asset_id_t id)
    {
        return reload_asset(kind, id, asset_iteration_request_origin_t::manual_request);
    }

    bool asset_manager_t::reload_asset(const asset_kind_t kind,
                                       const asset_id_t id,
                                       const asset_iteration_request_origin_t origin)
    {
        const auto status_before{ find_runtime_iteration_status(kind, id) };
        const asset_runtime_refresh_action_t action{
            status_before.has_value()
                ? recommended_runtime_refresh_action(*status_before, false)
                : asset_runtime_refresh_action_t::reload_now
        };
        note_runtime_refresh_request(kind, id, origin, action);

        switch (kind)
        {
            case asset_kind_t::font: return false;
            case asset_kind_t::audio: return _audio.reload(id);
            case asset_kind_t::texture: return _textures.reload(id);
            case asset_kind_t::sprite: return _sprites.reload(id);
            case asset_kind_t::tilemap: return false;
            case asset_kind_t::scene: return false;
            default: return false;
        }
    }

    bool asset_manager_t::reload_asset(const asset_kind_t kind, const std::string_view logical_id)
    {
        return reload_asset(kind, make_asset_id(logical_id));
    }

    asset_iteration_status_t asset_manager_t::enrich_runtime_iteration_status(asset_iteration_status_t status) const
    {
        const std::uint64_t key{ runtime_iteration_watch_key(status.kind, status.id) };
        if (const auto it{ _runtime_iteration_diagnostics.find(key) }; it != _runtime_iteration_diagnostics.end())
        {
            status.last_watch_change = it->second.last_watch_change;
            status.last_refresh_request_origin = it->second.last_refresh_request_origin;
            status.last_requested_action = it->second.last_requested_action;
        }
        return status;
    }

    void asset_manager_t::note_runtime_refresh_request(const asset_kind_t kind,
                                                       const asset_id_t id,
                                                       const asset_iteration_request_origin_t origin,
                                                       const asset_runtime_refresh_action_t action)
    {
        const std::uint64_t key{ runtime_iteration_watch_key(kind, id) };
        auto& diagnostics{ _runtime_iteration_diagnostics[key] };
        diagnostics.last_refresh_request_origin = origin;
        diagnostics.last_requested_action = action;
    }

    void asset_manager_t::poll_runtime_iteration_changes()
    {
        for (const auto& [id, record] : _textures.registry().records())
            update_runtime_iteration_watch(asset_kind_t::texture, id, record.logical_id, record.source_uri, record.manifest_uri);

        for (const auto& [id, record] : _sprites.registry().records())
            update_runtime_iteration_watch(asset_kind_t::sprite, id, record.logical_id, record.source_uri, record.manifest_uri);

        for (const auto& [id, record] : _audio.registry().records())
            update_runtime_iteration_watch(asset_kind_t::audio, id, record.logical_id, record.source_uri, record.manifest_uri);
    }

    void asset_manager_t::update_runtime_iteration_watch(const asset_kind_t kind,
                                                         const asset_id_t id,
                                                         const std::string_view logical_id,
                                                         const std::string_view source_uri,
                                                         const std::string_view manifest_uri)
    {
        const runtime_iteration_watch_snapshot_t current_snapshot{ capture_watch_snapshot(source_uri, manifest_uri) };
        const std::uint64_t key{ runtime_iteration_watch_key(kind, id) };

        const auto it{ _runtime_iteration_watches.find(key) };
        if (it == _runtime_iteration_watches.end())
        {
            _runtime_iteration_watches.emplace(key, current_snapshot);
            return;
        }

        if (it->second.source_write_time == current_snapshot.source_write_time &&
            it->second.manifest_write_time == current_snapshot.manifest_write_time)
        {
            return;
        }

        const bool source_changed{ it->second.source_write_time != current_snapshot.source_write_time };
        const bool manifest_changed{ it->second.manifest_write_time != current_snapshot.manifest_write_time };
        const asset_iteration_watch_change_t watch_change{ detect_watch_change(source_changed, manifest_changed) };
        _runtime_iteration_diagnostics[key].last_watch_change = watch_change;

        _runtime_iteration_watches[key] = current_snapshot;

        const auto status{ find_runtime_iteration_status(kind, id) };
        if (!status.has_value() ||
            status->reload_policy != asset_reload_policy_t::reloadable_live ||
            !status->loaded_in_runtime_cache)
        {
            return;
        }

        LOG_ASSET_INFO("Detected {} for {} asset '{}'; policy={}, action=reload_now, cached=yes",
                       to_string(watch_change),
                       to_string(kind),
                       logical_id,
                       to_string(status->reload_policy));
        const bool succeeded{ reload_asset(kind, id, asset_iteration_request_origin_t::automatic_watch_poll) };
        const auto refreshed_status{ find_runtime_iteration_status(kind, id) };
        if (!refreshed_status.has_value())
            return;

        if (succeeded)
        {
            LOG_ASSET_INFO("Automatic runtime refresh for {} asset '{}' succeeded; origin={}, invalidation={}, summary={}",
                           to_string(kind),
                           logical_id,
                           to_string(refreshed_status->last_load_origin),
                           to_string(refreshed_status->last_invalidation_reason),
                           describe_last_attempt_summary(*refreshed_status));
            return;
        }

        LOG_ASSET_WARN("Automatic runtime refresh for {} asset '{}' failed; invalidation={}, error={}",
                       to_string(kind),
                       logical_id,
                       to_string(refreshed_status->last_invalidation_reason),
                       refreshed_status->last_error.empty() ? "<none>" : refreshed_status->last_error);
    }

    void asset_manager_t::clear()
    {
        _audio.clear_all();
        _fonts.clear_all();
        _textures.clear_all();
        _sprites.clear_all();
        _tilemaps.clear_all();
        _scenes.clear_all();
        _runtime_iteration_watches.clear();
        _runtime_iteration_diagnostics.clear();
    }
} // namespace carrot::assets
