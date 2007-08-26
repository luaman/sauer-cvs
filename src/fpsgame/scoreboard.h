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
        const char *team;
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

    void bestplayers(vector<fpsent *> &best)
    {
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && o->type!=ENT_AI && o->state!=CS_SPECTATOR) best.add(o);
        }
        best.sort(playersort);   
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

    void bestteams(vector<const char *> &best)
    {
        vector<teamscore> teamscores;
        sortteams(teamscores);
        while(teamscores.length()>1 && teamscores.last().score < teamscores[0].score) teamscores.drop();
        loopv(teamscores) best.add(teamscores[i].team);
    }

    struct scoregroup : teamscore
    {
        vector<fpsent *> players;
    };
    vector<scoregroup *> groups;
    vector<fpsent *> spectators;

    static int scoregroupcmp(const scoregroup **x, const scoregroup **y)
    {
        if((*x)->score > (*y)->score) return -1;
        if((*x)->score < (*y)->score) return 1;
        if(!(*x)->team) return (*y)->team ? 1 : 0;
        return (*y)->team ? strcmp((*x)->team, (*y)->team) : -1;
    }

    int groupplayers()
    {
        int gamemode = cl.gamemode, numgroups = 0;
        spectators.setsize(0);
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(!o || o->type==ENT_AI) continue;
            if(o->state==CS_SPECTATOR) { spectators.add(o); continue; }
            const char *team = m_teammode ? o->team : NULL;
            bool found = false;
            loopj(numgroups)
            {
                scoregroup &g = *groups[j];
                if(team!=g.team && (!team || !g.team || strcmp(team, g.team))) continue;
                if(m_teammode && !m_capture) g.score += o->frags;
                g.players.add(o);
                found = true;
            }
            if(found) continue;
            if(numgroups>=groups.length()) groups.add(new scoregroup);
            scoregroup &g = *groups[numgroups++];
            g.team = team;
            g.score = m_capture ? cl.cpc.findscore(o->team).total : (m_teammode ? o->frags : 0);
            g.players.setsize(0);
            g.players.add(o);
        }
        loopi(numgroups) groups[i]->players.sort(playersort);
        spectators.sort(playersort);
        groups.sort(scoregroupcmp, 0, numgroups);
        return numgroups;
    }

    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menustart, 0.03f, NULL, false);
   
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

        int numgroups = groupplayers();
        loopk(numgroups)
        {
            if((k%2)==0) g.pushlist(); // horizontal
            
            scoregroup &sg = *groups[k];
            const char *icon = cl.fr.ogro() ? "ogro" : (sg.team && m_teammode ? (isteam(cl.player1->team, sg.team) ? "player_blue" : "player_red") : "player");
            int color = sg.team && m_teammode ? (isteam(cl.player1->team, sg.team) ? 0x60A0FF : 0xFF4040) : 0xFFFF80;
            
            g.pushlist(); // vertical

            if(sg.team && m_teammode)
            {
                int color = isteam(cl.player1->team, sg.team) ? 0x60A0FF : 0xFF4040;
                if(m_capture && sg.score>=10000) g.textf("%s: WIN", color, "server", sg.team);
                else g.textf("%s: %d", color, "server", sg.team, sg.score);
            }

            g.pushlist(); // horizontal

            if(!m_capture)
            { 
                g.pushlist();
                g.strut(7);
                g.text("frags", color, icon ? "server" : NULL);
                loopv(sg.players) g.textf("%d", 0xFFFFDD, icon, sg.players[i]->frags);
                g.poplist();
                icon = NULL;
            }

            if(multiplayer(false))
            {
                if(showpj())
                {
                    g.pushlist();
                    g.strut(6);
                    g.text("pj", color, icon ? "server" : NULL);
                    loopv(sg.players)
                    {
                        fpsent *o = sg.players[i];
                        if(o->state==CS_LAGGED) g.text("LAG", 0xFFFFDD, icon);
                        else g.textf("%d", 0xFFFFDD, icon, o->plag);
                    }
                    g.poplist();
                    icon = NULL;
                }
        
                if(showping())
                {
                    g.pushlist();
                    g.text("ping", color, icon ? "server" : NULL);
                    g.strut(6);
                    loopv(sg.players) g.textf("%d", 0xFFFFDD, icon, sg.players[i]->ping);
                    g.poplist();
                    icon = NULL;
                }
            }

            g.pushlist();
            g.text("name", color, icon ? "server" : NULL);
            loopv(sg.players)
            {
                fpsent *o = sg.players[i];
                int status = 0xFFFFDD;
                if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                else if(o->state==CS_DEAD) status = 0x808080;
                g.text(cl.colorname(o), status, icon);
            }
            g.poplist();
            icon = NULL;

            if(showclientnum() || cl.player1->privilege>=PRIV_MASTER)
            {
                g.space(1);
                g.pushlist();
                g.text("cn", color);
                loopv(sg.players) g.textf("%d", 0xFFFFDD, NULL, sg.players[i]->clientnum);
                g.poplist();
            }
                
            g.poplist(); // horizontal
            g.poplist(); // vertical

            if(k+1<numgroups && (k+1)%2) g.space(1);
            else g.poplist(); // horizontal
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
