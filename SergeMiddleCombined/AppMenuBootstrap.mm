// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// Standalone app only. Builds the macOS main menu in code, so the app does
// not need a compiled MainMenu.nib (ibtool needs full Xcode). It recreates
// what iPlug2's MainMenu.xib provides: an app menu whose items carry the
// SWELL command tags (About 40005, Preferences 40006, Quit 40007), and the
// SWELLAppController that the nib would have instantiated.
#if defined(APP_API) && defined(__APPLE__)
#import <Cocoa/Cocoa.h>

@interface SWELLAppController : NSObject
- (IBAction)onSysMenuCommand:(id)sender;
@end

static SWELLAppController* gSMController = nil;

@interface SMAppMenuBootstrap : NSObject
@end

@implementation SMAppMenuBootstrap
+ (void)load
{
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(willFinish:)
                                               name:NSApplicationWillFinishLaunchingNotification
                                             object:nil];
}

+ (NSMenuItem*)item:(NSString*)title tag:(NSInteger)tag key:(NSString*)key
{
  NSMenuItem* it = [[NSMenuItem alloc] initWithTitle:title action:@selector(onSysMenuCommand:) keyEquivalent:key];
  [it setTag:tag];
  [it setTarget:gSMController];
  return it;
}

+ (void)willFinish:(NSNotification*)note
{
  if (gSMController) return;
  gSMController = [[NSClassFromString(@"SWELLAppController") alloc] init];

  NSString* name = @"Serge Middle";
  NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
  NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:name action:nil keyEquivalent:@""];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:name];
  [appMenu addItem:[self item:[@"About " stringByAppendingString:name] tag:40005 key:@""]];
  [appMenu addItem:[NSMenuItem separatorItem]];
  [appMenu addItem:[self item:@"Preferences\u2026" tag:40006 key:@","]];
  [appMenu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* hide = [[NSMenuItem alloc] initWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
  [appMenu addItem:hide];
  [appMenu addItem:[NSMenuItem separatorItem]];
  [appMenu addItem:[self item:[@"Quit " stringByAppendingString:name] tag:40007 key:@"q"]];
  [appItem setSubmenu:appMenu];
  [mainMenu addItem:appItem];
  [NSApp setMainMenu:mainMenu];

  // Same entry point the nib would trigger.
  [gSMController performSelector:@selector(awakeFromNib)];
}
@end
#endif
