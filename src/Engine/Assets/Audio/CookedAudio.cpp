//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "CookedAudio.h"

#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        constexpr std::array<std::uint8_t, 8> caud_magic{
            'C', 'A', 'U', 'D', 0, 0, 0, 0
        };
        constexpr std::uint32_t caud_supported_version{ 1u };
        constexpr std::uint32_t caud_payload_offset{ 68u };

        [[nodiscard]] bool validate_cooked_audio(const cooked_audio_data_t& audio) noexcept
        {
            if (audio.cooked_format_version != caud_supported_version)
                return false;

            if (audio.sample_rate == 0u || audio.channels == 0u)
                return false;

            const size_t expected_sample_count{
                static_cast<size_t>(audio.frame_count) * static_cast<size_t>(audio.channels)
            };
            return audio.pcm_payload.size() == expected_sample_count;
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

    std::optional<std::vector<std::uint8_t>> serialize_cooked_audio(const cooked_audio_data_t& audio) noexcept
    {
        if (!validate_cooked_audio(audio))
            return std::nullopt;

        utils::file::binary_blob_writer_t writer;
        writer.reserve(caud_payload_offset + audio.pcm_payload.size() * sizeof(float));

        [[maybe_unused]] const size_t magic_offset{ writer.write_bytes(caud_magic) };
        [[maybe_unused]] const size_t version_offset{ writer.write_u32(audio.cooked_format_version) };
        [[maybe_unused]] const size_t importer_offset{ writer.write_u32(audio.importer_version) };
        [[maybe_unused]] const size_t flags_offset{ writer.write_u32(audio.flags) };
        [[maybe_unused]] const size_t source_hash_offset{ writer.write_u64(audio.invalidation.source_content_hash) };
        [[maybe_unused]] const size_t manifest_hash_offset{
            writer.write_u64(audio.invalidation.asset_definition_content_hash)
        };
        [[maybe_unused]] const size_t settings_hash_offset{ writer.write_u64(audio.invalidation.import_settings_hash) };
        [[maybe_unused]] const size_t reserved_hash_offset{ writer.write_u64(audio.invalidation.reserved_hash) };
        [[maybe_unused]] const size_t sample_rate_offset{ writer.write_u32(audio.sample_rate) };
        [[maybe_unused]] const size_t channels_offset{ writer.write_u32(audio.channels) };
        [[maybe_unused]] const size_t frame_count_offset{ writer.write_u32(audio.frame_count) };
        [[maybe_unused]] const size_t sample_count_offset{
            writer.write_u32(static_cast<std::uint32_t>(audio.pcm_payload.size()))
        };

        const auto bytes{
            std::as_bytes(std::span<const float>{ audio.pcm_payload.data(), audio.pcm_payload.size() })
        };
        [[maybe_unused]] const size_t payload_offset{ writer.write_bytes(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(bytes.data()),
            bytes.size()
        }) };

        return std::move(writer).take();
    }

    std::optional<cooked_audio_data_t> deserialize_cooked_audio(const std::span<const std::uint8_t> bytes) noexcept
    {
        if (bytes.size() < caud_payload_offset || !std::equal(caud_magic.begin(), caud_magic.end(), bytes.begin()))
            return std::nullopt;

        cooked_audio_data_t audio;
        size_t offset{ 8u };
        std::uint32_t sample_count{ 0u };

        if (!read_u32(bytes, offset, audio.cooked_format_version)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, audio.importer_version)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, audio.flags)) return std::nullopt; offset += 4u;
        if (!read_u64(bytes, offset, audio.invalidation.source_content_hash)) return std::nullopt; offset += 8u;
        if (!read_u64(bytes, offset, audio.invalidation.asset_definition_content_hash)) return std::nullopt; offset += 8u;
        if (!read_u64(bytes, offset, audio.invalidation.import_settings_hash)) return std::nullopt; offset += 8u;
        if (!read_u64(bytes, offset, audio.invalidation.reserved_hash)) return std::nullopt; offset += 8u;
        if (!read_u32(bytes, offset, audio.sample_rate)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, audio.channels)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, audio.frame_count)) return std::nullopt; offset += 4u;
        if (!read_u32(bytes, offset, sample_count)) return std::nullopt;

        const size_t payload_size{ static_cast<size_t>(sample_count) * sizeof(float) };
        if (caud_payload_offset + payload_size > bytes.size())
            return std::nullopt;

        audio.pcm_payload.resize(sample_count);
        std::memcpy(audio.pcm_payload.data(),
                    bytes.data() + caud_payload_offset,
                    payload_size);

        if (!validate_cooked_audio(audio))
            return std::nullopt;

        return audio;
    }

    bool write_cooked_audio_file(const std::filesystem::path& path,
                                 const cooked_audio_data_t& audio) noexcept
    {
        const auto serialized{ serialize_cooked_audio(audio) };
        return serialized && utils::file::write_binary_file(path, *serialized);
    }

    std::optional<cooked_audio_data_t> load_cooked_audio_file(const std::filesystem::path& path) noexcept
    {
        const auto bytes{ utils::file::load_binary_file(path) };
        if (!bytes)
            return std::nullopt;

        return deserialize_cooked_audio(*bytes);
    }
} // namespace carrot::assets
