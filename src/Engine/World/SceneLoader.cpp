//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SceneLoader.h"

#include "Assets/AssetManager.h"
#include "Assets/Scene/SceneAsset.h"
#include "Assets/Sprite/LoadedSpriteAsset.h"
#include "Assets/Tilemap/LoadedTilemapAsset.h"
#include "Assets/Tilemap/TypedObjectConventions.h"
#include "Core/GameContext.h"
#include "World/Import/TilemapWorldBridge.h"
#include "World/World.h"
#include "World/WorldUnits.h"

namespace carrot::world {
    namespace {
        [[nodiscard]] bool parse_hex_u8(const std::string_view text, uint8_t& value) noexcept
        {
            if (text.size() != 2u)
                return false;

            const auto hex_value_for = [](const char ch) -> int
            {
                if (ch >= '0' && ch <= '9')
                    return ch - '0';
                if (ch >= 'a' && ch <= 'f')
                    return 10 + (ch - 'a');
                if (ch >= 'A' && ch <= 'F')
                    return 10 + (ch - 'A');
                return -1;
            };

            const int hi{ hex_value_for(text[0]) };
            const int lo{ hex_value_for(text[1]) };
            if (hi < 0 || lo < 0)
                return false;

            value = static_cast<uint8_t>((hi << 4) | lo);
            return true;
        }

        [[nodiscard]] bool parse_light_color_hex(const std::string_view color_hex, chlm::float4& out_color) noexcept
        {
            if (color_hex.empty())
            {
                out_color = { 1.f, 1.f, 1.f, 1.f };
                return true;
            }

            if ((color_hex.size() != 7u && color_hex.size() != 9u) || color_hex.front() != '#')
                return false;

            uint8_t red{ 0u };
            uint8_t green{ 0u };
            uint8_t blue{ 0u };
            if (!parse_hex_u8(color_hex.substr(1u, 2u), red) ||
                !parse_hex_u8(color_hex.substr(3u, 2u), green) ||
                !parse_hex_u8(color_hex.substr(5u, 2u), blue))
            {
                return false;
            }

            out_color = {
                static_cast<float>(red) / 255.f,
                static_cast<float>(green) / 255.f,
                static_cast<float>(blue) / 255.f,
                1.f
            };
            return true;
        }

        [[nodiscard]] const world_object_t* find_marker(const world_t& world, const std::string_view marker_name) noexcept
        {
            return world.find_object_by_name(marker_name);
        }

