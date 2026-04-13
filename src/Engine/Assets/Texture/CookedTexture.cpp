//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "CookedTexture.h"

#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        constexpr std::array<std::uint8_t, 8> ctex_magic{
            'C', 'T', 'E', 'X', 0, 0, 0, 0
        };
        constexpr std::uint32_t ctex_supported_version{ 1u };
        constexpr std::uint32_t ctex_payload_offset{ 76u };

        [[nodiscard]] bool validate_cooked_texture(const cooked_texture_data_t& texture) noexcept
        {
            if (texture.cooked_format_version != ctex_supported_version)
                return false;

            if (texture.width == 0u || texture.height == 0u || texture.stride_bytes == 0u)
                return false;

            if (texture.format != rhi::texture_format_t::rgba8_unorm &&
                texture.format != rhi::texture_format_t::rgba8_srgb)
            {
                return false;
            }

            const size_t expected_size{
                static_cast<size_t>(texture.stride_bytes) * static_cast<size_t>(texture.height)
            };
            return texture.pixel_payload.size() == expected_size;
        }

        [[nodiscard]] bool read_u32(std::span<const std::uint8_t> bytes,
                                    const size_t offset,
                                    std::uint32_t& out_value) noexcept
        {
            if (offset + sizeof(std::uint32_t) > bytes.size())
                return false;

            out_value =
                (static_cast<std::uint32_t>(bytes[offset + 0]) << 0u) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
                (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
                (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
            return true;
        }

        [[nodiscard]] bool read_u64(std::span<const std::uint8_t> bytes,
                                    const size_t offset,
                                    std::uint64_t& out_value) noexcept
        {
            if (offset + sizeof(std::uint64_t) > bytes.size())
                return false;

            out_value =
                (static_cast<std::uint64_t>(bytes[offset + 0]) << 0u) |
                (static_cast<std::uint64_t>(bytes[offset + 1]) << 8u) |
                (static_cast<std::uint64_t>(bytes[offset + 2]) << 16u) |
                (static_cast<std::uint64_t>(bytes[offset + 3]) << 24u) |
                (static_cast<std::uint64_t>(bytes[offset + 4]) << 32u) |
                (static_cast<std::uint64_t>(bytes[offset + 5]) << 40u) |
                (static_cast<std::uint64_t>(bytes[offset + 6]) << 48u) |
                (static_cast<std::uint64_t>(bytes[offset + 7]) << 56u);
            return true;
        }
    } // namespace

    std::optional<std::vector<std::uint8_t>> serialize_cooked_texture(const cooked_texture_data_t& texture) noexcept
    {
        if (!validate_cooked_texture(texture))
            return std::nullopt;

        utils::file::binary_blob_writer_t writer;
        writer.reserve(ctex_payload_offset + texture.pixel_payload.size());

        [[maybe_unused]] const size_t magic_offset{ writer.write_bytes(ctex_magic) };
        [[maybe_unused]] const size_t version_offset{ writer.write_u32(texture.cooked_format_version) };
        [[maybe_unused]] const size_t importer_offset{ writer.write_u32(texture.importer_version) };
        [[maybe_unused]] const size_t flags_offset{ writer.write_u32(texture.flags) };
        [[maybe_unused]] const size_t source_hash_offset{ writer.write_u64(texture.invalidation.source_content_hash) };
        [[maybe_unused]] const size_t manifest_hash_offset{
            writer.write_u64(texture.invalidation.asset_definition_content_hash)
        };
        [[maybe_unused]] const size_t settings_hash_offset{ writer.write_u64(texture.invalidation.import_settings_hash) };
        [[maybe_unused]] const size_t reserved_hash_offset{ writer.write_u64(texture.invalidation.reserved_hash) };
        [[maybe_unused]] const size_t width_offset{ writer.write_u32(texture.width) };
        [[maybe_unused]] const size_t height_offset{ writer.write_u32(texture.height) };
        [[maybe_unused]] const size_t stride_offset{ writer.write_u32(texture.stride_bytes) };
        [[maybe_unused]] const size_t format_offset{ writer.write_u32(static_cast<std::uint32_t>(texture.format)) };
        [[maybe_unused]] const size_t payload_size_offset{
            writer.write_u32(static_cast<std::uint32_t>(texture.pixel_payload.size()))
        };
        [[maybe_unused]] const size_t reserved0_offset{ writer.write_u32(0u) };
        [[maybe_unused]] const size_t payload_offset{ writer.write_bytes(texture.pixel_payload) };

        return std::move(writer).take();
    }

    std::optional<cooked_texture_data_t> deserialize_cooked_texture(const std::span<const std::uint8_t> bytes) noexcept
    {
        if (bytes.size() < ctex_payload_offset || !std::equal(ctex_magic.begin(), ctex_magic.end(), bytes.begin()))
            return std::nullopt;

        cooked_texture_data_t texture;
        size_t offset{ 8u };
        std::uint32_t format{ 0u };
        std::uint32_t payload_size{ 0u };

        if (!read_u32(bytes, offset, texture.cooked_format_version)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, texture.importer_version)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, texture.flags)) return std::nullopt; offset += 4u;
        if (!read_u64(bytes, offset, texture.invalidation.source_content_hash)) return std::nullopt; offset += 8u;
        if (!read_u64(bytes, offset, texture.invalidation.asset_definition_content_hash)) return std::nullopt; offset += 8u;
        if (!read_u64(bytes, offset, texture.invalidation.import_settings_hash)) return std::nullopt; offset += 8u;
        if (!read_u64(bytes, offset, texture.invalidation.reserved_hash)) return std::nullopt; offset += 8u;
        if (!read_u32(bytes, offset, texture.width)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, texture.height)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, texture.stride_bytes)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, format)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, payload_size)) return std::nullopt;

        texture.format = static_cast<rhi::texture_format_t>(format);

        if (ctex_payload_offset + payload_size > bytes.size())
            return std::nullopt;

        texture.pixel_payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(ctex_payload_offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(ctex_payload_offset + payload_size));

        if (!validate_cooked_texture(texture))
            return std::nullopt;

        return texture;
    }

    bool write_cooked_texture_file(const std::filesystem::path& path,
                                   const cooked_texture_data_t& texture) noexcept
    {
        const auto serialized{ serialize_cooked_texture(texture) };
        return serialized && utils::file::write_binary_file(path, *serialized);
    }

    std::optional<cooked_texture_data_t> load_cooked_texture_file(const std::filesystem::path& path) noexcept
    {
        const auto bytes{ utils::file::load_binary_file(path) };
        if (!bytes)
            return std::nullopt;

        return deserialize_cooked_texture(*bytes);
    }
} // namespace carrot::assets
