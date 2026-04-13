//
// Created by Zack Shrout on 3/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AssetManager.h"

namespace carrot::assets {
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
        append(_textures.collect_iteration_statuses());
        append(_sprites.collect_iteration_statuses());
        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> asset_manager_t::find_runtime_iteration_status(const asset_kind_t kind,
                                                                                            const asset_id_t id) const
    {
        switch (kind)
        {
            case asset_kind_t::audio: return _audio.find_iteration_status(id);
            case asset_kind_t::texture: return _textures.find_iteration_status(id);
            case asset_kind_t::sprite: return _sprites.find_iteration_status(id);
            default: return std::nullopt;
        }
    }

    bool asset_manager_t::reload_asset(const asset_kind_t kind, const asset_id_t id)
    {
        switch (kind)
        {
            case asset_kind_t::audio: return _audio.reload(id);
            case asset_kind_t::texture: return _textures.reload(id);
            case asset_kind_t::sprite: return _sprites.reload(id);
            default: return false;
        }
    }

    bool asset_manager_t::reload_asset(const asset_kind_t kind, const std::string_view logical_id)
    {
        return reload_asset(kind, make_asset_id(logical_id));
    }

    void asset_manager_t::clear()
    {
        _audio.clear_all();
        _fonts.clear_all();
        _textures.clear_all();
        _sprites.clear_all();
        _tilemaps.clear_all();
        _scenes.clear_all();
    }
} // namespace carrot::assets
