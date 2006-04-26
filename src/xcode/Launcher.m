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
    CFDictionaryRef mode;
    CFIndex i, cnt;
    CFArrayRef modeList = CGDisplayAvailableModes(dspy);
    
	if ( modeList == NULL )
		exit(1);
	
    cnt = CFArrayGetCount(modeList);

    for ( i = 0; i < cnt; ++i )
    {
        mode = CFArrayGetValueAtIndex(modeList, i);
        [array addObject:[NSString stringWithFormat:@"%i x %i", numberForKey(mode, kCGDisplayWidth), numberForKey(mode, kCGDisplayHeight)]];
    }
}

@implementation Launcher

- (IBAction)playAction:(id)sender
{
	NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];
	NSString *cmd = [NSString stringWithFormat:@"cd %@; sauerbraten.app/Contents/MacOS/sauerbraten -w%@ -h%@%@%@&",
			[[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten"],
			[res objectAtIndex:0],
			[res objectAtIndex:1],
			[fullscreen state] == NSOffState ? @" -t" : @"",
			[shader state] == NSOffState ? @" -f" : @""];

	[[NSUserDefaultsController sharedUserDefaultsController] commitEditing];
		
	if ([cmd canBeConvertedToEncoding:[NSString defaultCStringEncoding]])
	{
		system([cmd cString]);
		[[NSApplication sharedApplication] terminate:self];
	}
	else
		NSRunAlertPanel(@"Error", @"Can't start Sauerbraten! Please move the directory containing Sauerbraten to a path that doesn't contain weird characters or start Sauerbraten manually.", @"OK", NULL, NULL);
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    CGDirectDisplayID display[kMaxDisplays];
    CGDisplayCount numDisplays;
    CGDisplayCount i;
    CGDisplayErr err;
	int h;
	
	array = [[NSMutableArray alloc] init];
	
    err = CGGetActiveDisplayList(kMaxDisplays, display, &numDisplays);
   
	if ( err != CGDisplayNoErr )
        exit(1);
    
    for ( i = 0; i < numDisplays; ++i )
        listModes(display[i]);
	
	[resolutions removeAllItems];
	
	for (h = 0; h < [array count]; h++)
	{
		if ([resolutions itemWithTitle:[array objectAtIndex:h]] == nil)
			[resolutions addItemWithTitle:[array objectAtIndex:h]];
	}
	
	[resolutions selectItemAtIndex: [[NSUserDefaults standardUserDefaults] integerForKey:@"resolution"]];
}
@end