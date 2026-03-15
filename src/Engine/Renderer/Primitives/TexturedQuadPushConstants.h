//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

struct textured_quad_push_constants_t
{
    float offset_x;
    float offset_y;
    float scale_x;
    float scale_y;
};

static_assert(sizeof(textured_quad_push_constants_t) == 16);