// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "pch.h"
#include "engine.h"
#ifndef WIN32
#include <dirent.h>
#endif

void itoa(char *s, int i) { s_sprintf(s)("%d", i); };
char *exchangestr(char *o, char *n) { delete[] o; return newstring(n); };

typedef hashtable<char *, ident> identtable;

identtable *idents = NULL;        // contains ALL vars/commands/aliases

vector<char *> mapnames;          // contains all legal mapnames in packages/base/

void clear_command()
{
    enumerate(idents, ident, i, if(i->_type==ID_ALIAS) { DELETEA(i->_name); DELETEA(i->_action); });
    if(idents) idents->clear();
};

void alias(char *name, char *action)
{
    ident *b = idents->access(name);
    if(!b) 
    {
        name = newstring(name);
        ident b(ID_ALIAS, name, 0, 0, 0, 0, 0, newstring(action), true, NULL);
        idents->access(name, &b);
    }
    else
    {
        if(b->_type==ID_ALIAS) b->_action = exchangestr(b->_action, action);
        else conoutf("cannot redefine builtin %s with an alias", name);
    };
};

COMMAND(alias, ARG_2STR);

// variable's and commands are registered through globals, see cube.h

int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist)
{
    if(!idents) idents = new identtable;
    ident v(ID_VAR, name, min, max, storage, (void *)fun, 0, 0, persist, NULL);
    idents->access(name, &v);
    return cur;
};

void setvar(char *name, int i) { *idents->access(name)->_storage = i; };
int getvar(char *name) { return *idents->access(name)->_storage; };
bool identexists(char *name) { return idents->access(name)!=NULL; };

char *getalias(char *name)
{
    ident *i = idents->access(name);
    return i && i->_type==ID_ALIAS ? i->_action : NULL;
};

bool addcommand(char *name, void (*fun)(), int narg)
{
    if(!idents) idents = new identtable;
    ident c(ID_COMMAND, name, 0, 0, 0, (void *)fun, narg, 0, false, NULL);
    idents->access(name, &c);
    return false;
};

void addident(char *name, ident *id)
{
    if(!idents) idents = new identtable;
    idents->access(name, id);
};

static vector<char> wordbuf;

char *parseexp(char *&p, int right);

#define savealias(name, body) \
{ \
    string oldaction; \
    char *hasalias = getalias(name); \
    if(hasalias) s_strcpy(oldaction, hasalias); \
    else oldaction[0] = '\0'; \
    body; \
    alias(name, oldaction); \
}

void domacro(char *&p)
{
    savealias("s", 
        alias("s", "");
        char *macro = parseexp(p, ']');
        execute(macro);
        char *sub = getalias("s");
        if(sub) while(*sub) wordbuf.add(*sub++);
    );
};

char *parseexp(char *&p, int right)          // parse any nested set of () or []
{
    int pos = wordbuf.length(), left = *p++;
    for(int brak = 1; brak; )
    {
        int c = *p++;
        if(c=='\r') continue;               // hack
        if(c=='@')
        {
            if(*p=='[')
            {
                domacro(p);
                continue;
            };
            char *ident = p;
            while(isalnum(*p)) p++;
            c = *p;
            *p = 0;
            char *alias = getalias(ident);
            *p = c;
            if(alias)
            {
                while(*alias) wordbuf.add(*alias++);
            }
            else
            {
                ident--;
                while(ident!=p) wordbuf.add(*ident++);
            };
            continue;
        };
        if(c==left) brak++;
        else if(c==right) brak--;
        else if(!c) { p--; conoutf("missing \"%c\"", right); wordbuf.setsize(pos); return NULL; };
        wordbuf.add(c);
    };
    wordbuf.pop();
    char *s = newstring(wordbuf.getbuf()+pos, wordbuf.length()-pos);
    if(left=='(')
    {
        string t;
        itoa(t, execute(s));                    // evaluate () exps directly, and substitute result
        s = exchangestr(s, t);
    };
    wordbuf.setsize(pos);
    return s;
};

char *lookup(char *n)                           // find value of ident referenced with $ in exp
{
    ident *id = idents->access(n+1);
    if(id) switch(id->_type)
    {
        case ID_VAR: string t; itoa(t, *(id->_storage)); return exchangestr(n, t);
        case ID_ALIAS: return exchangestr(n, id->_action);
    };
    conoutf("unknown alias lookup: %s", n+1);
    return n;
};

