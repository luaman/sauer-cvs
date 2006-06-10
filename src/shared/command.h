// script binding functionality

enum    // function signatures for script functions, see command.cpp
{
    ARG_1INT, ARG_2INT, ARG_3INT, ARG_4INT,
    ARG_NONE,
    ARG_1STR, ARG_2STR, ARG_3STR, ARG_4STR, ARG_5STR, ARG_6STR, ARG_7STR,
    ARG_DOWN, ARG_DWN1,
    ARG_CONC, ARG_VARI
};

enum { ID_VAR, ID_COMMAND, ID_ICOMMAND, ID_ALIAS };

enum { NO_OVERRIDE = INT_MAX, OVERRIDDEN = 0 };

template <class T> struct tident
{
    int _type;           // one of ID_* above
    char *_name;
    int _min, _max;      // ID_VAR
    int *_storage;       // ID_VAR
    int _override;       // either NO_OVERRIDE, OVERRIDDEN, or value
    void (*_fun)();      // ID_VAR, ID_COMMAND
    int _narg;           // ID_VAR, ID_COMMAND, ID_ICOMMAND
    char *_action;       // ID_ALIAS
    bool _persist;
    T *self;
    
    tident() {};
    tident(int t, char *n, int m, int x, int *s, void *f, int g, char *a, bool p, T *_s)
        : _type(t), _name(n), _min(m), _max(x), _storage(s), _override(NO_OVERRIDE), _fun((void (__cdecl *)(void))f), _narg(g), _action(a), _persist(p), self(_s) {};
    virtual ~tident() {};        

    tident &operator=(const tident &o) { memcpy(this, &o, sizeof(tident)); return *this; };        // force vtable copy, ugh
    
    int operator()() { return _narg; };
    
    virtual void run(char **args) {};
};

typedef tident<void> ident;

extern void addident(char *name, ident *id);

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, nargs) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)
#define _VAR(name, global, min, cur, max, persist)  int global = variable(#name, min, cur, max, &global, NULL, persist)
#define VARN(name, global, min, cur, max) _VAR(name, global, min, cur, max, false)
#define VAR(name, min, cur, max) _VAR(name, name, min, cur, max, false)
#define VARP(name, min, cur, max) _VAR(name, name, min, cur, max, true)
#define _VARF(name, global, min, cur, max, body, persist)  void var_##name(); int global = variable(#name, min, cur, max, &global, var_##name, persist); void var_##name() { body; }
#define VARFN(name, global, min, cur, max, body) _VARF(name, global, min, cur, max, body, false)
#define VARF(name, min, cur, max, body) _VARF(name, name, min, cur, max, body, false)
#define VARFP(name, min, cur, max, body) _VARF(name, name, min, cur, max, body, true)

#define IARG_CONC -1
#define IARG_BOTH -2

// new style macros, have the body inline, and allow binds to happen anywhere, even inside class constructors, and access the surrounding class
#define _COMMAND(t, ts, sv, tv, n, g, b) struct cmd_##n : tident<t> { cmd_##n(ts) : tident<t>(ID_ICOMMAND, #n, 0, 0, 0, 0, g, 0, false, sv) { addident(_name, (ident *)this); }; void run(char **args) { b; }; } icom_##n tv
#define ICOMMAND(n, g, b) _COMMAND(void, , NULL, , n, g, b)
#define CCOMMAND(t, n, g, b) _COMMAND(t, t *_s, _s, (this), n, g, b)
 
#define IVAR(n, m, c, x)  struct var_##n : ident { var_##n() : ident(ID_VAR, #n, m, x, &_narg, 0, c, 0, false, NULL) { addident(_name, this); }; } n
//#define ICALL(n, a) { char *args[] = a; icom_##n.run(args); }
