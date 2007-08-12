#import "Launcher.h"
#import "ConsoleView.h"
#include <stdlib.h>
#include <unistd.h> /* _exit() */
#include <util.h> /* forkpty() */

// User default keys
#define dkVERSION @"version"
#define dkUSERDIR @"userdir"
#define dkNAME @"name"
#define dkTEAM @"team"
#define dkFULLSCREEN @"fullscreen"
#define dkFSAA @"fsaa"
#define dkSHADER @"shader"
#define dkRESOLUTION @"resolution"
#define dkADVANCEDOPTS @"advancedOptions"
#define dkSERVEROPTS @"server_options"
#define dkDESCRIPTION @"server_description"
#define dkPASSWORD @"server_password"
#define dkMAXCLIENTS @"server_maxclients"


#define kMaxDisplays	16

// unless you want strings with "(null)" in them :-/
@interface NSUserDefaults(Extras)
- (NSString*)nonNullStringForKey:(NSString*)key;
@end

@implementation NSUserDefaults(Extras)
- (NSString*)nonNullStringForKey:(NSString*)key {
    NSString *result = [self stringForKey:key];
    return (result?result:@"");
}
@end

@interface Map : NSObject {
    NSString *path;
}
@end

@implementation Map
- (id)initWithPath:(NSString*)aPath 
{
    if((self = [super init])) 
        path = [[aPath stringByDeletingPathExtension] retain];
    return self;
}
- (void)dealloc 
{
    [path release];
    [super dealloc];
}
- (NSString*)path { return path; }
- (NSString*)name { return [path lastPathComponent]; }
- (NSImage*)image 
{ 
    NSImage *image = [[NSImage alloc] initWithContentsOfFile:[path stringByAppendingString:@".jpg"]]; 
    if(!image) image = [NSImage imageNamed:@"Nomap"];
    return image;
}
- (NSString*)text 
{
    NSString *text = [NSString alloc];
    NSError *error;
    if([text respondsToSelector:@selector(initWithContentsOfFile:encoding:error:)])
        text = [text initWithContentsOfFile:[path stringByAppendingString:@".txt"] encoding:NSASCIIStringEncoding error:&error];
    else
        text = [text initWithContentsOfFile:[path stringByAppendingString:@".txt"]]; //deprecated in 10.4
    if(!text) return @"";
    return text;
}
- (void)setText:(NSString*)text { } // wtf? - damn textfield believes it's editable
- (NSString*)tickIfExists:(NSString*)ext 
{
    unichar tickCh = 0x2713; 
    return [[NSFileManager defaultManager] fileExistsAtPath:[path stringByAppendingString:ext]] ? [NSString stringWithCharacters:&tickCh length:1] : @"";
}
- (NSString*)hasImage { return [self tickIfExists:@".jpg"]; }
- (NSString*)hasText { return [self tickIfExists:@".txt"]; }
- (NSString*)hasCfg { return [self tickIfExists:@".cfg"]; }
@end



static int numberForKey(CFDictionaryRef desc, CFStringRef key) 
{
    CFNumberRef value;
    int num = 0;
    if ((value = CFDictionaryGetValue(desc, key)) == NULL)
        return 0;
    CFNumberGetValue(value, kCFNumberIntType, &num);
    return num;
}

@implementation Launcher

- (void)switchViews:(NSToolbarItem *)item 
{
    NSView *prefsView = nil;
    switch([item tag]) 
    {
        case 1: prefsView = view1; break;
        case 2: prefsView = view2; break;
        case 3: prefsView = view3; break;
        case 4: prefsView = view4; break;
        case 5: prefsView = view5; break;
            //extend as see fit...
        default: return;
    }
    
    //to stop flicker, we make a temp blank view.
    NSView *tempView = [[NSView alloc] initWithFrame:[[window contentView] frame]];
    [window setContentView:tempView];
    [tempView release];

    //if no view then keep the blank one
    if(!prefsView) prefsView = tempView;
    
    //mojo to get the right frame for the new window.
    NSRect newFrame = [window frame];
    newFrame.size.height = [prefsView frame].size.height + ([window frame].size.height - [[window contentView] frame].size.height);
    newFrame.size.width = [prefsView frame].size.width;
    newFrame.origin.y += ([[window contentView] frame].size.height - [prefsView frame].size.height);
    
    //set the frame to newFrame and animate it. 
    [window setFrame:newFrame display:YES animate:YES];
    //set the main content view to the new view we have picked through the if structure above.
    [window setContentView:prefsView];
    [window setContentMinSize:[prefsView bounds].size];
}

