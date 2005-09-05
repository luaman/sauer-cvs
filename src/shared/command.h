// script binding functionality

enum    // function signatures for script functions, see command.cpp
{
    ARG_1INT, ARG_2INT, ARG_3INT, ARG_4INT,
    ARG_NONE,
    ARG_1STR, ARG_2STR, ARG_3STR, ARG_5STR,
    ARG_DOWN, ARG_DWN1,
    ARG_1EXP, ARG_2EXP,
    ARG_1EST, ARG_2EST,
    ARG_VARI
};

enum { ID_VAR, ID_COMMAND, ID_ICOMMAND, ID_ALIAS };

struct ident;

extern void addident(char *name, ident *id);

struct ident
{
    int _type;           // one of ID_* above
    char *_name;
    int _min, _max;      // ID_VAR
    int *_storage;       // ID_VAR
    void (*_fun)();      // ID_VAR, ID_COMMAND
    int _narg;           // ID_VAR, ID_COMMAND, ID_ICOMMAND
    char *_action;       // ID_ALIAS
    bool _persist;
    
    ident() {};
    ident(int t, char *n, int m, int x, int *s, void *f, int g, char *a, bool p)
        : _type(t), _name(n), _min(m), _max(x), _storage(s), _fun((void (__cdecl *)(void))f), _narg(g), _action(a), _persist(p) {};
        
    ident &operator=(const ident &o) { memcpy(this, &o, sizeof(ident)); return *this; };        // force vtable copy, ugh
    
    int operator()() { return _narg; };
    
    virtual void run(char **args) {};
};

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, nargs) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)
#define VARP(name, min, cur, max) int name = variable(#name, min, cur, max, &name, NULL, true)
#define VAR(name, min, cur, max)  int name = variable(#name, min, cur, max, &name, NULL, false)
#define VARF(name, min, cur, max, body)  void var_##name(); static int name = variable(#name, min, cur, max, &name, var_##name, false); void var_##name() { body; }
#define VARFP(name, min, cur, max, body) void var_##name(); static int name = variable(#name, min, cur, max, &name, var_##name, true); void var_##name() { body; }

#define IARG_VAR -1
#define IARG_BOTH -2

// new style macros, have the body inline, and allow binds to happen anywhere, even inside class constructors
#define ICOMMAND(n, g, b) struct cmd_##n : ident { cmd_##n() : ident(ID_ICOMMAND, #n, 0, 0, 0, 0, g, 0, false)      { addident(_name, this); }; void run(char **args) { b; }; } icom_##n
#define IVAR(n, m, c, x)  struct var_##n : ident { var_##n() : ident(ID_VAR,      #n, m, x, &_narg, 0, c, 0, false) { addident(_name, this); }; } n
//#define ICALL(n, a) { char *args[] = a; icom_##n.run(args); }