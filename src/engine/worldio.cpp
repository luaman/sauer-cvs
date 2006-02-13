// worldio.cpp: loading & saving of maps and savegames

#include "pch.h"
#include "engine.h"

void backup(char *name, char *backupname)
{
    remove(backupname);
    rename(name, backupname);
};

string cgzname, bakname, pcfname, mcfname;

void setnames(char *name)
{
    string pakname, mapname;
    char *slash = strpbrk(name, "/\\");
    if(slash)
    {
        s_strncpy(pakname, name, slash-name+1);
        s_strcpy(mapname, slash+1);
    }
    else
    {
        s_strcpy(pakname, "base");
        s_strcpy(mapname, name);
    };
    s_sprintf(cgzname)("packages/%s/%s.ogz",      pakname, mapname);
    s_sprintf(bakname)("packages/%s/%s_%d.BAK",   pakname, mapname, lastmillis);
    s_sprintf(pcfname)("packages/%s/package.cfg", pakname);
    s_sprintf(mcfname)("packages/%s/%s.cfg",      pakname, mapname);

    path(cgzname);
    path(bakname);
};

// save map as .cgz file. uses 2 layers of compression: first does simple run-length
// encoding and leaves out data for certain kinds of cubes, then zlib removes the
// last bits of redundancy. Both passes contribute greatly to the miniscule map sizes.

enum { OCTSAV_CHILDREN = 0, OCTSAV_EMPTY, OCTSAV_SOLID, OCTSAV_NORMAL, OCTSAV_LODCUBE };

void savec(cube *c, gzFile f)
{
    loopi(8)
    {
        if(c[i].children && !c[i].surfaces)
        {
            gzputc(f, OCTSAV_CHILDREN);
            savec(c[i].children, f);
        }
        else
        {
            if(c[i].children) gzputc(f, OCTSAV_LODCUBE);
            else if(isempty(c[i])) gzputc(f, OCTSAV_EMPTY);
            else if(isentirelysolid(c[i])) gzputc(f, OCTSAV_SOLID);
            else
            {
                gzputc(f, OCTSAV_NORMAL);
                gzwrite(f, c[i].edges, 12);
            };
            gzwrite(f, c[i].texture, 6);
            //loopj(3) gzputc(f, 0); //gzwrite(f, c[i].colour, 3);
            // save surface info for lighting
            if(!c[i].surfaces)
            {
                if(c[i].material != MAT_AIR)
                {
                    gzputc(f, 0x80);
                    gzputc(f, c[i].material);
                }
                else
                    gzputc(f, 0);
            }
            else
            {
                uchar mask = (c[i].material != MAT_AIR ? 0x80 : 0);
                loopj(6) if(c[i].surfaces[j].lmid >= LMID_RESERVED) mask |= 1 << j;
                gzputc(f, mask);
                if(c[i].material != MAT_AIR)
                    gzputc(f, c[i].material);
                loopj(6) if(c[i].surfaces[j].lmid >= LMID_RESERVED)
                {
                    surfaceinfo tmp = c[i].surfaces[j];
                    endianswap(&tmp.x, sizeof(ushort), 3);
                    gzwrite(f, &tmp, sizeof(surfaceinfo));
                };
            };
            if(c[i].children) savec(c[i].children, f);
        };
    };
};

cube *loadchildren(gzFile f);

void loadc(gzFile f, cube &c)
{
    bool haschildren = false;
    switch(gzgetc(f))
    {
        case OCTSAV_CHILDREN:
            c.children = loadchildren(f);
            return;

        case OCTSAV_LODCUBE: haschildren = true;    break;
        case OCTSAV_EMPTY:  emptyfaces(c);          break;
        case OCTSAV_SOLID:  solidfaces(c);          break;
        case OCTSAV_NORMAL: gzread(f, c.edges, 12); break;

        default:
            fatal("garbage in map");
    };
    gzread(f, c.texture, 6);
    if(hdr.version < 7) loopi(3) gzgetc(f); //gzread(f, c.colour, 3);
    else
    {
        uchar mask = gzgetc(f);
        if(mask & 0x80)
            c.material = gzgetc(f);
        mask &= 0x7F;
        if(mask)
        {
            c.surfaces = new surfaceinfo[6];
            loopi(6)
            {
                if(mask & (1 << i))
                {
                    gzread(f, &c.surfaces[i], sizeof(surfaceinfo));
                    endianswap(&c.surfaces[i].x, sizeof(ushort), 3);
                    if(hdr.version < 10) ++c.surfaces[i].lmid;
                }
                else
                    c.surfaces[i].lmid = LMID_AMBIENT;
            };
        };
    };
    c.children = (haschildren ? loadchildren(f) : NULL);
};

cube *loadchildren(gzFile f)
{
    cube *c = newcubes();
    loopi(8) loadc(f, c[i]);
    // TODO: remip c from children here
    return c;
};

