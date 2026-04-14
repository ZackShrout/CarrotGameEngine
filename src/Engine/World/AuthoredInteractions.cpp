//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AuthoredInteractions.h"

#include "Assets/AssetManager.h"
#include "Assets/Tilemap/TypedObjectConventions.h"

namespace carrot::world::authored {
    namespace {
        [[nodiscard]] std::string describe_world_object(const world_object_t& object)
        {
            std::string description;

            if (!object.name.empty())
                description = std::format("'{}'", object.name);
            else if (object.source && !object.source->object_name.empty())
                description = std::format("'{}'", object.source->object_name);
            else
                description = "'<unnamed>'";

            if (object.source)
            {
                description += std::format(" [tilemap='{}', layer='{}', object_id={}]",
                                           object.source->tilemap_logical_id,
                                           object.source->layer_name,
                                           object.source->object_id);
            }

            return description;
        }

        void append_issue(scene_validation_report_t& report, std::string issue)
        {
            LOG_CORE_WARN("{}", issue);
            report.issues.emplace_back(std::move(issue));
        }

        [[nodiscard]] const assets::tilemap_object_t* find_tilemap_object_by_name(const assets::tilemap_asset_t& tilemap,
                                                                                   const std::string_view name) noexcept
        {
            for (const assets::tilemap_layer_t& layer : tilemap.layers())
            {
                if (layer.kind != assets::tilemap_layer_kind_t::object)
                    continue;

                for (const assets::tilemap_object_t& object : layer.objects)
                {
                    if (object.name == name)
                        return &object;
                }
            }

            return nullptr;
        }
    } // namespace

    interaction_kind_t interaction_kind_for(const world_object_t& object) noexcept
    {
        if (object.type == "Sign")
            return interaction_kind_t::sign;

        if (object.type == "Door")
            return interaction_kind_t::door;

        if (object.type == "Container")
            return interaction_kind_t::container;

        return interaction_kind_t::none;
    }

    std::optional<sign_interaction_data_t> as_sign(const world_object_t& object) noexcept
    {
        if (const auto sign{ assets::as_typed_sign(object) })
        {
            return sign_interaction_data_t{
                .message_id = sign->message_id
            };
        }

        if (interaction_kind_for(object) != interaction_kind_t::sign)
            return std::nullopt;
        return std::nullopt;
    }

    std::optional<door_interaction_data_t> as_door(const world_object_t& object) noexcept
    {
        if (const auto door{ assets::as_typed_door(object) })
        {
            return door_interaction_data_t{
                .target_scene = door->target_scene,
                .target_map = door->target_map,
                .target_marker = door->target_marker
            };
        }

        if (interaction_kind_for(object) != interaction_kind_t::door)
            return std::nullopt;

        const std::optional<std::string_view> target_scene{ object.get_string_property("target_scene") };
        const std::optional<std::string_view> target_map{ object.get_string_property("target_map") };
        const std::optional<std::string_view> target_marker{ object.get_string_property("target_marker") };
        if (!target_scene && !target_map)
        {
            LOG_CORE_WARN("Door '{}' is missing both 'target_scene' and legacy 'target_map' properties",
                          describe_world_object(object));
            return std::nullopt;
        }

        if (!target_marker || target_marker->empty())
        {
            LOG_CORE_WARN("Door '{}' is missing required 'target_marker' property",
                          describe_world_object(object));
        }

        return std::nullopt;
    }

    std::optional<container_interaction_data_t> as_container(const world_object_t& object) noexcept
    {
        if (const auto container{ assets::as_typed_container(object) })
        {
            return container_interaction_data_t{
                .loot_table = container->loot_table
            };
        }

        if (interaction_kind_for(object) != interaction_kind_t::container)
            return std::nullopt;
        return std::nullopt;
    }

    std::optional<trigger_interaction_data_t> as_trigger(const world_object_t& object) noexcept
    {
        if (const auto trigger{ assets::as_typed_trigger(object) })
        {
            return trigger_interaction_data_t{
                .trigger_id = trigger->trigger_id,
                .trigger_kind = trigger->trigger_kind
            };
        }

        if (object.type != "Trigger" || !object.trigger)
            return std::nullopt;

        return trigger_interaction_data_t{
            .trigger_id = object.trigger->trigger_id,
            .trigger_kind = object.trigger->trigger_kind
        };
    }

    std::optional<interaction_outcome_t> resolve_interaction_outcome(const assets::asset_manager_t& assets,
                                                                     const world_object_t& object)
    {
        if (const std::optional<sign_interaction_data_t> sign{ as_sign(object) })
        {
            return interaction_outcome_t{
                .kind = interaction_outcome_kind_t::sign,
                .message_id = std::string{ sign->message_id },
                .transition = {},
                .loot_table = {}
            };
        }

        if (const std::optional<door_interaction_data_t> door{ as_door(object) })
        {
            const std::optional<scene::scene_transition_request_t> request{
                make_scene_transition_request(assets, object)
            };
            if (!request)
                return std::nullopt;

            return interaction_outcome_t{
                .kind = interaction_outcome_kind_t::scene_transition,
                .message_id = {},
                .transition = *request,
                .loot_table = {}
            };
        }

        if (const std::optional<container_interaction_data_t> container{ as_container(object) })
        {
            return interaction_outcome_t{
                .kind = interaction_outcome_kind_t::container,
                .message_id = {},
                .transition = {},
                .object_id = object.id,
                .loot_table = std::string{ container->loot_table }
            };
        }

        return std::nullopt;
    }

