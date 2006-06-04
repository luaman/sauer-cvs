#import "Launcher.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#include <unistd.h> /* unlink */

#define kMaxDisplays	16



/* 
 * Read the 'oqfrags' param, will be set to empty string if file-not-found, or param-not-found
 * @NOTE ugly, but easily expanded for other things - name, team maybe?
 * @TODO fix potential buffer overflow in writing to oqfrags
 *
 */
#define MAX_BUFF 2000
static void readAutoexec(const char *path, char *oqfrags) {
	char buff[MAX_BUFF];
	FILE *file = fopen(path, "r");
	if(file) {
		while(fgets(buff, MAX_BUFF, file) != NULL) {
			char *p = buff;		
			while((*p != '\0') && (*p < ' ')) p++; //skipwhite
			if(strncmp(p, "oqfrags ", 8) == 0) {
				p+=8;
				while((*p != '\0') && (*p < ' ')) p++;//skipwhite
				while(*p > ' ') *(oqfrags++) = *(p++);
				break;
			} 
		}
		fclose(file);
	}
	*oqfrags = '\0';
}

/* 
 * Updates the 'oqfrags' param (NULL will delete the param), will return 0 on success
 * - creates file if missing, preserves other line if file already exists
 *
 */
static int writeAutoexec(const char *path, char *oqfrags) {
	//src will be non-null if the file exists
	FILE *src = fopen(path, "r");
	if(src) fclose(src);
	
	char *oldpath;
	if(src) {
		oldpath = (char*)alloca(strlen(path+1));
		strcpy(oldpath, path);
		oldpath[strlen(oldpath)-4] = '\0';
		strcat(oldpath, ".bak");
		if(rename(path, oldpath) != 0) return -1; //can't update the file
		src = fopen(oldpath, "r");
	}
	FILE *dest = fopen(path, "w");
	
	if (dest)
	{
		if(src) { //copy lines across into new file
			char buff[MAX_BUFF];
			BOOL lf = NO;
			while(fgets(buff, MAX_BUFF, src) != NULL) {
				char *p = buff;	
				int len = strlen(buff) - 1;
				lf = (len != -1) && (buff[len] == '\n');	
				while((*p != '\0') && (*p < ' ')) p++; //skipwhite 
				if(strncmp(p, "oqfrags ", 8) == 0) continue; //skip line we want to update
				fprintf(dest, buff);
			}
			if(!lf) fprintf(dest, "\n"); //ensure there is always a final LF
		}
		
		if(oqfrags) fprintf(dest, "oqfrags %s\n", oqfrags);
		fclose(dest);
	}
	
	if(src) {
		fclose(src);
		unlink(oldpath); //delete old version
	}	
	return 0;
}

static int numberForKey( CFDictionaryRef desc, CFStringRef key )
{
    CFNumberRef value;
    int num = 0;

    if ( (value = CFDictionaryGetValue(desc, key)) == NULL )
        return 0;
		
    CFNumberGetValue(value, kCFNumberIntType, &num);
	
    return num;
}


@implementation Launcher

- (void)addResolutionsForDisplay:(CGDirectDisplayID)dspy 
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
        NSString *title = [NSString stringWithFormat:@"%i x %i", numberForKey(mode, kCGDisplayWidth), numberForKey(mode, kCGDisplayHeight)];
		if([resolutions itemWithTitle:title] == nil)
			[resolutions addItemWithTitle:title];
    }	
}


- (IBAction)playAction:(id)sender
{
	
	NSString *cwd = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten"];
	
	writeAutoexec([[cwd stringByAppendingPathComponent:@"autoexec.cfg"] fileSystemRepresentation], ([occlusion state] == NSOffState)?"0":NULL);
	
	NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];
	NSMutableArray *args = [[NSMutableArray alloc] init];
	[args addObject:[NSString stringWithFormat:@"-w%@", [res objectAtIndex:0]]];
	[args addObject:[NSString stringWithFormat:@"-h%@", [res objectAtIndex:1]]];
	if([fullscreen state] == NSOffState) [args addObject:@"-t"];
	if([shader state] == NSOffState) [args addObject:@"-f"];

	NSTask *task = [[NSTask alloc] init];
	[task setCurrentDirectoryPath:cwd];
	[task setLaunchPath:[cwd stringByAppendingPathComponent:@"sauerbraten.app/Contents/MacOS/sauerbraten"]];
	[task setArguments:args];
	[args release];
	
	NS_DURING
		[task launch];
		// should I kill self now or later? Choosing now. 
		
		//[task waitUntilExit];
		//int status = [task terminationStatus];
		[NSApp terminate:self];	
	NS_HANDLER
		//NSLog(@"%@", localException);
		NSRunAlertPanel(@"Error", @"Can't start Sauerbraten! Please move the directory containing Sauerbraten to a path that doesn't contain weird characters or start Sauerbraten manually.", @"OK", NULL, NULL);
	NS_ENDHANDLER
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    CGDirectDisplayID display[kMaxDisplays];
    CGDisplayCount numDisplays;
    CGDisplayCount i;
    CGDisplayErr err;
	
    err = CGGetActiveDisplayList(kMaxDisplays, display, &numDisplays);
   
	if ( err != CGDisplayNoErr )
        exit(1);
    
	[resolutions removeAllItems];
    for ( i = 0; i < numDisplays; ++i )
        [self addResolutionsForDisplay:display[i]];
	[resolutions selectItemAtIndex: [[NSUserDefaults standardUserDefaults] integerForKey:@"resolution"]];
	
	
	char oqfrags[50];
	NSString *cwd = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten"];
	readAutoexec([[cwd stringByAppendingPathComponent:@"autoexec.cfg"] fileSystemRepresentation], oqfrags);
	[(NSMatrix*)[occlusion controlView] selectCellAtRow:((strncmp(oqfrags, "0", 1) == 0)?1:0)  column:0];
	
	
	[[resolutions window] setDelegate:self]; // so can catch the window close
}

-(void)windowWillClose:(NSNotification *)notification 
{
	[NSApp terminate:self];
}

@end