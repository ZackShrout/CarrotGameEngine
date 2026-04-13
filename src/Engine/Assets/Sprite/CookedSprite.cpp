//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "CookedSprite.h"

#include "Utils/File/FileUtils.h"

#include <bit>

namespace carrot::assets {
    namespace {
        constexpr std::array<std::uint8_t, 8> csprite_magic{
            'C', 'S', 'P', 'R', 'I', 'T', 'E', 0
        };
        constexpr std::uint32_t cooked_sprite_supported_version{ 1u };

        [[nodiscard]] bool read_u8(std::span<const std::uint8_t> bytes,
                                   size_t& cursor,
                                   std::uint8_t& out_value) noexcept
        {
            if (cursor + 1u > bytes.size())
                return false;

            out_value = bytes[cursor];
            cursor += 1u;
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
    } // namespace

    std::optional<std::vector<std::uint8_t>> serialize_cooked_sprite(const cooked_sprite_data_t& cooked) noexcept
    {
        if (cooked.cooked_format_version != cooked_sprite_supported_version)
            return std::nullopt;

        utils::file::binary_blob_writer_t writer;
        writer.reserve(256u);

        [[maybe_unused]] const size_t magic_offset{ writer.write_bytes(csprite_magic) };
        [[maybe_unused]] const size_t version_offset{ writer.write_u32(cooked.cooked_format_version) };
        [[maybe_unused]] const size_t importer_offset{ writer.write_u32(cooked.importer_version) };
        [[maybe_unused]] const size_t source_hash_offset{ writer.write_u64(cooked.invalidation.source_content_hash) };
        [[maybe_unused]] const size_t manifest_hash_offset{ writer.write_u64(cooked.invalidation.asset_definition_content_hash) };
        [[maybe_unused]] const size_t settings_hash_offset{ writer.write_u64(cooked.invalidation.import_settings_hash) };
        [[maybe_unused]] const size_t reserved_hash_offset{ writer.write_u64(cooked.invalidation.reserved_hash) };

        write_string(writer, cooked.sprite.texture_id());
        [[maybe_unused]] const size_t pivot_x_offset{ writer.write_f32(cooked.sprite.default_pivot().x) };
        [[maybe_unused]] const size_t pivot_y_offset{ writer.write_f32(cooked.sprite.default_pivot().y) };
        [[maybe_unused]] const size_t pixels_per_unit_offset{ writer.write_f32(cooked.sprite.pixels_per_unit()) };

        const auto frames{ cooked.sprite.frames() };
        [[maybe_unused]] const size_t frame_count_offset{ writer.write_u32(static_cast<std::uint32_t>(frames.size())) };
        for (const sprite_frame_t& frame : frames)
        {
            write_string(writer, frame.name);
            [[maybe_unused]] const size_t x_offset{ writer.write_u32(frame.pixel_rect.position.x) };
            [[maybe_unused]] const size_t y_offset{ writer.write_u32(frame.pixel_rect.position.y) };
            [[maybe_unused]] const size_t w_offset{ writer.write_u32(frame.pixel_rect.size.x) };
            [[maybe_unused]] const size_t h_offset{ writer.write_u32(frame.pixel_rect.size.y) };
            [[maybe_unused]] const size_t frame_pivot_x_offset{ writer.write_f32(frame.pivot.x) };
            [[maybe_unused]] const size_t frame_pivot_y_offset{ writer.write_f32(frame.pivot.y) };
        }

        const auto animations{ cooked.sprite.animations() };
        [[maybe_unused]] const size_t animation_count_offset{
            writer.write_u32(static_cast<std::uint32_t>(animations.size()))
        };
        for (const sprite_animation_t& animation : animations)
        {
            write_string(writer, animation.name);
            [[maybe_unused]] const size_t loop_offset{ writer.write_u8(animation.loop ? 1u : 0u) };
            [[maybe_unused]] const size_t animation_frame_count_offset{
                writer.write_u32(static_cast<std::uint32_t>(animation.frames.size()))
            };
            for (const sprite_animation_frame_t& animation_frame : animation.frames)
            {
                [[maybe_unused]] const size_t frame_index_offset{ writer.write_u32(animation_frame.frame_index) };
                [[maybe_unused]] const size_t duration_offset{ writer.write_f32(animation_frame.duration_seconds) };
            }
        }

        return std::move(writer).take();
    }