        [[nodiscard]] chlm::float2 object_position_world(const assets::tilemap_object_t& object,
                                                         const chlm::float2 tilemap_origin_world) noexcept
        {
            return {
                tilemap_origin_world.x + world_units_t::pixels_to_world(object.x),
                tilemap_origin_world.y + world_units_t::pixels_to_world(object.y)
            };
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
            player.collision = collision_component_t{
                .half_extents = { 0.3f, 0.2f },
                .offset = { 0.f, -0.2f },
                .debug_display = collision_debug_display_t{
                    .filled = false,
                    .outline_thickness = 2.f,
                    .color = 0xFF00FFFFu
                }
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

        void create_map_object(world_t& world,
                               const assets::scene_asset_t& scene,
                               const assets::loaded_tilemap_asset_t& tilemap)
        {
            world_object_t& map_object{ world.create_object() };
            map_object.name = scene.map_object_name;
            map_object.type = "Tilemap";
            map_object.transform = transform_component_t{
                .position = scene.tilemap_world_position
            };
            map_object.tilemap = tilemap_component_t{
                .tilemap = &tilemap,
                .include_object_layers = false,
                .layer = renderer::render_layer_t::world_back,
                .order_in_layer = -100,
                .sampler_preset = renderer::quad_sampler_preset_t::pixel_clamp,
                .color = 0xFFFFFFFFu
            };
        }

        void import_authored_lighting(const std::string_view scene_id,
                                      world_t& world,
                                      const assets::scene_asset_t& scene,
                                      const assets::loaded_tilemap_asset_t& tilemap)
        {
            const chlm::float2 tilemap_origin_world{ scene.tilemap_world_position };
            bool has_authored_ambient{ false };

            for (const assets::tilemap_layer_t& layer : tilemap.tilemap().layers())
            {
                if (layer.kind != assets::tilemap_layer_kind_t::object)
                    continue;

                for (const assets::tilemap_object_t& object : layer.objects)
                {
                    const auto light{ assets::as_typed_light(object) };
                    if (!light)
                        continue;

                    if (light->kind == assets::typed_light_kind_t::spot)
                    {
                        LOG_ASSET_WARN("Scene '{}' ignores Light '{}' because kind 'spot' is not yet supported",
                                       scene_id,
                                       object.name.empty() ? "<unnamed>" : object.name);
                        continue;
                    }

                    chlm::float4 color{ 1.f, 1.f, 1.f, 1.f };
                    if (!parse_light_color_hex(light->color_hex, color))
                    {
                        LOG_ASSET_WARN("Scene '{}' ignores invalid Light color '{}' on object '{}'",
                                       scene_id,
                                       light->color_hex,
                                       object.name.empty() ? "<unnamed>" : object.name);
                        continue;
                    }

                    if (light->kind == assets::typed_light_kind_t::ambient)
                    {
                        if (has_authored_ambient)
                        {
                            LOG_ASSET_WARN("Scene '{}' authored multiple ambient Light objects; only the first is used",
                                           scene_id);
                            continue;
                        }

                        world.lighting().ambient_color = {
                            color.x * light->intensity,
                            color.y * light->intensity,
                            color.z * light->intensity,
                            1.f
                        };
                        has_authored_ambient = true;
                        continue;
                    }

                    if (!light->radius_world || *light->radius_world <= 0.f)
                    {
                        LOG_ASSET_WARN("Scene '{}' ignores point Light '{}' because it is missing a positive radius",
                                       scene_id,
                                       object.name.empty() ? "<unnamed>" : object.name);
                        continue;
                    }

                    world_lighting_state_t::point_light_t point_light{
                        .position_world = object_position_world(object, tilemap_origin_world),
                        .radius_world = *light->radius_world,
                        .reserved0 = 0.f,
                        .color = color,
                        .intensity = light->intensity
                    };

                    if (light->behavior == assets::typed_light_behavior_t::follow)
                    {
                        if (light->follow_target != "player")
                        {
                            LOG_ASSET_WARN("Scene '{}' ignores follow Light '{}' because follow_target '{}' is not supported",
                                           scene_id,
                                           object.name.empty() ? "<unnamed>" : object.name,
                                           light->follow_target);
                            continue;
                        }

                        const world_object_t* authoring_spawn{ world.find_object_by_name(scene.player_spawn_marker) };
                        if (!authoring_spawn || !authoring_spawn->transform)
                        {
                            LOG_ASSET_WARN("Scene '{}' cannot resolve authored player follow Light '{}' because player_spawn_marker '{}' was not imported",
                                           scene_id,
                                           object.name.empty() ? "<unnamed>" : object.name,
                                           scene.player_spawn_marker);
                            continue;
                        }

                        point_light.behavior = world_lighting_state_t::point_light_t::runtime_behavior_t::follow_object;
                        point_light.follow_object_name = scene.player_name;
                        point_light.follow_offset_world = {
                            point_light.position_world.x - authoring_spawn->transform->position.x,
                            point_light.position_world.y - authoring_spawn->transform->position.y
                        };
                    }

                    world.lighting().point_lights.push_back(std::move(point_light));
                }
            }
        }
    } // namespace

    scene_load_task_t::scene_load_task_t(const std::string_view scene_id,
                                         const std::string_view spawn_marker_override)
        : _scene_id{ scene_id },
          _spawn_marker_override{ spawn_marker_override }
    {
    }

    bool scene_load_task_t::advance(assets::asset_manager_t& assets)
    {
        if (is_ready_to_activate() || is_complete() || has_failed())
            return !has_failed();

        switch (_phase)
        {
            case phase_t::resolve_scene:
            {
                _scene_record = assets.scenes().registry().find(_scene_id);
                if (!_scene_record)
                {
                    LOG_ASSET_ERROR("Scene asset '{}' was not found", _scene_id);
                    fail();
                    return false;
                }

                _effective_spawn_marker = _spawn_marker_override.empty()
                                              ? std::string{ _scene_record->scene.player_spawn_marker }
                                              : _spawn_marker_override;
                _phase = phase_t::resolve_dependencies;
                return true;
            }
            case phase_t::resolve_dependencies:
            {
                _tilemap = assets.tilemaps().get(_scene_record->scene.tilemap_id);
                if (!_tilemap)
                {
                    LOG_ASSET_ERROR("Scene '{}' failed to load tilemap '{}'",
                                    _scene_id,
                                    _scene_record->scene.tilemap_id);
                    fail();
                    return false;
                }

                _player_sprite = assets.sprites().get(_scene_record->scene.player_sprite_id);
                if (!_player_sprite || !_player_sprite->valid())
                {
                    LOG_ASSET_ERROR("Scene '{}' failed to load player sprite '{}'",
                                    _scene_id,
                                    _scene_record->scene.player_sprite_id);
                    fail();
                    return false;
                }

                _phase = phase_t::initialize_world;
                return true;
            }
            case phase_t::initialize_world:
            {
                _staged_world.clear();
                _staged_world.set_presentation_origin_px(_scene_record->scene.presentation_origin_px);
                _staged_world.set_presentation_pixels_per_unit(world_units_t::default_pixels_per_unit);

                create_player(_staged_world, _scene_record->scene, *_player_sprite);
                create_map_object(_staged_world, _scene_record->scene, *_tilemap);

                _phase = phase_t::start_background_prepare;
                return true;
            }
            case phase_t::start_background_prepare:
            {
                _background_prepare_state = std::make_shared<background_prepare_state_t>();
                _background_prepare_thread.emplace([state = _background_prepare_state,
                                                    tilemap = _tilemap,
                                                    tilemap_origin_world = _scene_record->scene.tilemap_world_position]()
                {
                    import::prepared_tilemap_world_data_t prepared{
                        import::prepare_tilemap_world_data(*tilemap, tilemap_origin_world)
                    };

                    std::lock_guard lock{ state->mutex };
                    state->prepared_data = std::move(prepared);
                    state->complete = true;
                });
                _phase = phase_t::await_background_prepare;
                return true;
            }
            case phase_t::await_background_prepare:
            {
                if (!_background_prepare_state)
                {
                    fail();
                    return false;
                }

                std::optional<import::prepared_tilemap_world_data_t> prepared_data;
                {
                    std::lock_guard lock{ _background_prepare_state->mutex };
                    if (!_background_prepare_state->complete)
                        return true;

                    prepared_data = std::move(_background_prepare_state->prepared_data);
                }

                if (!prepared_data.has_value())
                {
                    LOG_ASSET_ERROR("Scene '{}' background prepare completed without prepared data", _scene_id);
                    fail();
                    return false;
                }

                _prepared_tilemap_world_data = std::move(*prepared_data);
                _background_prepare_thread.reset();
                _background_prepare_state.reset();
                _phase = phase_t::activate_authored_content;
                return true;
            }
            case phase_t::activate_authored_content:
            {
                const import::tilemap_world_bridge_result_t bridge_result{
                    import::apply_prepared_tilemap_world_data(_staged_world,
                                                              *_tilemap,
                                                              _prepared_tilemap_world_data)
                };
                import_authored_lighting(_scene_id, _staged_world, _scene_record->scene, *_tilemap);
                LOG_ASSET_INFO("Scene '{}': imported {} marker object(s), {} tile object(s), {} static collider(s), {} trigger(s)",
                               _scene_id,
                               bridge_result.markers_created,
                               bridge_result.tile_objects_created,
                               bridge_result.static_colliders_created,
                               bridge_result.triggers_created);

                _phase = phase_t::resolve_spawn;
                return true;
            }
            case phase_t::resolve_spawn:
            {
                world_object_t* player{ _staged_world.find_object_by_name(_scene_record->scene.player_name) };
                const world_object_t* spawn_marker{ find_marker(_staged_world, _effective_spawn_marker) };
                if (!player || !player->transform)
                {
                    LOG_ASSET_ERROR("Scene '{}' could not resolve player object '{}'",
                                    _scene_id,
                                    _scene_record->scene.player_name);
                    fail();
                    return false;
                }

                if (!spawn_marker || !spawn_marker->transform)
                {
                    LOG_ASSET_ERROR("Scene '{}' could not find required spawn marker '{}'",
                                    _scene_id,
                                    _effective_spawn_marker);
                    fail();
                    return false;
                }

                player->transform->position = spawn_marker->transform->position;
                _staged_world.refresh_bound_lights();

                LOG_ASSET_INFO("Loaded scene '{}': tilemap='{}', player='{}', spawn='{}', map_object='{}'",
                               _scene_id,
                               _scene_record->scene.tilemap_id,
                               _scene_record->scene.player_name,
                               _effective_spawn_marker,
                               _scene_record->scene.map_object_name);

                _phase = phase_t::ready_to_activate;
                return true;
            }
            case phase_t::ready_to_activate:
            case phase_t::complete:
            case phase_t::failed:
                break;
        }

        return !has_failed();
    }

    bool scene_load_task_t::is_ready_to_activate() const noexcept
    {
        return _phase == phase_t::ready_to_activate;
    }

    bool scene_load_task_t::is_complete() const noexcept
    {
        return _phase == phase_t::complete;
    }

    bool scene_load_task_t::has_failed() const noexcept
    {
        return _phase == phase_t::failed;
    }

    bool scene_load_task_t::is_background_preparing() const noexcept
    {
        return _phase == phase_t::start_background_prepare ||
               _phase == phase_t::await_background_prepare;
    }

    size_t scene_load_task_t::completed_steps() const noexcept
    {
        switch (_phase)
        {
            case phase_t::resolve_scene: return 0u;
            case phase_t::resolve_dependencies: return 1u;
            case phase_t::initialize_world: return 2u;
            case phase_t::start_background_prepare: return 3u;
            case phase_t::await_background_prepare: return 4u;
            case phase_t::activate_authored_content: return 5u;
            case phase_t::resolve_spawn: return 6u;
            case phase_t::ready_to_activate:
            case phase_t::complete: return total_steps();
            case phase_t::failed: return 0u;
        }

        return 0u;
    }

    size_t scene_load_task_t::total_steps() const noexcept
    {
        return 7u;
    }

    world_t scene_load_task_t::take_world()
    {
        _background_prepare_thread.reset();
        _background_prepare_state.reset();
        _phase = phase_t::complete;
        return std::move(_staged_world);
    }

    void scene_load_task_t::fail() noexcept
    {
        _background_prepare_thread.reset();
        _background_prepare_state.reset();
        _phase = phase_t::failed;
    }

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
        scene_load_task_t task{ scene_id, spawn_marker_override };
        while (!task.is_ready_to_activate() && !task.has_failed())
            (void)task.advance(assets);

        if (task.has_failed())
            return false;

        world = task.take_world();
        return true;
    }
} // namespace carrot::world