    bool dispatch_interaction_outcome(const interaction_outcome_t& outcome,
                                      const interaction_outcome_dispatch_t& dispatch) noexcept
    {
        switch (outcome.kind)
        {
            case interaction_outcome_kind_t::sign:
                if (dispatch.on_sign)
                {
                    dispatch.on_sign(outcome.message_id);
                    return true;
                }
                break;
            case interaction_outcome_kind_t::scene_transition:
                if (dispatch.on_scene_transition)
                {
                    dispatch.on_scene_transition(outcome.transition);
                    return true;
                }
                break;
            case interaction_outcome_kind_t::container:
                if (dispatch.on_container)
                {
                    dispatch.on_container(outcome.object_id, outcome.loot_table);
                    return true;
                }
                break;
            case interaction_outcome_kind_t::none:
                break;
        }

        if (dispatch.on_unhandled)
        {
            dispatch.on_unhandled(outcome);
            return true;
        }

        return false;
    }

    std::optional<scene::scene_transition_request_t> make_scene_transition_request(
        const assets::asset_manager_t& assets,
        const world_object_t& object)
    {
        const std::optional<door_interaction_data_t> door{ as_door(object) };
        if (!door)
            return std::nullopt;

        std::string scene_id;

        if (!door->target_scene.empty())
        {
            const assets::scene_asset_record_t* scene{ assets.scenes().registry().find(door->target_scene) };
            if (!scene)
            {
                LOG_CORE_WARN("Door '{}' references unknown target_scene '{}'",
                              describe_world_object(object),
                              door->target_scene);
                return std::nullopt;
            }

            scene_id = std::string{ door->target_scene };
        }
        else if (!door->target_map.empty())
        {
            const assets::scene_asset_record_t* scene{
                assets.scenes().registry().find_first_by_tilemap(door->target_map)
            };
            if (!scene)
            {
                LOG_CORE_WARN("Door '{}' references legacy target_map '{}' that does not resolve to a scene",
                              describe_world_object(object),
                              door->target_map);
                return std::nullopt;
            }

            scene_id = scene->logical_id;
        }
        else
        {
            LOG_CORE_WARN("Door '{}' could not resolve a transition target", describe_world_object(object));
            return std::nullopt;
        }

        return scene::scene_transition_request_t{
            .scene_id = std::move(scene_id),
            .marker_name = std::string{ door->target_marker }
        };
    }

    bool validate_scene_transition_target(const assets::asset_manager_t& assets,
                                          const world_object_t& object) noexcept
    {
        if (interaction_kind_for(object) != interaction_kind_t::door)
            return true;

        return make_scene_transition_request(assets, object).has_value();
    }

    bool validate_scene_transition_targets(assets::asset_manager_t& assets,
                                           const world_t& world) noexcept
    {
        return build_scene_validation_report(assets, world).valid();
    }

    scene_validation_report_t build_scene_validation_report(assets::asset_manager_t& assets,
                                                            const world_t& world) noexcept
    {
        scene_validation_report_t report;

        for (const world_object_t* object : world.find_objects_by_type("Door"))
        {
            if (!object)
                continue;

            const std::optional<door_interaction_data_t> door{ as_door(*object) };
            if (!door)
            {
                append_issue(report, std::format("Door {} has invalid or incomplete transition properties",
                                                 describe_world_object(*object)));
                continue;
            }

            if (!door->target_scene.empty())
            {
                const assets::scene_asset_record_t* destination_scene{
                    assets.scenes().registry().find(door->target_scene)
                };
                if (!destination_scene)
                {
                    append_issue(report, std::format("Door {} references unknown target_scene '{}'",
                                                     describe_world_object(*object),
                                                     door->target_scene));
                }
                else
                {
                    const assets::tilemap_asset_record_t* destination_tilemap{
                        assets.tilemaps().registry().find(destination_scene->scene.tilemap_id)
                    };
                    if (!destination_tilemap)
                    {
                        append_issue(report, std::format("Door {} could not resolve destination tilemap '{}' for scene '{}'",
                                                         describe_world_object(*object),
                                                         destination_scene->scene.tilemap_id,
                                                         destination_scene->logical_id));
                    }
                    else if (!find_tilemap_object_by_name(destination_tilemap->tilemap, door->target_marker))
                    {
                        append_issue(report, std::format("Door {} targets marker '{}' which does not exist in scene '{}'",
                                                         describe_world_object(*object),
                                                         door->target_marker,
                                                         destination_scene->logical_id));
                    }
                }
            }
            else if (!door->target_map.empty())
            {
                const assets::scene_asset_record_t* destination_scene{
                    assets.scenes().registry().find_first_by_tilemap(door->target_map)
                };
                if (!destination_scene)
                {
                    append_issue(report, std::format("Door {} references legacy target_map '{}' that does not resolve to a scene",
                                                     describe_world_object(*object),
                                                     door->target_map));
                }
                else
                {
                    const assets::tilemap_asset_record_t* destination_tilemap{
                        assets.tilemaps().registry().find(destination_scene->scene.tilemap_id)
                    };
                    if (!destination_tilemap)
                    {
                        append_issue(report, std::format("Door {} could not resolve destination tilemap '{}' for scene '{}'",
                                                         describe_world_object(*object),
                                                         destination_scene->scene.tilemap_id,
                                                         destination_scene->logical_id));
                    }
                    else if (!find_tilemap_object_by_name(destination_tilemap->tilemap, door->target_marker))
                    {
                        append_issue(report, std::format("Door {} targets marker '{}' which does not exist in scene '{}'",
                                                         describe_world_object(*object),
                                                         door->target_marker,
                                                         destination_scene->logical_id));
                    }
                }
            }
            else
            {
                append_issue(report, std::format("Door {} could not resolve a transition target",
                                                 describe_world_object(*object)));
            }
        }

        return report;
    }
} // namespace carrot::world::authored
