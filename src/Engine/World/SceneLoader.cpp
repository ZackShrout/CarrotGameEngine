//
// Created by Codex on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SceneLoader.h"

#include "Assets/AssetManager.h"
#include "Assets/Scene/SceneAsset.h"
#include "Assets/Sprite/LoadedSpriteAsset.h"
#include "Assets/Tilemap/LoadedTilemapAsset.h"
#include "Core/GameContext.h"
#include "World/Import/TilemapWorldBridge.h"
#include "World/World.h"
#include "World/WorldUnits.h"

namespace carrot::world {
    namespace {
        [[nodiscard]] const world_object_t* find_marker(const world_t& world, const std::string_view marker_name) noexcept
        {
            return world.find_object_by_name(marker_name);
        }

        void create_player(world_t& world, const assets::scene_asset_t& scene, const assets::loaded_sprite_asset_t& player_sprite)
        {
            const assets::sprite_frame_t* first_frame{ player_sprite.sprite().frame_at(0) };
            if (!first_frame)
            {
                LOG_ASSET_WARN("Scene player sprite '{}' has no frames", scene.player_sprite_id);
                return;
            }

            world_object_t& player{ world.create_object() };
            player.name = scene.player_name;
            player.type = scene.player_type;
            player.transform = transform_component_t{
                .position = { 0.f, 0.f },
                .scale = { 1.f, 1.f }
            };
            player.sprite = sprite_component_t{
                .sprite = &player_sprite,
                .frame = first_frame,
                .use_custom_pivot = true,
                .pivot = { 0.5f, 1.f },
                .layer = renderer::render_layer_t::actors,
                .order_mode = renderer::render_order_mode_t::anchor_bottom_y,
                .order_in_layer = 0,
                .color = 0xFFFFFFFFu,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp
            };
            player.sprite_animator = sprite_animator_component_t{ };
            player.sprite_animator->animator.set_sprite(&player_sprite);
            player.sprite_animator->animator.play("idle_down");
        }
    } // namespace

    bool scene_loader_t::load_scene(core::game_context_t& game,
                                    const std::string_view scene_id,
                                    const std::string_view spawn_marker_override)
    {
        return load_scene(game.world, game.assets, scene_id, spawn_marker_override);
    }

    bool scene_loader_t::load_scene(world_t& world,
                                    assets::asset_manager_t& assets,
                                    const std::string_view scene_id,
                                    const std::string_view spawn_marker_override)
    {
        const assets::scene_asset_record_t* scene_record{ assets.scenes().registry().find(scene_id) };
        if (!scene_record)
        {
            LOG_ASSET_ERROR("Scene asset '{}' was not found", scene_id);
            return false;
        }

        const assets::scene_asset_t& scene{ scene_record->scene };
        const assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get(scene.tilemap_id) };
        if (!tilemap)
        {
            LOG_ASSET_ERROR("Scene '{}' failed to load tilemap '{}'", scene_id, scene.tilemap_id);
            return false;
        }

        const assets::loaded_sprite_asset_t* player_sprite{ assets.sprites().get(scene.player_sprite_id) };
        if (!player_sprite || !player_sprite->valid())
        {
            LOG_ASSET_ERROR("Scene '{}' failed to load player sprite '{}'", scene_id, scene.player_sprite_id);
            return false;
        }

        world.clear();
        world.set_presentation_origin_px(scene.presentation_origin_px);
        world.set_presentation_pixels_per_unit(world_units_t::default_pixels_per_unit);

        create_player(world, scene, *player_sprite);

        world_object_t& map_object{ world.create_object() };
        map_object.name = scene.map_object_name;
        map_object.type = "Tilemap";
        map_object.transform = transform_component_t{
            .position = scene.tilemap_world_position
        };
        map_object.tilemap = tilemap_component_t{
            .tilemap = tilemap,
            .include_object_layers = false,
            .layer = renderer::render_layer_t::world_back,
            .order_in_layer = -100,
            .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp,
            .color = 0xFFFFFFFFu
        };

        const import::tilemap_world_bridge_result_t bridge_result{
            import::import_tilemap_objects(world, *tilemap)
        };
        LOG_ASSET_INFO("Scene '{}': imported {} marker object(s), {} tile object(s)",
                       scene_id,
                       bridge_result.markers_created,
                       bridge_result.tile_objects_created);

        const std::string_view effective_spawn_marker{
            !spawn_marker_override.empty() ? spawn_marker_override : std::string_view{ scene.player_spawn_marker }
        };

        world_object_t* player{ world.find_object_by_name(scene.player_name) };
        const world_object_t* spawn_marker{ find_marker(world, effective_spawn_marker) };
        if (!player || !player->transform)
        {
            LOG_ASSET_ERROR("Scene '{}' could not resolve player object '{}'", scene_id, scene.player_name);
            return false;
        }

        if (!spawn_marker || !spawn_marker->transform)
        {
            LOG_ASSET_ERROR("Scene '{}' could not find required spawn marker '{}'",
                            scene_id,
                            effective_spawn_marker);
            return false;
        }
        else
        {
            player->transform->position = spawn_marker->transform->position;
        }

        LOG_ASSET_INFO("Loaded scene '{}': tilemap='{}', player='{}', spawn='{}', map_object='{}'",
                       scene_id,
                       scene.tilemap_id,
                       scene.player_name,
                       effective_spawn_marker,
                       scene.map_object_name);
        return true;
    }
} // namespace carrot::world
