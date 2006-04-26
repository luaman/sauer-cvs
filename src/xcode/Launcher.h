#import <Cocoa/Cocoa.h>

@interface Launcher : NSObject
{
    IBOutlet NSPopUpButton *resolutions;
    IBOutlet NSButtonCell *fullscreen;
    IBOutlet NSButtonCell *shader;
}

- (IBAction)playAction:(id)sender;

@end