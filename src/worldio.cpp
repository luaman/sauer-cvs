// worldio.cpp: loading & saving of maps and savegames

#include "cube.h"

#ifdef WIN32
  #define _WINDOWS
  #define ZLIB_DLL
#endif
#include <zlib.h>

void backup(char *name, char *backupname)
{
    remove(backupname);
    rename(name, backupname);
};

string cgzname, bakname, pcfname, mcfname;

void setnames(char *name)
{
    string pakname, mapname;
    char *slash = strpbrk(name, "/\\");
    if(slash)
    {
        strn0cpy(pakname, name, slash-name+1);
        strcpy_s(mapname, slash+1);
    }
    else
    {
        strcpy_s(pakname, "base");
        strcpy_s(mapname, name);
    };
    sprintf_s(cgzname)("packages/%s/%s.ogz",      pakname, mapname);
    sprintf_s(bakname)("packages/%s/%s_%d.BAK",   pakname, mapname, lastmillis);
    sprintf_s(pcfname)("packages/%s/package.cfg", pakname);
    sprintf_s(mcfname)("packages/%s/%s.cfg",      pakname, mapname);

    path(cgzname);
    path(bakname);
};

// these two are used by getmap/sendmap.. transfers compressed maps directly

void writemap(char *mname, int msize, uchar *mdata)
{
    setnames(mname);
    backup(cgzname, bakname);
    FILE *f = fopen(cgzname, "wb");
    if(!f) { conoutf("could not write map to %s", (int)cgzname); return; };
    fwrite(mdata, 1, msize, f);
    fclose(f);
    conoutf("wrote map %s as file %s", (int)mname, (int)cgzname);
}

uchar *readmap(char *mname, int *msize)
{
    setnames(mname);
    uchar *mdata = (uchar *)loadfile(cgzname, msize);
    if(!mdata) { conoutf("could not read map %s", (int)cgzname); return NULL; };
    return mdata;
}

// save map as .cgz file. uses 2 layers of compression: first does simple run-length
// encoding and leaves out data for certain kinds of cubes, then zlib removes the
// last bits of redundancy. Both passes contribute greatly to the miniscule map sizes.

enum { OCTSAV_CHILDREN = 0, OCTSAV_EMPTY, OCTSAV_SOLID, OCTSAV_NORMAL };

void savec(cube *c, gzFile f)
{
    loopi(8)
    {
        if(c[i].children)
        {
            gzputc(f, OCTSAV_CHILDREN);
            savec(c[i].children, f);
        }
        else
        {
            if(isempty(c[i])) gzputc(f, OCTSAV_EMPTY);
            else if(isentirelysolid(c[i])) gzputc(f, OCTSAV_SOLID);
            else
            {
                gzputc(f, OCTSAV_NORMAL);
                gzwrite(f, c[i].edges, 12);
            };
            gzwrite(f, c[i].texture, 6);
            gzwrite(f, c[i].colour, 3);
        };
    };
};

cube *loadchildren(gzFile f);

void loadc(gzFile f, cube &c)
{
    switch(gzgetc(f))
    {
        case OCTSAV_CHILDREN:
            c.children = loadchildren(f);
            return;

        case OCTSAV_EMPTY:  emptyfaces(c);          break;
        case OCTSAV_SOLID:  solidfaces(c);          break;
        case OCTSAV_NORMAL: gzread(f, c.edges, 12); break;

        default:
            fatal("garbage in map");
    };
    gzread(f, c.texture, 6);
    gzread(f, c.colour, 3);
    c.children = NULL;
};

cube *loadchildren(gzFile f)
{
    cube *c = newcubes();
    loopi(8) loadc(f, c[i]);
    // TODO: remip c from children here
    return c;
};

void save_world(char *mname)
{
    resettagareas();    // wouldn't be able to reproduce tagged areas otherwise
    if(!*mname) mname = getclientmap();
    setnames(mname);
    backup(cgzname, bakname);
    gzFile f = gzopen(cgzname, "wb9");
    if(!f) { conoutf("could not write map to %s", (int)cgzname); return; };
    hdr.version = MAPVERSION;
    hdr.numents = 0;
    loopv(ents) if(ents[i].type!=NOTUSED) hdr.numents++;
    header tmp = hdr;
    endianswap(&tmp.version, sizeof(int), 16);
    gzwrite(f, &tmp, sizeof(header));
    loopv(ents)
    {
        if(ents[i].type!=NOTUSED)
        {
            entity tmp = ents[i];
            endianswap(&tmp.o, sizeof(int), 3);
            endianswap(&tmp.attr1, sizeof(short), 5);
            gzwrite(f, &tmp, sizeof(entity));
        };
    };

    savec(worldroot, f);

    gzclose(f);
    conoutf("wrote map file %s", (int)cgzname);
    settagareas();
};

