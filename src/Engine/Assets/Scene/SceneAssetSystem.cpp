//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SceneAssetSystem.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] asset_iteration_status_t make_scene_status(const scene_asset_record_t& record) noexcept
        {
            asset_iteration_status_t status;
            status.kind = asset_kind_t::scene;
            status.id = record.id;
            status.logical_id = record.logical_id;
            status.source_uri = record.source_uri;
            status.manifest_uri.clear();
            status.dependency_shape = asset_dependency_shape_t::scene_or_world_structure;
            status.watch_mode = asset_iteration_watch_mode_t::not_polled;
            status.dependency_summary =
                "Depends on scene-authored source only; changes affect scene composition, bindings, and runtime world bootstrap.";
            status.reload_policy = asset_reload_policy_t::restart_or_scene_rebuild_required;
            return status;
        }
    }

    std::vector<asset_iteration_status_t> scene_asset_system_t::collect_iteration_statuses() const
    {
        std::vector<asset_iteration_status_t> out;
        out.reserve(_registry.records().size());

        for (const auto& [id, record] : _registry.records())
        {
            (void)id;
            out.push_back(make_scene_status(record));
        }

        std::ranges::sort(out, {}, &asset_iteration_status_t::logical_id);
        return out;
    }

    std::optional<asset_iteration_status_t> scene_asset_system_t::find_iteration_status(const asset_id_t id) const
    {
        const scene_asset_record_t* record{ _registry.find(id) };
        if (!record)
            return std::nullopt;

        return make_scene_status(*record);
    }
} // namespace carrot::assets
