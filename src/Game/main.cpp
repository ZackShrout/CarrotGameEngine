//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "Game.h"

#include <CarrotEngine.h>

int main()
{
    sandbox::sandbox_t* game{ new sandbox::sandbox_t() };
    carrot::engine_t::get().run(game);

    delete game;

    return 0;
}
