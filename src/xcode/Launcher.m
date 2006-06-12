#import "Launcher.h"
#import "ConsoleView.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#include <unistd.h> /* unlink */
#include <util.h> /* forkpty */

#define kMaxDisplays	16
#define MAX_BUFF		2000

/* 
* Read the 'oqfrags' param, will be set to empty string if file-not-found, or param-not-found
 * @NOTE ugly, but easily expanded for other things - name, team maybe?
 * @TODO fix potential buffer overflow in writing to oqfrags
 *
 */
static void readAutoexec(const char *path, char *oqfrags)
{
	char buff[MAX_BUFF];
	FILE *file = fopen(path, "r");
	
	if (file)
	{
		while (fgets(buff, MAX_BUFF, file) != NULL)
		{
			char *p = buff;		
			
			while ((*p != '\0') && (*p < ' ')) p++; //skipwhite
			
			if (strncmp(p, "oqfrags ", 8) == 0)
			{
				p+=8;
				while ((*p != '\0') && (*p < ' ')) p++;//skipwhite
				while (*p > ' ') *(oqfrags++) = *(p++);
				break;
			} 
		}
		fclose(file);
	}
	*oqfrags = '\0';
}

/* 
* Updates the 'oqfrags' param (NULL will delete the param), will return 0 on success
 * - creates file if missing, preserves other lines if file already exists
 *
 */
static int writeAutoexec(const char *path, char *oqfrags)
{
	char *oldpath;
	//src will be non-null if the file exists
	FILE *src = fopen(path, "r");
	if(src) fclose(src);
	
	if(src)
	{
		oldpath = (char*)alloca(strlen(path+1));
		strcpy(oldpath, path);
		oldpath[strlen(oldpath)-4] = '\0';
		strcat(oldpath, ".bak");
		if(rename(path, oldpath) != 0) return -1; //can't update the file
		src = fopen(oldpath, "r");
	}
	
	FILE *dest = fopen(path, "w");
	
	if(dest)
	{
		if(src)
		{ //copy lines across into new file
			char buff[MAX_BUFF];
			BOOL lf = NO;
			while (fgets(buff, MAX_BUFF, src) != NULL)
			{
				char *p = buff;	
				int len = strlen(buff) - 1;
				lf = (len != -1) && (buff[len] == '\n');	
				while ((*p != '\0') && (*p < ' ')) p++; //skipwhite 
				if(strncmp(p, "oqfrags ", 8) == 0) continue; //skip line we want to update
				fprintf(dest, buff);
			}
			if(!lf) fprintf(dest, "\n"); //ensure there is always a final LF
		}
		if(oqfrags) fprintf(dest, "oqfrags %s\n", oqfrags);
		fclose(dest);
	}
	
	if(src)
	{
		fclose(src);
		unlink(oldpath); //delete old version
	}	
	return 0;
}

static int numberForKey(CFDictionaryRef desc, CFStringRef key)
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

- (void)serverTerminated
{
	if(server==-1) return;
	server = -1;
	[multiplayer setTitle:@"Run"];
	[console appendText:@"\n \n"];
}

- (void)setServerActive:(BOOL)start
{
	if((server==-1) != start) return;
	
	if(!start)
	{	//STOP
		
		//damn server, terminate isn't good enough for you - die, die, die...
		if((server!=-1) && (server!=0)) kill(server, SIGKILL); //@WARNING - you do not want a 0 or -1 to be accidentally sent a  kill!
		[self serverTerminated];
	} 
	else
	{	//START
		NSString *cwd = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten"];
		NSArray *opts = [[serverOptions stringValue] componentsSeparatedByString:@" "];
		
		const char *childCwd  = [cwd fileSystemRepresentation];
		const char *childPath = [[cwd stringByAppendingPathComponent:@"sauerbraten.app/Contents/MacOS/sauerbraten"] fileSystemRepresentation];
		const char **args = (const char**)malloc(sizeof(char*)*([opts count] + 3)); //3 = {path, -d, NULL}
		int i, fdm, argc = 0;
		
		args[argc++] = childPath;
		args[argc++] = "-d";

		for(i = 0; i < [opts count]; i++)
		{
			NSString *opt = [opts objectAtIndex:i];
			if([opt length] == 0) continue; //skip empty
			args[argc++] = [opt UTF8String];
		}
		
		args[argc++] = NULL;

		switch ( (server = forkpty(&fdm, NULL, NULL, NULL)) ) // forkpty so we can reliably grab SDL console
		{ 
			case -1:
				[console appendLine:@"Error - can't launch server"];
				[self serverTerminated];
				break;
			case 0: // child
				chdir(childCwd);
				execv(childPath, (char*const*)args);
				fprintf(stderr, "Error - can't launch server\n");
				_exit(0);
			default: // parent
				free(args);
				//fprintf(stderr, "fdm=%d\n", slave_name, fdm);
				[multiplayer setTitle:@"Stop"];
				
				NSFileHandle *taskOutput = [[NSFileHandle alloc] initWithFileDescriptor:fdm];
				NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
				[nc addObserver:self selector:@selector(serverDataAvailable:) name:NSFileHandleReadCompletionNotification object:taskOutput];
				[taskOutput readInBackgroundAndNotify];
				break;
		}
	}
}

