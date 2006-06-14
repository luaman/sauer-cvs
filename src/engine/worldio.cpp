// worldio.cpp: loading & saving of maps and savegames

#include "pch.h"
#include "engine.h"

void backup(char *name, char *backupname)
{
    remove(backupname);
    rename(name, backupname);
};

string cgzname, bakname, pcfname, mcfname;

void setnames(const char *fname)
{
    string name, pakname, mapname;
    s_strncpy(name, fname, 100);
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

ushort readushort(gzFile f)
{
    ushort t;
    gzread(f, &t, sizeof(ushort));
    endianswap(&t, sizeof(ushort), 1);
    return t;
}

void writeushort(gzFile f, ushort u)
{
    endianswap(&u, sizeof(ushort), 1);
    gzwrite(f, &u, sizeof(ushort));
}

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
            loopj(6) writeushort(f, c[i].texture[j]);
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
                uchar mask = (c[i].material != MAT_AIR ? 0x80 : 0) | (c[i].normals ? 0x40 : 0);
                loopj(6) if(c[i].surfaces[j].lmid >= LMID_RESERVED || (c[i].normals && !c[i].normals[j].normals[0].iszero())) mask |= 1 << j;
                gzputc(f, mask);
                if(c[i].material != MAT_AIR)
                    gzputc(f, c[i].material);
                loopj(6) if(mask & (1 << j))
                {
                    surfaceinfo tmp = c[i].surfaces[j];
                    endianswap(&tmp.x, sizeof(ushort), 3);
                    gzwrite(f, &tmp, sizeof(surfaceinfo));
                    if(c[i].normals) gzwrite(f, &c[i].normals[j], sizeof(surfacenormals));
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
    loopi(6) c.texture[i] = hdr.version<14 ? gzgetc(f) : readushort(f);
    if(hdr.version < 7) loopi(3) gzgetc(f); //gzread(f, c.colour, 3);
    else
    {
        uchar mask = gzgetc(f);
        if(mask & 0x80)
            c.material = gzgetc(f);
        if(mask & 0x3F)
        {
            newsurfaces(c);
            if(mask & 0x40) newnormals(c);
            loopi(6)
            {
                if(mask & (1 << i))
                {
                    gzread(f, &c.surfaces[i], sizeof(surfaceinfo));
                    endianswap(&c.surfaces[i].x, sizeof(ushort), 3);
                    if(hdr.version < 10) ++c.surfaces[i].lmid;
                    if(mask & 0x40) gzread(f, &c.normals[i], sizeof(surfacenormals));
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

void save_world(char *mname, bool nolms)
{
    if(!*mname) mname = cl->getclientmap();
    setnames(mname);
    backup(cgzname, bakname);
    gzFile f = gzopen(cgzname, "wb9");
    if(!f) { conoutf("could not write map to %s", cgzname); return; };
    hdr.version = MAPVERSION;
    hdr.numents = 0;
    const vector<extentity *> &ents = et->getents();
    loopv(ents) if(ents[i]->type!=ET_EMPTY) hdr.numents++;
    hdr.lightmaps = nolms ? 0 : lightmaps.length();
    header tmp = hdr;
    endianswap(&tmp.version, sizeof(int), 16);
    gzwrite(f, &tmp, sizeof(header));
    
    gzputc(f, (int)strlen(cl->gameident()));
    gzwrite(f, cl->gameident(), (int)strlen(cl->gameident())+1);
    writeushort(f, et->extraentinfosize());
    vector<char> extras;
    cl->writegamedata(extras);
    writeushort(f, extras.length());
    gzwrite(f, extras.getbuf(), extras.length());
    
    
    writeushort(f, texmru.length());
    loopv(texmru) writeushort(f, texmru[i]);
    char *ebuf = new char[et->extraentinfosize()];
    loopv(ents)
    {
        if(ents[i]->type!=ET_EMPTY)
        {
            entity tmp = *ents[i];
            endianswap(&tmp.o, sizeof(int), 3);
            endianswap(&tmp.attr1, sizeof(short), 5);
            gzwrite(f, &tmp, sizeof(entity));
            et->writeent(*ents[i], ebuf);
            if(et->extraentinfosize()) gzwrite(f, ebuf, et->extraentinfosize());
        };
    };
    delete[] ebuf;

    savec(worldroot, f);
    if(!nolms) loopv(lightmaps)
    {
        LightMap &lm = lightmaps[i];
        gzputc(f, lm.type);
        gzwrite(f, lm.data, sizeof(lm.data));
    };

    gzclose(f);
    conoutf("wrote map file %s", cgzname);
};

void swapXZ(cube *c)
{	
	loopi(8) 
	{
		swap(uint,   c[i].faces[0],   c[i].faces[2]);
		swap(ushort, c[i].texture[0], c[i].texture[4]);
		swap(ushort, c[i].texture[1], c[i].texture[5]);
		if(c[i].surfaces)
		{
			swap(surfaceinfo, c[i].surfaces[0], c[i].surfaces[4]);
			swap(surfaceinfo, c[i].surfaces[1], c[i].surfaces[5]);
		};
		if(c[i].children) swapXZ(c[i].children);
	};
};

void load_world(const char *mname)        // still supports all map formats that have existed since the earliest cube betas!
{
    int loadingstart = SDL_GetTicks();
    setnames(mname);
    gzFile f = gzopen(cgzname, "rb9");
    if(!f) { conoutf("could not read map %s", cgzname); return; };
    clearoverrides();
    computescreen(mname);
    gzread(f, &hdr, sizeof(header));
    endianswap(&hdr.version, sizeof(int), 16);
    if(strncmp(hdr.head, "OCTA", 4)!=0) fatal("while reading map: header malformatted");
    if(hdr.version>MAPVERSION) fatal("this map requires a newer version of cube 2");
    if(!hdr.ambient) hdr.ambient = 25;
    if(!hdr.lerpsubdivsize)
    {
        if(!hdr.lerpangle) hdr.lerpangle = 44;
        hdr.lerpsubdiv = 2;
        hdr.lerpsubdivsize = 4;
    };
    setvar("lightprecision", hdr.mapprec ? hdr.mapprec : 32);
    setvar("lighterror", hdr.maple ? hdr.maple : 8);
    setvar("lightlod", hdr.mapllod);
    setvar("lodsize", hdr.mapwlod);
    setvar("ambient", hdr.ambient);
    setvar("fullbright", 0);
    setvar("lerpangle", hdr.lerpangle);
    setvar("lerpsubdiv", hdr.lerpsubdiv);
    setvar("lerpsubdivsize", hdr.lerpsubdivsize);
    
    string gametype;
    s_strcpy(gametype, "fps");
    bool samegame = true;
    int eif = 0;
    if(hdr.version>=16)
    {
        int len = gzgetc(f);
        gzread(f, gametype, len+1);
    };
    if(strcmp(gametype, cl->gameident())!=0)
    {
        samegame = false;
        conoutf("WARNING: loading map from %s game, ignoring entities except for lights/mapmodels)", gametype);
    };
    if(hdr.version>=16)
    {
        eif = readushort(f);
        int extrasize = readushort(f);
        vector<char> extras;
        loopj(extrasize) extras.add(gzgetc(f));
        if(samegame) cl->readgamedata(extras);
    };
    
    show_out_of_renderloop_progress(0, "clearing world...");

    texmru.setsize(0);
    if(hdr.version<14)
    {
        uchar oldtl[256];
        gzread(f, oldtl, sizeof(oldtl));
        loopi(256) texmru.add(oldtl[i]);
    }
    else
    {
        ushort nummru = readushort(f);
        loopi(nummru) texmru.add(readushort(f));
    };

    freeocta(worldroot);
    worldroot = NULL;

    show_out_of_renderloop_progress(0, "loading entities...");
    vector<extentity *> &ents = et->getents();
    ents.setsize(0);

    char *ebuf = new char[et->extraentinfosize()];
    loopi(hdr.numents)
    {
        extentity &e = *et->newentity();
        ents.add(&e);
        gzread(f, &e, sizeof(entity));
        endianswap(&e.o, sizeof(int), 3);
        endianswap(&e.attr1, sizeof(short), 5);
        e.spawned = false;
        e.inoctanode = false;
        if(samegame)
        {
            if(et->extraentinfosize()) gzread(f, ebuf, et->extraentinfosize());
            et->readent(e, ebuf); 
        }
        else
        {
            loopj(eif) gzgetc(f);
            if(e.type>=ET_GAMESPECIFIC || hdr.version<=14)
            {
                ents.pop();
                continue;
            };
        };
        if(hdr.version <= 14 && e.type >= ET_MAPMODEL && e.type <= 16)
        {
            if(e.type == 16) e.type = ET_MAPMODEL;
            else e.type++;
        };
        if(!insideworld(e.o))
        {
            if(e.type != ET_LIGHT)
            {
                conoutf("warning: ent outside of world: enttype[%s] index %d (%f, %f, %f)", et->entname(e.type), i, e.o.x, e.o.y, e.o.z);
            };
        };
        if(hdr.version <= 14 && e.type == ET_MAPMODEL)
        {
            e.o.z += e.attr3;
            if(e.attr4) conoutf("warning: mapmodel ent (index %d) uses texture slot %d", i, e.attr4);
            e.attr3 = e.attr4 = 0;
        };
    };
    delete[] ebuf;

    show_out_of_renderloop_progress(0, "loading octree...");
    worldroot = loadchildren(f);

	if(hdr.version <= 11)
		swapXZ(worldroot);

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
            if(hdr.version >= 17) lm.type = gzgetc(f);
            gzread(f, lm.data, 3 * LM_PACKW * LM_PACKH);
            lm.finalize();
        };
        initlights();
    };

    gzclose(f);

    allchanged();

    conoutf("read map %s (%.1f seconds)", cgzname, (SDL_GetTicks()-loadingstart)/1000.0f);
    conoutf("%s", hdr.maptitle);
    estartmap(mname);
    overrideidents = true;
    execfile("data/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);
    overrideidents = false;

    precacheall();

    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type==ET_MAPMODEL && e.attr2 >= 0)
        {
            mapmodelinfo &mmi = getmminfo(e.attr2);
            if(!&mmi) conoutf("could not find map model: %d", e.attr2);
            else if(!loadmodel(mmi.name)) conoutf("could not load model: %s", mmi.name);
        };
    };
    entitiesinoctanodes();
};

void savecurrentmap() { save_world(cl->getclientmap()); };
void savemap(char *mname) { save_world(mname); };

COMMAND(savemap, "s");
COMMAND(savecurrentmap, "");


