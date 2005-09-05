// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "pch.h"
#include "engine.h"
#ifndef WIN32
#include <dirent.h>
#endif

void itoa(char *s, int i) { sprintf_s(s)("%d", i); };
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
        ident b(ID_ALIAS, name, 0, 0, 0, 0, 0, newstring(action), true);
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
    ident v(ID_VAR, name, min, max, storage, fun, 0, 0, persist);
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
    ident c(ID_COMMAND, name, 0, 0, 0, fun, narg, 0, false);
    idents->access(name, &c);
    return false;
};

void addident(char *name, ident *id)
{
    if(!idents) idents = new identtable;
    idents->access(name, id);
};

char *parseexp(char *&p, int right)             // parse any nested set of () or []
{
    int left = *p++;
    char *word = p;
    for(int brak = 1; brak; )
    {
        int c = *p++;
        if(c=='\r') *(p-1) = ' ';               // hack
        if(c==left) brak++;
        else if(c==right) brak--;
        else if(!c) { p--; conoutf("missing \"%c\"", right); return NULL; };
    };
    char *s = newstring(word, p-word-1);
    if(left=='(')
    {
        string t;
        itoa(t, execute(s));                    // evaluate () exps directly, and substitute result
        s = exchangestr(s, t);
    };
    return s;
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
    return newstring(word, p-word);
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

void vari(char **w, int n, char *r)
{
    r[0] = 0;
    for(int i = 1; i<n; i++)       
    {
        strcat_s(r, w[i]);  // make string-list out of all arguments
        if(i==n-1) break;
        strcat_s(r, " ");
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
            if(*s=='$') s = lookup(s);          // substitute variables
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
            if(!val && *c!='0') conoutf("unknown command: %s", c);
        }
        else switch(id->_type)
        {
            case ID_ICOMMAND:
                switch(id->_narg)
                {
                    default: id->run(w+1); break;
                    case IARG_BOTH: { id->run((char **)isdown); break; };
                    case IARG_VAR:  { string r; vari(w, numargs, r); char *rr = r; id->run(&rr); break; };
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
                    case ARG_3STR: if(isdown) ((void (__cdecl *)(char *, char *, char*))id->_fun)(w[1], w[2], w[3]); break;
                    case ARG_5STR: if(isdown) ((void (__cdecl *)(char *, char *, char*, char*, char*))id->_fun)(w[1], w[2], w[3], w[4], w[5]); break;
                    case ARG_DOWN: ((void (__cdecl *)(bool))id->_fun)(isdown); break;
                    case ARG_DWN1: ((void (__cdecl *)(bool, char *))id->_fun)(isdown, w[1]); break;
                    case ARG_1EXP: if(isdown) val = ((int (__cdecl *)(int))id->_fun)(execute(w[1])); break;
                    case ARG_2EXP: if(isdown) val = ((int (__cdecl *)(int, int))id->_fun)(execute(w[1]), execute(w[2])); break;
                    case ARG_1EST: if(isdown) val = ((int (__cdecl *)(char *))id->_fun)(w[1]); break;
                    case ARG_2EST: if(isdown) val = ((int (__cdecl *)(char *, char *))id->_fun)(w[1], w[2]); break;
                    case ARG_VARI: if(isdown)
                    {
                        string r;               // limit, remove
                        vari(w, numargs, r);
                        ((void (__cdecl *)(char *))id->_fun)(r);
                        break;
                    }
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
                    sprintf_sd(t)("arg%d", i);          // set any arguments as (global) arg values so functions can access them
                    alias(t, w[i]);
                };
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

int completesize = 0, completeidx = 0;

void resetcomplete() { completesize = 0; };

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
    #endif
    else conoutf("unable to read base folder for map autocomplete");
};

void complete(char *s)
{
    if(*s!='/')
    {
        string t;
        strcpy_s(t, s);
        strcpy_s(s, "/");
        strcat_s(s, t);
    };
    if(!s[1]) return;
    if(!completesize) { completesize = (int)strlen(s)-1; completeidx = 0; };
    int idx;
    if(completesize >= 4 && strncmp(s,"/map ", 5)==0)           // complete a mapname in packages/base/ instead of a command
    {
        if (mapnames.empty()) buildmapnames();
        idx = 0;
        loopi(mapnames.length())
        {
            if(strncmp(mapnames[i], s+5, completesize-4)==0 && idx++==completeidx)
            {
                strcpy_s(s, "/");
                strcat_s(s, "map ");
                strcat_s(s, mapnames[i]);
            };
        }
    }
    else
    {
        idx = 0;
        enumerate(idents, ident, id,
            if(strncmp(id->_name, s+1, completesize)==0 && idx++==completeidx)
            {
                strcpy_s(s, "/");
                strcat_s(s, id->_name);
            };
        );
    };
    completeidx++;
    if(completeidx>=idx) completeidx = 0;
};

bool execfile(char *cfgfile)
{
    string s;
    strcpy_s(s, cfgfile);
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
    writeclientinfo(f);
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

void concatword(char *s)
{
    for(char *a = s, *b = s; *a = *b; b++) if(*a!=' ') a++;   
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
COMMAND(concat, ARG_VARI);
COMMAND(concatword, ARG_VARI);
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

ICOMMAND(echo, IARG_VAR, conoutf("%s", args[0]));

