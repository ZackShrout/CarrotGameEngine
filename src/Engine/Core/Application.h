//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "Common/CommonHeaders.h"

namespace carrot::core {
    class ce_application_t
    {
    public:
        ce_application_t() noexcept = default;
        virtual ~ce_application_t() = default;

        DISABLE_COPY_AND_MOVE(ce_application_t);

        virtual void on_tick([[maybe_unused]] const float delta_time) {}

    private:
    };
} // namespace carrot
