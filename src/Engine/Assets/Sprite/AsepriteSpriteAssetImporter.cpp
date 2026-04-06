//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AsepriteSpriteAssetImporter.h"

#include "Assets/AssetID.h"

namespace carrot::assets {
    namespace {
        [[nodiscard]] std::string normalize_identifier(const std::string_view input)
        {
            std::string out{ };
            out.reserve(input.size());

            bool last_was_underscore{ false };

            for (const char ch: input)
            {
                const unsigned char uch{ static_cast<unsigned char>(ch) };

                if (std::isalnum(uch))
                {
                    out.push_back(static_cast<char>(std::tolower(uch)));
                    last_was_underscore = false;
                    continue;
                }

                // Treat separators/punctuation as underscore boundaries.
                if (!last_was_underscore && !out.empty())
                {
                    out.push_back('_');
                    last_was_underscore = true;
                }
            }

            // Trim trailing underscores.
            while (!out.empty() && out.back() == '_')
                out.pop_back();

            // Trim leading underscores.
            while (!out.empty() && out.front() == '_')
                out.erase(out.begin());

            return out;
        }

        std::string normalize_aseprite_name(std::string_view name)
        {
            constexpr std::string_view k_extension{ ".aseprite" };

            if (name.size() >= k_extension.size() && name.substr(name.size() - k_extension.size()) == k_extension)
            {
                name.remove_suffix(k_extension.size());
            }

            return normalize_identifier(name);
        }

        std::string normalize_tag_name(const std::string_view name)
        {
            return normalize_identifier(name);
        }

        bool parse_frame_rect(const utils::json::json_object_view_t& frame_obj, chlm::uint_rect& out_rect)
        {
            const double x{ frame_obj.get_number_or("x", -1) };
            const double y{ frame_obj.get_number_or("y", -1) };
            const double w{ frame_obj.get_number_or("w", -1) };
            const double h{ frame_obj.get_number_or("h", -1) };

            if (x < 0 || y < 0 || w <= 0 || h <= 0)
                return false;

            out_rect.position = { static_cast<uint32_t>(x), static_cast<uint32_t>(y) };
            out_rect.size = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
            return true;
        }
    } // anonymous namespace

    bool aseprite_sprite_asset_importer_t::import(const utils::json::json_document_t& doc,
                                                  sprite_asset_registry_t& registry, const std::string_view logical_id,
                                                  const std::string_view texture_id,
                                                  const std::optional<chlm::float2> pivot_override,
                                                  const std::optional<float> pixels_per_unit_override)
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
        record.sprite.set_default_pivot(pivot_override.value_or(chlm::float2{ 0.5f, 0.5f }));
        record.sprite.set_pixels_per_unit(pixels_per_unit_override.value_or(32.f));

        const utils::json::json_object_view_t root{ doc.root().as_object() };

        if (!root.has("frames"))
        {
            LOG_ASSET_ERROR("Aseprite sprite '{}' missing 'frames'", logical_id);
            return false;
        }

        const utils::json::json_object_view_t frames_obj{ root.get_object("frames") };

        std::vector<uint32_t> frame_durations_ms;
        frame_durations_ms.reserve(128);

        for (const auto [aseprite_frame_name_sv, frame_value]: frames_obj)
        {
            const utils::json::json_object_view_t frame_json{ frame_value.as_object() };

            if (frame_json.get_bool_or("rotated", false))
            {
                LOG_ASSET_ERROR("Aseprite sprite '{}' uses rotated frames, which are not supported yet.", logical_id);
                return false;
            }

            if (frame_json.get_bool_or("trimmed", false))
            {
                LOG_ASSET_ERROR("Aseprite sprite '{}' uses trimmed frames, which are not supported yet.", logical_id);
                return false;
            }

            if (!frame_json.has("frame"))
            {
                LOG_ASSET_ERROR("Aseprite sprite '{}' frame '{}' missing 'frame' rect.", logical_id,
                                aseprite_frame_name_sv);
                return false;
            }

            sprite_frame_t frame{ };
            frame.name = normalize_aseprite_name(aseprite_frame_name_sv);
            frame.pivot = record.sprite.default_pivot();

            if (!parse_frame_rect(frame_json.get_object("frame"), frame.pixel_rect))
            {
                LOG_ASSET_ERROR("Aseprite sprite '{}' frame '{}' has invalid rect.",
                                logical_id, aseprite_frame_name_sv);
                return false;
            }

            const double duration_ms{ frame_json.get_number_or("duration", -1) };
            if (duration_ms <= 0)
            {
                LOG_ASSET_ERROR("Aseprite sprite '{}' frame '{}' has invalid duration {}.",
                                logical_id, aseprite_frame_name_sv, duration_ms);
                return false;
            }

            record.sprite.add_frame(std::move(frame));
            frame_durations_ms.push_back(static_cast<uint32_t>(duration_ms));
        }

        if (root.has("meta"))
        {
            const utils::json::json_object_view_t meta{ root.get_object("meta") };

            if (meta.has("frameTags"))
            {
                const utils::json::json_array_view_t frame_tags{ meta.get_array("frameTags") };

                for (const auto tag_value: frame_tags)
                {
                    const auto tag{ tag_value.as_object() };

                    const std::string_view raw_name{ tag.get_string("name") };
                    const double from{ tag.get_number_or("from", -1) };
                    const double to{ tag.get_number_or("to", -1) };
                    const std::string_view direction{ tag.get_string("direction") };

                    if (!raw_name.data() || from < 0 || to < from)
                    {
                        LOG_ASSET_ERROR("Aseprite sprite '{}' has invalid frame tag.", logical_id);
                        return false;
                    }

                    if (direction.data() && direction != "forward")
                    {
                        LOG_ASSET_ERROR("Aseprite sprite '{}' tag '{}' uses unsupported direction '{}'.",
                                        logical_id, raw_name, direction);
                        return false;
                    }

                    if (static_cast<size_t>(to) >= frame_durations_ms.size())
                    {
                        LOG_ASSET_ERROR("Aseprite sprite '{}' tag '{}' references invalid frame range {}-{}.",
                                        logical_id, raw_name, from, to);
                        return false;
                    }

                    sprite_animation_t animation{ };
                    animation.name = normalize_tag_name(raw_name);
                    animation.loop = true;

                    for (int64_t i{ static_cast<int64_t>(from) }; i <= static_cast<int64_t>(to); ++i)
                    {
                        const uint32_t duration_ms{ frame_durations_ms[static_cast<size_t>(i)] };
                        if (duration_ms == 0)
                        {
                            LOG_ASSET_ERROR("Aseprite sprite '{}' tag '{}' contains frame {} with invalid duration 0.",
                                            logical_id, raw_name, i);
                            return false;
                        }

                        sprite_animation_frame_t anim_frame{ };
                        anim_frame.frame_index = static_cast<uint32_t>(i);
                        anim_frame.duration_seconds = static_cast<float>(duration_ms) / 1000.0f;

                        animation.frames.emplace_back(anim_frame);
                    }

                    record.sprite.add_animation(std::move(animation));
                }
            }
        }

        if (!record.sprite.build_lookup_tables())
        {
            LOG_ASSET_ERROR("Failed to finalize imported Aseprite sprite '{}'.", logical_id);
            return false;
        }

        if (!registry.register_asset(std::move(record)))
        {
            LOG_ASSET_ERROR("Failed to register imported Aseprite sprite '{}'.", logical_id);
            return false;
        }

        LOG_ASSET_INFO("Registered Aseprite sprite asset '{}'", logical_id);
        return true;
    }
} // namespace carrot::assets
