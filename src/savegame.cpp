// loading and saving of savegames, dumps the spawn state of all mapents, the full state of all dynents (monsters + player)

#include "pch.h"
#include "game.h"

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
    if(!f) { conoutf("could not write %s", fn); return; };
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
    conoutf("wrote %s", fn);
};

gzFile f = NULL;

void loadgame(char *name)
{
    if(multiplayer()) return;
    sprintf_sd(fn)("savegames/%s.csgz", name);
    f = gzopen(fn, "rb9");
    if(!f) { conoutf("could not open %s", fn); return; };

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
    player1->lastaction = lastmillis;

    int nmonsters = gzgeti(f);
    dvector &monsters = getmonsters();
    if(nmonsters!=monsters.length()) return loadgameout();
    loopv(monsters)
    {
        gzread(f, monsters[i], sizeof(dynent));
        monsters[i]->enemy = player1;                                       // lazy, could save id of enemy instead
        monsters[i]->lastaction = monsters[i]->trigger = lastmillis+500;    // also lazy, but no real noticable effect on game
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
