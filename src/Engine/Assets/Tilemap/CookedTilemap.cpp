//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "CookedTilemap.h"

#include "Utils/File/FileUtils.h"

#include <bit>

namespace carrot::assets {
    namespace {
        constexpr std::array<std::uint8_t, 8> cmap_magic{
            'C', 'M', 'A', 'P', 0, 0, 0, 0
        };
        constexpr std::uint32_t cooked_tilemap_supported_version{ 4u };

        [[nodiscard]] bool read_u8(std::span<const std::uint8_t> bytes,
                                   size_t& cursor,
                                   std::uint8_t& out_value) noexcept
        {
            if (cursor + 1u > bytes.size())
                return false;
            out_value = bytes[cursor++];
            return true;
        }

        [[nodiscard]] bool read_u32(std::span<const std::uint8_t> bytes,
                                    size_t& cursor,
                                    std::uint32_t& out_value) noexcept
        {
            if (cursor + 4u > bytes.size())
                return false;

            out_value =
                (static_cast<std::uint32_t>(bytes[cursor + 0u]) << 0u) |
                (static_cast<std::uint32_t>(bytes[cursor + 1u]) << 8u) |
                (static_cast<std::uint32_t>(bytes[cursor + 2u]) << 16u) |
                (static_cast<std::uint32_t>(bytes[cursor + 3u]) << 24u);
            cursor += 4u;
            return true;
        }

        [[nodiscard]] bool read_u64(std::span<const std::uint8_t> bytes,
                                    size_t& cursor,
                                    std::uint64_t& out_value) noexcept
        {
            if (cursor + 8u > bytes.size())
                return false;

            out_value =
                (static_cast<std::uint64_t>(bytes[cursor + 0u]) << 0u) |
                (static_cast<std::uint64_t>(bytes[cursor + 1u]) << 8u) |
                (static_cast<std::uint64_t>(bytes[cursor + 2u]) << 16u) |
                (static_cast<std::uint64_t>(bytes[cursor + 3u]) << 24u) |
                (static_cast<std::uint64_t>(bytes[cursor + 4u]) << 32u) |
                (static_cast<std::uint64_t>(bytes[cursor + 5u]) << 40u) |
                (static_cast<std::uint64_t>(bytes[cursor + 6u]) << 48u) |
                (static_cast<std::uint64_t>(bytes[cursor + 7u]) << 56u);
            cursor += 8u;
            return true;
        }

        [[nodiscard]] bool read_f32(std::span<const std::uint8_t> bytes,
                                    size_t& cursor,
                                    float& out_value) noexcept
        {
            std::uint32_t bits{ 0u };
            if (!read_u32(bytes, cursor, bits))
                return false;

            out_value = std::bit_cast<float>(bits);
            return true;
        }

        [[nodiscard]] bool read_f64(std::span<const std::uint8_t> bytes,
                                    size_t& cursor,
                                    double& out_value) noexcept
        {
            std::uint64_t bits{ 0u };
            if (!read_u64(bytes, cursor, bits))
                return false;

            out_value = std::bit_cast<double>(bits);
            return true;
        }

        [[nodiscard]] bool read_string(std::span<const std::uint8_t> bytes,
                                       size_t& cursor,
                                       std::string& out_value) noexcept
        {
            std::uint32_t length{ 0u };
            if (!read_u32(bytes, cursor, length))
                return false;

            if (cursor + length > bytes.size())
                return false;

            out_value.assign(reinterpret_cast<const char*>(bytes.data() + cursor), length);
            cursor += length;
            return true;
        }

        void write_string(utils::file::binary_blob_writer_t& writer, const std::string_view value)
        {
            [[maybe_unused]] const size_t length_offset{ writer.write_u32(static_cast<std::uint32_t>(value.size())) };
            [[maybe_unused]] const size_t value_offset{
                writer.write_bytes(std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(value.data()),
                    value.size()
                })
            };
        }

