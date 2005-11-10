// creation of scoreboard pseudo-menu

struct scoreboard
{
    bool scoreson;

    scoreboard() : scoreson(false)
    {
        CCOMMAND(scoreboard, showscores, IARG_BOTH, self->showscores(args!=NULL));
    };

    void showscores(bool on)
    {
        scoreson = on;
        menuset(((int)on)-1);
    };

    struct sline { string s; };

    void render(fpsclient &cl, int gamemode)
    {
        if(!scoreson) return;

        vector<sline> scorelines;

        const int maxteams = 4;
        char *teamname[maxteams];
        int teamscore[maxteams], teamsused;
        string teamscores;

        scorelines.setsize(0);
        fpsent *o;
        for(int i = 0; o = (fpsent *)cl.iterdynents(i); i++) if(o && !o->monsterstate) 
        {
            s_sprintfd(lag)("%d", o->plag);
            s_sprintf(scorelines.add().s)("%d\t%s\t%d\t%s\t%s", o->frags, o->state==CS_LAGGED ? "LAG" : lag, o->ping, o->team, o->name);
            menumanual(0, scorelines.length()-1, scorelines.last().s); 
        };
        sortmenu(0, scorelines.length());
        if(m_teammode)
        {
            teamsused = 0;
            fpsent *o;
            for(int i = 0; o = (fpsent *)cl.iterdynents(i); i++) if(o && !o->monsterstate)
            {
                loopi(teamsused) if(strcmp(teamname[i], o->team)==0) { teamscore[i] += o->frags; return; };
                if(teamsused==maxteams) return;
                teamname[teamsused] = o->team;
                teamscore[teamsused++] = o->frags;
            };
            teamscores[0] = 0;
            loopj(teamsused)
            {
                s_sprintf(teamscores)("[ %s: %d ]", teamname[j], teamscore[j]);
            };
            menumanual(0, scorelines.length(), "");
            menumanual(0, scorelines.length()+1, teamscores);
        };
    };
};
