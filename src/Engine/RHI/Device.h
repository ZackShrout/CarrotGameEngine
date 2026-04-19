//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::rhi {
    class rhi_device_t
    {
    public:
        virtual ~rhi_device_t() = default;

        /**
         * Backend-owned low-level device object.
         *
         * Carrot's current live renderer path no longer builds its practical
         * contract around `rhi_device_t`. Frame recording, resource creation,
         * and presentation management primarily happen through `rhi_context_t`.
         *
         * Concrete backends may still expose additional helper methods on their
         * device classes, but those are backend-local details rather than part
         * of a shared renderer-facing factory contract.
         */
    };
} // namespace carrot::rhi