char *parseword(char *&p)                       // parse single argument, including expressions
{
    p += strspn(p, " \t\r");
    if(p[0]=='/' && p[1]=='/') p += strcspn(p, "\n\0");  
    if(*p=='\"')
    {
        p++;
        char *word = p;
        p += strcspn(p, "\"\r\n\0");
        char *s = newstring(word, p-word);
        if(*p=='\"') p++;
        return s;
    };
    if(*p=='(') return parseexp(p, ')');
    if(*p=='[') return parseexp(p, ']');
    char *word = p;
    p += strcspn(p, "; \t\r\n\0");
    if(p-word==0) return NULL;
    char *s = newstring(word, p-word);
    if(*s=='$') return lookup(s);               // substitute variables
    return s;
};

void conc(char **w, int n, char *r, const char *sep = " ")
{
    r[0] = 0;
    for(int i = 0; i<n; i++)       
    {
        s_strcat(r, w[i]);  // make string-list out of all arguments
        if(i==n-1) break;
        if(sep) s_strcat(r, sep);
    };
};

int execute(char *p, bool isdown)               // all evaluation happens here, recursively
{
    const int MAXWORDS = 25;                    // limit, remove
    char *w[MAXWORDS];
    int val = 0;
    for(bool cont = true; cont;)                // for each ; seperated statement
    {
        int numargs = MAXWORDS;
        loopi(MAXWORDS)                         // collect all argument values
        {
            w[i] = "";
            if(i>numargs) continue;
            char *s = parseword(p);             // parse and evaluate exps
            if(!s) { numargs = i; s = ""; };
            w[i] = s;
        };
        
        p += strcspn(p, ";\n\0");
        cont = *p++!=0;                         // more statements if this isn't the end of the string
        char *c = w[0];
        if(*c=='/') c++;                        // strip irc-style command prefix
        if(!*c) continue;                       // empty statement
        
        ident *id = idents->access(c);
        if(!id)
        {
            val = atoi(c);
            if(!val && *c!='0')
                conoutf("unknown command: %s", c);
        }
        else switch(id->_type)
        {
            case ID_ICOMMAND:
                switch(id->_narg)
                {
                    default: if(isdown) id->run(w+1); break;
                    case IARG_BOTH: id->run((char **)isdown); break;
                    case IARG_CONC:  if(isdown) { string r; conc(w+1, numargs-1, r); char *rr = r; id->run(&rr); }; break;
                };
                break;
        
            case ID_COMMAND:                    // game defined commands       
                switch(id->_narg)                // use very ad-hoc function signature, and just call it
                { 
                    case ARG_1INT: if(isdown) ((void (__cdecl *)(int))id->_fun)(atoi(w[1])); break;
                    case ARG_2INT: if(isdown) ((void (__cdecl *)(int, int))id->_fun)(atoi(w[1]), atoi(w[2])); break;
                    case ARG_3INT: if(isdown) ((void (__cdecl *)(int, int, int))id->_fun)(atoi(w[1]), atoi(w[2]), atoi(w[3])); break;
                    case ARG_4INT: if(isdown) ((void (__cdecl *)(int, int, int, int))id->_fun)(atoi(w[1]), atoi(w[2]), atoi(w[3]), atoi(w[4])); break;
                    case ARG_NONE: if(isdown) ((void (__cdecl *)())id->_fun)(); break;
                    case ARG_1STR: if(isdown) ((void (__cdecl *)(char *))id->_fun)(w[1]); break;
                    case ARG_2STR: if(isdown) ((void (__cdecl *)(char *, char *))id->_fun)(w[1], w[2]); break;
                    case ARG_3STR: if(isdown) ((void (__cdecl *)(char *, char *, char *))id->_fun)(w[1], w[2], w[3]); break;
                    case ARG_4STR: if(isdown) ((void (__cdecl *)(char *, char *, char *, char *))id->_fun)(w[1], w[2], w[3], w[4]); break;
                    case ARG_5STR: if(isdown) ((void (__cdecl *)(char *, char *, char *, char *, char *))id->_fun)(w[1], w[2], w[3], w[4], w[5]); break;
                    case ARG_6STR: if(isdown) ((void (__cdecl *)(char *, char *, char *, char *, char *, char *))id->_fun)(w[1], w[2], w[3], w[4], w[5], w[6]); break;
                    case ARG_7STR: if(isdown) ((void (__cdecl *)(char *, char *, char *, char *, char *, char *, char *))id->_fun)(w[1], w[2], w[3], w[4], w[5], w[6], w[7]); break;

                    case ARG_DOWN: ((void (__cdecl *)(bool))id->_fun)(isdown); break;
                    case ARG_DWN1: ((void (__cdecl *)(bool, char *))id->_fun)(isdown, w[1]); break;
                    case ARG_1EXP: if(isdown) val = ((int (__cdecl *)(int))id->_fun)(execute(w[1])); break;
                    case ARG_2EXP: if(isdown) val = ((int (__cdecl *)(int, int))id->_fun)(execute(w[1]), execute(w[2])); break;
                    case ARG_1EST: if(isdown) val = ((int (__cdecl *)(char *))id->_fun)(w[1]); break;
                    case ARG_2EST: if(isdown) val = ((int (__cdecl *)(char *, char *))id->_fun)(w[1], w[2]); break;
                    case ARG_CONC: if(isdown)
                    {
                        string r;               // limit, remove
                        conc(w+1, numargs-1, r);
                        ((void (__cdecl *)(char *))id->_fun)(r);
                        break;
                    };
                    case ARG_VARI: if(isdown) ((void (__cdecl *)(char **, int))id->_fun)(w+1, numargs-1); break;
                    
                };
                break;
       
            case ID_VAR:                        // game defined variabled 
                if(isdown)
                {
                    if(!w[1][0]) conoutf("%s = %d", c, *id->_storage);      // var with no value just prints its current value
                    else
                    {
                        int i1 = atoi(w[1]);
                        if(i1<id->_min || i1>id->_max)
                        {
                            i1 = i1<id->_min ? id->_min : id->_max;                // clamp to valid range
                            conoutf("valid range for %s is %d..%d", c, id->_min, id->_max);
                        }
                        *id->_storage = i1;
                        if(id->_fun) ((void (__cdecl *)())id->_fun)();            // call trigger function if available
                    };
                };
                break;
                
            case ID_ALIAS:                              // alias, also used as functions and (global) variables
                for(int i = 1; i<numargs; i++)
                {
                    s_sprintfd(t)("arg%d", i);          // set any arguments as (global) arg values so functions can access them
                    alias(t, w[i]);
                };
                string t;
                itoa(t, numargs-1);
                alias("numargs", t);
                char *action = newstring(id->_action);   // create new string here because alias could rebind itself
                val = execute(action, isdown);
                delete[] action;
                break;
        };
        loopj(numargs) delete[] w[j];
    };
    return val;
};

