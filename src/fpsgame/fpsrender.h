
struct fpsrender
{      
    fpsclient &cl;

    fpsrender(fpsclient &_cl) : cl(_cl) {}

    vector<fpsent *> bestplayers;
    vector<const char *> bestteams;

    IVARP(ogro, 0, 0, 1);

    void renderplayer(fpsent *d, const char *mdlname)
    {
        int lastaction = d->lastaction, attack = d->gunselect==GUN_FIST ? ANIM_PUNCH : ANIM_SHOOT, delay = ogro() ? 300 : cl.ws.reloadtime(d->gunselect)+50;
        if(cl.intermission && d->state!=CS_DEAD)
        {
            lastaction = cl.lastmillis;
            attack = ANIM_LOSE|ANIM_LOOP;
            delay = 1000;
            int gamemode = cl.gamemode;
            if(m_teammode) loopv(bestteams) { if(!strcmp(bestteams[i], d->team)) { attack = ANIM_WIN|ANIM_LOOP; break; } }
            else if(bestplayers.find(d)>=0) attack = ANIM_WIN|ANIM_LOOP;
        }
        else if(d->state==CS_ALIVE && d->lasttaunt && cl.lastmillis-d->lasttaunt<1000 && cl.lastmillis-d->lastaction>delay)
        {
            lastaction = d->lasttaunt;
            attack = ANIM_TAUNT;
            delay = 1000;
        }
        modelattach a[4] = { { NULL }, { NULL }, { NULL }, { NULL } };
        static const char *vweps[] = {"vwep/fist", "vwep/shotg", "vwep/chaing", "vwep/rocket", "vwep/rifle", "vwep/gl", "vwep/pistol"};
        int ai = 0;
        if((!ogro() || d->gunselect!=GUN_FIST) && d->gunselect<=GUN_PISTOL)
        {
            a[ai].name = ogro() ? "monster/ogro/vwep" : vweps[d->gunselect];
            a[ai].tag = "tag_weapon";
            a[ai].anim = ANIM_VWEP|ANIM_LOOP;
            a[ai].basetime = 0;
            ai++;
        }
        if(!ogro() && d->state==CS_ALIVE)
        {
            if(d->quadmillis)
            {
                a[ai].name = "quadspheres";
                a[ai].tag = "tag_powerup";
                a[ai].anim = ANIM_POWERUP|ANIM_LOOP;
                a[ai].basetime = 0;
                ai++;
            }
            if(d->armour)
            {
                a[ai].name = d->armourtype==A_GREEN ? "shield/green" : (d->armourtype==A_YELLOW ? "shield/yellow" : NULL);
                a[ai].tag = "tag_shield";
                a[ai].anim = ANIM_SHIELD|ANIM_LOOP;
                a[ai].basetime = 0;
                ai++;
            }
        }
        renderclient(d, mdlname, a[0].name ? a : NULL, attack, delay, lastaction, cl.intermission ? 0 : d->lastpain);
#if 0
        if(d->state!=CS_DEAD && d->quadmillis) 
        {
            vec color(1, 1, 1), dir(0, 0, 1);
            rendermodel(color, dir, "quadrings", ANIM_MAPMODEL|ANIM_LOOP, vec(d->o).sub(vec(0, 0, d->eyeheight/2)), 360*cl.lastmillis/1000.0f, 0, MDL_DYNSHADOW | MDL_CULL_VFC | MDL_CULL_DIST);
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

        const char *mdlnames[3] = 
        { 
            ogro() ? "monster/ogro" : "ironsnout",
            ogro() ? "monster/ogro/blue" : "ironsnout/blue",
            ogro() ? "monster/ogro/red" : "ironsnout/red"
        };

        fpsent *exclude = NULL;
        if(cl.player1->state==CS_SPECTATOR && cl.following>=0 && !cl.followdist())
            exclude = cl.getclient(cl.following);

        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR && d->state!=CS_SPAWNING && d!=exclude)
        {
            int mdl = 0;
            if(m_assassin) mdl = cl.asc.targets.find(d)>=0 ? 2 : (cl.asc.hunters.find(d)>=0 ? 0 : 1);
            else if(teamskins() || m_teammode) mdl = isteam(cl.player1->team, d->team) ? 1 : 2;
            if(d->state!=CS_DEAD || d->superdamage<50) renderplayer(d, mdlnames[mdl]);
            s_strcpy(d->info, cl.colorname(d, NULL, "@"));
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); }
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, mdl ? (mdl==1 ? 16 : 13) : 11, 1);
        }
        if(isthirdperson()) renderplayer(cl.player1, teamskins() || m_teamskins ? mdlnames[1] : mdlnames[0]);
        cl.ms.monsterrender();
        cl.mo.render();
        cl.et.renderentities();
        cl.ws.renderprojectiles();
        if(m_capture) cl.cpc.renderbases();
        else if(m_ctf) cl.ctf.renderflags();

        endmodelbatches();
    }
};
