#include "pch.h"
#include "engine.h"

VAR(importcuberemip, 0, 1024, 2048);

struct cubeloader
{
    enum                              // block types, order matters!
    {
        C_SOLID = 0,                  // entirely solid cube [only specifies wtex]
        C_CORNER,                     // half full corner of a wall
        C_FHF,                        // floor heightfield using neighbour vdelta values
        C_CHF,                        // idem ceiling
        C_SPACE,                      // entirely empty cube
        C_SEMISOLID,                  // generated by mipmapping
        C_MAXTYPE
    };
    
    struct c_sqr
    {
        uchar type;                 // one of the above
        char floor, ceil;           // height, in cubes
        uchar wtex, ftex, ctex;     // wall/floor/ceil texture ids
        uchar vdelta;               // vertex delta, used for heightfield cubes
        uchar utex;                 // upper wall tex id
    };

    struct c_persistent_entity        // map entity
    {
        short x, y, z;              // cube aligned position
        short attr1;
        uchar type;                 // type is one of the above
        uchar attr2, attr3, attr4;
    };

    struct c_header                   // map file format header
    {
        char head[4];               // "CUBE"
        int version;                // any >8bit quantity is a little indian
        int headersize;             // sizeof(header)
        int sfactor;                // in bits
        int numents;
        char maptitle[128];
        uchar texlists[3][256];
        int waterlevel;
        int reserved[15];
    };

    c_sqr *world;
    int ssize;
    int x0, x1, y0, y1, z0, z1;
    c_sqr *o[4];
    int lastremip;
    int progress;

    void create_ent(c_persistent_entity &ce)
    {
        if(ce.type>=7) ce.type++;  // grenade ammo
        if(ce.type>=8) ce.type++;  // pistol ammo
        if(ce.type==16) ce.type = ET_MAPMODEL;
        else if(ce.type>=ET_MAPMODEL && ce.type<16) ce.type++;
        if(ce.type>=ET_ENVMAP) ce.type++;
        if(ce.type>=ET_PARTICLES) ce.type++; 
        if(ce.type>=ET_SOUND) ce.type++;
        if(ce.type>=ET_SPOTLIGHT) ce.type++;
        extentity &e = *et->newentity();
        et->getents().add(&e);
        e.type = ce.type;
        e.spawned = false;
        e.inoctanode = false;
        e.o = vec(ce.x*4+hdr.worldsize/4, ce.y*4+hdr.worldsize/4, (ce.z+ce.attr3)*4+hdr.worldsize/2);
        e.color[0] = e.color[1] = e.color[2] = 255;
        e.attr1 = ce.attr1;
        e.attr2 = ce.attr2;
        if(e.type == ET_MAPMODEL) e.attr3 = e.attr4 = 0;
        else
        {
            e.attr3 = ce.attr3;
            e.attr4 = ce.attr4;
        }
        e.attr5 = 0;
    }

    cube &getcube(int x, int y, int z)
    {
        return lookupcube(x*4+hdr.worldsize/4, y*4+hdr.worldsize/4, z*4+hdr.worldsize/2, 4);
    }

    int neighbours(c_sqr &t)
    {
        o[0] = &t;
        o[1] = &t+1;
        o[2] = &t+ssize;
        o[3] = &t+ssize+1;
        int best = 0xFFFF;
        loopi(4) if(o[i]->vdelta<best) best = o[i]->vdelta;
        return best;
    }

    void preprocess_cubes()     // pull up heighfields to where they don't cross cube boundaries
    {
        for(;;)
        {
            bool changed = false;
            loop(x, ssize)
            {
                loop(y, ssize)
                {
                    c_sqr &t = world[x+y*ssize];
                    if(t.type==C_FHF || t.type==C_CHF)
                    {
                        int bottom = (neighbours(t)&(~3))+4;
                        loopj(4) if(o[j]->vdelta>bottom) { o[j]->vdelta = bottom; changed = true; }
                    }
                }
            }
            if(!changed) break;
        }
    }

    int getfloorceil(c_sqr &s, int &floor, int &ceil)
    {
        floor = s.floor;
        ceil = s.ceil;
        int cap = 0;
        switch(s.type)
        {
            case C_SOLID: floor = ceil; break;
            case C_FHF: floor -= (cap = neighbours(s)&(~3))/4; break;
            case C_CHF: ceil  += (cap = neighbours(s)&(~3))/4; break;
        }
        return cap;
    }

