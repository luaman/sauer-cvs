#define sgetstr() { char *t = text; do { *t = getint(p); } while(*t++); }   // used by networking



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

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, nargs) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)
#define VAR(name, min, cur, max) int name = variable(#name, min, cur, max, &name, NULL)
#define VARF(name, min, cur, max, body) void var_##name(); static int name = variable(#name, min, cur, max, &name, var_##name); void var_##name() { body; }


