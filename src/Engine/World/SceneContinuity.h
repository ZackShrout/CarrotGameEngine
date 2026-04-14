//
// Created by Zack Shrout on 4/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "WorldObject.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace carrot::world {
    class world_t;

    [[nodiscard]] std::string build_runtime_object_identity(const world_object_t& object);
    [[nodiscard]] std::string make_scene_runtime_object_key(std::string_view scene_id,
                                                            const world_object_t& object);
    [[nodiscard]] std::string make_scene_runtime_object_flag_key(std::string_view scene_id,
                                                                 const world_object_t& object,
                                                                 std::string_view flag_name);
    void set_world_object_bool_property(world_object_t& object,
                                        std::string_view property_name,
                                        bool value);

    class scene_runtime_flag_store_t
    {
    public:
        void mark(std::string_view scene_id,
                  const world_object_t& object,
                  std::string_view flag_name);
        [[nodiscard]] bool contains(std::string_view scene_id,
                                    const world_object_t& object,
                                    std::string_view flag_name) const;
        void clear() noexcept;
        [[nodiscard]] size_t size() const noexcept { return _flags.size(); }

    private:
        std::unordered_set<std::string> _flags;
    };

    using continuity_object_predicate_t = std::function<bool(const world_object_t&)>;
    using continuity_object_callback_t = std::function<void(world_object_t&)>;

    size_t apply_scene_runtime_flag_to_matching_objects(std::string_view scene_id,
                                                        world_t& world,
                                                        const scene_runtime_flag_store_t& flags,
                                                        std::string_view flag_name,
                                                        const continuity_object_predicate_t& predicate,
                                                        const continuity_object_callback_t& callback);
} // namespace carrot::world
