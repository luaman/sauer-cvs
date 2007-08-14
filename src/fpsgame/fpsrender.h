
struct fpsrender
{      
    fpsclient &cl;

    fpsrender(fpsclient &_cl) : cl(_cl) {}

    vector<fpsent *> bestplayers;
    vector<char *> bestteams;

    void renderplayer(fpsent *d, const char *mdlname)
    {
#if 0
        static const char *vweps[] = {"vwep/fist", "vwep/shotg", "vwep/chaing", "vwep/rocket", "vwep/rifle", "vwep/gl", "vwep/pistol"};
//        static const char *vweps[] = {"vwep/fist", "vwep/chaing", "vwep/chaing", "vwep/chaing", "vwep/chaing", "vwep/chaing", "vwep/chaing"};
        const char *vwepname = d->gunselect<=GUN_PISTOL ? vweps[d->gunselect] : NULL;
        int lastaction = d->lastaction, attack = d->gunselect==GUN_FIST ? ANIM_PUNCH : ANIM_SHOOT, delay = cl.ws.reloadtime(d->gunselect)+50;
        if(cl.intermission && d->state!=CS_DEAD)
        {
            lastaction = cl.lastmillis;
            attack = ANIM_LOSE|ANIM_LOOP;
            delay = 1000;
            int gamemode = cl.gamemode;
            if(m_teammode) loopv(bestteams) { if(!strcmp(bestteams[i], d->team)) { attack = ANIM_WIN|ANIM_LOOP; break; } }
            else if(bestplayers.find(d)>=0) attack = ANIM_WIN|ANIM_LOOP;
        }
        else if(d->state==CS_ALIVE && cl.lastmillis-d->lasttaunt<1000 && cl.lastmillis-d->lastaction>delay)
        {
            lastaction = d->lasttaunt;
            attack = ANIM_TAUNT;
            delay = 1000;
        }
        const char *shieldname = NULL, *pupname = NULL;
        if(d->state==CS_ALIVE)
        {
            if(d->quadmillis) pupname = "quadspheres";
            if(d->armourtype==A_GREEN && d->armour) shieldname = "shield/green";
            if(d->armourtype==A_YELLOW && d->armour) shieldname = "shield/yellow";
        }
        renderclient(d, mdlname, vwepname, shieldname, pupname, attack, delay, lastaction, cl.intermission ? 0 : d->lastpain);
#else
        const char *vwepname = "monster/ogro/vwep";
        int lastaction = d->lastaction, attack = d->gunselect==GUN_FIST ? ANIM_PUNCH : ANIM_SHOOT, delay = 300;
        renderclient(d, mdlname, vwepname, NULL, NULL, attack, delay, lastaction, d->lastpain);
#endif
#if 0
        if(d->state!=CS_DEAD && d->quadmillis) 
        {
            vec color(1, 1, 1), dir(0, 0, 1);
            rendermodel(color, dir, "quadrings", ANIM_MAPMODEL|ANIM_LOOP, 0, 0, vec(d->o).sub(vec(0, 0, d->eyeheight/2)), 360*cl.lastmillis/1000.0f, 0, 0, 0, NULL, MDL_DYNSHADOW | MDL_CULL_VFC | MDL_CULL_DIST);
        }
#endif
    }

    IVARP(teamskins, 0, 0, 1);

    void rendergame(int gamemode)
    {
        if(cl.intermission)
        {
            if(m_teammode) { bestteams.setsize(0); cl.sb.bestteams(bestteams); }
            else { bestplayers.setsize(0); cl.sb.bestplayers(bestplayers); }
        }

        startmodelbatches();

        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR)
        {
#if 0
            const char *mdlname = teamskins() || m_teammode ? (isteam(cl.player1->team, d->team) ? "ironsnout/blue" : "ironsnout/red") : "ironsnout";
#else
            const char *mdlname = teamskins() || m_teammode ? (isteam(cl.player1->team, d->team) ? "monster/ogro/blue" : "monster/ogro/red") : "monster/ogro";
#endif
            if(d->state!=CS_DEAD || d->superdamage<50) renderplayer(d, mdlname);
            s_strcpy(d->info, cl.colorname(d, NULL, "@"));
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); }
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, m_teammode ? (isteam(cl.player1->team, d->team) ? 16 : 13) : 11, 1);
        }
#if 0        
        if(isthirdperson()) renderplayer(cl.player1, teamskins() || m_teammode ? "ironsnout/blue" : "ironsnout");
#else
        if(isthirdperson()) renderplayer(cl.player1, teamskins() || m_teammode ? "monster/ogro/blue" : "monster/ogro");
#endif

        cl.ms.monsterrender();
        cl.et.renderentities();
        cl.ws.renderprojectiles();
        if(m_capture) cl.cpc.renderbases();

        endmodelbatches();
    }

};
