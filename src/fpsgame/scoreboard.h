// creation of scoreboard pseudo-menu

struct scoreboard
{
    bool scoreson;

    scoreboard() : scoreson(false)
    {
        CCOMMAND(scoreboard, showscores, "D", self->showscores(args!=NULL));
    };

    void showscores(bool on)
    {
        scoreson = on;
        menuset(((int)on)-1);
    };

    struct sline { string s; };

    struct teamscore
    {
        char *team;
        int score;
        teamscore() {};
        teamscore(char *s, int n) : team(s), score(n) {};
    };

    static int teamscorecmp(const teamscore *x, const teamscore *y)
    {
        if(x->score > y->score) return -1;
        if(x->score < y->score) return 1;
        return 0;
    };

    void render(fpsclient &cl, int gamemode)
    {
        if(!scoreson) return;

        vector<sline> scorelines;
        vector<teamscore> teamscores;
        bool showclientnum = cl.cc.currentmaster>=0 && cl.cc.currentmaster==cl.cc.clientnum;

        scorelines.setsize(0);
        loopi(cl.numdynents()) 
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && o->type!=ENT_AI)
            {
                const char *master = cl.cc.currentmaster>= 0 && (cl.cc.currentmaster==i-1 || (!i && cl.cc.currentmaster==cl.cc.clientnum)) ? "\f0" : "";
                string name;
                if(showclientnum) s_sprintf(name)("%s \f0(%d)", o->name, !i ? cl.cc.clientnum : i-1);
                else s_strcpy(name, o->name);
                if(o->state == CS_SPECTATOR) s_sprintf(scorelines.add().s)("SPECTATOR\t\t\t%s%s", master, name);
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
            if(m_capture)
            {
                loopv(cl.cpc.scores) if(cl.cpc.scores[i].total)
                    teamscores.add(teamscore(cl.cpc.scores[i].team, cl.cpc.scores[i].total));
            }
            else loopi(cl.numdynents()) 
            {
                fpsent *o = (fpsent *)cl.iterdynents(i);
                if(o && o->type!=ENT_AI && o->frags)
                {
                    teamscore *ts = NULL;
                    loopv(teamscores) if(!strcmp(teamscores[i].team, o->team)) { ts = &teamscores[i]; break; };
                    if(!ts) teamscores.add(teamscore(o->team, o->frags));
                    else ts->score += o->frags;
                };
            };
            teamscores.sort(teamscorecmp);
            while(teamscores.length() && teamscores.last().score <= 0) teamscores.drop();
            if(teamscores.length())
            {
                string teamline;
                teamline[0] = 0;
                loopvj(teamscores)
                {
                    if(j >= 4) break;
                    s_sprintfd(s)("[ %s: %d ]", teamscores[j].team, teamscores[j].score);
                    s_strcat(teamline, s);
                };
                menumanual(0, scorelines.length(), "");
                menumanual(0, scorelines.length()+1, teamline);
            };
        };
    };
};
