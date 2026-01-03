//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

namespace sandbox {
    class sandbox_t : public carrot::core::ce_application_t
    {
        void on_tick([[maybe_unused]] float delta_time) override;
    };
} // namespace sandbox
