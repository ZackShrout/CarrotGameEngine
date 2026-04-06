//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SandboxSceneBootstrap.h"
#include "WorldInteractionHelpers.h"

namespace sandbox {
    bool bootstrap_scene(carrot::core::game_context_t& game,
                         const std::string_view scene_id,
                         const std::string_view spawn_marker_override)
    {
        if (!carrot::world::scene_loader_t::load_scene(game, scene_id, spawn_marker_override))
            return false;

        size_t container_count{ 0 };
        for (const carrot::world::world_object_t* object : game.world.find_objects_by_type("Container"))
            if (object) ++container_count;
        const size_t door_count{ game.world.find_objects_by_type("Door").size() };
        const size_t sign_count{ game.world.find_objects_by_type("Sign").size() };
        LOG_ASSET_INFO("Sandbox scene hybrids: Container={}, Door={}, Sign={}", container_count, door_count, sign_count);

        const scene_validation_report_t report{ build_scene_validation_report(game.assets, game.world) };
        if (!report.valid())
        {
            LOG_ASSET_ERROR("Scene '{}' validation failed with {} issue(s)", scene_id, report.issues.size());
            return false;
        }

        LOG_ASSET_INFO("Scene '{}' validation passed", scene_id);
        return true;
    }
} // namespace sandbox
