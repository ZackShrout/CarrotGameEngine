//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetID.h"
#include "Assets/Sprite/SpriteAnimation.h"
#include "Assets/Sprite/SpriteFrame.h"

#include <chlm/CarrotHLM.h>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace carrot::assets {
    class sprite_asset_t
    {
    public:
        [[nodiscard]] std::string_view texture_id() const noexcept { return _texture_id; }

        [[nodiscard]] chlm::float2 default_pivot() const noexcept { return _default_pivot; }
        [[nodiscard]] float pixels_per_unit() const noexcept { return _pixels_per_unit; }

        [[nodiscard]] const sprite_frame_t* find_frame(std::string_view name) const noexcept;
        [[nodiscard]] const sprite_animation_t* find_animation(std::string_view name) const noexcept;

        [[nodiscard]] const sprite_frame_t* frame_at(uint32_t index) const noexcept;
        [[nodiscard]] const sprite_animation_t* animation_at(uint32_t index) const noexcept;

        [[nodiscard]] std::span<const sprite_frame_t> frames() const noexcept { return _frames; }
        [[nodiscard]] std::span<const sprite_animation_t> animations() const noexcept { return _animations; }

        void set_texture_id(std::string texture_id) { _texture_id = std::move(texture_id); }
        void set_default_pivot(const chlm::float2 pivot) noexcept { _default_pivot = pivot; }
        void set_pixels_per_unit(float pixels_per_unit) noexcept;

        void add_frame(sprite_frame_t frame) { _frames.emplace_back(std::move(frame)); }
        void add_animation(sprite_animation_t animation) { _animations.emplace_back(std::move(animation)); }

        [[nodiscard]] bool build_lookup_tables();

    private:
        std::string _texture_id;
        chlm::float2 _default_pivot{ 0.5f, 0.5f };
        float _pixels_per_unit{ 1.f };

        std::vector<sprite_frame_t> _frames;
        std::vector<sprite_animation_t> _animations;

        std::unordered_map<std::string, uint32_t> _frame_lookup;
        std::unordered_map<std::string, uint32_t> _animation_lookup;
    };

    struct sprite_asset_record_t
    {
        asset_id_t id{ 0 };
        std::string logical_id;
        std::string source_uri;
        std::string manifest_uri;
        std::uint32_t schema_version{ 1u };
        sprite_asset_t sprite;
    };
} // namespace carrot::assets
