//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioAssetLoader.h"

#include "Audio/Sample/WavLoader.h"
#include "CookedAudio.h"
#include "Assets/ImportedAssetCache.h"
#include "IO/VirtualFileSystem.h"
#include "Utils/File/FileUtils.h"

namespace carrot::assets {
    namespace {
        constexpr std::uint32_t audio_importer_version{ 1u };

        template<typename T>
        void hash_append_value(std::uint64_t& hash, const T& value) noexcept
        {
            const auto bytes{
                std::as_bytes(std::span<const T, 1>{ &value, 1 })
            };
            hash_append_bytes(hash,
                              std::span<const std::uint8_t>{
                                  reinterpret_cast<const std::uint8_t*>(bytes.data()),
                                  bytes.size()
                              });
        }

        void hash_append_string(std::uint64_t& hash, const std::string_view value) noexcept
        {
            hash_append_bytes(hash,
                              std::span<const std::uint8_t>{
                                  reinterpret_cast<const std::uint8_t*>(value.data()),
                                  value.size()
                              });
        }

        [[nodiscard]] std::uint64_t compute_import_settings_hash(const audio_asset_record_t& record) noexcept
        {
            std::uint64_t hash{ 14695981039346656037ull };
            hash_append_value(hash, record.schema_version);
            hash_append_string(hash, record.logical_id);
            hash_append_string(hash, record.source_uri);
            hash_append_value(hash, static_cast<std::uint32_t>(record.bus));
            hash_append_value(hash, record.gain);
            hash_append_value(hash, record.pitch);
            hash_append_value(hash, record.gain_variance);
            hash_append_value(hash, record.pitch_variance);
            hash_append_value(hash, static_cast<std::uint8_t>(record.streamed ? 1u : 0u));
            hash_append_value(hash, static_cast<std::uint8_t>(record.looping ? 1u : 0u));
            hash_append_value(hash, record.loop_start);
            hash_append_value(hash, record.loop_end);
            hash_append_value(hash, static_cast<std::uint32_t>(record.spatial));
            hash_append_value(hash, static_cast<std::uint32_t>(record.distance));
            hash_append_value(hash, record.pan);
            hash_append_value(hash, record.min_distance);
            hash_append_value(hash, record.max_distance);
            hash_append_value(hash, record.max_voices);
            hash_append_value(hash, record.priority);
            return hash;
        }

        [[nodiscard]] imported_asset_invalidation_t build_expected_invalidation(const audio_asset_record_t& record,
                                                                                const io::virtual_file_system_t& vfs)
        {
            imported_asset_invalidation_t invalidation;
            invalidation.import_settings_hash = compute_import_settings_hash(record);

            if (const auto source_hash{ hash_vfs_file_contents(vfs, record.source_uri) })
                invalidation.source_content_hash = *source_hash;

            if (!record.manifest_uri.empty())
            {
                if (const auto manifest_hash{ hash_vfs_file_contents(vfs, record.manifest_uri) })
                    invalidation.asset_definition_content_hash = *manifest_hash;
            }

            return invalidation;
        }

        [[nodiscard]] audio_asset_load_result_t load_streamed_audio_asset(const audio_asset_record_t& record) noexcept
        {
            loaded_audio_asset_t loaded{ };
            loaded.record = &record;
            return { std::move(loaded), audio_asset_load_error::ok };
        }

        [[nodiscard]] audio_asset_load_result_t create_loaded_audio_asset(const audio_asset_record_t& record,
                                                                          const cooked_audio_data_t& cooked) noexcept
        {
            auto sample{ std::make_unique<audio::audio_sample_t>() };
            sample->frame_count = cooked.frame_count;
            sample->channels = cooked.channels;
            sample->sample_rate = cooked.sample_rate;

            const size_t sample_count{ cooked.pcm_payload.size() };
            sample->data = static_cast<float*>(std::malloc(sample_count * sizeof(float)));
            if (!sample->data)
                return { { }, audio_asset_load_error::decode_failed };

            std::memcpy(sample->data,
                        cooked.pcm_payload.data(),
                        sample_count * sizeof(float));

            loaded_audio_asset_t loaded{ };
            loaded.record = &record;
            loaded.sample = std::move(sample);
            return { std::move(loaded), audio_asset_load_error::ok };
        }

