//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::renderer {
    class renderer_t;

    class renderer_service_t
    {
    public:
        static void provide(renderer_t* instance);
        [[nodiscard]] static renderer_t& get();
        [[nodiscard]] static renderer_t* try_get() noexcept { return _instance; }
        static void reset() noexcept { _instance = nullptr; }
        [[nodiscard]] static bool is_provided() noexcept { return _instance != nullptr; }

    private:
        inline static renderer_t* _instance{ nullptr };
    };
} // namespace carrot::renderer
