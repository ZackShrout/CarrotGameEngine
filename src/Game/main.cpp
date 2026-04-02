//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Game.h"

#include <CarrotEngine.h>

int main()
{
    sandbox::sandbox_t game{ };
    carrot::engine_t engine{ };

    engine.init();
    return engine.run(&game);
}
