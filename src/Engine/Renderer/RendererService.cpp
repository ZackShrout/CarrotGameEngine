//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "RendererService.h"

namespace carrot::renderer {
    void renderer_service_t::provide(renderer_t* instance)
    {
        // We still overwrite — last write wins (helps hot-reload experiments)
        if (_instance != nullptr) [[unlikely]]
                LOG_GRAPHICS_WARN("Renderer service already provided — double registration?");

        _instance = instance;
    }

    renderer_t& renderer_service_t::get()
    {
        if (_instance == nullptr) [[unlikely]]
                LOG_GRAPHICS_FATAL("Attempted to use renderer_service_t::get() before renderer_t was provided");

        return *_instance;
    }
} // namespace carrot::renderer