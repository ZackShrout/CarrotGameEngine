//
// Created by Zack Shrout on 2/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "MetalLayerBridge.h"

#include "Core/Logger.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

extern "C" {

void* metal_create_layer(void* ns_view, void* mtl_device, uint32_t width, uint32_t height)
{
    NSView* view{ static_cast<NSView*>(ns_view) };
    id<MTLDevice> device{ static_cast<id<MTLDevice>>(mtl_device) };

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
    return static_cast<void*>(layer);
}

void metal_set_layer_pixel_format_srgb(void* layer)
{
    CAMetalLayer* metal_layer{ static_cast<CAMetalLayer*>(layer) };

    if (!metal_layer)
        return;
    
    metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
}

void metal_destroy_layer(void* layer)
{
    CAMetalLayer* metal_layer{ static_cast<CAMetalLayer*>(layer) };
    [metal_layer release];
}

void metal_resize_layer(void* layer, uint32_t width, uint32_t height)
{
    CAMetalLayer* metal_layer{ static_cast<CAMetalLayer*>(layer) };
    CGFloat scale{ metal_layer.contentsScale };
    metal_layer.drawableSize = CGSizeMake(width * scale, height * scale);
}

void* metal_next_drawable(void* layer)
{
    CAMetalLayer* metal_layer{ static_cast<CAMetalLayer*>(layer) };
    id<CAMetalDrawable> drawable{ [metal_layer nextDrawable] };

    if (!drawable) return nullptr;

    [drawable retain];
    return static_cast<void*>(drawable);
}

void metal_release_drawable(void* drawable)
{
    id<CAMetalDrawable> d{ static_cast<id<CAMetalDrawable>>(drawable) };
    [d release];
}

void* metal_layer_get_device(void* layer)
{
    CAMetalLayer* metal_layer{ static_cast<CAMetalLayer*>(layer) };
    id<MTLDevice> device{ metal_layer.device };

    return static_cast<void*>(device); // not retained; device lifetime is global
}

void metal_add_command_buffer_completion_signal(void* command_buffer, void* semaphore) noexcept
{
    id<MTLCommandBuffer> cb{ static_cast<id<MTLCommandBuffer>>(command_buffer) };
    dispatch_semaphore_t sem{ static_cast<dispatch_semaphore_t>(semaphore) };

    [cb addCompletedHandler:^(id<MTLCommandBuffer>) {
        dispatch_semaphore_signal(sem);
    }];
}

void metal_present_drawable(void* command_buffer, void* drawable) noexcept
{
    id<MTLCommandBuffer> cb{ static_cast<id<MTLCommandBuffer>>(command_buffer) };
    id<CAMetalDrawable> d{ static_cast<id<CAMetalDrawable>>(drawable) };

    if (!cb || !d)
        return;

    [cb presentDrawable:d];
}

uint64_t metal_texture_resource_id(void* texture) noexcept
{
    id<MTLTexture> tex{ static_cast<id<MTLTexture>>(texture) };
    
    if (!tex)
        return 0;

    return tex.gpuResourceID._impl;
}

uint64_t metal_sampler_resource_id(void* sampler) noexcept
{
    id<MTLSamplerState> samp{ static_cast<id<MTLSamplerState>>(sampler) };
    
    if (!samp)
        return 0;
    
    return samp.gpuResourceID._impl;
}

uint64_t metal_buffer_gpu_address(void* buffer) noexcept
{
    id<MTLBuffer> buff{ static_cast<id<MTLBuffer>>(buffer) };
    
    if (!buff)
        return 0;
    
    return buff.gpuAddress;
}


}
