//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Sprite/LoadedSpriteAsset.h"

#include <cstdint>
#include <string_view>

namespace carrot::assets {
    class sprite_animator_t
    {
    public:
        void set_sprite(const loaded_sprite_asset_t* sprite) noexcept;

        void play(std::string_view animation_name, bool restart_if_same = false);
        void stop() noexcept;
        void pause() noexcept;
        void resume() noexcept;
        void reset() noexcept;

        void update(float delta_time) noexcept;

        [[nodiscard]] const loaded_sprite_asset_t* sprite() const noexcept { return _sprite; }
        [[nodiscard]] const sprite_animation_t* current_animation() const noexcept { return _animation; }
        [[nodiscard]] const sprite_frame_t* current_frame() const noexcept;

        [[nodiscard]] bool is_playing() const noexcept { return _playing; }
        [[nodiscard]] bool is_paused() const noexcept { return _paused; }
        [[nodiscard]] bool is_finished() const noexcept { return _finished; }

    private:
        void reset_playback_state() noexcept;

        const loaded_sprite_asset_t* _sprite{ nullptr };
        const sprite_animation_t* _animation{ nullptr };

        uint32_t _frame_index_in_animation{ 0 };
        float _accumulated_time{ 0.0f };

        bool _playing{ false };
        bool _paused{ false };
        bool _finished{ false };
    };
} // namespace carrot::assets
