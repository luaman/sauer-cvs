
struct fpsrender
{      
    void renderplayer(fpsent *d, const char *mdlname)
    {
//      static const char *vweps[] = {NULL, "vwep/shotg", "vwep/chaing", "vwep/rocket", "vwep/rifle", "vwep/gl", "vwep/pistol"};
        static const char *vweps[] = {NULL, "vwep/chaing", "vwep/chaing", "vwep/chaing", "vwep/chaing", "vwep/chaing", "vwep/chaing"};
        const char *vwepname = d->gunselect<=GUN_PISTOL ? vweps[d->gunselect] : NULL;
        int attack = d->gunselect==GUN_FIST ? ANIM_PUNCH : ANIM_SHOOT;
        renderclient(d, mdlname, vwepname, attack, d->lastaction, d->lastpain);
    }

    void rendergame(fpsclient &cl, int gamemode)
    {
        startmodelbatches();

        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR)
        {
            const char *mdlname = m_teammode ? (isteam(cl.player1->team, d->team) ? "ironsnout/blue" : "ironsnout/red") : "ironsnout";
            if(d->state!=CS_DEAD || d->superdamage<50) renderplayer(d, mdlname);
            s_strcpy(d->info, cl.colorname(d, NULL, "@"));
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); }
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, m_teammode ? (isteam(cl.player1->team, d->team) ? 16 : 13) : 11, 1);
        }
        if(isthirdperson()) renderplayer(cl.player1, m_teammode ? "ironsnout/blue" : "ironsnout");

        cl.ms.monsterrender();
        cl.et.renderentities();
        cl.ws.renderprojectiles();
        if(m_capture) cl.cpc.renderbases();

        endmodelbatches();
    }

};
