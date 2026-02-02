//
// Created by Zack Shrout on 1/30/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AppDelegate.h"

@implementation app_delegate_t

@synthesize window = _window;
@synthesize info = _info;

- (instancetype)initWithInfo:(const carrot::core::platform::app_delegate_info_t &)info
{
    self = [super init];
    if (self) {
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
    NSApplication *app = [notification object];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    _window = [[NSWindow alloc] initWithContentRect:_info._window_rect
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];

    [_window setTitle:[NSString stringWithUTF8String:_info._window_title.data()]];
    [_window center];
    [_window makeKeyAndOrderFront:nil];

    NSApplication *app = [notification object];
    [app activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

@end