void save_world(char *mname)
{
    if(!*mname) mname = cl->getclientmap();
    setnames(mname);
    backup(cgzname, bakname);
    gzFile f = gzopen(cgzname, "wb9");
    if(!f) { conoutf("could not write map to %s", cgzname); return; };
    hdr.version = MAPVERSION;
    hdr.numents = 0;
    loopv(et->getents()) if(et->getents()[i]->type!=ET_EMPTY) hdr.numents++;
    hdr.lightmaps = lightmaps.length();
    header tmp = hdr;
    endianswap(&tmp.version, sizeof(int), 16);
    gzwrite(f, &tmp, sizeof(header));
    loopv(et->getents())
    {
        if(et->getents()[i]->type!=ET_EMPTY)
        {
            entity tmp = *et->getents()[i];
            endianswap(&tmp.o, sizeof(int), 3);
            endianswap(&tmp.attr1, sizeof(short), 5);
            gzwrite(f, &tmp, sizeof(entity));
            et->writeent(*et->getents()[i]);
        };
    };

    savec(worldroot, f);
    loopv(lightmaps)
    {
        LightMap &lm = lightmaps[i];
        gzwrite(f, lm.data, sizeof(lm.data));
    }

    gzclose(f);
    conoutf("wrote map file %s", cgzname);
};

void load_world(char *mname)        // still supports all map formats that have existed since the earliest cube betas!
{
    setnames(mname);
    gzFile f = gzopen(cgzname, "rb9");
    if(!f) { conoutf("could not read map %s", cgzname); return; };
    computescreen(mname);
    gzread(f, &hdr, sizeof(header));
    endianswap(&hdr.version, sizeof(int), 16);
    if(strncmp(hdr.head, "OCTA", 4)!=0) fatal("while reading map: header malformatted");
    if(hdr.version>MAPVERSION) fatal("this map requires a newer version of sauerbraten");
    if(!hdr.ambient) hdr.ambient = 25;
    setvar("lightprecision", hdr.mapprec ? hdr.mapprec : 32);
    setvar("lighterror", hdr.maple ? hdr.maple : 8);
    setvar("lightlod", hdr.mapllod);
    setvar("lodsize", hdr.mapwlod);
    setvar("ambient", hdr.ambient);
    setvar("fullbright", 0);

    show_out_of_renderloop_progress(0, "loading entities...");
    et->getents().setsize(0);

    loopi(hdr.numents)
    {
        extentity &e = *et->newentity();
        et->getents().add(&e);
        gzread(f, &e, sizeof(entity));
        endianswap(&e.o, sizeof(int), 3);
        endianswap(&e.attr1, sizeof(short), 5);
        e.spawned = false;
        e.inoctanode = false;
        et->readent(e);
        if(e.o.x<0 || e.o.x>hdr.worldsize ||
           e.o.y<0 || e.o.y>hdr.worldsize ||
           e.o.z<0 || e.o.z>hdr.worldsize)
        {
            if(e.type != ET_LIGHT)
            {
                conoutf("warning: ent outside of world: enttype[%d] index %d (%f, %f, %f)", e.type, i, e.o.x, e.o.y, e.o.z);
                //et->getents().pop();
            };
        };
    };

    show_out_of_renderloop_progress(0, "clearing world...");
    freeocta(worldroot);

    show_out_of_renderloop_progress(0, "loading octree...");
    worldroot = loadchildren(f);

    if(hdr.version <= 8)
        converttovectorworld();

    show_out_of_renderloop_progress(0, "validating...");
    validatec(worldroot, hdr.worldsize>>1);

    resetlightmaps();
    if(hdr.version < 7 || !hdr.lightmaps) clearlights();
    else 
    {
        loopi(hdr.lightmaps)
        {
            show_out_of_renderloop_progress(i/(float)hdr.lightmaps, "loading lightmaps...");
            LightMap &lm = lightmaps.add();
            gzread(f, lm.data, 3 * LM_PACKW * LM_PACKH);
            lm.finalize();
        };
        initlights();
    };

    gzclose(f);

    allchanged();

    conoutf("read map %s (%d milliseconds)", cgzname, SDL_GetTicks()-lastmillis);
    conoutf("%s", hdr.maptitle);
    estartmap(mname);
    execfile("data/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);

    precacheall();

    loopv(et->getents())
    {
        extentity &e = *et->getents()[i];
        if(e.type==ET_MAPMODEL)
        {
            mapmodelinfo &mmi = getmminfo(e.attr2);
            if(!&mmi) conoutf("could not find map model: %d", e.attr2);
            else loadmodel(mmi.name);
        };
    };
    entitiesinoctanodes();
};

void savecurrentmap() { save_world(cl->getclientmap()); };

COMMANDN(savemap, save_world, ARG_1STR);
COMMAND(savecurrentmap, ARG_NONE);


