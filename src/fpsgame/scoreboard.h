// creation of scoreboard

struct scoreboard : g3d_callback
{
    bool scoreson;
    vec menupos;
    int menustart;
    fpsclient &cl;

    IVARP(showclientnum, 0, 0, 1);
    IVARP(showpj, 0, 1, 1);
    IVARP(showping, 0, 1, 1);
    IVARP(showspectators, 0, 1, 1);

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

    void sortplayers(vector<fpsent *> &sbplayers, vector<fpsent *> *spectators = NULL)
    {
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && o->type!=ENT_AI)
            {
                if(spectators && o->state==CS_SPECTATOR) spectators->add(o);
                else sbplayers.add(o);
            }
        }

        sbplayers.sort(playersort);
        if(spectators) spectators->sort(playersort);
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

        vector<fpsent *> sbplayers, spectators;
        sortplayers(sbplayers, &spectators);
      
        vector<teamscore> teamscores; 
        if(m_teammode)
        {
            sortteams(teamscores);
            vector<fpsent *> teamplayers;
            loopv(teamscores)
            {
                const char *team = teamscores[i].team;
                loopvj(sbplayers) if(!strcmp(sbplayers[j]->team, team)) teamplayers.add(sbplayers[j]);
            }
            sbplayers = teamplayers;
        }

        g.pushlist();
    
        g.pushlist();
        g.strut(7);
        g.text(m_capture ? "score" : "frags", 0xFFFF80, "server");
        loopv(sbplayers) 
        { 
            fpsent *o = sbplayers[i];
            string score;
            if(o->state==CS_SPECTATOR) score[0] = '\0';
            else if(m_capture)
            {
                int total = cl.cpc.findscore(o->team).total;
                if(total>=10000) s_strcpy(score, "WIN");
                else s_sprintf(score)("%d", total);
            }
            else s_sprintf(score)("%d", o->frags);
            g.text(score, 0xFFFFDD, cl.fr.ogro() ? "ogro" : (m_teammode ? (isteam(cl.player1->team, o->team) ? "player_blue" : "player_red") : "player"));
        }
        g.poplist();

        if(multiplayer(false))
        {
            if(showpj())
            {
                g.pushlist();
                g.strut(4);
                g.text("pj", 0xFFFF80);
                loopv(sbplayers)
                {
                    fpsent *o = sbplayers[i];
                    if(o->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                    else g.textf("%d", 0xFFFFDD, NULL, o->plag);
                }
                g.poplist();
            }
    
            if(showping())
            {
                g.pushlist();
                g.text("ping", 0xFFFF80);
                g.strut(5);
                loopv(sbplayers) g.textf("%d", 0xFFFFDD, NULL, sbplayers[i]->ping);
                g.poplist();
            }
        }

        if(m_teammode)
        {
            g.pushlist();
            g.text("team", 0xFFFF80);
            g.strut(5);
            loopv(sbplayers) g.text(sbplayers[i]->team, isteam(cl.player1->team, sbplayers[i]->team) ? 0x60A0FF : 0xFF4040);
            g.poplist();
        }

        g.pushlist();
        g.text("name", 0xFFFF80);
        loopv(sbplayers)
        {
            fpsent *o = sbplayers[i];
            int status = 0xFFFFDD;
            if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
            else if(o->state==CS_DEAD) status = 0x808080;
            g.text(cl.colorname(o), status);
        }
        g.poplist();

        if(showclientnum() || cl.player1->privilege>=PRIV_MASTER)
        {
            g.space(1);
            g.pushlist();
            g.text("cn", 0xFFFF80);
            loopv(sbplayers) g.textf("%d", 0xFFFFDD, NULL, sbplayers[i]->clientnum);
            g.poplist();
        }
            
        g.poplist();

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
        
        if(showspectators() && spectators.length())
        {
            if(showclientnum() || cl.player1->privilege>=PRIV_MASTER)
            {
                g.pushlist();
                
                g.pushlist();
                g.text("spectator", 0xFFFF80, "server");
                loopv(spectators) g.text(cl.colorname(spectators[i]), 0xFFFFDD, cl.fr.ogro() ? "ogro" : "player");
                g.poplist();

                g.space(1);
                g.pushlist();
                g.text("cn", 0xFFFF80);
                loopv(spectators) g.textf("%d", 0xFFFFDD, NULL, spectators[i]->clientnum);
                g.poplist();

                g.poplist();
            }
            else
            {
                g.textf("%d spectator%s", 0xFFFF80, "server", spectators.length(), spectators.length()!=1 ? "s" : "");
                loopv(spectators)
                {
                    if((i%3)==0) g.pushlist();
                    fpsent *o = spectators[i];
                    int status = 0xFFFFDD;
                    if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    g.text(cl.colorname(o), status, i%3 ? NULL : (cl.fr.ogro() ? "ogro" : "player"));
                    if(i+1<spectators.length() && (i+1)%3) g.space(1);
                    else g.poplist();
                }
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
