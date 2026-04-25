//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SaveService.h"

#include "IO/VirtualFileSystem.h"
#include "Utils/File/FileUtils.h"
#include "Utils/JSON/Public/JsonDocument.h"
#include "Utils/JSON/Public/JsonWriter.h"

#include <algorithm>
#include <cstring>

namespace carrot::save {
    namespace {
        constexpr std::string_view k_slots_directory_name{ "slots" };
        constexpr std::string_view k_slot_metadata_file_name{ "metadata.json" };
        constexpr std::string_view k_default_payload_file_name{ "payload.bin" };
        constexpr std::string_view k_payload_magic{ "CSAV" };
        constexpr std::string_view k_temp_file_suffix{ ".tmp" };
        constexpr std::uint32_t k_supported_save_format_version{ 1u };
        constexpr std::uint32_t k_supported_payload_format_version{ 1u };
        constexpr std::string_view k_supported_build_compatibility{ "carrot-dev" };

        [[nodiscard]] bool read_u32(const std::span<const std::uint8_t> bytes,
                                    const size_t offset,
                                    std::uint32_t& value) noexcept
        {
            if (offset + sizeof(std::uint32_t) > bytes.size())
                return false;

            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return true;
        }

        [[nodiscard]] bool read_u8(const std::span<const std::uint8_t> bytes,
                                   const size_t offset,
                                   std::uint8_t& value) noexcept
        {
            if (offset + sizeof(std::uint8_t) > bytes.size())
                return false;

            value = bytes[offset];
            return true;
        }

    } // namespace

    std::string_view to_string(const save_slot_kind_t kind) noexcept
    {
        switch (kind)
        {
            case save_slot_kind_t::manual: return "manual";
            case save_slot_kind_t::autosave: return "autosave";
            case save_slot_kind_t::temp: return "temp";
        }

        return "unknown";
    }

    std::string_view to_string(const save_request_kind_t kind) noexcept
    {
        switch (kind)
        {
            case save_request_kind_t::none: return "none";
            case save_request_kind_t::save_slot: return "save_slot";
            case save_request_kind_t::load_slot: return "load_slot";
        }

        return "unknown";
    }

    std::string_view to_string(const save_request_outcome_t outcome) noexcept
    {
        switch (outcome)
        {
            case save_request_outcome_t::none: return "none";
            case save_request_outcome_t::pending: return "pending";
            case save_request_outcome_t::succeeded: return "succeeded";
            case save_request_outcome_t::failed: return "failed";
        }

        return "unknown";
    }

    std::string_view to_string(const save_failure_reason_t reason) noexcept
    {
        switch (reason)
        {
            case save_failure_reason_t::none: return "none";
            case save_failure_reason_t::missing: return "missing";
            case save_failure_reason_t::invalid: return "invalid";
            case save_failure_reason_t::corrupt: return "corrupt";
            case save_failure_reason_t::incompatible: return "incompatible";
            case save_failure_reason_t::io_error: return "io_error";
        }

        return "unknown";
    }

    std::string_view to_string(const save_section_owner_t owner) noexcept
    {
        switch (owner)
        {
            case save_section_owner_t::engine: return "engine";
            case save_section_owner_t::gameplay: return "gameplay";
        }

        return "unknown";
    }

    bool save_section_collector_t::add_section(const std::string_view section_id,
                                               const save_section_owner_t owner,
                                               const std::span<const std::uint8_t> bytes,
                                               std::string& error_detail)
    {
        if (!save_service_t::is_valid_slot_id(section_id))
        {
            error_detail = std::format("Save section id '{}' must use only letters, digits, '-' or '_'.", section_id);
            return false;
        }

        const auto duplicate{
            std::ranges::find_if(_sections, [section_id](const save_payload_section_t& section)
            {
                return section.section_id == section_id;
            })
        };
        if (duplicate != _sections.end())
        {
            error_detail = std::format("Save section '{}' was registered more than once.", section_id);
            return false;
        }

        save_payload_section_t section;
        section.section_id = std::string{ section_id };
        section.owner = owner;
        section.bytes.assign(bytes.begin(), bytes.end());
        _sections.push_back(std::move(section));
        return true;
    }

