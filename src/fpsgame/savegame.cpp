// loading and saving of savegames, dumps the spawn state of all mapents, the full state of all dynents (monsters + player)

#include "pch.h"
#include "game.h"

extern monsterset ms;




void gzputi(gzFile f, int i) { gzwrite(f, &i, sizeof(int)); };

int gzgeti(gzFile f)
{
    int i;
    if(gzread(f, &i, sizeof(int))<sizeof(int)) fatal("savegame file corrupt (short)");
    return i;
};

ICOMMAND(savegame, 1,
{
    if(!m_classicsp) { conoutf("can only save classic sp games"); return; };
    sprintf_sd(fn)("savegames/%s.csgz", args[0]);
    gzFile f = gzopen(fn, "wb9");
    if(!f) { conoutf("could not write %s", fn); return; };
    gzwrite(f, (void *)"CUBESAVE", 8);
    gzputc(f, islittleendian);
    gzputi(f, SAVEGAMEVERSION);
    gzputi(f, sizeof(fpsent));
    gzwrite(f, getclientmap(), _MAXDEFSTR);
    gzputi(f, gamemode);
    gzputi(f, ents.length());
    loopv(ents) gzputc(f, ents[i]->spawned);
    gzwrite(f, player1, sizeof(fpsent));
    gzputi(f, ms.monsters.length());
    loopv(ms.monsters) gzwrite(f, ms.monsters[i], sizeof(monsterset::monster));
    gzclose(f);
    conoutf("wrote %s", fn);
});

gzFile f = NULL;

ICOMMAND(loadgame, 1,
{
    if(multiplayer()) return;
    sprintf_sd(fn)("savegames/%s.csgz", args[0]);
    f = gzopen(fn, "rb9");
    if(!f) { conoutf("could not open %s", fn); return; };

    string buf;
    gzread(f, buf, 8);
    if(strncmp(buf, "CUBESAVE", 8)) goto out;
    if(gzgetc(f)!=islittleendian) goto out;     // not supporting save->load accross incompatible architectures simpifies things a LOT
    if(gzgeti(f)!=SAVEGAMEVERSION || gzgeti(f)!=sizeof(fpsent)) goto out;
    string mapname;
    gzread(f, mapname, _MAXDEFSTR);
    nextmode = gzgeti(f);
    changemap(mapname); // continue below once map has been loaded and client & server have updated
    return;
    out:
    gzclose(f);
    f = NULL;
    conoutf("aborting: savegame from a different version of cube or cpu architecture");
});

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
        ents[i]->spawned = gzgetc(f)!=0;
        if(ents[i]->type==CARROT && !ents[i]->spawned) trigger(ents[i]->attr1, ents[i]->attr2, true);
    };
    restoreserverstate(ents);

    gzread(f, player1, sizeof(fpsent));
    player1->lastaction = lastmillis;

    int nmonsters = gzgeti(f);
    if(nmonsters!=ms.monsters.length()) return loadgameout();
    loopv(ms.monsters)
    {
        gzread(f, ms.monsters[i], sizeof(monsterset::monster));
        ms.monsters[i]->enemy = player1;                                       // lazy, could save id of enemy instead
        ms.monsters[i]->lastaction = ms.monsters[i]->trigger = lastmillis+500;    // also lazy, but no real noticable effect on game
    };
    ms.restoremonsterstate();

    string buf;
    int bytesleft = gzread(f, buf, 1);
    gzclose(f);
    f = NULL;
    if(bytesleft==1) fatal("savegame file corrupt (long)");
    conoutf("savegame restored");
};
