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
        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> asset_manager_t::find_runtime_iteration_status(const asset_kind_t kind,
                                                                                            const asset_id_t id) const
    {
        switch (kind)
        {
            case asset_kind_t::font: return _fonts.find_iteration_status(id);
            case asset_kind_t::audio: return _audio.find_iteration_status(id);
            case asset_kind_t::texture: return _textures.find_iteration_status(id);
            case asset_kind_t::sprite: return _sprites.find_iteration_status(id);
            case asset_kind_t::tilemap: return _tilemaps.find_iteration_status(id);
            case asset_kind_t::scene: return _scenes.find_iteration_status(id);
            default: return std::nullopt;
        }
    }

    bool asset_manager_t::reload_asset(const asset_kind_t kind, const asset_id_t id)
    {
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

        _runtime_iteration_watches[key] = current_snapshot;

        const auto status{ find_runtime_iteration_status(kind, id) };
        if (!status.has_value() ||
            status->reload_policy != asset_reload_policy_t::reloadable_live ||
            !status->loaded_in_runtime_cache)
        {
            return;
        }

        LOG_ASSET_INFO("Detected change for {} asset '{}'; attempting automatic reload",
                       to_string(kind),
                       logical_id);
        (void)reload_asset(kind, id);
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
    }
} // namespace carrot::assets
