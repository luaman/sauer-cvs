// creation of scoreboard

struct scoreboard : g3d_callback
{
    bool scoreson;
    vec menupos;
    int menustart;
    fpsclient &cl;

    IVARP(showclientnum, 0, 0, 1);

    scoreboard(fpsclient &_cl) : scoreson(false), cl(_cl)
    {
        CCOMMAND(scoreboard, showscores, "D", self->showscores(args!=NULL));
    }

    void showscores(bool on)
    {
        if(!scoreson && on)
        {
            menupos = menuinfrontofplayer();
            menustart = starttime();
        }
        scoreson = on;
    }

    struct sline { string s; };

    struct teamscore
    {
        char *team;
        int score;
        teamscore() {}
        teamscore(char *s, int n) : team(s), score(n) {}
    };

    static int teamscorecmp(const teamscore *x, const teamscore *y)
    {
        if(x->score > y->score) return -1;
        if(x->score < y->score) return 1;
        return strcmp(x->team, y->team);
    }
    
    static int playersort(const fpsent **a, const fpsent **b)
    {
        if((*a)->state==CS_SPECTATOR)
        {
            if((*b)->state==CS_SPECTATOR) return strcmp((*a)->name, (*b)->name);
            else return 1;
        }
        else if((*b)->state==CS_SPECTATOR) return -1;
        if((*a)->frags > (*b)->frags) return -1;
        if((*a)->frags < (*b)->frags) return 1;
        return strcmp((*a)->name, (*b)->name);
    }

    void sortplayers(vector<fpsent *> &sbplayers)
    {
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && o->type!=ENT_AI) sbplayers.add(o);
        }

        sbplayers.sort(playersort);
    }

    void bestplayers(vector<fpsent *> &best)
    {
        sortplayers(best);
        while(best.length()>1 && best.last()->frags < best[0]->frags) best.drop();
    }

    void sortteams(vector<teamscore> &teamscores)
    {
        int gamemode = cl.gamemode;
        if(m_capture)
        {
            loopv(cl.cpc.scores) teamscores.add(teamscore(cl.cpc.scores[i].team, cl.cpc.scores[i].total));
        }
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && o->type!=ENT_AI)
            {
                teamscore *ts = NULL;
                loopv(teamscores) if(!strcmp(teamscores[i].team, o->team)) { ts = &teamscores[i]; break; }
                if(!ts) teamscores.add(teamscore(o->team, m_capture ? 0 : o->frags));
                else if(!m_capture) ts->score += o->frags;
            }
        }
        teamscores.sort(teamscorecmp);
    }

    void bestteams(vector<char *> &best)
    {
        vector<teamscore> teamscores;
        sortteams(teamscores);
        while(teamscores.length()>1 && teamscores.last().score < teamscores[0].score) teamscores.drop();
        loopv(teamscores) best.add(teamscores[i].team);
    }

    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menustart, 0.04f, NULL, false);
   
        int gamemode = cl.gamemode;
        s_sprintfd(modemapstr)("%s: %s", fpsserver::modestr(gamemode), cl.getclientmap()[0] ? cl.getclientmap() : "[new map]");
        if((gamemode>1 || (gamemode==0 && multiplayer(false))) && cl.minremain >= 0)
        {
            if(!cl.minremain) s_strcat(modemapstr, ", intermission");
            else
            {
                s_sprintfd(timestr)(", %d %s remaining", cl.minremain, cl.minremain==1 ? "minute" : "minutes");
                s_strcat(modemapstr, timestr);
            }
        }
        g.text(modemapstr, 0xFFFF80, "server");
        if(m_capture) g.text("score\tpj\tping\tteam\tname", 0xFFFF80, "server"); 
        else if(m_teammode) g.text("frags\tpj\tping\tteam\tname", 0xFFFF80, "server");
        else g.text("frags\tpj\tping\tname", 0xFFFF80, "server");

        vector<fpsent *> sbplayers;
        sortplayers(sbplayers);
      
        vector<teamscore> teamscores; 
        if(m_teammode)
        {
            sortteams(teamscores);
            vector<fpsent *> teamplayers;
            loopv(teamscores)
            {
                const char *team = teamscores[i].team;
                loopvj(sbplayers) if(sbplayers[j]->state!=CS_SPECTATOR && !strcmp(sbplayers[j]->team, team)) teamplayers.add(sbplayers[j]);
            }
            loopv(sbplayers) if(sbplayers[i]->state==CS_SPECTATOR) teamplayers.add(sbplayers[i]);
            sbplayers = teamplayers;
        }
 
        loopv(sbplayers) 
        {
            fpsent *o = sbplayers[i];
            const char *status = "";
            if(o->privilege) status = o->privilege>=PRIV_ADMIN ? "\f6" : "\f0";
            else if(o->state==CS_DEAD) status = "\f4";
            string name, team;
            if(cl.duplicatename(o)) s_sprintf(name)("%s%s", status, cl.colorname(o));
            else if(showclientnum() || cl.player1->privilege>=PRIV_MASTER) s_sprintf(name)("%s%s \f0(%d)", status, cl.colorname(o), o->clientnum);
            else s_sprintf(name)("%s%s", status, cl.colorname(o));
            if(m_teammode)
            {
                if(o->state==CS_SPECTATOR) s_strcpy(team, "\t");
                else s_sprintf(team)("%s%s\f9\t", isteam(cl.player1->team, o->team) ? "\f1" : "\f3", o->team);
            }
            else team[0] = '\0';
            string line;
            if(o->state==CS_SPECTATOR) s_sprintf(line)("SPECTATOR\t\t%s%s", team, name);
            else
            {
                s_sprintfd(lag)("%d", o->plag);
                string score;
                if(m_capture) 
                { 
                    int total = cl.cpc.findscore(o->team).total; 
                    if(total>=10000) s_strcpy(score, "WIN");
                    else s_sprintf(score)("%d", total);
                }
                else s_sprintf(score)("%d", o->frags);
                s_sprintf(line)("%s\t%s\t%d\t%s%s", score, o->state==CS_LAGGED ? "LAG" : lag, o->ping, team, name);
            }
            g.text(line, 0xFFFFDD, o->state!=CS_SPECTATOR && m_teammode ? (isteam(cl.player1->team, o->team) ? "player_blue" : "player_red") : "player");
        }

        if(m_teammode)
        {
            while(teamscores.length() && teamscores.last().score <= 0) teamscores.drop();
            if(teamscores.length())
            {
                string teamline;
                teamline[0] = 0;
                loopvj(teamscores)
                {
                    if(j >= 4) break;
                    string score;
                    if(m_capture && teamscores[j].score>=10000) s_strcpy(score, "WIN");
                    else s_sprintf(score)("%d", teamscores[j].score);
                    s_sprintfd(s)("[ %s: %s ]", teamscores[j].team, score);
                    s_strcat(teamline, s);
                }
                g.text(teamline, 0xFFFF40);
            }
        }
        
        g.end();
    }
    
    void show()
    {
        if(scoreson) 
        {
            g3d_addgui(this, menupos, true);
        }
    }
};