// tab-completion of all idents and base maps

int completesize = 0;
string lastcomplete;

void resetcomplete() { completesize = 0; }

int mapnamecmp(char **x, char **y)
{
    return strcmp(*x, *y);
};

void buildmapnames()
{
    #if defined(WIN32)
    WIN32_FIND_DATA	FindFileData;
    HANDLE Find = FindFirstFile("packages\\base\\*.ogz", &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            mapnames.add(newstring(FindFileData.cFileName, (int)strlen(FindFileData.cFileName) - 4));
        } while(FindNextFile(Find, &FindFileData));
    }
    #elif defined(__GNUC__)
    DIR *d = opendir("packages/base");;
    struct dirent *dir;
    int namelength;
    if(d)
    {
        while((dir = readdir(d)) != NULL)
        {
            namelength = strlen(dir->d_name) - 4;
            if(namelength > 0 && strncmp(dir->d_name+namelength, ".ogz", 4)==0)  mapnames.add(newstring(dir->d_name, namelength));
        };
        closedir(d);
    }
    #else
    if(0)
    #endif
    else conoutf("unable to read base folder for map autocomplete");
};

void complete(char *s)
{
    if(*s!='/')
    {
        string t;
        s_strcpy(t, s);
        s_strcpy(s, "/");
        s_strcat(s, t);
    };
    if(!s[1]) return;
    if(!completesize) { completesize = (int)strlen(s)-1; lastcomplete[0] = '\0'; };
    char *prefix = "/", 
         *nextcomplete = NULL;
    if(completesize >= 4 && strncmp(s,"/map ", 5)==0)           // complete a mapname in packages/base/ instead of a command
    {
        prefix = "/map ";
        if(mapnames.empty()) buildmapnames();
        loopi(mapnames.length())
        {
            if(strncmp(mapnames[i], s+5, completesize-4)==0 && 
               strcmp(mapnames[i], lastcomplete) > 0 && (!nextcomplete || strcmp(mapnames[i], nextcomplete) < 0))
                nextcomplete = mapnames[i];
        };
    }
    else
    {
        enumerate(idents, ident, id,
            if(strncmp(id->_name, s+1, completesize)==0 &&
               strcmp(id->_name, lastcomplete) > 0 && (!nextcomplete || strcmp(id->_name, nextcomplete) < 0))
                nextcomplete = id->_name;
        );
    };
    if(nextcomplete)
    {
        s_strcpy(s, prefix);
        s_strcat(s, nextcomplete);
        s_strcpy(lastcomplete, nextcomplete);
    }
    else lastcomplete[0] = '\0';
};

