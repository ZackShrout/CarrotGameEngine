//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "WorldObject.h"

#include <vector>

namespace carrot::world {
    class world_t
    {
    public:
        [[nodiscard]] world_object_t& create_object();
        [[nodiscard]] world_object_t* find_object_by_name(std::string_view name) noexcept;
        [[nodiscard]] const world_object_t* find_object_by_name(std::string_view name) const noexcept;
        [[nodiscard]] const std::vector<world_object_t>& objects() const noexcept { return _objects; }
        [[nodiscard]] std::vector<world_object_t>& objects() noexcept { return _objects; }

    private:
        world_object_id_t _next_id{ 1 };
        std::vector<world_object_t> _objects;
    };
} // namespace carrot::world
