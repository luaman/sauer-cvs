
struct fpsrender
{      
    void renderclient(fpsclient &cl, fpsent *d, bool team, char *mdlname, float scale, bool hellpig)
    {
        int anim = ANIM_IDLE;
        float speed = 100.0f;
        float mz = d->o.z-d->eyeheight+6.2f*scale;
        int basetime = -((int)(size_t)d&0xFFF);
        bool attack = (d->monsterstate==M_ATTACKING || (!d->monsterstate && cl.lastmillis-d->lastaction<500));
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
        else if(d->state==CS_EDITING)                   { anim = ANIM_EDIT; }
        else if(d->state==CS_LAGGED)                    { anim = ANIM_LAG; }
        else if(d->monsterstate==M_PAIN || cl.lastmillis-d->lastpain<200)  { anim = ANIM_PAIN; }
        else if(d->timeinair > 100)                     { anim = attack ? ANIM_JUMP_ATTACK : ANIM_JUMP; /*comment out for md2 -> *//*basetime = cl.lastmillis-d->timeinair;*/ }
        else if((!d->move && !d->strafe)/* || !d->moving*/) { anim = attack ? ANIM_IDLE_ATTACK : ANIM_IDLE; }
        else                                            { anim = attack ? ANIM_RUN_ATTACK : ANIM_RUN; speed = 4800/d->maxspeed*scale; if(hellpig) speed = 1200/d->maxspeed;  };
        uchar color[3];
        lightreaching(d->o, color);
        rendermodel(color, mdlname, anim, (int)(size_t)d, 0, d->o.x, mz, d->o.y, d->yaw+90, d->pitch/2, team, scale, speed, basetime, d);
    };
    
    void rendergame(fpsclient &cl, int gamemode)
    {
        fpsent *d;
        loopv(cl.players) if(d = cl.players[i]) renderclient(cl, d, isteam(cl.player1->team, d->team), "monster/ogro", 1.0f, false);
        if(isthirdperson()) renderclient(cl, cl.player1, false, "monster/ogro", 1.0, false);
        cl.ms.monsterrender();
        cl.et.renderentities();
    };

};