        void write_property(utils::file::binary_blob_writer_t& writer, const tilemap_property_t& property)
        {
            write_string(writer, property.name);

            if (const bool* bool_value{ std::get_if<bool>(&property.value) })
            {
                [[maybe_unused]] const size_t type_offset{ writer.write_u8(0u) };
                [[maybe_unused]] const size_t value_offset{ writer.write_u8(*bool_value ? 1u : 0u) };
            }
            else if (const double* number_value{ std::get_if<double>(&property.value) })
            {
                [[maybe_unused]] const size_t type_offset{ writer.write_u8(1u) };
                [[maybe_unused]] const size_t value_offset{ writer.write_u64(std::bit_cast<std::uint64_t>(*number_value)) };
            }
            else
            {
                [[maybe_unused]] const size_t type_offset{ writer.write_u8(2u) };
                const std::string* string_value{ std::get_if<std::string>(&property.value) };
                write_string(writer, string_value ? std::string_view{ *string_value } : std::string_view{});
            }
        }

        [[nodiscard]] bool read_property(std::span<const std::uint8_t> bytes,
                                         size_t& cursor,
                                         tilemap_property_t& out_property) noexcept
        {
            if (!read_string(bytes, cursor, out_property.name))
                return false;

            std::uint8_t type{ 0u };
            if (!read_u8(bytes, cursor, type))
                return false;

            switch (type)
            {
                case 0u:
                {
                    std::uint8_t value{ 0u };
                    if (!read_u8(bytes, cursor, value))
                        return false;
                    out_property.value = value != 0u;
                    return true;
                }
                case 1u:
                {
                    double value{ 0.0 };
                    if (!read_f64(bytes, cursor, value))
                        return false;
                    out_property.value = value;
                    return true;
                }
                case 2u:
                {
                    std::string value;
                    if (!read_string(bytes, cursor, value))
                        return false;
                    out_property.value = std::move(value);
                    return true;
                }
                default:
                    return false;
            }
        }
    } // namespace

    std::optional<std::vector<std::uint8_t>> serialize_cooked_tilemap(const cooked_tilemap_data_t& cooked) noexcept
    {
        if (cooked.cooked_format_version != cooked_tilemap_supported_version)
            return std::nullopt;

        utils::file::binary_blob_writer_t writer;
        writer.reserve(1024u);

        [[maybe_unused]] const size_t magic_offset{ writer.write_bytes(cmap_magic) };
        [[maybe_unused]] const size_t version_offset{ writer.write_u32(cooked.cooked_format_version) };
        [[maybe_unused]] const size_t importer_offset{ writer.write_u32(cooked.importer_version) };
        [[maybe_unused]] const size_t source_hash_offset{ writer.write_u64(cooked.invalidation.source_content_hash) };
        [[maybe_unused]] const size_t manifest_hash_offset{ writer.write_u64(cooked.invalidation.asset_definition_content_hash) };
        [[maybe_unused]] const size_t settings_hash_offset{ writer.write_u64(cooked.invalidation.import_settings_hash) };
        [[maybe_unused]] const size_t reserved_hash_offset{ writer.write_u64(cooked.invalidation.reserved_hash) };

        [[maybe_unused]] const size_t orientation_offset{
            writer.write_u8(static_cast<std::uint8_t>(cooked.tilemap.orientation()))
        };
        [[maybe_unused]] const size_t width_offset{ writer.write_u32(cooked.tilemap.width()) };
        [[maybe_unused]] const size_t height_offset{ writer.write_u32(cooked.tilemap.height()) };
        [[maybe_unused]] const size_t tile_width_offset{ writer.write_u32(cooked.tilemap.tile_width()) };
        [[maybe_unused]] const size_t tile_height_offset{ writer.write_u32(cooked.tilemap.tile_height()) };
        write_string(writer, cooked.tilemap.source_format());

        [[maybe_unused]] const size_t property_count_offset{
            writer.write_u32(static_cast<std::uint32_t>(cooked.tilemap.properties().size()))
        };
        for (const tilemap_property_t& property : cooked.tilemap.properties())
            write_property(writer, property);

        [[maybe_unused]] const size_t tileset_count_offset{
            writer.write_u32(static_cast<std::uint32_t>(cooked.tilemap.tilesets().size()))
        };
        for (const tilemap_tileset_t& tileset : cooked.tilemap.tilesets())
        {
            write_string(writer, tileset.name);
            [[maybe_unused]] const size_t first_gid_offset{ writer.write_u32(tileset.first_gid) };
            write_string(writer, tileset.source_uri);
            write_string(writer, tileset.image_source_uri);
            [[maybe_unused]] const size_t tileset_tile_width_offset{ writer.write_u32(tileset.tile_width) };
            [[maybe_unused]] const size_t tileset_tile_height_offset{ writer.write_u32(tileset.tile_height) };
            [[maybe_unused]] const size_t tileset_image_width_offset{ writer.write_u32(tileset.image_width) };
            [[maybe_unused]] const size_t tileset_image_height_offset{ writer.write_u32(tileset.image_height) };
            [[maybe_unused]] const size_t tileset_tile_count_offset{ writer.write_u32(tileset.tile_count) };
            [[maybe_unused]] const size_t tileset_columns_offset{ writer.write_u32(tileset.columns) };

            [[maybe_unused]] const size_t animation_count_offset{
                writer.write_u32(static_cast<std::uint32_t>(tileset.tile_animations.size()))
            };
            for (const auto& animation : tileset.tile_animations)
            {
                [[maybe_unused]] const size_t animation_tile_id_offset{ writer.write_u32(animation.tile_id) };
                [[maybe_unused]] const size_t animation_frame_count_offset{
                    writer.write_u32(static_cast<std::uint32_t>(animation.frames.size()))
                };
                for (const auto& frame : animation.frames)
                {
                    [[maybe_unused]] const size_t frame_tile_id_offset{ writer.write_u32(frame.tile_id) };
                    [[maybe_unused]] const size_t duration_offset{ writer.write_u32(frame.duration_ms) };
                }
            }

            [[maybe_unused]] const size_t collision_count_offset{
                writer.write_u32(static_cast<std::uint32_t>(tileset.tile_collisions.size()))
            };
            for (const auto& collision : tileset.tile_collisions)
            {
                [[maybe_unused]] const size_t collision_tile_id_offset{ writer.write_u32(collision.tile_id) };
                [[maybe_unused]] const size_t rect_count_offset{
                    writer.write_u32(static_cast<std::uint32_t>(collision.collision_rects.size()))
                };
                for (const auto& rect : collision.collision_rects)
                {
                    [[maybe_unused]] const size_t x_offset{ writer.write_f32(rect.x) };
                    [[maybe_unused]] const size_t y_offset{ writer.write_f32(rect.y) };
                    [[maybe_unused]] const size_t rect_width_offset{ writer.write_f32(rect.width) };
                    [[maybe_unused]] const size_t rect_height_offset{ writer.write_f32(rect.height) };
                }
                [[maybe_unused]] const size_t polygon_count_offset{
                    writer.write_u32(static_cast<std::uint32_t>(collision.collision_polygons.size()))
                };
                for (const auto& polygon : collision.collision_polygons)
                {
                    [[maybe_unused]] const size_t point_count_offset{
                        writer.write_u32(static_cast<std::uint32_t>(polygon.points.size()))
                    };
                    for (const auto& point : polygon.points)
                    {
                        [[maybe_unused]] const size_t point_x_offset{ writer.write_f32(point.x) };
                        [[maybe_unused]] const size_t point_y_offset{ writer.write_f32(point.y) };
                    }
                }
            }

            [[maybe_unused]] const size_t sort_metadata_count_offset{
                writer.write_u32(static_cast<std::uint32_t>(tileset.tile_sort_metadata.size()))
            };
            for (const auto& sort_metadata : tileset.tile_sort_metadata)
            {
                [[maybe_unused]] const size_t tile_id_offset{ writer.write_u32(sort_metadata.tile_id) };
                [[maybe_unused]] const size_t span_down_offset{ writer.write_u32(sort_metadata.span_down) };
                [[maybe_unused]] const size_t anchor_offset_y_offset{ writer.write_u32(sort_metadata.anchor_offset_y) };
            }
        }

        [[maybe_unused]] const size_t layer_count_offset{
            writer.write_u32(static_cast<std::uint32_t>(cooked.tilemap.layers().size()))
        };
        for (const tilemap_layer_t& layer : cooked.tilemap.layers())
        {
            [[maybe_unused]] const size_t kind_offset{ writer.write_u8(static_cast<std::uint8_t>(layer.kind)) };
            write_string(writer, layer.name);
            write_string(writer, layer.authored_type);
            [[maybe_unused]] const size_t layer_width_offset{ writer.write_u32(layer.width) };
            [[maybe_unused]] const size_t layer_height_offset{ writer.write_u32(layer.height) };
            [[maybe_unused]] const size_t visible_offset{ writer.write_u8(layer.visible ? 1u : 0u) };
            [[maybe_unused]] const size_t opacity_offset{ writer.write_f32(layer.opacity) };

            [[maybe_unused]] const size_t layer_property_count_offset{
                writer.write_u32(static_cast<std::uint32_t>(layer.properties.size()))
            };
            for (const auto& property : layer.properties)
                write_property(writer, property);

            [[maybe_unused]] const size_t gid_count_offset{ writer.write_u32(static_cast<std::uint32_t>(layer.gids.size())) };
            for (const std::uint32_t gid : layer.gids)
                [[maybe_unused]] const size_t gid_offset{ writer.write_u32(gid) };

            [[maybe_unused]] const size_t object_count_offset{
                writer.write_u32(static_cast<std::uint32_t>(layer.objects.size()))
            };
            for (const auto& object : layer.objects)
            {
                [[maybe_unused]] const size_t object_id_offset{ writer.write_u32(object.id) };
                write_string(writer, object.name);
                write_string(writer, object.type);
                [[maybe_unused]] const size_t geometry_kind_offset{
                    writer.write_u8(static_cast<std::uint8_t>(object.geometry_kind))
                };
                [[maybe_unused]] const size_t x_offset{ writer.write_f32(object.x) };
                [[maybe_unused]] const size_t y_offset{ writer.write_f32(object.y) };
                [[maybe_unused]] const size_t object_width_offset{ writer.write_f32(object.width) };
                [[maybe_unused]] const size_t object_height_offset{ writer.write_f32(object.height) };
                [[maybe_unused]] const size_t rotation_offset{ writer.write_f32(object.rotation) };
                [[maybe_unused]] const size_t object_visible_offset{ writer.write_u8(object.visible ? 1u : 0u) };
                [[maybe_unused]] const size_t gid_offset{ writer.write_u32(object.gid) };

                [[maybe_unused]] const size_t point_count_offset{
                    writer.write_u32(static_cast<std::uint32_t>(object.geometry_points.size()))
                };
                for (const auto& point : object.geometry_points)
                {
                    [[maybe_unused]] const size_t point_x_offset{ writer.write_f32(point.x) };
                    [[maybe_unused]] const size_t point_y_offset{ writer.write_f32(point.y) };
                }

                [[maybe_unused]] const size_t object_property_count_offset{
                    writer.write_u32(static_cast<std::uint32_t>(object.properties.size()))
                };
                for (const auto& property : object.properties)
                    write_property(writer, property);
            }
        }

        [[maybe_unused]] const size_t issue_count_offset{
            writer.write_u32(static_cast<std::uint32_t>(cooked.tilemap.validation_issues().size()))
        };
        for (const tilemap_validation_issue_t& issue : cooked.tilemap.validation_issues())
        {
            [[maybe_unused]] const size_t severity_offset{
                writer.write_u8(static_cast<std::uint8_t>(issue.severity))
            };
            write_string(writer, issue.code);
            write_string(writer, issue.message);
        }

        return std::move(writer).take();
    }

    std::optional<cooked_tilemap_data_t> deserialize_cooked_tilemap(const std::span<const std::uint8_t> bytes) noexcept
    {
        if (bytes.size() < 41u || !std::equal(cmap_magic.begin(), cmap_magic.end(), bytes.begin()))
            return std::nullopt;

        cooked_tilemap_data_t cooked;
        size_t cursor{ 8u };
        if (!read_u32(bytes, cursor, cooked.cooked_format_version)) return std::nullopt;
        if (!read_u32(bytes, cursor, cooked.importer_version)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.source_content_hash)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.asset_definition_content_hash)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.import_settings_hash)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.reserved_hash)) return std::nullopt;

        if (cooked.cooked_format_version != cooked_tilemap_supported_version)
            return std::nullopt;

        std::uint8_t orientation{ 0u };
        if (!read_u8(bytes, cursor, orientation)) return std::nullopt;
        cooked.tilemap.set_orientation(static_cast<tilemap_orientation_t>(orientation));

        std::uint32_t width{ 0u };
        std::uint32_t height{ 0u };
        std::uint32_t tile_width{ 0u };
        std::uint32_t tile_height{ 0u };
        if (!read_u32(bytes, cursor, width)) return std::nullopt;
        if (!read_u32(bytes, cursor, height)) return std::nullopt;
        if (!read_u32(bytes, cursor, tile_width)) return std::nullopt;
        if (!read_u32(bytes, cursor, tile_height)) return std::nullopt;
        cooked.tilemap.set_size(width, height);
        cooked.tilemap.set_tile_size(tile_width, tile_height);

        std::string source_format;
        if (!read_string(bytes, cursor, source_format)) return std::nullopt;
        cooked.tilemap.set_source_format(std::move(source_format));

        std::uint32_t property_count{ 0u };
        if (!read_u32(bytes, cursor, property_count)) return std::nullopt;
        for (std::uint32_t i{ 0u }; i < property_count; ++i)
        {
            tilemap_property_t property;
            if (!read_property(bytes, cursor, property)) return std::nullopt;
            cooked.tilemap.add_property(std::move(property));
        }

        std::uint32_t tileset_count{ 0u };
        if (!read_u32(bytes, cursor, tileset_count)) return std::nullopt;
        for (std::uint32_t i{ 0u }; i < tileset_count; ++i)
        {
            tilemap_tileset_t tileset;
            if (!read_string(bytes, cursor, tileset.name)) return std::nullopt;
            if (!read_u32(bytes, cursor, tileset.first_gid)) return std::nullopt;
            if (!read_string(bytes, cursor, tileset.source_uri)) return std::nullopt;
            if (!read_string(bytes, cursor, tileset.image_source_uri)) return std::nullopt;
            if (!read_u32(bytes, cursor, tileset.tile_width)) return std::nullopt;
            if (!read_u32(bytes, cursor, tileset.tile_height)) return std::nullopt;
            if (!read_u32(bytes, cursor, tileset.image_width)) return std::nullopt;
            if (!read_u32(bytes, cursor, tileset.image_height)) return std::nullopt;
            if (!read_u32(bytes, cursor, tileset.tile_count)) return std::nullopt;
            if (!read_u32(bytes, cursor, tileset.columns)) return std::nullopt;

            std::uint32_t animation_count{ 0u };
            if (!read_u32(bytes, cursor, animation_count)) return std::nullopt;
            for (std::uint32_t anim_i{ 0u }; anim_i < animation_count; ++anim_i)
            {
                tilemap_tileset_t::tile_animation_t animation;
                if (!read_u32(bytes, cursor, animation.tile_id)) return std::nullopt;

                std::uint32_t animation_frame_count{ 0u };
                if (!read_u32(bytes, cursor, animation_frame_count)) return std::nullopt;
                animation.frames.reserve(animation_frame_count);
                for (std::uint32_t frame_i{ 0u }; frame_i < animation_frame_count; ++frame_i)
                {
                    tilemap_tileset_t::animation_frame_t frame;
                    if (!read_u32(bytes, cursor, frame.tile_id)) return std::nullopt;
                    if (!read_u32(bytes, cursor, frame.duration_ms)) return std::nullopt;
                    animation.frames.emplace_back(frame);
                }
                animation.rebuild_cached_duration();
                tileset.tile_animations.emplace_back(std::move(animation));
            }

            std::uint32_t collision_count{ 0u };
            if (!read_u32(bytes, cursor, collision_count)) return std::nullopt;
            for (std::uint32_t collision_i{ 0u }; collision_i < collision_count; ++collision_i)
            {
                tilemap_tileset_t::tile_collision_t collision;
                if (!read_u32(bytes, cursor, collision.tile_id)) return std::nullopt;

                std::uint32_t rect_count{ 0u };
                if (!read_u32(bytes, cursor, rect_count)) return std::nullopt;
                collision.collision_rects.reserve(rect_count);
                for (std::uint32_t rect_i{ 0u }; rect_i < rect_count; ++rect_i)
                {
                    tilemap_tileset_t::collision_rect_t rect;
                    if (!read_f32(bytes, cursor, rect.x)) return std::nullopt;
                    if (!read_f32(bytes, cursor, rect.y)) return std::nullopt;
                    if (!read_f32(bytes, cursor, rect.width)) return std::nullopt;
                    if (!read_f32(bytes, cursor, rect.height)) return std::nullopt;
                    collision.collision_rects.emplace_back(rect);
                }

                std::uint32_t polygon_count{ 0u };
                if (!read_u32(bytes, cursor, polygon_count)) return std::nullopt;
                collision.collision_polygons.reserve(polygon_count);
                for (std::uint32_t polygon_i{ 0u }; polygon_i < polygon_count; ++polygon_i)
                {
                    tilemap_tileset_t::collision_polygon_t polygon;
                    std::uint32_t point_count{ 0u };
                    if (!read_u32(bytes, cursor, point_count)) return std::nullopt;
                    polygon.points.reserve(point_count);
                    for (std::uint32_t point_i{ 0u }; point_i < point_count; ++point_i)
                    {
                        float point_x{ 0.f };
                        float point_y{ 0.f };
                        if (!read_f32(bytes, cursor, point_x)) return std::nullopt;
                        if (!read_f32(bytes, cursor, point_y)) return std::nullopt;
                        polygon.points.emplace_back(chlm::float2{ point_x, point_y });
                    }
                    collision.collision_polygons.emplace_back(std::move(polygon));
                }

                tileset.tile_collisions.emplace_back(std::move(collision));
            }

            std::uint32_t sort_metadata_count{ 0u };
            if (!read_u32(bytes, cursor, sort_metadata_count)) return std::nullopt;
            tileset.tile_sort_metadata.reserve(sort_metadata_count);
            for (std::uint32_t sort_i{ 0u }; sort_i < sort_metadata_count; ++sort_i)
            {
                tilemap_tileset_t::tile_sort_metadata_t sort_metadata;
                if (!read_u32(bytes, cursor, sort_metadata.tile_id)) return std::nullopt;
                if (!read_u32(bytes, cursor, sort_metadata.span_down)) return std::nullopt;
                if (!read_u32(bytes, cursor, sort_metadata.anchor_offset_y)) return std::nullopt;
                tileset.tile_sort_metadata.emplace_back(std::move(sort_metadata));
            }

            tileset.rebuild_animation_lookup();
            tileset.rebuild_sort_metadata_lookup();
            cooked.tilemap.add_tileset(std::move(tileset));
        }

        std::uint32_t layer_count{ 0u };
        if (!read_u32(bytes, cursor, layer_count)) return std::nullopt;
        for (std::uint32_t i{ 0u }; i < layer_count; ++i)
        {
            tilemap_layer_t layer;
            std::uint8_t kind{ 0u };
            if (!read_u8(bytes, cursor, kind)) return std::nullopt;
            layer.kind = static_cast<tilemap_layer_kind_t>(kind);
            if (!read_string(bytes, cursor, layer.name)) return std::nullopt;
            if (!read_string(bytes, cursor, layer.authored_type)) return std::nullopt;
            if (!read_u32(bytes, cursor, layer.width)) return std::nullopt;
            if (!read_u32(bytes, cursor, layer.height)) return std::nullopt;
            std::uint8_t visible{ 0u };
            if (!read_u8(bytes, cursor, visible)) return std::nullopt;
            layer.visible = visible != 0u;
            if (!read_f32(bytes, cursor, layer.opacity)) return std::nullopt;

            std::uint32_t layer_property_count{ 0u };
            if (!read_u32(bytes, cursor, layer_property_count)) return std::nullopt;
            for (std::uint32_t prop_i{ 0u }; prop_i < layer_property_count; ++prop_i)
            {
                tilemap_property_t property;
                if (!read_property(bytes, cursor, property)) return std::nullopt;
                layer.properties.emplace_back(std::move(property));
            }

            std::uint32_t gid_count{ 0u };
            if (!read_u32(bytes, cursor, gid_count)) return std::nullopt;
            layer.gids.reserve(gid_count);
            for (std::uint32_t gid_i{ 0u }; gid_i < gid_count; ++gid_i)
            {
                std::uint32_t gid{ 0u };
                if (!read_u32(bytes, cursor, gid)) return std::nullopt;
                layer.gids.emplace_back(gid);
            }

            std::uint32_t object_count{ 0u };
            if (!read_u32(bytes, cursor, object_count)) return std::nullopt;
            layer.objects.reserve(object_count);
            for (std::uint32_t obj_i{ 0u }; obj_i < object_count; ++obj_i)
            {
                tilemap_object_t object;
                if (!read_u32(bytes, cursor, object.id)) return std::nullopt;
                if (!read_string(bytes, cursor, object.name)) return std::nullopt;
                if (!read_string(bytes, cursor, object.type)) return std::nullopt;
                std::uint8_t geometry_kind{ 0u };
                if (!read_u8(bytes, cursor, geometry_kind)) return std::nullopt;
                object.geometry_kind = static_cast<tilemap_object_t::geometry_kind_t>(geometry_kind);
                if (!read_f32(bytes, cursor, object.x)) return std::nullopt;
                if (!read_f32(bytes, cursor, object.y)) return std::nullopt;
                if (!read_f32(bytes, cursor, object.width)) return std::nullopt;
                if (!read_f32(bytes, cursor, object.height)) return std::nullopt;
                if (!read_f32(bytes, cursor, object.rotation)) return std::nullopt;
                std::uint8_t object_visible{ 0u };
                if (!read_u8(bytes, cursor, object_visible)) return std::nullopt;
                object.visible = object_visible != 0u;
                if (!read_u32(bytes, cursor, object.gid)) return std::nullopt;

                std::uint32_t point_count{ 0u };
                if (!read_u32(bytes, cursor, point_count)) return std::nullopt;
                object.geometry_points.reserve(point_count);
                for (std::uint32_t point_i{ 0u }; point_i < point_count; ++point_i)
                {
                    float point_x{ 0.0f };
                    float point_y{ 0.0f };
                    if (!read_f32(bytes, cursor, point_x)) return std::nullopt;
                    if (!read_f32(bytes, cursor, point_y)) return std::nullopt;
                    object.geometry_points.emplace_back(chlm::float2{ point_x, point_y });
                }

                std::uint32_t object_property_count{ 0u };
                if (!read_u32(bytes, cursor, object_property_count)) return std::nullopt;
                for (std::uint32_t prop_i{ 0u }; prop_i < object_property_count; ++prop_i)
                {
                    tilemap_property_t property;
                    if (!read_property(bytes, cursor, property)) return std::nullopt;
                    object.properties.emplace_back(std::move(property));
                }

                layer.objects.emplace_back(std::move(object));
            }

            cooked.tilemap.add_layer(std::move(layer));
        }

        std::uint32_t issue_count{ 0u };
        if (!read_u32(bytes, cursor, issue_count)) return std::nullopt;
        for (std::uint32_t i{ 0u }; i < issue_count; ++i)
        {
            tilemap_validation_issue_t issue;
            std::uint8_t severity{ 0u };
            if (!read_u8(bytes, cursor, severity)) return std::nullopt;
            issue.severity = static_cast<tilemap_validation_issue_severity_t>(severity);
            if (!read_string(bytes, cursor, issue.code)) return std::nullopt;
            if (!read_string(bytes, cursor, issue.message)) return std::nullopt;
            cooked.tilemap.add_validation_issue(std::move(issue));
        }

        if (cursor != bytes.size())
            return std::nullopt;

        return cooked;
    }

    bool write_cooked_tilemap_file(const std::filesystem::path& path,
                                   const cooked_tilemap_data_t& tilemap) noexcept
    {
        const auto serialized{ serialize_cooked_tilemap(tilemap) };
        return serialized && utils::file::write_binary_file(path, *serialized);
    }

    std::optional<cooked_tilemap_data_t> load_cooked_tilemap_file(const std::filesystem::path& path) noexcept
    {
        const auto bytes{ utils::file::load_binary_file(path) };
        if (!bytes)
            return std::nullopt;

        return deserialize_cooked_tilemap(*bytes);
    }
} // namespace carrot::assets