    const save_payload_section_t* loaded_save_slot_t::find_section(const std::string_view section_id) const noexcept
    {
        const auto it{
            std::ranges::find_if(sections, [section_id](const save_payload_section_t& section)
            {
                return section.section_id == section_id;
            })
        };
        return it == sections.end() ? nullptr : &(*it);
    }

    std::string loaded_save_slot_t::describe_sections() const
    {
        if (sections.empty())
            return "no sections";

        std::string result;
        for (size_t index{ 0u }; index < sections.size(); ++index)
        {
            const save_payload_section_t& section{ sections[index] };
            if (!result.empty())
                result += ", ";

            result += std::format("{}:{}({} bytes)",
                                  to_string(section.owner),
                                  section.section_id,
                                  section.bytes.size());
        }

        return result;
    }

    bool save_service_t::configured() const noexcept
    {
        return save_root_path().has_value();
    }

    std::optional<std::filesystem::path> save_service_t::save_root_path() const noexcept
    {
        const std::optional<io::vfs_mount_point_t> mount{ _vfs.get_mount("save") };
        if (!mount.has_value())
            return std::nullopt;

        return mount->root.lexically_normal();
    }

    std::optional<std::filesystem::path> save_service_t::slots_root_path() const noexcept
    {
        const std::optional<std::filesystem::path> root{ save_root_path() };
        if (!root.has_value())
            return std::nullopt;

        return (*root / k_slots_directory_name).lexically_normal();
    }

    std::vector<save_slot_summary_t> save_service_t::list_slots() const
    {
        std::vector<save_slot_summary_t> slots;

        const std::optional<std::filesystem::path> slots_root{ slots_root_path() };
        if (!slots_root.has_value() || !std::filesystem::exists(*slots_root))
            return slots;

        std::error_code iterate_error;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(*slots_root, iterate_error))
        {
            if (iterate_error)
            {
                LOG_CORE_WARN("Failed to enumerate save slots in '{}': {}",
                              utils::file::to_log_string(*slots_root),
                              iterate_error.message());
                return {};
            }

            if (!entry.is_directory())
                continue;

            const std::optional<save_slot_metadata_t> metadata{
                load_metadata_from_file(slot_metadata_path_for(entry.path()))
            };
            if (!metadata.has_value())
                continue;

            slots.push_back(save_slot_summary_t{
                .slot_id = metadata->slot_id,
                .native_path = entry.path().lexically_normal(),
                .metadata = *metadata
            });
        }