- (void)serverDataAvailable:(NSNotification *)note
{
	NSFileHandle *taskOutput = [note object];
    NSData *data = [[note userInfo] objectForKey:NSFileHandleNotificationDataItem];
	
    if (data && [data length])
	{
		NSString *text = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];		
		[console appendText:text];
		[text release];					
        [taskOutput readInBackgroundAndNotify]; //wait for more data
    }
	else
	{
		NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
		[nc removeObserver:self name:NSFileHandleReadCompletionNotification object:taskOutput];
		close([taskOutput fileDescriptor]);
		[self setServerActive:NO];
	}
}

- (BOOL)playFile:(NSString*)filename
{
	NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];	
	NSMutableArray *args = [[NSMutableArray alloc] init];
	NSString *cwd = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten"];
	
	writeAutoexec([[cwd stringByAppendingPathComponent:@"autoexec.cfg"] fileSystemRepresentation], ([occlusion state] == NSOffState) ? "0" : NULL);
	
	[args addObject:[NSString stringWithFormat:@"-w%@", [res objectAtIndex:0]]];
	[args addObject:[NSString stringWithFormat:@"-h%@", [res objectAtIndex:1]]];
	
	if([fullscreen state] == NSOffState) [args addObject:@"-t"];
	if([shader state] == NSOffState) [args addObject:@"-f"];
	if(filename)
	{
		if([filename hasSuffix:@".ogz"]) filename = [filename substringToIndex:[filename length]-4];
		[args addObject:[NSString stringWithFormat:@"-l%@",filename]];
	}
	
	NSTask *task = [[NSTask alloc] init];
	[task setCurrentDirectoryPath:cwd];
	[task setLaunchPath:[cwd stringByAppendingPathComponent:@"sauerbraten.app/Contents/MacOS/sauerbraten"]];
	[task setArguments:args];
	[args release];
	
	BOOL okay = YES;
	
	NS_DURING
		[task launch];
		if(server==-1) [NSApp terminate:self];	//if there is a server then don't exit!
	NS_HANDLER
		//NSLog(@"%@", localException);
		NSRunAlertPanel(@"Error", @"Can't start Sauerbraten! Please move the directory containing Sauerbraten to a path that doesn't contain weird characters or start Sauerbraten manually.", @"OK", NULL, NULL);
		okay = NO;
	NS_ENDHANDLER
	
	return okay;
}

- (IBAction)multiplayerAction:(id)sender { [self setServerActive:(server==-1)]; }

- (IBAction)playAction:(id)sender { [self playFile:nil]; }

- (IBAction)helpAction:(id)sender
{
	NSString *file = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"README.html"];
	NSWorkspace *ws = [NSWorkspace sharedWorkspace];
	[ws openURL:[NSURL fileURLWithPath:file]];
}

- (void)awakeFromNib
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
	[occlusion setState:((strncmp(oqfrags, "0", 1) == 0) ? NSOffState : NSOnState)];
	
	[serverOptions setFocusRingType:NSFocusRingTypeNone];
	
	server = -1;
	
	[[resolutions window] setDelegate:self]; // so can catch the window close
	
	[NSApp setDelegate:self]; //so can catch the double-click & dropped files
}

-(void)windowWillClose:(NSNotification *)notification
{
	[self setServerActive:NO];
	[NSApp terminate:self];
}

//we register 'ogz' as a doc type
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	NSString *cwd = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten/packages"];
	if(![filename hasPrefix:cwd])
	{
		NSRunAlertPanel(@"Error", @"Can only load maps that are within the sauerbraten/packages/ folder.", @"OK", NULL, NULL);
		//@TODO give user option to copy it into the packages folder?
		return NO;
	}
	filename = [filename substringFromIndex:[cwd length]+1]; //+1 to skip the leading '/'
	return [self playFile:filename];
}
@end