void load_world(char *mname)        // still supports all map formats that have existed since the earliest cube betas!
{
    setnames(mname);
    gzFile f = gzopen(cgzname, "rb9");
    if(!f) { conoutf("could not read map %s", (int)cgzname); return; };
    gzread(f, &hdr, sizeof(header));
    endianswap(&hdr.version, sizeof(int), 16);
    if(strncmp(hdr.head, "OCTA", 4)!=0) fatal("while reading map: header malformatted");
    if(hdr.version>MAPVERSION) fatal("this map requires a newer version of cube");
    ents.setsize(0);
    loopi(hdr.numents)
    {
        entity &e = ents.add();
        gzread(f, &e, sizeof(entity));
        endianswap(&e.o, sizeof(int), 3);
        endianswap(&e.attr1, sizeof(short), 5);
        e.spawned = false;
    };

    freeocta(worldroot);
    worldroot = loadchildren(f);
    allchanged();

    gzclose(f);
    settagareas();
    conoutf("read map %s (%d milliseconds)", (int)cgzname, SDL_GetTicks()-lastmillis);
    conoutf("%s", (int)hdr.maptitle);
    startmap(mname);
    loopi(256)
    {
        sprintf_sd(aliasname)("level_trigger_%d", i);     // can this be done smarter?
        if(identexists(aliasname)) alias(aliasname, "");
    };
    execfile("data/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);
};

COMMANDN(savemap, save_world, ARG_1STR);

// loading and saving of savegames, dumps the spawn state of all mapents, the full state of all dynents (monsters + player)

void gzputi(gzFile f, int i) { gzwrite(f, &i, sizeof(int)); };

int gzgeti(gzFile f)
{
    int i;
    if(gzread(f, &i, sizeof(int))<sizeof(int)) fatal("savegame file corrupt (short)");
    return i;
};

void savegame(char *name)
{
    if(!m_classicsp) { conoutf("can only save classic sp games"); return; };
    sprintf_sd(fn)("savegames/%s.csgz", name);
    gzFile f = gzopen(fn, "wb9");
    if(!f) { conoutf("could not write %s", (int)fn); return; };
    gzwrite(f, (void *)"CUBESAVE", 8);
    gzputc(f, islittleendian);
    gzputi(f, SAVEGAMEVERSION);
    gzputi(f, sizeof(dynent));
    gzwrite(f, getclientmap(), _MAXDEFSTR);
    gzputi(f, gamemode);
    gzputi(f, ents.length());
    loopv(ents) gzputc(f, ents[i].spawned);
    gzwrite(f, player1, sizeof(dynent));
    dvector &monsters = getmonsters();
    gzputi(f, monsters.length());
    loopv(monsters) gzwrite(f, monsters[i], sizeof(dynent));
    gzclose(f);
    conoutf("wrote %s", (int)fn);
};

gzFile f = NULL;

void loadgame(char *name)
{
    if(multiplayer()) return;
    sprintf_sd(fn)("savegames/%s.csgz", name);
    f = gzopen(fn, "rb9");
    if(!f) { conoutf("could not open %s", (int)fn); return; };

    string buf;
    gzread(f, buf, 8);
    if(strncmp(buf, "CUBESAVE", 8)) goto out;
    if(gzgetc(f)!=islittleendian) goto out;     // not supporting save->load accross incompatible architectures simpifies things a LOT
    if(gzgeti(f)!=SAVEGAMEVERSION || gzgeti(f)!=sizeof(dynent)) goto out;
    string mapname;
    gzread(f, mapname, _MAXDEFSTR);
    nextmode = gzgeti(f);
    changemap(mapname); // continue below once map has been loaded and client & server have updated
    return;
    out:
    gzclose(f);
    f = NULL;
    conoutf("aborting: savegame from a different version of cube or cpu architecture");
};

void loadgameout()
{
    gzclose(f);
    f = NULL;
    conoutf("loadgame incomplete: savegame from a different version of this map");
};

void loadgamerest()
{
    if(!f) return;

    if(gzgeti(f)!=ents.length()) return loadgameout();
    loopv(ents)
    {
        ents[i].spawned = gzgetc(f)!=0;
        if(ents[i].type==CARROT && !ents[i].spawned) trigger(ents[i].attr1, ents[i].attr2, true);
    };
    restoreserverstate(ents);

    gzread(f, player1, sizeof(dynent));
    player1->lastattack = lastmillis;

    int nmonsters = gzgeti(f);
    dvector &monsters = getmonsters();
    if(nmonsters!=monsters.length()) return loadgameout();
    loopv(monsters)
    {
        gzread(f, monsters[i], sizeof(dynent));
        monsters[i]->enemy = player1;                                       // lazy, could save id of enemy instead
        monsters[i]->lastattack = monsters[i]->trigger = lastmillis+500;    // also lazy, but no real noticable effect on game
    };
    restoremonsterstate();

    string buf;
    int bytesleft = gzread(f, buf, 1);
    gzclose(f);
    f = NULL;
    if(bytesleft==1) fatal("savegame file corrupt (long)");
    conoutf("savegame restored");
};

COMMAND(savegame, ARG_1STR);
COMMAND(loadgame, ARG_1STR);