bool execfile(char *cfgfile)
{
    string s;
    s_strcpy(s, cfgfile);
    char *buf = loadfile(path(s), NULL);
    if(!buf) return false;
    execute(buf);
    delete[] buf;
    return true;
};

void exec(char *cfgfile)
{
    if(!execfile(cfgfile)) conoutf("could not read \"%s\"", cfgfile);
};

void writecfg()
{
    FILE *f = fopen("config.cfg", "w");
    if(!f) return;
    fprintf(f, "// automatically written on exit, do not modify\n// delete this file to have defaults.cfg overwrite these settings\n// modify settings in game, or put settings in autoexec.cfg to override anything\n\n");
    cc->writeclientinfo(f);
    fprintf(f, "\n");
    enumerate(idents, ident, id,
        if(id->_type==ID_VAR && id->_persist)
        {
            fprintf(f, "%s %d\n", id->_name, *id->_storage);
        };
    );
    fprintf(f, "\n");
    writebinds(f);
    fprintf(f, "\n");
    enumerate(idents, ident, id,
        if(id->_type==ID_ALIAS && !strstr(id->_name, "nextmap_"))
        {
            fprintf(f, "alias \"%s\" [%s]\n", id->_name, id->_action);
        };
    );
    fclose(f);
};

COMMAND(writecfg, ARG_NONE);

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

void intset(char *name, int v) { string b; itoa(b, v); alias(name, b); };

ICOMMAND(if, 3, execute(args[0][0]!='0' ? args[1] : args[2]));

ICOMMAND(loop, 2, { int t = atoi(args[0]); loopi(t) { intset("i", i); execute(args[1]); }; });
ICOMMAND(while, 2, while(execute(args[0])) execute(args[1]));    // can't get any simpler than this :)

void onrelease(bool on, char *body) { if(!on) execute(body); };

void concat(char *s) { alias("s", s); };

void concatword(char **args, int numargs)
{
    string s;
    conc(args, numargs, s, NULL);
    concat(s);
};

void format(char **args, int numargs)
{
    string s;
    char *f = args[0], *r = s;
    while(*f && r < s+sizeof(s)-1)
    {
        int c = *f++;
        if(c == '%')
        {
            int i = *f++;
            if(i >= '1' && i <= '9')
            {
                i -= '0';
                const char *sub = i < numargs ? args[i] : "";
                while(*sub && r < s+sizeof(s)-1) *r++ = *sub++;
            }
            else *r++ = i;
        }
        else *r++ = c;
    };
    *r = '\0';
    concat(s);
};

int listlen(char *a)
{
    if(!*a) return 0;
    int n = 0;
    while(*a) if(*a++==' ') n++;
    return n+1;
};

void at(char *s, char *pos)
{
    int n = atoi(pos);
    loopi(n) s += strspn(s += strcspn(s, " \0"), " ");
    s[strcspn(s, " \0")] = 0;
    concat(s);
};

COMMAND(onrelease, ARG_DWN1);
COMMAND(exec, ARG_1STR);
COMMAND(concat, ARG_CONC);
COMMAND(concatword, ARG_VARI);
COMMAND(format, ARG_VARI);
COMMAND(at, ARG_2STR);
COMMAND(listlen, ARG_1EST);

int add(int a, int b)   { return a+b; };         COMMANDN(+, add, ARG_2EXP);
int mul(int a, int b)   { return a*b; };         COMMANDN(*, mul, ARG_2EXP);
int sub(int a, int b)   { return a-b; };         COMMANDN(-, sub, ARG_2EXP);
int divi(int a, int b)  { return b ? a/b : 0; }; COMMANDN(div, divi, ARG_2EXP);
int mod(int a, int b)   { return b ? a%b : 0; }; COMMAND(mod, ARG_2EXP);
int equal(int a, int b) { return (int)(a==b); }; COMMANDN(=, equal, ARG_2EXP);
int lt(int a, int b)    { return (int)(a<b); };  COMMANDN(<, lt, ARG_2EXP);
int gt(int a, int b)    { return (int)(a>b); };  COMMANDN(>, gt, ARG_2EXP);

int strcmpa(char *a, char *b) { return strcmp(a,b)==0; };  COMMANDN(strcmp, strcmpa, ARG_2EST);

int rndn(int a)    { return a>0 ? rnd(a) : 0; };  COMMANDN(rnd, rndn, ARG_1EXP);

ICOMMAND(echo, IARG_CONC, conoutf("%s", args[0]));