    void boundingbox()
    {
        x0 = y0 = ssize;
        x1 = y1 = 0;
        z0 = 128;
        z1 = -128;
        loop(x, ssize) loop(y, ssize)
        {
            c_sqr &t = world[x+y*ssize];
            if(t.type!=C_SOLID)
            {
                if(x<x0) x0 = x;
                if(y<y0) y0 = y;
                if(x>x1) x1 = x;
                if(y>y1) y1 = y;
                int floor, ceil;
                getfloorceil(t, floor, ceil);
                if(floor<z0) z0 = floor;
                if(ceil>z1) z1 = ceil;
            }
        }
    }

    void hf(int x, int y, int z, int side, int dir, int cap)
    {
        cube &c = getcube(x, y, z);
        loopi(2) loopj(2) edgeset(cubeedge(c, 2, i, j), side, dir*(o[(j<<1)+i]->vdelta-cap)*2+side*8);
    }

    bool cornersolid(int z, c_sqr *s) { return s->type==C_SOLID || z<s->floor || z>=s->ceil; }

    void createcorner(cube &c, int lstart, int lend, int rstart, int rend)
    {
        int ledge = edgemake(lstart, lend);
        int redge = edgemake(rstart, rend);
        cubeedge(c, 1, 0, 0) = ledge;
        cubeedge(c, 1, 1, 0) = ledge;
        cubeedge(c, 1, 0, 1) = redge;
        cubeedge(c, 1, 1, 1) = redge;
    }

    void create_cubes()
    {
        preprocess_cubes();
        boundingbox();
        lastremip = allocnodes;
        progress = 0;
        for(int x = x0-1; x<=x1+1; x++) for(int y = y0-1; y<=y1+1; y++)
        {
            c_sqr &s = world[x+y*ssize];
            int floor, ceil, cap = getfloorceil(s, floor, ceil);
            for(int z = z0-1; z<=z1+1; z++)
            {
                cube &c = getcube(x, y, z);
                c.texture[O_LEFT] = c.texture[O_RIGHT] = c.texture[O_BACK] = c.texture[O_FRONT] = s.type!=C_SOLID && z<ceil ? s.wtex : s.utex;
                c.texture[O_BOTTOM] = s.ctex;
                c.texture[O_TOP] = s.ftex;
                if(z>=floor && z<ceil)
                {
                    setfaces(c, F_EMPTY);
                }
                else if(s.type==C_CORNER)
                {
                    c_sqr *ts, *bs, *ls, *rs;
                    bool tc = cornersolid(z, ts = &s-ssize);
                    bool bc = cornersolid(z, bs = &s+ssize);
                    bool lc = cornersolid(z, ls = &s-1);
                    bool rc = cornersolid(z, rs = &s+1);
                    if     (tc && lc && !bc && !rc) createcorner(c, 0, 8, 0, 0);    // TOP LEFT
                    else if(tc && !lc && !bc && rc) createcorner(c, 0, 0, 0, 8);    // TOP RIGHT
                    else if(!tc && lc && bc && !rc) createcorner(c, 0, 8, 8, 8);    // BOT LEFT
                    else if(!tc && !lc && bc && rc) createcorner(c, 8, 8, 0, 8);    // BOT RIGHT
                    else        // fix texture on ground of a corner
                    {
                        if      (ts->floor-1==z && bs->floor-1!=z) { c.texture[O_TOP] = ts->ftex; }
                        else if (ts->floor-1!=z && bs->floor-1==z) { c.texture[O_TOP] = bs->ftex; }
                        if      (ts->ceil==z && bs->ceil!=z)       { c.texture[O_BOTTOM] = ts->ctex; }
                        else if (ts->ceil!=z && bs->ceil==z)       { c.texture[O_BOTTOM] = bs->ctex; }
                    }
                }
            }
            switch(s.type)
            {
                case C_FHF: hf(x, y, floor-1, 1, -1, cap); break;
                case C_CHF: hf(x, y, ceil, 0, 1, cap); break;
            }
            if(importcuberemip && (allocnodes - lastremip) * 8 > importcuberemip * 1024)
            {
                remipworld();
                lastremip = allocnodes;
            }
            if((progress++&0x7F)==0)
            {
                float bar2 = float((y1-y0+2)*(x-x0+1) + y-y0+1) / float((y1-y0+2)*(x1-x0+2));
                s_sprintfd(text2)("%d%%", int(bar2*100));
                show_out_of_renderloop_progress(0, "creating cubes...", bar2, text2);
            }
        }
    }

