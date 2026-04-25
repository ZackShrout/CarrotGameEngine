//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameplayRuntimeState.h"

#include "Assets/Tilemap/TypedObjectConventions.h"
#include "Utils/File/FileUtils.h"

#include <cstring>

namespace sandbox {
    namespace {
        constexpr std::string_view k_container_open_flag{ "opened" };
        constexpr std::string_view k_gameplay_durable_state_magic{ "SGSV" };

        [[nodiscard]] bool read_u32(const std::span<const std::uint8_t> bytes,
                                    const size_t offset,
                                    std::uint32_t& value) noexcept
        {
            if (offset + sizeof(std::uint32_t) > bytes.size())
                return false;

            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return true;
        }

        [[nodiscard]] bool read_string(const std::span<const std::uint8_t> bytes,
                                       size_t& cursor,
                                       std::string& value) noexcept
        {
            std::uint32_t size{ 0u };
            if (!read_u32(bytes, cursor, size))
                return false;
            cursor += sizeof(std::uint32_t);
            if (cursor + size > bytes.size())
                return false;

            value.assign(reinterpret_cast<const char*>(bytes.data() + cursor), size);
            cursor += size;
            return true;
        }

        void write_string(carrot::utils::file::binary_blob_writer_t& writer, const std::string_view value)
        {
            (void)writer.write_u32(static_cast<std::uint32_t>(value.size()));
            (void)writer.write_bytes(std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(value.data()),
                value.size()
            });
        }
    } // namespace

    void capture_player_runtime_state(gameplay_runtime_state_t& runtime_state,
                                      const carrot::world::player_controller_t& player_controller) noexcept
    {
        if (!player_controller.controlled_object())
            return;

        runtime_state.player_facing = player_controller.facing_direction();
    }

    void apply_runtime_state_to_player(const gameplay_runtime_state_t& runtime_state,
                                       carrot::world::player_controller_t& player_controller) noexcept
    {
        if (!runtime_state.player_facing)
            return;

        player_controller.set_facing_direction(*runtime_state.player_facing);
    }

    void mark_container_open(gameplay_runtime_state_t& runtime_state,
                             const std::string_view scene_id,
                             const carrot::world::world_object_t& container)
    {
        runtime_state.scene_flags.mark(scene_id, container, k_container_open_flag);
    }

    bool is_container_open(const gameplay_runtime_state_t& runtime_state,
                           const std::string_view scene_id,
                           const carrot::world::world_object_t& container)
    {
        return runtime_state.scene_flags.contains(scene_id, container, k_container_open_flag);
    }

    void apply_runtime_state_to_scene(const std::string_view scene_id,
                                      carrot::world::world_t& world,
                                      const gameplay_runtime_state_t& runtime_state)
    {
        (void)carrot::world::apply_scene_runtime_flag_to_matching_objects(
            scene_id,
            world,
            runtime_state.scene_flags,
            k_container_open_flag,
            [](const carrot::world::world_object_t& object)
            {
                return carrot::assets::as_typed_container(object).has_value();
            },
            [](carrot::world::world_object_t& object)
            {
                carrot::world::set_world_object_bool_property(object, "interactable", false);
            }
        );
    }

    std::optional<std::vector<std::uint8_t>> serialize_durable_state(
        const gameplay_durable_state_t& durable_state) noexcept
    {
        carrot::utils::file::binary_blob_writer_t writer;
        (void)writer.write_bytes(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(k_gameplay_durable_state_magic.data()),
            k_gameplay_durable_state_magic.size()
        });
        (void)writer.write_u32(1u);
        write_string(writer, durable_state.scene_id);
        write_string(writer, durable_state.spawn_marker);
        (void)writer.write_u8(static_cast<std::uint8_t>(durable_state.runtime_state.player_facing.has_value()));
        (void)writer.write_u8(durable_state.runtime_state.player_facing.has_value()
                                  ? static_cast<std::uint8_t>(*durable_state.runtime_state.player_facing)
                                  : 0u);
        (void)writer.write_u8(0u);
        (void)writer.write_u8(0u);

        const std::vector<std::string> flag_keys{ durable_state.runtime_state.scene_flags.keys() };
        (void)writer.write_u32(static_cast<std::uint32_t>(flag_keys.size()));
        for (const std::string& key : flag_keys)
            write_string(writer, key);

        return std::vector<std::uint8_t>(writer.data().begin(), writer.data().end());
    }

    std::optional<gameplay_durable_state_t> deserialize_durable_state(const std::span<const std::uint8_t> bytes) noexcept
    {
        if (bytes.size() < 12u)
            return std::nullopt;

        const std::string_view magic{ reinterpret_cast<const char*>(bytes.data()), k_gameplay_durable_state_magic.size() };
        if (magic != k_gameplay_durable_state_magic)
            return std::nullopt;

        std::uint32_t version{ 0u };
        if (!read_u32(bytes, 4u, version) || version != 1u)
            return std::nullopt;

        size_t cursor{ 8u };
        gameplay_durable_state_t state;
        if (!read_string(bytes, cursor, state.scene_id) || state.scene_id.empty())
            return std::nullopt;
        if (!read_string(bytes, cursor, state.spawn_marker))
            return std::nullopt;
        if (cursor + 4u > bytes.size())
            return std::nullopt;

        const bool has_facing{ bytes[cursor] != 0u };
        const std::uint8_t facing_bits{ bytes[cursor + 1u] };
        cursor += 4u;
        if (has_facing)
        {
            if (facing_bits > static_cast<std::uint8_t>(carrot::world::facing_direction_t::right))
                return std::nullopt;
            state.runtime_state.player_facing = static_cast<carrot::world::facing_direction_t>(facing_bits);
        }

        std::uint32_t flag_count{ 0u };
        if (!read_u32(bytes, cursor, flag_count))
            return std::nullopt;
        cursor += sizeof(std::uint32_t);

        std::vector<std::string> flag_keys;
        flag_keys.reserve(flag_count);
        for (std::uint32_t index{ 0u }; index < flag_count; ++index)
        {
            std::string key;
            if (!read_string(bytes, cursor, key))
                return std::nullopt;
            flag_keys.push_back(std::move(key));
        }

        state.runtime_state.scene_flags.replace_keys(flag_keys);
        return state;
    }
} // namespace sandbox
