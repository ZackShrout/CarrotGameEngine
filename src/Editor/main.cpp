//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "EditorApp.h"

#include <CarrotEngine.h>

int main()
{
    carrot::editor::editor_app_t app;
    carrot::engine_t engine;

    engine.init();
    return engine.run(&app);
}
