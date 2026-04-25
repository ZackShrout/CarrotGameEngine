//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::save {
    enum class save_slot_kind_t : std::uint8_t
    {
        manual = 0,
        autosave,
        temp
    };

    enum class save_request_kind_t : std::uint8_t
    {
        none = 0,
        save_slot,
        load_slot
    };

    enum class save_request_outcome_t : std::uint8_t
    {
        none = 0,
        pending,
        succeeded,
        failed
    };

    enum class save_failure_reason_t : std::uint8_t
    {
        none = 0,
        missing,
        invalid,
        corrupt,
        incompatible,
        io_error
    };

    [[nodiscard]] std::string_view to_string(save_slot_kind_t kind) noexcept;
    [[nodiscard]] std::string_view to_string(save_request_kind_t kind) noexcept;
    [[nodiscard]] std::string_view to_string(save_request_outcome_t outcome) noexcept;
    [[nodiscard]] std::string_view to_string(save_failure_reason_t reason) noexcept;

    enum class save_section_owner_t : std::uint8_t
    {
        engine = 0,
        gameplay
    };

    [[nodiscard]] std::string_view to_string(save_section_owner_t owner) noexcept;

    struct save_slot_metadata_t
    {
        std::string slot_id;
        save_slot_kind_t slot_kind{ save_slot_kind_t::manual };
        std::uint64_t timestamp_utc_unix_seconds{ 0u };
        std::string scene_id;
        std::string scene_label;
        std::string spawn_marker;
        double playtime_seconds{ 0.0 };
        std::uint32_t save_format_version{ 1u };
        std::uint32_t payload_format_version{ 1u };
        std::string build_compatibility{ "carrot-dev" };
        std::string payload_file{ "payload.bin" };
        std::uint64_t payload_size_bytes{ 0u };
    };

    struct save_request_t
    {
        save_request_kind_t kind{ save_request_kind_t::none };
        std::string slot_id;
        save_slot_kind_t slot_kind{ save_slot_kind_t::manual };
        std::string scene_id;
        std::string scene_label;
        std::string spawn_marker;
        double playtime_seconds{ 0.0 };
    };

    struct save_slot_summary_t
    {
        std::string slot_id;
        std::filesystem::path native_path;
        save_slot_metadata_t metadata;
    };

    struct save_payload_section_t
    {
        std::string section_id;
        save_section_owner_t owner{ save_section_owner_t::engine };
        std::vector<std::uint8_t> bytes;
    };

    class save_section_collector_t
    {
    public:
        [[nodiscard]] bool add_section(std::string_view section_id,
                                       save_section_owner_t owner,
                                       std::span<const std::uint8_t> bytes,
                                       std::string& error_detail);

        [[nodiscard]] const std::vector<save_payload_section_t>& sections() const noexcept { return _sections; }

    private:
        std::vector<save_payload_section_t> _sections;
    };

    struct loaded_save_slot_t
    {
        save_slot_metadata_t metadata;
        std::vector<save_payload_section_t> sections;

        [[nodiscard]] const save_payload_section_t* find_section(std::string_view section_id) const noexcept;
        [[nodiscard]] std::string describe_sections() const;
    };

    struct save_operation_status_t
    {
        save_request_kind_t request_kind{ save_request_kind_t::none };
        save_request_outcome_t outcome{ save_request_outcome_t::none };
        save_failure_reason_t failure_reason{ save_failure_reason_t::none };
        std::string slot_id;
        std::string detail;

        [[nodiscard]] bool finished() const noexcept
        {
            return outcome == save_request_outcome_t::succeeded ||
                   outcome == save_request_outcome_t::failed;
        }
    };

    class isave_participant_t
    {
    public:
        virtual ~isave_participant_t() = default;

        [[nodiscard]] virtual std::string_view participant_name() const noexcept = 0;
        [[nodiscard]] virtual save_section_owner_t owner() const noexcept = 0;
        virtual bool capture_save_sections(const save_request_t& request,
                                           save_section_collector_t& collector,
                                           save_operation_status_t& status) = 0;
        virtual bool apply_loaded_sections(const loaded_save_slot_t& slot,
                                           save_operation_status_t& status) = 0;
    };

    class save_participant_registry_t
    {
    public:
        void add(isave_participant_t& participant) { _participants.push_back(std::ref(participant)); }
        [[nodiscard]] const std::vector<std::reference_wrapper<isave_participant_t>>& participants() const noexcept
        {
            return _participants;
        }

    private:
        std::vector<std::reference_wrapper<isave_participant_t>> _participants;
    };

    class save_service_t
    {
    public:
        explicit save_service_t(io::virtual_file_system_t& vfs) noexcept
            : _vfs(vfs) {}

        [[nodiscard]] bool configured() const noexcept;
        [[nodiscard]] std::optional<std::filesystem::path> save_root_path() const noexcept;
        [[nodiscard]] std::optional<std::filesystem::path> slots_root_path() const noexcept;
        [[nodiscard]] std::vector<save_slot_summary_t> list_slots() const;
        [[nodiscard]] save_operation_status_t execute(const save_request_t& request) const;
        [[nodiscard]] save_operation_status_t write_slot(const save_request_t& request,
                                                        std::span<const save_payload_section_t> sections) const;
        [[nodiscard]] std::optional<loaded_save_slot_t> load_slot(std::string_view slot_id,
                                                                  save_operation_status_t& status) const;
        [[nodiscard]] std::optional<save_slot_metadata_t> read_slot_metadata(std::string_view slot_id) const;

        [[nodiscard]] static bool is_valid_slot_id(std::string_view slot_id) noexcept;

    private:
        [[nodiscard]] save_operation_status_t save_slot(const save_request_t& request,
                                                        std::span<const save_payload_section_t> sections) const;
        [[nodiscard]] std::optional<std::filesystem::path> ensure_slots_root() const noexcept;
        [[nodiscard]] static std::filesystem::path slot_path_for(const std::filesystem::path& slots_root,
                                                                std::string_view slot_id);
        [[nodiscard]] static std::filesystem::path slot_metadata_path_for(const std::filesystem::path& slot_path);
        [[nodiscard]] static std::filesystem::path slot_payload_path_for(const std::filesystem::path& slot_path,
                                                                         std::string_view payload_file);
        [[nodiscard]] static std::optional<save_slot_metadata_t> load_metadata_from_file(
            const std::filesystem::path& path) noexcept;
        [[nodiscard]] static std::optional<save_slot_metadata_t> load_metadata_for_load(
            const std::filesystem::path& path,
            std::string_view slot_id,
            save_operation_status_t& status) noexcept;
        [[nodiscard]] static std::string serialize_metadata_to_json(const save_slot_metadata_t& metadata);
        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> build_payload_container(
            const save_slot_metadata_t& metadata,
            std::span<const save_payload_section_t> sections) noexcept;
        [[nodiscard]] static std::optional<std::vector<save_payload_section_t>> load_payload_sections(
            const std::filesystem::path& path,
            std::uint32_t expected_payload_version,
            std::string& error_detail) noexcept;

        io::virtual_file_system_t& _vfs;
    };
} // namespace carrot::save
