//
// Created by Zack Shrout on 2/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "MetalLayerBridge.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

extern "C" {

void* metal_create_layer(
    void* ns_view,
    void* mtl_device,
    uint32_t width,
    uint32_t height)
{
    NSView* view{ (NSView*)ns_view };
    id<MTLDevice> device{ (id<MTLDevice>)mtl_device };

    view.wantsLayer = YES;

    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;

    CGFloat scale{ view.window.backingScaleFactor };
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(width * scale, height * scale);

    view.layer = layer;

    [layer retain];
    return (void*)layer;
}

void metal_destroy_layer(void* layer)
{
    CAMetalLayer* metal_layer{ (CAMetalLayer*)layer };
    [metal_layer release];
}

void metal_resize_layer(void* layer, uint32_t width, uint32_t height)
{
    CAMetalLayer* metal_layer{ (CAMetalLayer*)layer };
    CGFloat scale{ metal_layer.contentsScale };
    metal_layer.drawableSize = CGSizeMake(width * scale, height * scale);
}

void* metal_next_drawable(void* layer)
{
    CAMetalLayer* metal_layer{ (CAMetalLayer*)layer };
    id<CAMetalDrawable> drawable{ [metal_layer nextDrawable] };

    if (!drawable) return nullptr;

    [drawable retain];
    return (void*)drawable;
}

void metal_release_drawable(void* drawable)
{
    id<CAMetalDrawable> d{ (id<CAMetalDrawable>)drawable };
    [d release];
}

void* metal_layer_get_device(void* layer)
{
    CAMetalLayer* metal_layer{ (CAMetalLayer*)layer };
    id<MTLDevice> device{ metal_layer.device };

    return (void*)device; // not retained; device lifetime is global
}

}