- (NSToolbarItem*)addToolBarItem:(NSString*)name 
{
    int n = [toolBarItems count] + 1;
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:[NSString stringWithFormat:@"%0d", n]];
    [item setTag:n];
    [item setTarget:self];
    [item setAction:@selector(switchViews:)];
    [toolBarItems setObject:item forKey:[item itemIdentifier]];
    [item setLabel:NSLocalizedString(name, @"")];
    [item setImage:[NSImage imageNamed:name]];
    [item release];
    return item;
}

- (void)initViews 
{
    toolBarItems = [[NSMutableDictionary alloc] init];
    NSToolbarItem *first = [self addToolBarItem:@"Main"];
    [self addToolBarItem:@"Maps"];
    [self addToolBarItem:@"Keys"];
    [self addToolBarItem:@"Server"];
    [self addToolBarItem:@"EisenStern"];

    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:NSToolbarFlexibleSpaceItemIdentifier];
    [toolBarItems setObject:item forKey:[item itemIdentifier]];
    [item release];

    [[self addToolBarItem:@"Help"] setAction:@selector(helpAction:)];

    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"PreferencePanes"];
    [toolbar setDelegate:self]; 
    [toolbar setAllowsUserCustomization:NO]; 
    [toolbar setAutosavesConfiguration:NO];  
    [window setToolbar:toolbar]; 
    [toolbar release];
    if([window respondsToSelector:@selector(setShowsToolbarButton:)]) [window setShowsToolbarButton:NO]; //10.4+
    
    //Make it select the first by default
    [toolbar setSelectedItemIdentifier:[first itemIdentifier]];
    [self switchViews:first]; 
}