        [[nodiscard]] cooked_audio_data_t build_cooked_audio(const audio::audio_sample_t& sample,
                                                             const imported_asset_invalidation_t& invalidation) noexcept
        {
            cooked_audio_data_t cooked{ };
            cooked.importer_version = audio_importer_version;
            cooked.invalidation = invalidation;
            cooked.sample_rate = sample.sample_rate;
            cooked.channels = sample.channels;
            cooked.frame_count = sample.frame_count;

            const size_t sample_count{
                static_cast<size_t>(sample.frame_count) * static_cast<size_t>(sample.channels)
            };
            cooked.pcm_payload.assign(sample.data, sample.data + static_cast<std::ptrdiff_t>(sample_count));
            return cooked;
        }
    } // namespace

    std::filesystem::path cooked_audio_cache_path(const std::string_view logical_id,
                                                  const io::virtual_file_system_t& vfs) noexcept
    {
        return imported_asset_cache_path(logical_id, "audio", ".caud", vfs);
    }

    audio_asset_load_result_t load_audio_asset(const audio_asset_record_t& record,
                                               const io::virtual_file_system_t& vfs) noexcept
    {
        if (record.id == 0 || record.logical_id.empty() || record.source_uri.empty())
            return { { }, audio_asset_load_error::invalid_record };

        const auto native_path{ vfs.resolve_native_path(record.source_uri) };
        if (!native_path)
            return { { }, audio_asset_load_error::resolve_failed };

        if (!std::filesystem::exists(*native_path))
            return { { }, audio_asset_load_error::source_not_found };

        if (!record.manifest_uri.empty())
        {
            const auto manifest_path{ vfs.resolve_native_path(record.manifest_uri) };
            if (!manifest_path || !std::filesystem::exists(*manifest_path))
                return { { }, audio_asset_load_error::manifest_not_found };
        }

        if (record.streamed)
        {
            LOG_ASSET_INFO("Audio asset '{}' is streamed; skipping cooked sample cache.", record.logical_id);
            return load_streamed_audio_asset(record);
        }

        const imported_asset_invalidation_t expected_invalidation{
            build_expected_invalidation(record, vfs)
        };

        const std::filesystem::path cooked_path{ cooked_audio_cache_path(record.logical_id, vfs) };
        if (!cooked_path.empty() && std::filesystem::exists(cooked_path))
        {
            const auto cooked{ load_cooked_audio_file(cooked_path) };
            if (!cooked)
            {
                LOG_ASSET_WARN("Audio asset '{}' has unreadable cooked artifact '{}'; regenerating.",
                               record.logical_id,
                               cooked_path.string());
            }
            else if (is_imported_asset_current(cooked->invalidation,
                                               expected_invalidation,
                                               cooked->importer_version,
                                               audio_importer_version))
            {
                LOG_ASSET_INFO("Loaded audio asset '{}' from cooked cache", record.logical_id);
                return create_loaded_audio_asset(record, *cooked);
            }
            else
            {
                LOG_ASSET_INFO("Cooked audio '{}' is stale; regenerating '{}'.",
                               cooked_path.string(),
                               record.logical_id);
            }
        }

        std::unique_ptr<audio::audio_sample_t> decoded_sample{ audio::load_wav_file(native_path->string()) };
        if (!decoded_sample)
            return { { }, audio_asset_load_error::decode_failed };

        const cooked_audio_data_t cooked{
            build_cooked_audio(*decoded_sample, expected_invalidation)
        };

        if (!cooked_path.empty())
        {
            const auto serialized{ serialize_cooked_audio(cooked) };
            if (!serialized)
                return { { }, audio_asset_load_error::cooked_serialize_failed };

            if (!utils::file::write_binary_file(cooked_path, *serialized))
                return { { }, audio_asset_load_error::cooked_write_failed };

            LOG_ASSET_INFO("Regenerated cooked audio '{}' for asset '{}'.",
                           cooked_path.string(),
                           record.logical_id);
        }
        else
        {
            LOG_ASSET_INFO("Audio asset '{}' loaded from source without cooked cache (no save mount).",
                           record.logical_id);
        }

        loaded_audio_asset_t loaded{ };
        loaded.record = &record;
        loaded.sample = std::move(decoded_sample);
        return { std::move(loaded), audio_asset_load_error::ok };
    }
} // namespace carrot::assets
