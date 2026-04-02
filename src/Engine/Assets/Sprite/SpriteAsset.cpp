//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SpriteAsset.h"

namespace carrot::assets {
    const sprite_frame_t* sprite_asset_t::find_frame(const std::string_view name) const noexcept
    {
        const auto it{ _frame_lookup.find(std::string(name)) };
        if (it == _frame_lookup.end())
            return nullptr;

        const uint32_t index{ it->second };
        if (index >= _frames.size())
            return nullptr;

        return &_frames[index];
    }

    const sprite_animation_t* sprite_asset_t::find_animation(std::string_view name) const noexcept
    {
        const auto it{ _animation_lookup.find(std::string(name)) };
        if (it == _animation_lookup.end())
            return nullptr;

        const uint32_t index{ it->second };
        if (index >= _animations.size())
            return nullptr;

        return &_animations[index];
    }

    const sprite_frame_t* sprite_asset_t::frame_at(uint32_t index) const noexcept
    {
        if (index >= _frames.size())
            return nullptr;

        return &_frames[index];
    }

    const sprite_animation_t* sprite_asset_t::animation_at(uint32_t index) const noexcept
    {
        if (index >= _animations.size())
            return nullptr;

        return &_animations[index];
    }

    void sprite_asset_t::set_pixels_per_unit(float pixels_per_unit) noexcept
    {
        if (pixels_per_unit <= 0.f)
        {
            LOG_ASSET_WARN("Attempted to set sprite pixels_per_unit to {}. Falling back to 1.f.", pixels_per_unit);
            _pixels_per_unit = 1.f;

            return;
        }

        _pixels_per_unit = pixels_per_unit;
    }

    bool sprite_asset_t::build_lookup_tables()
    {
        _frame_lookup.clear();
        _animation_lookup.clear();

        _frame_lookup.reserve(_frames.size());
        _animation_lookup.reserve(_animations.size());

        for (uint32_t i{ 0 }; i < static_cast<uint32_t>(_frames.size()); ++i)
        {
            const sprite_frame_t& frame{ _frames[i] };

            if (frame.name.empty())
            {
                LOG_ASSET_ERROR("Sprite asset contains a frame with an empty name.");
                return false;
            }

            const auto [it, inserted]{ _frame_lookup.emplace(frame.name, i) };
            if (!inserted)
            {
                LOG_ASSET_ERROR("Duplicate sprite frame name detected: '{}'.", frame.name);
                return false;
            }

            if (chlm::empty(frame.pixel_rect))
            {
                LOG_ASSET_WARN("Sprite frame '{}' has an empty pixel rect.", frame.name);
            }
        }

        for (uint32_t i{ 0 }; i < static_cast<uint32_t>(_animations.size()); ++i)
        {
            const sprite_animation_t& animation{ _animations[i] };

            if (animation.name.empty())
            {
                LOG_ASSET_ERROR("Sprite asset contains an animation with an empty name.");
                return false;
            }

            const auto [it, inserted]{ _animation_lookup.emplace(animation.name, i) };
            if (!inserted)
            {
                LOG_ASSET_ERROR("Duplicate sprite animation name detected: '{}'.", animation.name);
                return false;
            }

            if (animation.frames.empty())
            {
                LOG_ASSET_WARN("Sprite animation '{}' has no frames.", animation.name);
                continue;
            }

            for (const sprite_animation_frame_t& anim_frame: animation.frames)
            {
                if (anim_frame.frame_index >= _frames.size())
                {
                    LOG_ASSET_ERROR("Sprite animation '{}' references invalid frame index {}. Frame count is {}.",
                                    animation.name, anim_frame.frame_index, _frames.size());
                    return false;
                }

                if (anim_frame.duration_seconds <= 0.0f)
                {
                    LOG_ASSET_ERROR("Sprite animation '{}' contains invalid frame duration {}.", animation.name,
                                    anim_frame.duration_seconds);
                    return false;
                }
            }
        }

        if (_texture_id.empty())
        {
            LOG_ASSET_WARN("Sprite asset was built without a texture id.");
        }

        if (_pixels_per_unit <= 0.f)
        {
            LOG_ASSET_WARN("Sprite asset has invalid pixels_per_unit {}. Resetting to 1.f.", _pixels_per_unit);
            _pixels_per_unit = 1.f;
        }

        return true;
    }
} // namespace carrot::assets
