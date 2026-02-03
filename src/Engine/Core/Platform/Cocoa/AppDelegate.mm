//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AppDelegate.h"

#include "CocoaWindow.h"

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
    [NSApp terminate:nil];
}

- (NSWindow *)createAndReturnWindow
{
    _window = [[NSWindow alloc] initWithContentRect:_info._window_rect
                                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                backing:NSBackingStoreBuffered
                                defer:NO];

    [_window setTitle:[NSString stringWithUTF8String:_info._window_title.data()]];
    [_window center];

    [_window setAcceptsMouseMovedEvents:YES];
    [_window setRestorable:NO];

    [_window setDelegate:self];

    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    // === METAL KIT VIEW ===
    MTKView* mtk_view{ [[MTKView alloc] initWithFrame:[_window contentRectForFrameRect:[_window frame]]
                                                               device:MTLCreateSystemDefaultDevice()] };

    mtk_view.device = MTLCreateSystemDefaultDevice();             // redundant but explicit
    mtk_view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    mtk_view.depthStencilPixelFormat = MTLPixelFormatInvalid;     // no depth for now (add later if needed)
    mtk_view.sampleCount = 1;
    mtk_view.preferredFramesPerSecond = 60;                        // or 0 = uncapped
    mtk_view.clearColor = MTLClearColorMake(0.02, 0.2, 0.4, 1.0);

    // Important for polling-style engine (you call drawing manually):
    mtk_view.paused = YES;                  // prevents automatic draw calls
    mtk_view.enableSetNeedsDisplay = NO;    // we won't use setNeedsDisplay
    mtk_view.autoResizeDrawable = YES;      // automatically updates drawableSize on resize

    [_window setContentView:mtk_view];

    //WindowView* view{ [[WindowView alloc] initWithFrame:[_window contentRectForFrameRect:[_window frame]]] };
    //[_window setContentView:view];

    NSTrackingAreaOptions options{ NSTrackingMouseEnteredAndExited |
                                   NSTrackingMouseMoved |
                                   NSTrackingActiveInKeyWindow |
                                   NSTrackingInVisibleRect };

    NSTrackingArea* trackingArea{ [[NSTrackingArea alloc] initWithRect:[mtk_view bounds]
                                                          options:options
                                                          owner:mtk_view
                                                          userInfo:nil] };

    [mtk_view addTrackingArea:trackingArea];
    [trackingArea release];
    [mtk_view updateTrackingAreas];

    [mtk_view becomeFirstResponder];


    //id<MTLDevice> device{ MTLCreateSystemDefaultDevice() };
    //CAMetalLayer* layer{ [CAMetalLayer layer] };
    //layer.device = device;
    //layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    //layer.framebufferOnly = YES;
    //layer.displaySyncEnabled = YES; // vsync enabled
    //
    //view.layer = layer;
    //view.wantsLayer = YES;
    //
    //CGRect initialRect{ _info._window_rect };
    //CGFloat scale{ [_window backingScaleFactor] };          // usually from main screen if window not yet visible
    //if (scale <= 0) scale = [[NSScreen mainScreen] backingScaleFactor];
    //
    //layer.drawableSize = CGSizeMake(CGRectGetWidth(initialRect) * scale, CGRectGetHeight(initialRect) * scale);

    if (_owner) _owner->set_mtk_view(mtk_view);

    [mtk_view release];

    return _window;
}

@end