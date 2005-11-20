#import "Launcher.h"
#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#define kMaxDisplays	16

NSMutableArray *array;
	
static int numberForKey( CFDictionaryRef desc, CFStringRef key )
{
    CFNumberRef value;
    int num = 0;

    if ( (value = CFDictionaryGetValue(desc, key)) == NULL )
        return 0;
    CFNumberGetValue(value, kCFNumberIntType, &num);
    return num;
}


static void listModes(CGDirectDisplayID dspy)
{
    CFArrayRef modeList;
    CFDictionaryRef mode;
    CFIndex i, cnt;

    modeList = CGDisplayAvailableModes(dspy);
    if ( modeList == NULL )
    {
        printf( "Display is invalid\n" );
        return;
    }
    cnt = CFArrayGetCount(modeList);
    //printf( "Display 0x%x: %d modes:\n", (unsigned int)dspy, (int)cnt );
    for ( i = 0; i < cnt; ++i )
    {
        mode = CFArrayGetValueAtIndex(modeList, i);
        [array addObject:[NSString stringWithFormat:@"%i x %i", numberForKey(mode, kCGDisplayWidth), numberForKey(mode, kCGDisplayHeight)]];

    }
}

@implementation MyObject

- (IBAction)playAction:(id)sender
{
	NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];

	NSString *cmd = [NSString stringWithFormat:@"cd %@; sauerbraten.app/Contents/MacOS/sauerbraten -w%@ -h%@ %@&",
			[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent],
			[res objectAtIndex:0],
			[res objectAtIndex:1],
			[fullscreen state] == NSOffState ? @"-t " : @""];

	if ([cmd canBeConvertedToEncoding:[NSString defaultCStringEncoding]])
	{
		system([cmd cString]);
		exit(0);
	}
	else
	{
		NSRunAlertPanel(@"Error", @"Can't start Sauerbraten! Either move the directory containing this application to a path that doesn't contain weird characters or start Sauerbraten manually.", @"OK", NULL, NULL);

	}
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    CGDirectDisplayID display[kMaxDisplays];
    CGDisplayCount numDisplays;
    CGDisplayCount i;
    CGDisplayErr err;

	array = [[NSMutableArray alloc] init];
	
    err = CGGetActiveDisplayList(kMaxDisplays,
                                 display,
                                 &numDisplays);
    if ( err != CGDisplayNoErr )
    {
        printf("Cannot get displays (%d)\n", err);
        exit( 1 );
    }
    
    //printf( "%d displays found\n", (int)numDisplays );
    for ( i = 0; i < numDisplays; ++i )
        listModes(display[i]);
	
	[resolutions removeAllItems];
	
	int h;
	for (h = 0; h < [array count]; h++)
	{
		id haha = [resolutions itemWithTitle:[array objectAtIndex:h]];
		if (haha == nil)
			[resolutions addItemWithTitle:[array objectAtIndex:h]];
	}
}
@end
