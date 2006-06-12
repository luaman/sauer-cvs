#import <Cocoa/Cocoa.h>

@class ConsoleView;

@interface Launcher : NSObject
{
    IBOutlet NSPopUpButton *resolutions;
    IBOutlet NSButton *fullscreen;
    IBOutlet NSButton *shader;
	IBOutlet NSButton *occlusion;
	
	IBOutlet NSButton *multiplayer;
	IBOutlet ConsoleView *console;
	IBOutlet NSTextField *serverOptions;
	
@private
	pid_t server;
}

- (IBAction)playAction:(id)sender;

- (IBAction)multiplayerAction:(id)sender;

- (IBAction)helpAction:(id)sender;


@end