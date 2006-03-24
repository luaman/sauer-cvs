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
        bool showclientnum = cl.cc.currentmaster>=0 && cl.cc.currentmaster==cl.cc.clientnum;

        scorelines.setsize(0);
        loopi(cl.numdynents()) 
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && o->type!=ENT_AI)
            {
                const char *master = cl.cc.currentmaster>= 0 && (cl.cc.currentmaster==i-1 || (!i && cl.cc.currentmaster==cl.cc.clientnum)) ? "\f" : "";
                string name;
                if(showclientnum) s_sprintf(name)("%s \f(%d)", o->name, !i ? cl.cc.clientnum : i-1);
                else s_strcpy(name, o->name);
                if(o->state == CS_SPECTATOR) s_sprintf(scorelines.add().s)("SPECTATOR\t\t\t%s%s", master, o->name);
                else
                {
                    s_sprintfd(lag)("%d", o->plag);
                    s_sprintf(scorelines.add().s)("%d\t%s\t%d\t%s\t%s%s", m_capture ? cl.cpc.findscore(o->team).total : o->frags, o->state==CS_LAGGED ? "LAG" : lag, o->ping, o->team, master, name);
                };
                menumanual(0, scorelines.length()-1, scorelines.last().s); 
            }
        };

        sortmenu(0, scorelines.length());
        if(m_teammode)
        {
            teamsused = 0;
            if(m_capture)
            {
                loopv(cl.cpc.scores) if(cl.cpc.scores[i].total)
                {
                    teamname[teamsused] = cl.cpc.scores[i].team;
                    teamscore[teamsused++] = cl.cpc.scores[i].total;
                    if(teamsused>=maxteams) break;
                };
            }
            else loopi(cl.numdynents()) 
            {
                fpsent *o = (fpsent *)cl.iterdynents(i);
                if(o && o->type!=ENT_AI && o->frags)
                {
                    loopi(teamsused) if(strcmp(teamname[i], o->team)==0) { teamscore[i] += o->frags; goto out; };
                    if(teamsused<maxteams)
                    {
                        teamname[teamsused] = o->team;
                        teamscore[teamsused++] = o->frags;
                    };
                    out:;
                };
            };
            if(teamsused)
            {
                teamscores[0] = 0;
                loopj(teamsused)
                {
                    s_sprintfd(s)("[ %s: %d ]", teamname[j], teamscore[j]);
                    s_strcat(teamscores, s);
                };
                menumanual(0, scorelines.length(), "");
                menumanual(0, scorelines.length()+1, teamscores);
            };
        };
    };
};