/*
 * toolbar delegate methods
 */
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag 
{
    return [toolBarItems objectForKey:itemIdentifier];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)theToolbar 
{
    return [self toolbarDefaultItemIdentifiers:theToolbar];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)theToolbar 
{
    return [NSArray arrayWithObjects:@"1", @"2", @"3", @"4", @"5", NSToolbarFlexibleSpaceItemIdentifier, @"7", nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers: (NSToolbar *)toolbar 
{
    return [NSArray arrayWithObjects:@"1", @"2", @"3", @"4", @"5", nil];
}






- (void)addResolutionsForDisplay:(CGDirectDisplayID)dspy 
{
    CFIndex i, cnt;
    CFArrayRef modeList = CGDisplayAvailableModes(dspy);
    if(modeList == NULL) return;
    cnt = CFArrayGetCount(modeList);
    for(i = 0; i < cnt; i++) {
        CFDictionaryRef mode = CFArrayGetValueAtIndex(modeList, i);
        NSString *title = [NSString stringWithFormat:@"%i x %i", numberForKey(mode, kCGDisplayWidth), numberForKey(mode, kCGDisplayHeight)];
        if(![resolutions itemWithTitle:title]) [resolutions addItemWithTitle:title];
    }	
}

- (void)initResolutions 
{
    CGDirectDisplayID display[kMaxDisplays];
    CGDisplayCount numDisplays;
    [resolutions removeAllItems];
    if(CGGetActiveDisplayList(kMaxDisplays, display, &numDisplays) == CGDisplayNoErr) 
    {
        CGDisplayCount i;
        for (i = 0; i < numDisplays; i++)
            [self addResolutionsForDisplay:display[i]];
    }
    [resolutions selectItemAtIndex: [[NSUserDefaults standardUserDefaults] integerForKey:dkRESOLUTION]];	
}




/* directory where the executable lives */
-(NSString *)cwd 
{
    return [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"sauerbraten"];
}

/* directory where user files are kept */
- (NSString*)userdir {
    if(![[NSUserDefaults standardUserDefaults] boolForKey:dkUSERDIR]) return [self cwd];
    // /Users/<name>/Application Support/sauerbraten
    FSRef folder;
    NSString *path = nil;
    if(FSFindFolder(kUserDomain, kApplicationSupportFolderType, NO, &folder) == noErr) {
        CFURLRef url = CFURLCreateFromFSRef(kCFAllocatorDefault, &folder);
        path = [(NSURL *)url path];
        CFRelease(url);
        path = [path stringByAppendingPathComponent:@"sauerbraten"];
    }
    return path;
}

/* build key array from config data */
-(NSArray *)getKeys:(NSDictionary *)dict 
{	
    NSMutableArray *arr = [[NSMutableArray alloc] init];
    NSEnumerator *e = [dict keyEnumerator];
    NSString *key;
    while ((key = [e nextObject])) 
    {
        NSString *trig;
        if([key hasPrefix:@"editbind"]) 
            trig = [key substringFromIndex:9];
        else if([key hasPrefix:@"bind"]) 
            trig = [key substringFromIndex:5];
        else 
            continue;
        [arr addObject:[NSDictionary dictionaryWithObjectsAndKeys: //keys used in nib
            trig, @"key",
            [key hasPrefix:@"editbind"]?@"edit":@"", @"mode",
            [dict objectForKey:key], @"action",
            nil]];
    }
    return arr;
}


/*
 * extract a dictionary from the config files containing:
 * - name, team, gamma strings
 * - bind/editbind '.' key strings
 */
-(NSDictionary *)readConfigFiles 
{
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    [dict setObject:@"" forKey:@"name"]; //ensure these entries are never nil
    [dict setObject:@"" forKey:@"team"]; 
    
    NSString *files[] = {@"config.cfg", @"autoexec.cfg"};
    int i;
    for(i = 0; i < sizeof(files)/sizeof(NSString*); i++) 
    {
        NSString *file = [[self userdir] stringByAppendingPathComponent:files[i]];
        
        NSArray *lines = [[NSString stringWithContentsOfFile:file] componentsSeparatedByString:@"\n"];
        
        if(i==0 && !lines)  // ugh - special case when first run...
        { 
            file = [[self cwd] stringByAppendingPathComponent:@"data/defaults.cfg"];
            lines = [[NSString stringWithContentsOfFile:file] componentsSeparatedByString:@"\n"];
        }
		
        NSString *line; 
        NSEnumerator *e = [lines objectEnumerator];
        while(line = [e nextObject]) 
        {
            NSRange r; // more flexible to do this manually rather than via NSScanner...
            int j = 0;
            while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
            r.location = j;
            while(j < [line length] && [line characterAtIndex:j] > ' ') j++; //until white
            r.length = j - r.location;
            NSString *type = [line substringWithRange:r];
			
            while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
            if(j < [line length] && [line characterAtIndex:j] == '"') 
            {
                r.location = ++j;
                while(j < [line length] && [line characterAtIndex:j] != '"') j++; //until close quote
                r.length = (j++) - r.location;
            } else {
                r.location = j;
                while(j < [line length] && [line characterAtIndex:j] > ' ') j++; //until white
                r.length = j - r.location;
            }
            NSString *value = [line substringWithRange:r];
            
            while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
            NSString *remainder = [line substringFromIndex:j];
			
            if([type isEqual:@"name"] || [type isEqual:@"team"] || [type isEqual:@"gamma"]) 
                [dict setObject:value forKey:type];
            else if([type isEqual:@"bind"] || [type isEqual:@"editbind"]) 
                [dict setObject:remainder forKey:[NSString stringWithFormat:@"%@.%@", type,value]];
        }
    }
    return dict;
}

- (void)serverTerminated
{
    if(server==-1) return;
    server = -1;
    [multiplayer setTitle:@"Start"];
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
        NSString *cwd = [self cwd];
        NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
                       
        NSArray *opts = [[defs nonNullStringForKey:dkSERVEROPTS] componentsSeparatedByString:@" "];
		
        const char *childCwd  = [cwd fileSystemRepresentation];
        const char *childPath = [[cwd stringByAppendingPathComponent:@"sauerbraten.app/Contents/MacOS/sauerbraten"] fileSystemRepresentation];
        const char **args = (const char**)malloc(sizeof(char*)*([opts count] + 3 + 4)); //3 = {path, -d, NULL}, and +4 again for optional settings...
        int i, fdm, argc = 0;
		
        args[argc++] = childPath;
        args[argc++] = "-d";
        
        for(i = 0; i < [opts count]; i++)
        {
            NSString *opt = [opts objectAtIndex:i];
            if([opt length] == 0) continue; //skip empty
            args[argc++] = [opt UTF8String];
        }
        
        NSString *desc = [defs nonNullStringForKey:dkDESCRIPTION];
        if (![desc isEqualToString:@""]) args[argc++] = [[NSString stringWithFormat:@"-n%@", desc] UTF8String];
        
        NSString *pass = [defs nonNullStringForKey:dkPASSWORD];
        if (![pass isEqualToString:@""]) args[argc++] = [[NSString stringWithFormat:@"-p%@", pass] UTF8String];
		
        int clients = [defs integerForKey:dkMAXCLIENTS];
        if (clients > 0) args[argc++] = [[NSString stringWithFormat:@"-c%d", clients] UTF8String];
        
        if([defs boolForKey:dkUSERDIR]) args[argc++] = [[NSString stringWithFormat:@"-q%@", [self userdir]] UTF8String];
        
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

/*
 * nil will just launch the fps game
 * "-rpg" will launch the rpg demo
 * otherwise we are specifying a map to play
 */
- (BOOL)playFile:(id)filename 
{	
    NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
    
    NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];	
    NSMutableArray *args = [[NSMutableArray alloc] init];
    NSString *cwd = [self cwd];
	
    [args addObject:[NSString stringWithFormat:@"-w%@", [res objectAtIndex:0]]];
    [args addObject:[NSString stringWithFormat:@"-h%@", [res objectAtIndex:1]]];
    [args addObject:@"-z32"]; //otherwise seems to have a fondness to use -z16 which looks crap
	
    if([defs integerForKey:dkFULLSCREEN] == 0) [args addObject:@"-t"];
    [args addObject:[NSString stringWithFormat:@"-a%d", [defs integerForKey:dkFSAA]]];
    [args addObject:[NSString stringWithFormat:@"-f%d", [defs integerForKey:dkSHADER]]];
    
    if([defs boolForKey:dkUSERDIR]) [args addObject:[NSString stringWithFormat:@"-q%@", [self userdir]]];

    NSMutableArray *cmds = [[NSMutableArray alloc] init];
    NSString *name = [defs nonNullStringForKey:dkNAME];
    if(name) [cmds addObject:[NSString stringWithFormat:@"name \"%@\"", name]];
    //NOTEAM name = [defs nonNullStringForKey:dkTEAM];
    //NOTEAM if(name) [cmds addObject:[NSString stringWithFormat:@"team \"%@\"", name]];
    
    if(filename) 
    {
        if([filename isEqual:@"-rpg"]) {
            [cmds removeAllObjects]; // rpg current doesn't require name/team
            [args addObject:@"-grpg"]; //demo the rpg game
        } else if([filename hasPrefix:@"-x"])
            [cmds addObject:[filename substringFromIndex:2]];
        else
            [args addObject:[NSString stringWithFormat:@"-l%@",filename]];
	}
    
    if([cmds count] > 0) 
    {
        NSString *script = [cmds objectAtIndex:0];
        int i;
        for(i = 1; i < [cmds count]; i++) script = [NSString stringWithFormat:@"%@;%@", script, [cmds objectAtIndex:i]];
        [args addObject:[NSString stringWithFormat:@"-x%@", script]];
    }
    
    NSString *adv = [defs nonNullStringForKey:dkADVANCEDOPTS];
    if(![adv isEqual:@""]) [args addObjectsFromArray:[adv componentsSeparatedByString:@" "]];
    
    NSTask *task = [[NSTask alloc] init];
    [task setCurrentDirectoryPath:cwd];
    [task setLaunchPath:[cwd stringByAppendingPathComponent:@"sauerbraten.app/Contents/MacOS/sauerbraten"]];
    [task setArguments:args];
    [task setEnvironment:[NSDictionary dictionaryWithObjectsAndKeys: 
        @"1", @"SDL_SINGLEDISPLAY",
        @"1", @"SDL_ENABLEAPPEVENTS", nil
        ]]; // makes Command-H, Command-M and Command-Q work at least when not in fullscreen
    [args release];
    [cmds release];
	
    BOOL okay = YES;
	
    NS_DURING
        [task launch];
        if(server==-1) [NSApp terminate:self];	//if there is a server then don't exit!
    NS_HANDLER
        //NSLog(@"%@", localException);
        NSBeginCriticalAlertSheet(
            @"Can't start Sauerbraten", nil, nil, nil,
            window, nil, nil, nil, nil,
            @"Please move the directory containing Sauerbraten to a path that doesn't contain weird characters or start Sauerbraten manually.");
        okay = NO;
    NS_ENDHANDLER
        
    return okay;
}


- (void)initMaps:(NSString*)dir //@note threaded!
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *file;
    dir = [dir stringByAppendingPathComponent:@"packages"];
    NSDirectoryEnumerator *enumerator = [[NSFileManager defaultManager] enumeratorAtPath:dir];
    while(file = [enumerator nextObject]) 
    {
        if([[file pathExtension] isEqualToString: @"ogz"]) 
            [self performSelectorOnMainThread:@selector(addMap:) withObject:[dir stringByAppendingPathComponent:file] waitUntilDone:NO];
    }
    [pool release];
    //@todo - update a progress indicator - not unless silly numbers of files, or incredibly slow mac..
}

-(void)addMap:(NSString*)path 
{
    [maps addObject:[[Map alloc] initWithPath:path]]; 
}



- (void)awakeFromNib 
{
    [self initViews];
    [window setBackgroundColor:[NSColor colorWithDeviceRed:0.90 green:0.90 blue:0.90 alpha:1.0]]; //Apples 'mercury' crayon color
	
    NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
    NSFileManager *fm = [NSFileManager defaultManager];
    
    NSString *appVersion = [[[NSBundle bundleForClass:[self class]] infoDictionary] objectForKey:@"CFBundleVersion"];
	NSString *version = [defs stringForKey:dkVERSION];
	if(!version || ![version isEqual:appVersion]) 
    {
		NSLog(@"Upgraded Version...");
        //need to flush lurking config files - they're automatically generated, so no big deal...
        [fm removeFileAtPath:[[self userdir] stringByAppendingPathComponent:@"init.cfg"] handler:nil];
        [fm removeFileAtPath:[[self userdir] stringByAppendingPathComponent:@"config.cfg"] handler:nil];
    }
	[defs setObject:appVersion forKey:dkVERSION];
    
    NSDictionary *dict = [self readConfigFiles];
    [keys addObjects:[self getKeys:dict]];
    
    if([[defs nonNullStringForKey:dkNAME] isEqual:@""]) 
    {
        NSString *name = [dict objectForKey:@"name"];
        if([name isEqual:@""] || [name isEqual:@"unnamed"]) name = NSUserName();
        [defs setValue:name forKey:dkNAME];
    }
    //NOTEAM if([[defs nonNullStringForKey:dkTEAM] isEqual:@""]) [defs setValue:[dict objectForKey:@"team"] forKey:dkTEAM];
    
    NSString *dir = [self cwd];
    if(![fm fileExistsAtPath:dir]) NSLog(@"Missing sauebraten?!"); // @TODO error indicator?
    else if(![fm isWritableFileAtPath:dir]) [defs setBool:YES forKey:dkUSERDIR];  // auto-enable userdir
	
    [NSThread detachNewThreadSelector: @selector(initMaps:) toTarget:self withObject:[self cwd]];
    if([defs boolForKey:dkUSERDIR]) [NSThread detachNewThreadSelector: @selector(initMaps:) toTarget:self withObject:[self userdir]];
    
    [self initResolutions];
    server = -1;
    [NSApp setDelegate:self]; //so can catch the double-click, dropped files, termination
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:self andSelector:@selector(getUrl:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
}

#pragma mark -
#pragma mark application delegate

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

- (void)applicationWillTerminate: (NSNotification *)note {
    [self setServerActive:NO];
}


//we register 'ogz' and 'dmo' as doc types
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename 
{
    NSString *dirs[] = {[self cwd], [self userdir]};
    BOOL demo = [filename hasSuffix:@".dmo"];
    if(!demo && ![filename hasSuffix:@".ogz"]) return NO;
    filename = [filename substringToIndex:[filename length]-4]; //chop off extension
    int i;
    for(i = 0; i < 2; i++) {
        NSString *pkg = dirs[i];
        if(!demo) pkg = [pkg stringByAppendingPathComponent:@"packages"];
        if([filename hasPrefix:pkg]) {
            filename = [filename substringFromIndex:[pkg length]+1]; //+1 to skip the leading '/'
            return [self playFile:demo?[NSString stringWithFormat:@"-xdemo %@", filename]:filename];
        }
    }
    NSBeginCriticalAlertSheet(
        @"Invalid file location", @"Ok", @"Cancel", nil,
        window, self, @selector(openPackageFolder:returnCode:contextInfo:), nil, nil,
        @"Can only load files that are within the sauerbraten/packages/ folder. Do you want to show this folder?");
    //@TODO give user option to copy it into the packages folder?
    return NO;
}

- (void)openPackageFolder:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo 
{
    if(returnCode == 0) return;
    [self openUserdir:nil]; //close enough... otherwise need to ensure that sauerbraten/packages folder exists
}

//we register 'sauerbraten' as a url scheme
- (void)getUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSURL *url = [NSURL URLWithString:[[event paramDescriptorForKeyword:keyDirectObject] stringValue]];
    if(!url) return;
    [self playFile:[NSString stringWithFormat:@"-xconnect %@", [url host]]]; 
}

#pragma mark -
#pragma mark interface actions

- (IBAction)multiplayerAction:(id)sender 
{ 
    [window makeFirstResponder:window]; //ensure fields are exited and committed
    [self setServerActive:(server==-1)]; 
}

- (IBAction)playAction:(id)sender 
{ 
    [window makeFirstResponder:window]; //ensure fields are exited and committed
    [self playFile:nil]; 
}

- (IBAction)playRpg:(id)sender 
{ 
    [self playFile:@"-rpg"]; 
}

- (IBAction)playMap:(id)sender
{
    NSArray *sel = [maps selectedObjects];
    if(sel && [sel count] > 0) [self playFile:[[sel objectAtIndex:0] path]];
}

- (IBAction)helpAction:(id)sender 
{
    NSString *file = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"README.html"];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:file]];
}

- (IBAction)openUserdir:(id)sender 
{
    NSString *dir = [self userdir];
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:dir]) [fm createDirectoryAtPath:dir attributes:nil]; //ensure there is a folder to open
    [[NSWorkspace sharedWorkspace] openFile:dir];
}

@end
