//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AppDelegate.h"

#include "WindowView.h"

@implementation app_delegate_t

@synthesize window = _window;
@synthesize info = _info;

- (instancetype)initWithInfo:(const carrot::core::platform::app_delegate_info_t &)info
{
    self = [super init];
    if (self)
    {
        _info = info;
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

    WindowView* view{ [[WindowView alloc] initWithFrame:[_window contentRectForFrameRect:[_window frame]]] };
    [_window setContentView:view];

    NSTrackingAreaOptions options{ NSTrackingMouseEnteredAndExited |
                                   NSTrackingMouseMoved |
                                   NSTrackingActiveInKeyWindow |
                                   NSTrackingInVisibleRect };

    NSTrackingArea* trackingArea{ [[NSTrackingArea alloc] initWithRect:[view bounds]
                                                          options:options
                                                          owner:view
                                                          userInfo:nil] };

    [view addTrackingArea:trackingArea];
    [trackingArea release];

    [view updateTrackingAreas];

    [view becomeFirstResponder];

    [view release];

    return _window;
}

@end