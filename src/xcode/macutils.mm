#import <Cocoa/Cocoa.h>

#define _MAXDEFSTR 260
inline char *s_strncpy(char *d, const char *s, size_t m) { strncpy(d,s,m); d[m-1] = 0; return d; };
inline char *s_strcat(char *d, const char *s) { size_t n = strlen(d); return s_strncpy(d+n,s,_MAXDEFSTR-n); };

void mac_pasteconsole(char *commandbuf)
{	
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *type = [pasteboard availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]];
    if (type != nil) {
        NSString *contents = [pasteboard stringForType:type];
        if (contents != nil)
			s_strcat(commandbuf, [contents lossyCString]);
    }
}

/*
 * 0x1030 = 10.3
 * 0x1040 = 10.4
 * 0x1050 = 10.5
 */
int mac_osversion() 
{
    SInt32 MacVersion;
    Gestalt(gestaltSystemVersion, &MacVersion);
    return MacVersion;
}