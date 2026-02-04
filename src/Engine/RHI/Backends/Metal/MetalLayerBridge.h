//
// Created by Zack Shrout on 2/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void* metal_create_layer(
    void* ns_view,
    void* mtl_device,
    uint32_t width,
    uint32_t height);

void metal_destroy_layer(void* layer);
void metal_resize_layer(void* layer, uint32_t width, uint32_t height);
void* metal_next_drawable(void* layer);
void metal_release_drawable(void* drawable);
void* metal_layer_get_device(void* layer);

#ifdef __cplusplus
}
#endif
