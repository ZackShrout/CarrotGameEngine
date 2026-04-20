//
// Created by Zack Shrout on 4/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#define CARROT_EFFECT_MODE_NONE 0u
#define CARROT_EFFECT_MODE_BATTLE_SWIRL_OUT 1u
#define CARROT_EFFECT_MODE_BATTLE_SWIRL_IN 2u

#ifdef __cplusplus
namespace carrot::renderer {
    inline constexpr float k_effect_mode_none{ static_cast<float>(CARROT_EFFECT_MODE_NONE) };
    inline constexpr float k_effect_mode_battle_swirl_out{ static_cast<float>(CARROT_EFFECT_MODE_BATTLE_SWIRL_OUT) };
    inline constexpr float k_effect_mode_battle_swirl_in{ static_cast<float>(CARROT_EFFECT_MODE_BATTLE_SWIRL_IN) };
} // namespace carrot::renderer
#endif
