
struct fpsrender
{      
    void rendergame(fpsclient &cl, int gamemode)
    {
        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR)
        {
            renderclient(d, isteam(cl.player1->team, d->team), "monster/ogro", 1.0f, false, d->lastaction, d->lastpain);
            s_strcpy(d->info, d->name);
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); };
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, 11, 1);
        };
        if(isthirdperson()) renderclient(cl.player1, false, "monster/ogro", 1.0f, false, cl.player1->lastaction, cl.player1->lastpain);
        cl.ms.monsterrender();
        cl.et.renderentities();
        if(m_capture) cl.cpc.renderbases();
    };

};
