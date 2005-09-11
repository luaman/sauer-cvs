
struct fpsrender
{
    // render players & monsters
    // very messy ad-hoc handling of animation frames, should be made more configurable
    
    int *range, *frame;
    
    fpsrender()
    {
        //                      0                   4                   8       10      12     14     16         18
        //                      D    D    D    D'   D    D    D    D'   A   A'  P   P'  I   I' R,  R'  E    L    J   J'
        static int _frame[] = { 178, 184, 190, 137, 183, 189, 197, 164, 46, 51, 54, 32, 0,  0, 40, 1,  162, 162, 67, 168 };
        static int _range[] = { 6,   6,   8,   28,  1,   1,   1,   1,   8,  19, 4,  18, 40, 1, 6,  15, 1,   1,   1,  1   };
        frame = _frame;
        range = _range;
    };

/*
    void renderclient(fpsclient &cl, fpsent *d, bool team, char *mdlname, float scale, bool hellpig)
    {
        int n = 3;
        float speed = 100.0f;
        float mz = d->o.z-d->eyeheight+6.2f*scale;
        int basetime = -((int)(size_t)d&0xFFF);
        bool att = d==cl.player1 && isthirdperson() && cl.lastmillis-d->lastaction<500;
        if(d->state==CS_DEAD)
        {
            int r;
            if(hellpig) { n = 2; r = range[3]; } else { n = (int)(size_t)d%3; r = range[n]; };
            basetime = d->lastaction;
            int t = cl.lastmillis-d->lastaction;
            if(t<0 || t>20000) return;
            if(t>(r-1)*100) { n += 4; if(t>(r+10)*100) { t -= (r+10)*100; mz -= t*t/10000000000.0f*t/16.0f; }; };
            if(mz<-1000) return;
            //mdl = (((int)d>>6)&1)+1;
            //mz = d->o.z-d->eyeheight+0.8f;
            //scale = 1.2f;
        }
        else if(d->state==CS_EDITING)                   { n = 16; }
        else if(d->state==CS_LAGGED)                    { n = 17; }
        else if(d->monsterstate==M_ATTACKING || att)    { n = 8;  }
        else if(d->monsterstate==M_PAIN)                { n = 10; }
        else if(d->timeinair > 100)                     { n = 18; }
        else if((!d->move && !d->strafe) || !d->moving) { n = 12; }
        else                                            { n = 14; speed = 4800/d->maxspeed*scale; if(hellpig) speed = 1200/d->maxspeed;  };
        if(hellpig) { n++; scale *= 32; mz -= 7.6f; };
        uchar color[3];
        lightreaching(d->o, color);
        glColor3ubv(color);
        rendermodel(mdlname, frame[n], range[n], 0, d->o.x, mz, d->o.y, d->yaw+90, d->pitch/2, team, scale, speed, basetime, d);
    };
*/

    void renderclient(fpsclient &cl, fpsent *d, bool team, char *mdlname, float scale, bool hellpig)
    {
        bool att = d==cl.player1 && isthirdperson() && cl.lastmillis-d->lastaction<500;
        if(d->state == CS_DEAD)                         { md3setanim(d, BOTH_DEATH1); }
        else if(d->state == CS_EDITING)                 { md3setanim(d, LEGS_IDLECR); md3setanim(d, TORSO_GESTURE); }
        else if(d->state == CS_LAGGED)                  { md3setanim(d, LEGS_IDLECR); md3setanim(d, TORSO_DROP); }
        else
        {
            if(!d->onfloor && d->timeinair>100)             { md3setanim(d, LEGS_JUMP); }
            else if((!d->move && !d->strafe))               { md3setanim(d, LEGS_IDLE); }
            else                                            { md3setanim(d, LEGS_RUN); }
        
            if(d->monsterstate == M_ATTACKING || d->attacking)       { md3setanim(d, TORSO_ATTACK2); }
            else                                            { md3setanim(d, TORSO_STAND); }
        };
        rendermd3player(0, d, d->gunselect);
    };  

    void rendergame(fpsclient &cl, int gamemode)
    {
        fpsent *d;
        loopv(cl.players) if(d = cl.players[i]) renderclient(cl, d, isteam(cl.player1->team, d->team), "monster/ogro", 1.0f, false);
        if(isthirdperson()) renderclient(cl, cl.player1, false, "monster/ogro", 1.0f, false);
        cl.ms.monsterrender();
        cl.et.renderentities();
    };

};