    void load_cube_world(char *mname)
    {
        string pakname, cgzname;
        s_sprintf(pakname)("cube/%s", mname);
        s_sprintf(cgzname)("packages/%s.cgz", pakname);
        gzFile f = gzopen(path(cgzname), "rb9");
        if(!f) { conoutf("could not read cube map %s", cgzname); return; }
        emptymap(12, true);
        freeocta(worldroot);
        worldroot = newcubes(F_SOLID);
        c_header hdr;
        gzread(f, &hdr, sizeof(c_header)-sizeof(int)*16);
        endianswap(&hdr.version, sizeof(int), 4);
        if(strncmp(hdr.head, "CUBE", 4)!=0) fatal("while reading map: header malformatted");
        if(hdr.version>5) fatal("this map requires a newer version of sauerbraten");
        s_sprintfd(cs)("importing %s", cgzname);
        computescreen(cs);
        if(hdr.version>=4)
        {
            gzread(f, &hdr.waterlevel, sizeof(int)*16);
            endianswap(&hdr.waterlevel, sizeof(int), 16);
        }
        else
        {
            hdr.waterlevel = -100000;
        }
        loopi(hdr.numents)
        {
            c_persistent_entity e;
            gzread(f, &e, sizeof(c_persistent_entity));
            endianswap(&e, sizeof(short), 4);
            create_ent(e);
        }
        ssize = 1<<hdr.sfactor;
        world = new c_sqr[ssize*ssize];
        c_sqr *t = NULL;
        loopk(ssize*ssize)
        {
            c_sqr *s = &world[k];
            int type = gzgetc(f);
            switch(type)
            {
                case 255:
                {
                    int n = gzgetc(f);
                    for(int i = 0; i<n; i++, k++) memcpy(&world[k], t, sizeof(c_sqr));
                    k--;
                    break;
                }
                case 254: // only in MAPVERSION<=2
                {
                    memcpy(s, t, sizeof(c_sqr));
                    gzgetc(f);
                    gzgetc(f);
                    break;
                }
                case C_SOLID:
                {
                    s->type = C_SOLID;
                    s->wtex = gzgetc(f);
                    s->vdelta = gzgetc(f);
                    if(hdr.version<=2) { gzgetc(f); gzgetc(f); }
                    s->ftex = DEFAULT_FLOOR;
                    s->ctex = DEFAULT_CEIL;
                    s->utex = s->wtex;
                    s->floor = 0;
                    s->ceil = 16;
                    break;
                }
                default:
                {
                    if(type<0 || type>=C_MAXTYPE)
                    {
                        s_sprintfd(t)("%d @ %d", type, k);
                        fatal("while reading map: type out of range: ", t);
                    }
                    s->type = type;
                    s->floor = gzgetc(f);
                    s->ceil = gzgetc(f);
                    if(s->floor>=s->ceil) s->floor = s->ceil-1;  // for pre 12_13
                    s->wtex = gzgetc(f);
                    s->ftex = gzgetc(f);
                    s->ctex = gzgetc(f);
                    if(hdr.version<=2) { gzgetc(f); gzgetc(f); }
                    s->vdelta = gzgetc(f);
                    s->utex = (hdr.version>=2) ? gzgetc(f) : s->wtex;
                    if(hdr.version>=5) gzgetc(f);
                    s->type = type;
                }
            }
            t = s;
        }
        gzclose(f);

        string cfgname;
        s_sprintf(cfgname)("packages/cube/%s.cfg", mname);
        exec("packages/cube/package.cfg");
        exec(path(cfgname));
        create_cubes();
        remipworld();
        clearlights();
        allchanged();
        loopv(et->getents()) if(et->getents()[i]->type!=ET_LIGHT) dropenttofloor(et->getents()[i]);
        entitiesinoctanodes();
        conoutf("read cube map %s (%d milliseconds)", cgzname, SDL_GetTicks()-totalmillis);
        startmap(pakname);
    }
};

void importcube(char *name) { cubeloader().load_cube_world(name); }
COMMAND(importcube, "s");
