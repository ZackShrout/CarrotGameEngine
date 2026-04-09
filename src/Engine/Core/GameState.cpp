//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameState.h"

#include "GameContext.h"
#include "GameRuntime.h"

namespace carrot::core {
    igame_state_t::igame_state_t(game_runtime_t& runtime) noexcept
        : _runtime(runtime)
    {
    }

    game_context_t& igame_state_t::game() noexcept
    {
        return _runtime.game();
    }

    const game_context_t& igame_state_t::game() const noexcept
    {
        return _runtime.game();
    }
} // namespace carrot::core
