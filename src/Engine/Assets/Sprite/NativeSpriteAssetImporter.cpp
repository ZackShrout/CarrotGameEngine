//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "NativeSpriteAssetImporter.h"

#include "Assets/AssetID.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] bool read_float2(const utils::json::json_object_view_t& obj, const char* key,
                                       chlm::float2& out_value)
        {
            if (!obj.has(key))
                return false;

            const utils::json::json_object_view_t value{ obj.get_object(key) };

            const double x{ value.get_number_or("x", 0.0) };
            const double y{ value.get_number_or("y", 0.0) };

            out_value = {
                static_cast<float>(x),
                static_cast<float>(y)
            };

            return true;
        }

        [[nodiscard]] bool parse_frame_rect(const utils::json::json_object_view_t& frame_json,
                                            chlm::uint_rect& out_rect)
        {
            const double x{ frame_json.get_number_or("x", -1) };
            const double y{ frame_json.get_number_or("y", -1) };
            const double w{ frame_json.get_number_or("w", -1) };
            const double h{ frame_json.get_number_or("h", -1) };

            if (x < 0 || y < 0 || w <= 0 || h <= 0)
                return false;

            out_rect.position = {
                static_cast<uint32_t>(x),
                static_cast<uint32_t>(y)
            };

            out_rect.size = {
                static_cast<uint32_t>(w),
                static_cast<uint32_t>(h)
            };

            return true;
        }
    }

    bool native_sprite_asset_importer_t::import(const utils::json::json_document_t& doc,
                                                sprite_asset_registry_t& registry, const std::string_view logical_id,
                                                const std::string_view texture_id)
    {
        if (!is_valid_logical_asset_id(logical_id))
        {
            LOG_ASSET_ERROR("Invalid logical asset id '{}'", logical_id);
            return false;
        }

        if (!is_valid_logical_asset_id(texture_id))
        {
            LOG_ASSET_ERROR("Invalid texture asset id '{}'", texture_id);
            return false;
        }

        sprite_asset_record_t record{ };
        record.id = make_asset_id(logical_id);
        record.logical_id = std::string{ logical_id };

        if (registry.contains(record.id))
        {
            LOG_ASSET_ERROR("Duplicate sprite asset id '{}'", logical_id);
            return false;
        }

        record.sprite.set_texture_id(std::string{ texture_id });
        record.sprite.set_default_pivot({ 0.5f, 0.5f });
        record.sprite.set_pixels_per_unit(1.f);

        const utils::json::json_object_view_t root{ doc.root().as_object() };

        chlm::float2 pivot{ 0.5f, 0.5f };
        if (read_float2(root, "pivot", pivot))
            record.sprite.set_default_pivot(pivot);

        record.sprite.set_pixels_per_unit(static_cast<float>(root.get_number_or("pixels_per_unit", 1.0)));

        if (!root.has("frames"))
        {
            LOG_ASSET_ERROR("Sprite asset '{}' is missing required 'frames' object", logical_id);
            return false;
        }

        const utils::json::json_object_view_t frames_obj{ root.get_object("frames") };
        std::unordered_map<std::string, uint32_t> frame_name_to_index;
        uint32_t frame_index{ 0 };

        for (const auto& [frame_name_sv, frame_value]: frames_obj)
        {
            if (frame_name_sv.empty())
            {
                LOG_ASSET_ERROR("Sprite asset '{}' contains a frame with an empty name", logical_id);
                return false;
            }

            const std::string frame_name{ frame_name_sv };

            if (frame_name_to_index.contains(frame_name))
            {
                LOG_ASSET_ERROR("Sprite asset '{}' contains duplicate frame name '{}'", logical_id, frame_name);
                return false;
            }

            const utils::json::json_object_view_t frame_json{ frame_value.as_object() };

            sprite_frame_t frame{ };
            frame.name = frame_name;
            frame.pivot = record.sprite.default_pivot();

            if (!parse_frame_rect(frame_json, frame.pixel_rect))
            {
                LOG_ASSET_ERROR("Sprite asset '{}' frame '{}' has invalid or missing x/y/w/h values", logical_id,
                                frame_name);
                return false;
            }

            read_float2(frame_json, "pivot", frame.pivot);

            record.sprite.add_frame(std::move(frame));
            frame_name_to_index.emplace(frame_name, frame_index++);
        }

        if (root.has("animations"))
        {
            const utils::json::json_object_view_t animations_obj{ root.get_object("animations") };

            for (const auto& [anim_name_sv, anim_value]: animations_obj)
            {
                if (anim_name_sv.empty())
                {
                    LOG_ASSET_ERROR("Sprite asset '{}' contains an animation with an empty name", logical_id);
                    return false;
                }

                const std::string anim_name{ anim_name_sv };
                const utils::json::json_object_view_t anim_json{ anim_value.as_object() };

                sprite_animation_t animation{ };
                animation.name = anim_name;
                animation.fps = static_cast<float>(anim_json.get_number_or("fps", 12.0));
                animation.loop = anim_json.get_bool_or("loop", true);

                if (!anim_json.has("frames"))
                {
                    LOG_ASSET_ERROR("Sprite asset '{}' animation '{}' is missing required 'frames' array", logical_id,
                                    anim_name);
                    return false;
                }

                const utils::json::json_array_view_t frame_array{ anim_json.get_array("frames") };
                for (const auto& frame_name_value: frame_array)
                {
                    const std::string_view frame_name{ frame_name_value.as_string() };
                    if (!frame_name.data())
                    {
                        LOG_ASSET_ERROR("Sprite asset '{}' animation '{}' contains a non-string frame reference",
                                        logical_id, anim_name);
                        return false;
                    }

                    const auto it{ frame_name_to_index.find(std::string{ frame_name }) };
                    if (it == frame_name_to_index.end())
                    {
                        LOG_ASSET_ERROR("Sprite asset '{}' animation '{}' references unknown frame '{}'", logical_id,
                                        anim_name, frame_name);
                        return false;
                    }

                    animation.frame_indices.push_back(it->second);
                }

                record.sprite.add_animation(std::move(animation));
            }
        }

        if (!record.sprite.build_lookup_tables())
        {
            LOG_ASSET_ERROR("Failed to finalize sprite asset '{}'", logical_id);
            return false;
        }

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register sprite asset '{}'", logical_id);
            return false;
        }

        LOG_ASSET_INFO("Registered native sprite asset '{}'", logical_id);
        return true;
    }
} // namespace carrot::assets
