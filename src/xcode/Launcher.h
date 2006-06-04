#import <Cocoa/Cocoa.h>

@interface Launcher : NSObject
{
    IBOutlet NSPopUpButton *resolutions;
    IBOutlet NSButtonCell *fullscreen;
    IBOutlet NSButtonCell *shader;
	IBOutlet NSButtonCell *occlusion;
}

- (IBAction)playAction:(id)sender;

@end