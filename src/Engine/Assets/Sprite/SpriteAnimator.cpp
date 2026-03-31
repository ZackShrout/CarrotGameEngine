//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "SpriteAnimator.h"

namespace carrot::assets {
    void sprite_animator_t::set_sprite(const loaded_sprite_asset_t* sprite) noexcept
    {
        _sprite = sprite;
        _animation = nullptr;
        reset_playback_state();
    }

    void sprite_animator_t::play(std::string_view animation_name, const bool restart_if_same/* = false*/)
    {
        if (!_sprite)
        {
            LOG_ASSET_WARN("sprite_animator_t::play('{}') called with no sprite set.", animation_name);
            return;
        }

        const sprite_animation_t* animation{ _sprite->find_animation(animation_name) };
        if (!animation)
        {
            LOG_ASSET_WARN("sprite_animator_t could not find animation '{}' on sprite.", animation_name);
            return;
        }

        if (_animation == animation && !restart_if_same)
        {
            if (_finished) return;

            _playing = true;
            _paused = false;
            return;
        }

        _animation = animation;
        _frame_index_in_animation = 0;
        _accumulated_time = 0.0f;
        _playing = true;
        _paused = false;
        _finished = false;
    }

    void sprite_animator_t::stop() noexcept
    {
        _playing = false;
        _paused = false;
        _finished = false;
        _frame_index_in_animation = 0;
        _accumulated_time = 0.f;
    }

    void sprite_animator_t::pause() noexcept
    {
        if (_playing)
            _paused = true;
    }

    void sprite_animator_t::resume() noexcept
    {
        if (_playing)
            _paused = false;
    }

    void sprite_animator_t::reset() noexcept
    {
        _frame_index_in_animation = 0;
        _accumulated_time = 0.0f;
        _paused = false;
        _finished = false;
        _playing = (_animation != nullptr);
    }

    void sprite_animator_t::update(const float delta_time) noexcept
    {
        if (!_sprite || !_animation || !_playing || _paused || _finished)
            return;

        if (delta_time <= 0.f)
            return;

        if (_animation->frame_indices.empty())
            return;

        if (_animation->fps <= 0.f)
            return;

        const float frame_duration{ 1.f / _animation->fps };
        _accumulated_time += delta_time;

        while (_accumulated_time >= frame_duration)
        {
            _accumulated_time -= frame_duration;
            const uint32_t last_frame_index{ static_cast<uint32_t>(_animation->frame_indices.size() - 1) };

            if (_frame_index_in_animation < last_frame_index)
            {
                ++_frame_index_in_animation;
                continue;
            }

            if (_animation->loop)
            {
                _frame_index_in_animation = 0;
                continue;
            }

            _frame_index_in_animation = last_frame_index;
            _finished = true;
            _playing = false;
            break;
        }
    }

    const sprite_frame_t* sprite_animator_t::current_frame() const noexcept
    {
        if (!_sprite || !_animation)
            return nullptr;

        if (_animation->frame_indices.empty())
            return nullptr;

        if (_frame_index_in_animation >= _animation->frame_indices.size())
            return nullptr;

        const uint32_t frame_index{ _animation->frame_indices[_frame_index_in_animation] };

        return _sprite->sprite().frame_at(frame_index);
    }

    void sprite_animator_t::reset_playback_state() noexcept
    {
        _frame_index_in_animation = 0;
        _accumulated_time = 0.0f;
        _playing = false;
        _paused = false;
        _finished = false;
    }
} // namespace carrot::assets