        std::ranges::sort(slots, [](const save_slot_summary_t& lhs, const save_slot_summary_t& rhs)
        {
            return lhs.slot_id < rhs.slot_id;
        });
        return slots;
    }

    save_operation_status_t save_service_t::execute(const save_request_t& request) const
    {
        switch (request.kind)
        {
            case save_request_kind_t::save_slot:
                return save_slot(request, std::span<const save_payload_section_t>{});
            case save_request_kind_t::load_slot:
            {
                save_operation_status_t status;
                (void)load_slot(request.slot_id, status);
                return status;
            }
            case save_request_kind_t::none:
            default:
                return save_operation_status_t{
                    .request_kind = request.kind,
                    .outcome = save_request_outcome_t::failed,
                    .failure_reason = save_failure_reason_t::invalid,
                    .slot_id = request.slot_id,
                    .detail = "No save request kind was provided."
                };
        }
    }

    save_operation_status_t save_service_t::write_slot(const save_request_t& request,
                                                       const std::span<const save_payload_section_t> sections) const
    {
        return save_slot(request, sections);
    }

    std::optional<loaded_save_slot_t> save_service_t::load_slot(const std::string_view slot_id,
                                                                save_operation_status_t& status) const
    {
        if (!is_valid_slot_id(slot_id))
        {
            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::invalid,
                .slot_id = std::string{ slot_id },
                .detail = "Slot id must be non-empty and use only letters, digits, '-' or '_'."
            };
            return std::nullopt;
        }

        const std::optional<std::filesystem::path> slots_root{ slots_root_path() };
        if (!slots_root.has_value())
        {
            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::missing,
                .slot_id = std::string{ slot_id },
                .detail = "Save root is not configured."
            };
            return std::nullopt;
        }

        const std::filesystem::path slot_path{ slot_path_for(*slots_root, slot_id) };
        const std::optional<save_slot_metadata_t> metadata{
            load_metadata_for_load(slot_metadata_path_for(slot_path), slot_id, status)
        };
        if (!metadata.has_value())
            return std::nullopt;

        std::string payload_error;
        std::optional<std::vector<save_payload_section_t>> sections{
            load_payload_sections(slot_payload_path_for(slot_path, metadata->payload_file),
                                  metadata->payload_format_version,
                                  payload_error)
        };
        if (!sections.has_value())
        {
            save_failure_reason_t failure_reason{ save_failure_reason_t::corrupt };
            if (payload_error.find("could not be loaded") != std::string::npos)
                failure_reason = save_failure_reason_t::missing;
            else if (payload_error.find("unsupported payload version") != std::string::npos)
                failure_reason = save_failure_reason_t::incompatible;

            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = failure_reason,
                .slot_id = std::string{ slot_id },
                .detail = std::move(payload_error)
            };
            return std::nullopt;
        }

        status = save_operation_status_t{
            .request_kind = save_request_kind_t::load_slot,
            .outcome = save_request_outcome_t::succeeded,
            .failure_reason = save_failure_reason_t::none,
            .slot_id = std::string{ slot_id },
            .detail = std::format("Resolved save slot '{}' for load.", slot_id)
        };
        return loaded_save_slot_t{
            .metadata = *metadata,
            .sections = std::move(*sections)
        };
    }

    std::optional<save_slot_metadata_t> save_service_t::read_slot_metadata(const std::string_view slot_id) const
    {
        if (!is_valid_slot_id(slot_id))
            return std::nullopt;

        const std::optional<std::filesystem::path> slots_root{ slots_root_path() };
        if (!slots_root.has_value())
            return std::nullopt;

        const std::filesystem::path slot_path{ slot_path_for(*slots_root, slot_id) };
        return load_metadata_from_file(slot_metadata_path_for(slot_path));
    }

    bool save_service_t::is_valid_slot_id(const std::string_view slot_id) noexcept
    {
        if (slot_id.empty())
            return false;

        for (const char ch : slot_id)
        {
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' ||
                ch == '_')
            {
                continue;
            }

            return false;
        }

        return true;
    }

    save_operation_status_t save_service_t::save_slot(const save_request_t& request,
                                                      const std::span<const save_payload_section_t> sections) const
    {
        const std::string_view slot_id{ request.slot_id };
        if (!is_valid_slot_id(slot_id))
        {
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::invalid,
                .slot_id = std::string{ slot_id },
                .detail = "Slot id must be non-empty and use only letters, digits, '-' or '_'."
            };
        }

        const std::optional<std::filesystem::path> slots_root{ ensure_slots_root() };
        if (!slots_root.has_value())
        {
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::missing,
                .slot_id = std::string{ slot_id },
                .detail = "Save root is not configured."
            };
        }

        const std::filesystem::path slot_path{ slot_path_for(*slots_root, slot_id) };
        std::error_code create_error;
        std::filesystem::create_directories(slot_path, create_error);
        if (create_error)
        {
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::io_error,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Failed to create slot directory '{}': {}",
                                      utils::file::to_log_string(slot_path),
                                      create_error.message())
            };
        }

        save_slot_metadata_t metadata{
            .slot_id = std::string{ slot_id },
            .slot_kind = request.slot_kind,
            .timestamp_utc_unix_seconds = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()),
            .scene_id = request.scene_id,
            .scene_label = request.scene_label.empty() ? request.scene_id : request.scene_label,
            .spawn_marker = request.spawn_marker,
            .playtime_seconds = std::max(0.0, request.playtime_seconds),
            .save_format_version = k_supported_save_format_version,
            .payload_format_version = k_supported_payload_format_version,
            .build_compatibility = std::string{ k_supported_build_compatibility },
            .payload_file = std::string{ k_default_payload_file_name },
            .payload_size_bytes = 0u
        };

        std::optional<std::vector<std::uint8_t>> payload_bytes{ build_payload_container(metadata, sections) };
        if (!payload_bytes.has_value())
        {
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::invalid,
                .slot_id = std::string{ slot_id },
                .detail = "Failed to build save payload container."
            };
        }

        metadata.payload_size_bytes = payload_bytes->size();

        const std::filesystem::path payload_path{ slot_payload_path_for(slot_path, metadata.payload_file) };
        const std::filesystem::path payload_temp_path{ payload_path.string() + std::string{ k_temp_file_suffix } };
        const std::filesystem::path metadata_path{ slot_metadata_path_for(slot_path) };
        const std::filesystem::path metadata_temp_path{ metadata_path.string() + std::string{ k_temp_file_suffix } };
        std::error_code cleanup_error;
        std::filesystem::remove(payload_temp_path, cleanup_error);
        cleanup_error.clear();
        std::filesystem::remove(metadata_temp_path, cleanup_error);

        if (!utils::file::write_binary_file(payload_temp_path, *payload_bytes))
        {
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::io_error,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Failed to write save payload temp file '{}'.",
                                      utils::file::to_log_string(payload_temp_path))
            };
        }

        const std::string metadata_json{ serialize_metadata_to_json(metadata) };
        const std::span<const std::uint8_t> metadata_bytes{
            reinterpret_cast<const std::uint8_t*>(metadata_json.data()),
            metadata_json.size()
        };
        if (!utils::file::write_binary_file(metadata_temp_path, metadata_bytes))
        {
            std::filesystem::remove(payload_temp_path, cleanup_error);
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::io_error,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Failed to write save metadata temp file '{}'.",
                                      utils::file::to_log_string(metadata_temp_path))
            };
        }

        std::error_code rename_error;
        std::filesystem::rename(payload_temp_path, payload_path, rename_error);
        if (rename_error)
        {
            std::filesystem::remove(payload_temp_path, cleanup_error);
            std::filesystem::remove(metadata_temp_path, cleanup_error);
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::io_error,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Failed to finalize save payload '{}': {}",
                                      utils::file::to_log_string(payload_path),
                                      rename_error.message())
            };
        }

        rename_error.clear();
        std::filesystem::rename(metadata_temp_path, metadata_path, rename_error);
        if (rename_error)
        {
            std::filesystem::remove(metadata_temp_path, cleanup_error);
            return save_operation_status_t{
                .request_kind = save_request_kind_t::save_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::io_error,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Failed to finalize save metadata '{}': {}",
                                      utils::file::to_log_string(metadata_path),
                                      rename_error.message())
            };
        }

        return save_operation_status_t{
            .request_kind = save_request_kind_t::save_slot,
            .outcome = save_request_outcome_t::succeeded,
            .failure_reason = save_failure_reason_t::none,
            .slot_id = std::string{ slot_id },
            .detail = std::format("Prepared save slot '{}' with metadata and payload container.", slot_id)
        };
    }

    std::optional<std::filesystem::path> save_service_t::ensure_slots_root() const noexcept
    {
        const std::optional<std::filesystem::path> slots_root{ slots_root_path() };
        if (!slots_root.has_value())
            return std::nullopt;

        std::error_code create_error;
        std::filesystem::create_directories(*slots_root, create_error);
        if (create_error)
        {
            LOG_CORE_WARN("Failed to create save slots root '{}': {}",
                          utils::file::to_log_string(*slots_root),
                          create_error.message());
            return std::nullopt;
        }

        return slots_root;
    }

    std::filesystem::path save_service_t::slot_path_for(const std::filesystem::path& slots_root,
                                                        const std::string_view slot_id)
    {
        return (slots_root / std::string{ slot_id }).lexically_normal();
    }

    std::filesystem::path save_service_t::slot_metadata_path_for(const std::filesystem::path& slot_path)
    {
        return (slot_path / std::string{ k_slot_metadata_file_name }).lexically_normal();
    }

    std::filesystem::path save_service_t::slot_payload_path_for(const std::filesystem::path& slot_path,
                                                                const std::string_view payload_file)
    {
        return (slot_path / std::string{ payload_file }).lexically_normal();
    }

    std::optional<save_slot_metadata_t> save_service_t::load_metadata_from_file(const std::filesystem::path& path) noexcept
    {
        if (!std::filesystem::exists(path))
            return std::nullopt;

        utils::json::json_document_t doc;
        if (!doc.parse_from_file(path.string().c_str()) || !doc.root().is_object())
            return std::nullopt;

        const utils::json::json_object_view_t root{ doc.root().as_object() };
        save_slot_metadata_t metadata;
        metadata.slot_id = std::string{ root.get_string_or("slot_id", {}) };
        if (metadata.slot_id.empty())
            return std::nullopt;

        const std::string_view kind_name{ root.get_string_or("slot_kind", "manual") };
        if (kind_name == "manual")
            metadata.slot_kind = save_slot_kind_t::manual;
        else if (kind_name == "autosave")
            metadata.slot_kind = save_slot_kind_t::autosave;
        else if (kind_name == "temp")
            metadata.slot_kind = save_slot_kind_t::temp;
        else
            return std::nullopt;

        metadata.timestamp_utc_unix_seconds = static_cast<std::uint64_t>(
            std::max(0.0, root.get_number_or("timestamp_utc_unix_seconds", 0.0)));
        metadata.scene_id = std::string{ root.get_string_or("scene_id", {}) };
        metadata.scene_label = std::string{ root.get_string_or("scene_label", {}) };
        metadata.spawn_marker = std::string{ root.get_string_or("spawn_marker", {}) };
        metadata.playtime_seconds = std::max(0.0, root.get_number_or("playtime_seconds", 0.0));
        metadata.save_format_version = static_cast<std::uint32_t>(
            std::max(0.0, root.get_number_or("save_format_version", 1.0)));
        metadata.payload_format_version = static_cast<std::uint32_t>(
            std::max(0.0, root.get_number_or("payload_format_version", 1.0)));
        metadata.build_compatibility = std::string{ root.get_string_or("build_compatibility", "carrot-dev") };
        metadata.payload_file = std::string{ root.get_string_or("payload_file", k_default_payload_file_name) };
        metadata.payload_size_bytes = static_cast<std::uint64_t>(
            std::max(0.0, root.get_number_or("payload_size_bytes", 0.0)));
        return metadata;
    }

    std::optional<save_slot_metadata_t> save_service_t::load_metadata_for_load(const std::filesystem::path& path,
                                                                                const std::string_view slot_id,
                                                                                save_operation_status_t& status) noexcept
    {
        if (!std::filesystem::exists(path))
        {
            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::missing,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Save slot '{}' metadata file is missing.", slot_id)
            };
            return std::nullopt;
        }

        const std::optional<save_slot_metadata_t> metadata{ load_metadata_from_file(path) };
        if (!metadata.has_value())
        {
            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::corrupt,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Save slot '{}' metadata is invalid or corrupt.", slot_id)
            };
            return std::nullopt;
        }

        if (metadata->save_format_version != k_supported_save_format_version)
        {
            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::incompatible,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Save slot '{}' uses unsupported save format version {} (expected {}).",
                                      slot_id,
                                      metadata->save_format_version,
                                      k_supported_save_format_version)
            };
            return std::nullopt;
        }

        if (metadata->payload_format_version != k_supported_payload_format_version)
        {
            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::incompatible,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Save slot '{}' metadata uses unsupported payload format version {} (expected {}).",
                                      slot_id,
                                      metadata->payload_format_version,
                                      k_supported_payload_format_version)
            };
            return std::nullopt;
        }

        if (metadata->build_compatibility != k_supported_build_compatibility)
        {
            status = save_operation_status_t{
                .request_kind = save_request_kind_t::load_slot,
                .outcome = save_request_outcome_t::failed,
                .failure_reason = save_failure_reason_t::incompatible,
                .slot_id = std::string{ slot_id },
                .detail = std::format("Save slot '{}' targets incompatible build '{}'.",
                                      slot_id,
                                      metadata->build_compatibility)
            };
            return std::nullopt;
        }

        return metadata;
    }

    std::string save_service_t::serialize_metadata_to_json(const save_slot_metadata_t& metadata)
    {
        utils::json::json_writer_t writer;
        writer.begin_object();
        writer.key("slot_id");
        writer.value(metadata.slot_id);
        writer.key("slot_kind");
        writer.value(to_string(metadata.slot_kind));
        writer.key("timestamp_utc_unix_seconds");
        writer.value(metadata.timestamp_utc_unix_seconds);
        writer.key("scene_id");
        writer.value(metadata.scene_id);
        writer.key("scene_label");
        writer.value(metadata.scene_label);
        writer.key("spawn_marker");
        writer.value(metadata.spawn_marker);
        writer.key("playtime_seconds");
        writer.value(metadata.playtime_seconds);
        writer.key("save_format_version");
        writer.value(metadata.save_format_version);
        writer.key("payload_format_version");
        writer.value(metadata.payload_format_version);
        writer.key("build_compatibility");
        writer.value(metadata.build_compatibility);
        writer.key("payload_file");
        writer.value(metadata.payload_file);
        writer.key("payload_size_bytes");
        writer.value(metadata.payload_size_bytes);
        writer.end_object();

        std::string json{ writer.take() };
        json.push_back('\n');
        return json;
    }

    std::optional<std::vector<std::uint8_t>> save_service_t::build_payload_container(
        const save_slot_metadata_t& metadata,
        const std::span<const save_payload_section_t> sections) noexcept
    {
        utils::file::binary_blob_writer_t writer;
        const std::span<const std::uint8_t> magic_bytes{
            reinterpret_cast<const std::uint8_t*>(k_payload_magic.data()),
            k_payload_magic.size()
        };
        (void)writer.write_bytes(magic_bytes);
        (void)writer.write_u32(metadata.payload_format_version);
        (void)writer.write_u32(static_cast<std::uint32_t>(sections.size()));

        for (const save_payload_section_t& section : sections)
        {
            const std::span<const std::uint8_t> section_id_bytes{
                reinterpret_cast<const std::uint8_t*>(section.section_id.data()),
                section.section_id.size()
            };
            (void)writer.write_u32(static_cast<std::uint32_t>(section.section_id.size()));
            (void)writer.write_u32(static_cast<std::uint32_t>(section.bytes.size()));
            (void)writer.write_u8(static_cast<std::uint8_t>(section.owner));
            (void)writer.write_u8(0u);
            (void)writer.write_u8(0u);
            (void)writer.write_u8(0u);
            (void)writer.write_bytes(section_id_bytes);
            (void)writer.write_bytes(section.bytes);
        }

        return std::vector<std::uint8_t>(writer.data().begin(), writer.data().end());
    }

    std::optional<std::vector<save_payload_section_t>> save_service_t::load_payload_sections(
        const std::filesystem::path& path,
        const std::uint32_t expected_payload_version,
        std::string& error_detail) noexcept
    {
        const std::optional<std::vector<std::uint8_t>> bytes{ utils::file::load_binary_file(path) };
        if (!bytes.has_value())
        {
            error_detail = std::format("Save payload '{}' could not be loaded.",
                                       utils::file::to_log_string(path));
            return std::nullopt;
        }

        if (bytes->size() < 12u)
        {
            error_detail = std::format("Save payload '{}' is too small to be valid.",
                                       utils::file::to_log_string(path));
            return std::nullopt;
        }

        const std::string_view actual_magic{
            reinterpret_cast<const char*>(bytes->data()),
            k_payload_magic.size()
        };
        if (actual_magic != k_payload_magic)
        {
            error_detail = std::format("Save payload '{}' has an invalid magic header.",
                                       utils::file::to_log_string(path));
            return std::nullopt;
        }

        std::uint32_t actual_version{ 0u };
        if (!read_u32(*bytes, 4u, actual_version))
        {
            error_detail = std::format("Save payload '{}' is truncated before its version header.",
                                       utils::file::to_log_string(path));
            return std::nullopt;
        }
        if (actual_version != expected_payload_version)
        {
            error_detail = std::format("Save payload '{}' uses unsupported payload version {} (expected {}).",
                                       utils::file::to_log_string(path),
                                       actual_version,
                                       expected_payload_version);
            return std::nullopt;
        }

        std::uint32_t section_count{ 0u };
        if (!read_u32(*bytes, 8u, section_count))
        {
            error_detail = std::format("Save payload '{}' is truncated before its section table.",
                                       utils::file::to_log_string(path));
            return std::nullopt;
        }

        size_t cursor{ 12u };
        std::vector<save_payload_section_t> sections;
        sections.reserve(section_count);
        for (std::uint32_t section_index{ 0u }; section_index < section_count; ++section_index)
        {
            std::uint32_t section_id_size{ 0u };
            std::uint32_t payload_size{ 0u };
            std::uint8_t owner_bits{ 0u };
            if (!read_u32(*bytes, cursor, section_id_size)) break;
            cursor += 4u;
            if (!read_u32(*bytes, cursor, payload_size)) break;
            cursor += 4u;
            if (cursor + 4u > bytes->size() || !read_u8(*bytes, cursor, owner_bits)) break;
            cursor += 4u;

            if (cursor + section_id_size + payload_size > bytes->size())
                break;

            const std::string_view section_id{
                reinterpret_cast<const char*>(bytes->data() + cursor),
                section_id_size
            };
            cursor += section_id_size;

            save_payload_section_t section;
            section.section_id = std::string{ section_id };
            if (owner_bits == static_cast<std::uint8_t>(save_section_owner_t::engine))
                section.owner = save_section_owner_t::engine;
            else if (owner_bits == static_cast<std::uint8_t>(save_section_owner_t::gameplay))
                section.owner = save_section_owner_t::gameplay;
            else
            {
                error_detail = std::format("Save payload '{}' contains an unknown section owner value {}.",
                                           utils::file::to_log_string(path),
                                           owner_bits);
                return std::nullopt;
            }

            section.bytes.assign(bytes->begin() + static_cast<std::ptrdiff_t>(cursor),
                                 bytes->begin() + static_cast<std::ptrdiff_t>(cursor + payload_size));
            cursor += payload_size;
            sections.push_back(std::move(section));
        }

        if (sections.size() != section_count)
        {
            error_detail = std::format("Save payload '{}' is truncated while reading section data.",
                                       utils::file::to_log_string(path));
            return std::nullopt;
        }

        return sections;
    }
} // namespace carrot::save
