//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SandboxSceneBootstrap.h"

#include "SceneHelpers.h"
#include "World/Import/TilemapWorldBridge.h"

namespace sandbox {
    namespace {
        void log_missing_scene_object(std::string_view name)
        {
            LOG_ASSET_WARN("Sandbox scene bootstrap could not find world object '{}'", name);
        }
    }

    void bootstrap_scene(const carrot::core::game_context_t& game)
    {
        carrot::world::world_t& world{ game.world };
        carrot::assets::asset_manager_t& assets{ game.assets };

        world.set_presentation_origin_px({ 0.f, 0.f });
        world.set_presentation_pixels_per_unit(carrot::world::world_units_t::default_render_pixels_per_unit);

        const carrot::assets::loaded_sprite_asset_t* vraden_sprite{ assets.sprites().get("sprite.vraden") };
        if (vraden_sprite)
        {
            if (const carrot::assets::sprite_frame_t* first_frame{ vraden_sprite->sprite().frame_at(0) })
            {
                carrot::world::world_object_t& vraden{ world.create_object() };
                vraden.name = "Vraden";
                vraden.type = "Character";
                vraden.transform = carrot::world::transform_component_t{
                    .position = { 0.f, 0.f },
                    .scale = { 1.f, 1.f }
                };
                vraden.sprite = carrot::world::sprite_component_t{
                    .sprite = vraden_sprite,
                    .frame = first_frame,
                    .use_custom_pivot = true,
                    .pivot = { 0.5f, 1.f },
                    .layer = carrot::renderer::render_layer_t::actors,
                    .order_in_layer = 0,
                    .color = 0xFFFFFFFFu,
                    .sampler_preset = carrot::renderer::quad_sampler_preset_t::pixel_clamp
                };
                vraden.sprite_animator = carrot::world::sprite_animator_component_t{ };
                vraden.sprite_animator->animator.set_sprite(vraden_sprite);
                vraden.sprite_animator->animator.play("idle_down");
            }
        }
        else
        {
            LOG_ASSET_WARN("Sandbox scene bootstrap failed to load sprite 'sprite.vraden'");
        }

        const carrot::assets::loaded_tilemap_asset_t* tilemap{ assets.tilemaps().get("tilemap.test.overworld") };
        if (!tilemap)
        {
            LOG_ASSET_WARN("Sandbox scene bootstrap failed to load tilemap 'tilemap.test.overworld'");
            return;
        }

        carrot::world::world_object_t& map_object{ world.create_object() };
        map_object.name = "OverworldMap";
        map_object.type = "Tilemap";
        map_object.transform = carrot::world::transform_component_t{
            .position = { 0.f, 0.f }
        };
        map_object.tilemap = carrot::world::tilemap_component_t{
            .tilemap = tilemap,
            .include_object_layers = false,
            .layer = carrot::renderer::render_layer_t::world_back,
            .order_in_layer = -100,
            .sampler_preset = carrot::renderer::quad_sampler_preset_t::pixel_clamp,
            .color = 0xFFFFFFFFu
        };

        const carrot::assets::tilemap_asset_t& map{ tilemap->tilemap() };

        uint32_t object_count{ 0 };
        for (const carrot::assets::tilemap_layer_t& layer : map.layers())
        {
            if (layer.kind == carrot::assets::tilemap_layer_kind_t::object)
                object_count += static_cast<uint32_t>(layer.objects.size());
        }

        LOG_ASSET_INFO(
            "Loaded tilemap '{}': {}x{} tiles, tile size {}x{}, layers={}, tilesets={}, objects={}",
            tilemap->record()->logical_id,
            map.width(),
            map.height(),
            map.tile_width(),
            map.tile_height(),
            map.layers().size(),
            map.tilesets().size(),
            object_count
        );

        const carrot::world::import::tilemap_world_bridge_result_t bridge_result{
            carrot::world::import::import_tilemap_objects(world, *tilemap)
        };
        LOG_ASSET_INFO("Sandbox scene import: {} marker object(s), {} tile object(s)",
                       bridge_result.markers_created,
                       bridge_result.tile_objects_created);

        if (const carrot::world::world_object_t* player_spawn{ find_player_spawn(world) })
        {
            LOG_ASSET_INFO(
                "World marker 'PlayerSpawn': pos=({}, {}), source_layer='{}', source_object_id={}",
                player_spawn->transform ? player_spawn->transform->position.x : 0.f,
                player_spawn->transform ? player_spawn->transform->position.y : 0.f,
                player_spawn->source ? player_spawn->source->layer_name : std::string_view{ "<missing>" },
                player_spawn->source ? player_spawn->source->object_id : 0u
            );

            if (carrot::world::world_object_t* vraden{ find_player(world) })
            {
                vraden->transform = carrot::world::transform_component_t{
                    .position = {
                        player_spawn->transform ? player_spawn->transform->position.x : 0.f,
                        player_spawn->transform ? player_spawn->transform->position.y : 0.f
                    },
                    .scale = { 1.f, 1.f }
                };
            }
        }
        else
        {
            LOG_ASSET_WARN("Sandbox scene bootstrap could not find marker 'PlayerSpawn'");
        }

        const size_t chest_count{ world.find_objects_by_type("Chest").size() };
        const size_t door_count{ world.find_objects_by_type("Door").size() };
        const size_t sign_count{ world.find_objects_by_type("Sign").size() };
        LOG_ASSET_INFO("Sandbox scene hybrids: Chest={}, Door={}, Sign={}", chest_count, door_count, sign_count);

        if (!world.find_object_by_name("StarterChest"))
            log_missing_scene_object("StarterChest");

        if (!world.find_object_by_name("NorthDoor"))
            log_missing_scene_object("NorthDoor");

        if (!world.find_object_by_name("WelcomeSign"))
            log_missing_scene_object("WelcomeSign");
    }
} // namespace sandbox
