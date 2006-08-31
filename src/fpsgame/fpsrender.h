
struct fpsrender
{      
    void rendergame(fpsclient &cl, int gamemode)
    {
        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR)
        {
            renderclient(d, m_teammode ? (isteam(cl.player1->team, d->team) ? "monster/ogro/blue" : "monster/ogro/red") : "monster/ogro", false, d->lastaction, d->lastpain);
            s_strcpy(d->info, d->name);
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); };
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, 11, 1);
        };
        if(isthirdperson()) renderclient(cl.player1, "monster/ogro", false, cl.player1->lastaction, cl.player1->lastpain);
        cl.ms.monsterrender();
        cl.et.renderentities();
        cl.ws.renderprojectiles();
        if(m_capture) cl.cpc.renderbases();
    };

};
