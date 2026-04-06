#pragma once

#include "World/Controllers/PlayerController.h"
#include "World/World.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace sandbox {
    struct gameplay_runtime_state_t
    {
        std::optional<carrot::world::facing_direction_t> player_facing;
        std::unordered_set<std::string> opened_containers;
    };

    [[nodiscard]] std::string make_scene_runtime_object_key(std::string_view scene_id,
                                                            const carrot::world::world_object_t& object);
    void capture_player_runtime_state(gameplay_runtime_state_t& runtime_state,
                                      const carrot::world::player_controller_t& player_controller) noexcept;
    void apply_runtime_state_to_player(const gameplay_runtime_state_t& runtime_state,
                                       carrot::world::player_controller_t& player_controller) noexcept;
    void mark_container_open(gameplay_runtime_state_t& runtime_state,
                             std::string_view scene_id,
                             const carrot::world::world_object_t& container);
    [[nodiscard]] bool is_container_open(const gameplay_runtime_state_t& runtime_state,
                                         std::string_view scene_id,
                                         const carrot::world::world_object_t& container);
    void apply_runtime_state_to_scene(std::string_view scene_id,
                                      carrot::world::world_t& world,
                                      const gameplay_runtime_state_t& runtime_state);
} // namespace sandbox
