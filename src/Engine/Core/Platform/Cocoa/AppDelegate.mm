//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AppDelegate.h"

#include "CocoaWindow.h"

#import "CocoaContentView.h"

#include <QuartzCore/QuartzCore.h>
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>

@implementation app_delegate_t

@synthesize window = _window;
@synthesize info = _info;

- (instancetype)initWithInfo:(const carrot::core::platform::app_delegate_info_t &)info owner:(carrot::core::platform::cocoa_window_t*)owner
{
    self = [super init];
    if (self)
    {
        _info = info;
        _owner = owner;
    }
    return self;
}

- (void)dealloc
{
    [_window release];
    [super dealloc];
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    NSApplication* app{ [notification object] };
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (_owner)
    {
        _owner->set_should_close(true);
        _owner->on_window_closed(carrot::events::window_closed_t{});
    }
}

- (void)windowDidResize:(NSNotification *)notification
{
    if (!_owner || !_window) return;

    NSView* content_view{ [_window contentView] };
    if (!content_view) return;

    const NSRect bounds{ [content_view bounds] };

    const uint32_t width{ static_cast<uint32_t>(bounds.size.width) };
    const uint32_t height{ static_cast<uint32_t>(bounds.size.height) };

    _owner->update_size(width, height);
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    if (_owner)
            _owner->set_fullscreen_state(true);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    if (_owner)
            _owner->set_fullscreen_state(false);
}

- (NSWindow *)createAndReturnWindow
{
    _window = [[NSWindow alloc] initWithContentRect:_info._window_rect
                                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                backing:NSBackingStoreBuffered
                                defer:NO];
    
    [_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

    [_window setTitle:[NSString stringWithUTF8String:_info._window_title.data()]];
    [_window center];

    [_window setRestorable:NO];

    [_window setDelegate:self];

    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    
    cocoa_content_view_t* content_view{[[cocoa_content_view_t alloc] initWithFrame:[_window contentRectForFrameRect:[_window frame]] owner:_owner]};
    
    [_window setContentView:content_view];
    [content_view setWantsLayer:YES];

    CAMetalLayer* metal_layer{ [CAMetalLayer layer] };
    metal_layer.device = MTLCreateSystemDefaultDevice();
    metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    metal_layer.framebufferOnly = YES;
    metal_layer.drawableSize = CGSizeMake(content_view.bounds.size.width, content_view.bounds.size.height);

    [content_view setLayer:metal_layer];
    [_window makeFirstResponder:content_view];

    if (_owner) _owner->set_metal_layer(metal_layer);

    [content_view release];

    return _window;
}

@end
