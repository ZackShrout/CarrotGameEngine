//
// Created by Codex on 4/5/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TilemapValidation.h"
#include "TypedObjectConventions.h"

#include <unordered_set>

namespace carrot::assets {
    namespace {
        void add_issue(std::vector<tilemap_validation_issue_t>& issues,
                       const tilemap_validation_issue_severity_t severity,
                       std::string code,
                       std::string message)
        {
            issues.push_back({
                .severity = severity,
                .code = std::move(code),
                .message = std::move(message)
            });
        }
    } // namespace

    std::vector<tilemap_validation_issue_t> validate_tiled_authored_data(const tilemap_asset_t& tilemap)
    {
        std::vector<tilemap_validation_issue_t> issues;
        std::unordered_set<std::string> visibility_zone_ids;

        for (const tilemap_layer_t& layer : tilemap.layers())
        {
            if (layer.kind != tilemap_layer_kind_t::object)
                continue;

            for (const tilemap_object_t& object : layer.objects)
            {
                const auto visibility_zone_id{ object.get_string_property("visibility_zone_id") };

                if (object.type == "VisibilityZone")
                {
                    if (!visibility_zone_id.has_value() || visibility_zone_id->empty())
                    {
                        add_issue(issues,
                                  tilemap_validation_issue_severity_t::warning,
                                  "tiled.visibility_zone.missing_id",
                                  std::format("Object '{}' in layer '{}' uses type 'VisibilityZone' but has no non-empty 'visibility_zone_id'.",
                                              object.name.empty() ? "<unnamed>" : object.name,
                                              layer.name.empty() ? "<unnamed>" : layer.name));
                    }
                    else
                    {
                        visibility_zone_ids.emplace(*visibility_zone_id);
                    }

                    if (object.gid != 0)
                    {
                        add_issue(issues,
                                  tilemap_validation_issue_severity_t::warning,
                                  "tiled.visibility_zone.non_rectangle_object",
                                  std::format("Object '{}' in layer '{}' uses type 'VisibilityZone' but is a tile object. Author visibility zones as rectangle objects.",
                                              object.name.empty() ? "<unnamed>" : object.name,
                                              layer.name.empty() ? "<unnamed>" : layer.name));
                    }

                    if (object.width <= 0.f || object.height <= 0.f)
                    {
                        add_issue(issues,
                                  tilemap_validation_issue_severity_t::warning,
                                  "tiled.visibility_zone.zero_size",
                                  std::format("Object '{}' in layer '{}' uses type 'VisibilityZone' but has zero size.",
                                              object.name.empty() ? "<unnamed>" : object.name,
                                              layer.name.empty() ? "<unnamed>" : layer.name));
                    }
                }
                else if (visibility_zone_id.has_value() && !visibility_zone_id->empty())
                {
                    add_issue(issues,
                              tilemap_validation_issue_severity_t::warning,
                              "tiled.visibility_zone.property_without_type",
                              std::format("Object '{}' in layer '{}' has 'visibility_zone_id = {}' but type is '{}'. Only type 'VisibilityZone' activates visibility zones.",
                                          object.name.empty() ? "<unnamed>" : object.name,
                                          layer.name.empty() ? "<unnamed>" : layer.name,
                                          *visibility_zone_id,
                                          object.type.empty() ? "<empty>" : object.type));
                }

                if (object.type == "Sign" && !as_typed_sign(object))
                {
                    add_issue(issues,
                              tilemap_validation_issue_severity_t::warning,
                              "tiled.object.sign.missing_message_id",
                              std::format("Object '{}' in layer '{}' uses type 'Sign' but is missing required 'message_id'.",
                                          object.name.empty() ? "<unnamed>" : object.name,
                                          layer.name.empty() ? "<unnamed>" : layer.name));
                }

                if (object.type == "Container" && !as_typed_container(object))
                {
                    add_issue(issues,
                              tilemap_validation_issue_severity_t::warning,
                              "tiled.object.container.missing_loot_table",
                              std::format("Object '{}' in layer '{}' uses type '{}' but is missing required 'loot_table'.",
                                          object.name.empty() ? "<unnamed>" : object.name,
                                          layer.name.empty() ? "<unnamed>" : layer.name,
                                          object.type));
                }

                if (object.type == "Door")
                {
                    if (!as_typed_door(object))
                    {
                        add_issue(issues,
                                  tilemap_validation_issue_severity_t::warning,
                                  "tiled.object.door.invalid_target",
                                  std::format("Object '{}' in layer '{}' uses type 'Door' but is missing a valid 'target_marker' and either 'target_scene' or legacy 'target_map'.",
                                              object.name.empty() ? "<unnamed>" : object.name,
                                              layer.name.empty() ? "<unnamed>" : layer.name));
                    }
                    else
                    {
                        const auto target_scene{ object.get_string_property("target_scene") };
                        const auto target_map{ object.get_string_property("target_map") };
                        if (target_scene && !target_scene->empty() && target_map && !target_map->empty())
                        {
                            add_issue(issues,
                                      tilemap_validation_issue_severity_t::warning,
                                      "tiled.object.door.mixed_target_modes",
                                      std::format("Object '{}' in layer '{}' uses type 'Door' and sets both 'target_scene' and legacy 'target_map'. 'target_scene' should be preferred.",
                                                  object.name.empty() ? "<unnamed>" : object.name,
                                                  layer.name.empty() ? "<unnamed>" : layer.name));
                        }
                    }
                }

                if (object.type == "Trigger" && !as_typed_trigger(object))
                {
                    add_issue(issues,
                              tilemap_validation_issue_severity_t::warning,
                              "tiled.object.trigger.missing_fields",
                              std::format("Object '{}' in layer '{}' uses type 'Trigger' but is missing required 'trigger_id' or 'trigger_kind'.",
                                          object.name.empty() ? "<unnamed>" : object.name,
                                          layer.name.empty() ? "<unnamed>" : layer.name));
                }

                if (!object.type.empty() &&
                    !is_known_tiled_object_type(object.type) &&
                    object.type != "Path" &&
                    object.type != "Region")
                {
                    add_issue(issues,
                              tilemap_validation_issue_severity_t::warning,
                              "tiled.object.unknown_type",
                              std::format("Object '{}' in layer '{}' uses unknown object type '{}'.",
                                          object.name.empty() ? "<unnamed>" : object.name,
                                          layer.name.empty() ? "<unnamed>" : layer.name,
                                          object.type));
                }
            }
        }

        for (const tilemap_layer_t& layer : tilemap.layers())
        {
            const bool conditional_front{ layer.get_bool_property("carrot_conditional_front").value_or(false) };
            const bool always_front{ layer.get_bool_property("carrot_always_front").value_or(false) };

            if (conditional_front && always_front)
            {
                add_issue(issues,
                          tilemap_validation_issue_severity_t::warning,
                          "tiled.layer.front_policy_conflict",
                          std::format("Layer '{}' sets both 'carrot_conditional_front' and 'carrot_always_front'. 'carrot_always_front' wins.",
                                      layer.name.empty() ? "<unnamed>" : layer.name));
            }

            const auto explicit_visibility_zone{ layer.get_string_property("carrot_visibility_zone") };
            const auto inherited_visibility_zone{ layer.get_string_property("visibility_zone_id") };

            if (explicit_visibility_zone.has_value() &&
                inherited_visibility_zone.has_value() &&
                *explicit_visibility_zone != *inherited_visibility_zone)
            {
                add_issue(issues,
                          tilemap_validation_issue_severity_t::warning,
                          "tiled.layer.visibility_zone_override_conflict",
                          std::format("Layer '{}' sets both 'carrot_visibility_zone = {}' and 'visibility_zone_id = {}'. The explicit layer property wins.",
                                      layer.name.empty() ? "<unnamed>" : layer.name,
                                      *explicit_visibility_zone,
                                      *inherited_visibility_zone));
            }

            const std::optional<std::string_view> effective_visibility_zone{
                explicit_visibility_zone.has_value() ? explicit_visibility_zone : inherited_visibility_zone
            };

            if (effective_visibility_zone.has_value() &&
                !effective_visibility_zone->empty() &&
                !visibility_zone_ids.contains(std::string{ *effective_visibility_zone }))
            {
                add_issue(issues,
                          tilemap_validation_issue_severity_t::warning,
                          "tiled.layer.visibility_zone_without_matching_object",
                          std::format("Layer '{}' binds to visibility zone '{}' but the map has no object with type 'VisibilityZone' and matching 'visibility_zone_id'.",
                                      layer.name.empty() ? "<unnamed>" : layer.name,
                                      *effective_visibility_zone));
            }
        }

        return issues;
    }
} // namespace carrot::assets