    std::optional<cooked_sprite_data_t> deserialize_cooked_sprite(const std::span<const std::uint8_t> bytes) noexcept
    {
        if (bytes.size() < 40u || !std::equal(csprite_magic.begin(), csprite_magic.end(), bytes.begin()))
            return std::nullopt;

        cooked_sprite_data_t cooked;
        size_t cursor{ 8u };
        if (!read_u32(bytes, cursor, cooked.cooked_format_version)) return std::nullopt;
        if (!read_u32(bytes, cursor, cooked.importer_version)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.source_content_hash)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.asset_definition_content_hash)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.import_settings_hash)) return std::nullopt;
        if (!read_u64(bytes, cursor, cooked.invalidation.reserved_hash)) return std::nullopt;

        if (cooked.cooked_format_version != cooked_sprite_supported_version)
            return std::nullopt;

        std::string texture_id;
        if (!read_string(bytes, cursor, texture_id)) return std::nullopt;
        cooked.sprite.set_texture_id(std::move(texture_id));

        float default_pivot_x{ 0.0f };
        float default_pivot_y{ 0.0f };
        if (!read_f32(bytes, cursor, default_pivot_x)) return std::nullopt;
        if (!read_f32(bytes, cursor, default_pivot_y)) return std::nullopt;
        cooked.sprite.set_default_pivot({ default_pivot_x, default_pivot_y });

        float pixels_per_unit{ 1.0f };
        if (!read_f32(bytes, cursor, pixels_per_unit)) return std::nullopt;
        cooked.sprite.set_pixels_per_unit(pixels_per_unit);

        std::uint32_t frame_count{ 0u };
        if (!read_u32(bytes, cursor, frame_count)) return std::nullopt;
        for (std::uint32_t i{ 0u }; i < frame_count; ++i)
        {
            sprite_frame_t frame;
            if (!read_string(bytes, cursor, frame.name)) return std::nullopt;
            std::uint32_t frame_x{ 0u };
            std::uint32_t frame_y{ 0u };
            std::uint32_t frame_w{ 0u };
            std::uint32_t frame_h{ 0u };
            float pivot_x{ 0.5f };
            float pivot_y{ 0.5f };
            if (!read_u32(bytes, cursor, frame_x)) return std::nullopt;
            if (!read_u32(bytes, cursor, frame_y)) return std::nullopt;
            if (!read_u32(bytes, cursor, frame_w)) return std::nullopt;
            if (!read_u32(bytes, cursor, frame_h)) return std::nullopt;
            if (!read_f32(bytes, cursor, pivot_x)) return std::nullopt;
            if (!read_f32(bytes, cursor, pivot_y)) return std::nullopt;
            frame.pixel_rect.position = { frame_x, frame_y };
            frame.pixel_rect.size = { frame_w, frame_h };
            frame.pivot = { pivot_x, pivot_y };
            cooked.sprite.add_frame(std::move(frame));
        }

        std::uint32_t animation_count{ 0u };
        if (!read_u32(bytes, cursor, animation_count)) return std::nullopt;
        for (std::uint32_t i{ 0u }; i < animation_count; ++i)
        {
            sprite_animation_t animation;
            if (!read_string(bytes, cursor, animation.name)) return std::nullopt;
            std::uint8_t loop{ 0u };
            if (!read_u8(bytes, cursor, loop)) return std::nullopt;
            animation.loop = loop != 0u;

            std::uint32_t animation_frame_count{ 0u };
            if (!read_u32(bytes, cursor, animation_frame_count)) return std::nullopt;
            animation.frames.reserve(animation_frame_count);
            for (std::uint32_t frame_i{ 0u }; frame_i < animation_frame_count; ++frame_i)
            {
                sprite_animation_frame_t animation_frame;
                if (!read_u32(bytes, cursor, animation_frame.frame_index)) return std::nullopt;
                if (!read_f32(bytes, cursor, animation_frame.duration_seconds)) return std::nullopt;
                animation.frames.emplace_back(animation_frame);
            }

            cooked.sprite.add_animation(std::move(animation));
        }

        if (cursor != bytes.size() || !cooked.sprite.build_lookup_tables())
            return std::nullopt;

        return cooked;
    }

    bool write_cooked_sprite_file(const std::filesystem::path& path,
                                  const cooked_sprite_data_t& sprite) noexcept
    {
        const auto serialized{ serialize_cooked_sprite(sprite) };
        return serialized && utils::file::write_binary_file(path, *serialized);
    }

    std::optional<cooked_sprite_data_t> load_cooked_sprite_file(const std::filesystem::path& path) noexcept
    {
        const auto bytes{ utils::file::load_binary_file(path) };
        if (!bytes)
            return std::nullopt;

        return deserialize_cooked_sprite(*bytes);
    }
} // namespace carrot::assets
