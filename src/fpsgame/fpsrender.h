
struct fpsrender
{      
    void renderclient(fpsclient &cl, fpsent *d, bool team, char *mdlname, float scale, int monsterstate)
    {
        int anim = ANIM_IDLE;
        float speed = 100.0f;
        float mz = d->o.z-d->eyeheight+6.2f*scale;
        int basetime = -((int)(size_t)d&0xFFF);
        bool attack = (monsterstate==M_ATTACKING || (d->type!=ENT_AI && cl.lastmillis-d->lastaction<200));
        if(d->state==CS_DEAD)
        {
            anim = ANIM_DYING;
            int r = 6;//FIXME, 3rd anim & hellpig take longer
            basetime = d->lastaction;
            int t = cl.lastmillis-d->lastaction;
            if(t<0 || t>20000) return;
            if(t>(r-1)*100) { anim = ANIM_DEAD; if(t>(r+10)*100) { t -= (r+10)*100; mz -= t*t/10000000000.0f*t/16.0f; }; };
            if(mz<-1000) return;
        }
        else if(d->state==CS_EDITING || d->state==CS_SPECTATOR) { anim = ANIM_EDIT; }
        else if(d->state==CS_LAGGED)                            { anim = ANIM_LAG; }
        else if(monsterstate==M_PAIN || cl.lastmillis-d->lastpain<300) { anim = ANIM_PAIN; }
        else if(d->timeinair>100)                           { anim = attack ? ANIM_JUMP_ATTACK : ANIM_JUMP; /*comment out for md2 -> *//*basetime = cl.lastmillis-d->timeinair;*/ }
        else if((!d->move && !d->strafe)/* || !d->moving*/) { anim = attack ? ANIM_IDLE_ATTACK : ANIM_IDLE; }
        else                                                { anim = attack ? ANIM_RUN_ATTACK : ANIM_RUN; speed = 5500/d->maxspeed*scale; };
        vec color, dir;
        lightreaching(d->o, color, dir);
        rendermodel(color, dir, mdlname, anim, (int)(size_t)d, 0, d->o.x, d->o.y, mz, d->yaw+90, d->pitch/4, team, speed, basetime, d, (MDL_CULL_VFC | MDL_CULL_OCCLUDED) | (d->type==ENT_PLAYER ? 0 : MDL_CULL_DIST));
    };
    
    void rendergame(fpsclient &cl, int gamemode)
    {
        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR)
        {
            renderclient(cl, d, isteam(cl.player1->team, d->team), "monster/ogro", 1.0f, M_NONE);
            s_strcpy(d->info, d->name);
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); };
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, 11, 1);
        };
        if(isthirdperson()) renderclient(cl, cl.player1, false, "monster/ogro", 1.0f, M_NONE);
        cl.ms.monsterrender();
        cl.et.renderentities();
        if(m_capture) cl.cpc.renderbases();
    };

};
