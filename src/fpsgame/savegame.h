// loading and saving of savegames, dumps the spawn state of all mapents, the full state of all dynents (monsters + player)

struct gamesaver
{
    fpsclient &cl;
    vector<extentity *> &ents;
    gzFile f;
    
    gamesaver(fpsclient &_cl) : cl(_cl), ents(_cl.et.ents), f(NULL)
    {
        // FIXME needs testing
        //CCOMMAND(gamesaver, savegame, 1, self->gamesave(args[0]));
        //CCOMMAND(gamesaver, loadgame, 1, self->gameload(args[0]));
    };

    void gzputi(gzFile f, int i) { gzwrite(f, &i, sizeof(int)); };

    int gzgeti(gzFile f)
    {
        int i;
        if(gzread(f, &i, sizeof(int))<sizeof(int)) fatal("savegame file corrupt (short)");
        return i;
    };

    void gamesave(char *arg)
    {
        int gamemode = cl.gamemode;
        if(!m_classicsp) { conoutf("can only save classic sp games"); return; };
        s_sprintfd(fn)("savegames/%s.csgz", arg);
        gzFile f = gzopen(fn, "wb9");
        if(!f) { conoutf("could not write %s", fn); return; };
        gzwrite(f, (void *)"CUBESAVE", 8);
        gzputc(f, islittleendian);
        gzputi(f, SAVEGAMEVERSION);
        gzputi(f, sizeof(fpsent));
        gzwrite(f, cl.getclientmap(), _MAXDEFSTR);
        gzputi(f, gamemode);
        gzputi(f, ents.length());
        loopv(ents) gzputc(f, ents[i]->spawned);
        gzwrite(f, cl.player1, sizeof(fpsent));
        gzputi(f, cl.ms.monsters.length());
        loopv(cl.ms.monsters) gzwrite(f, cl.ms.monsters[i], sizeof(monsterset::monster));
        gzclose(f);
        conoutf("wrote %s", fn);
    };

    void gameload(char *arg)
    {
        if(multiplayer()) return;
        s_sprintfd(fn)("savegames/%s.csgz", arg);
        f = gzopen(fn, "rb9");
        if(!f) { conoutf("could not open %s", fn); return; };

        string buf;
        gzread(f, buf, 8);
        if(strncmp(buf, "CUBESAVE", 8)) goto out;
        if(gzgetc(f)!=islittleendian) goto out;     // not supporting save->load accross incompatible architectures simpifies things a LOT
        if(gzgeti(f)!=SAVEGAMEVERSION || gzgeti(f)!=sizeof(fpsent)) goto out;
        string mapname;
        gzread(f, mapname, _MAXDEFSTR);
        cl.nextmode = gzgeti(f);
        cl.cc.changemap(mapname); // continue below once map has been loaded and client & server have updated
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
            ents[i]->spawned = gzgetc(f)!=0;
            if(ents[i]->type==CARROT && !ents[i]->spawned) cl.et.trigger(ents[i]->attr1, ents[i]->attr2, true);
        };
        //BREAK
        //restoreserverstate(ents);

        gzread(f, cl.player1, sizeof(fpsent));
        cl.player1->lastaction = cl.lastmillis;

        int nmonsters = gzgeti(f);
        if(nmonsters!=cl.ms.monsters.length()) return loadgameout();
        loopv(cl.ms.monsters)
        {
            gzread(f, cl.ms.monsters[i], sizeof(monsterset::monster));
            cl.ms.monsters[i]->enemy = cl.player1;                                       // lazy, could save id of enemy instead
            cl.ms.monsters[i]->lastaction = cl.ms.monsters[i]->trigger = cl.lastmillis+500;    // also lazy, but no real noticable effect on game
        };
        cl.ms.restoremonsterstate();

        string buf;
        int bytesleft = gzread(f, buf, 1);
        gzclose(f);
        f = NULL;
        if(bytesleft==1) fatal("savegame file corrupt (long)");
        conoutf("savegame restored");
    };
};