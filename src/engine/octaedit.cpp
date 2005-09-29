#include "pch.h"
#include "engine.h"

void boxs(int d, int x, int y, int xs, int ys, int z)
{
    int v[4][3] =
    {
        {z, y,    x},
        {z, y,    x+xs},
        {z, y+ys, x+xs},
        {z, y+ys, x}
    };
    glBegin(GL_POLYGON);
    loopi(4) glVertex3i(v[i][X(d)], v[i][Z(d)], v[i][Y(d)]);
    glEnd();
};

block3 sel;
block3 lastsel;

int orient = 0;
int gridsize = 32;
int corner = 0;
ivec cor, lastcor;
ivec cur, lastcur;
int selcx, selcxs, selcy, selcys;

bool editmode = false;
bool havesel = false;
bool dragging = false;

void forcenextundo() { lastsel.orient = -1; };
void cancelsel()     { havesel = false; forcenextundo(); };

VARF(gridpower, 2, 5, 16,
{
    if(dragging) return;
    gridsize = 1<<gridpower;
    if(gridsize>=hdr.worldsize) gridsize = hdr.worldsize/2;
    cancelsel();
});

void editdrag(bool on)
{
    if(dragging = on)
    {
        cancelsel();
        if(cor[0]<0) return;
        lastcur = cur;
        lastcor = cor;
        sel.grid = gridsize;
        sel.orient = orient;
    };
};

void reorient()
{
    selcx = 0;
    selcy = 0;
    selcxs = sel.s[R(dimension(orient))]*2;
    selcys = sel.s[C(dimension(orient))]*2;
    sel.orient = orient;
};

VAR(editing,0,0,1);

void toggleedit()
{
    if(player->state==CS_DEAD) return;                 // do not allow dead players to edit to avoid state confusion
    if(!editmode && !cc->allowedittoggle()) return;         // not in most multiplayer modes
    if(!(editmode = !editmode))
    {
        cl->entinmap(player, false);                       // find spawn closest to current floating pos
    }
    else
    {
        cl->resetgamestate();
    };
    cancelsel();
    keyrepeat(editmode);
    editing = editmode;
};

COMMAND(reorient, ARG_NONE);
COMMANDN(edittoggle, toggleedit, ARG_NONE);

bool noedit()
{
    vec o(sel.o.v);
    vec s(sel.s.v);
    s.mul(float(sel.grid) / 2.0f);
    o.add(s);
    bool viewable = (isvisiblesphere(1.0f, o.x, o.y, o.z) != VFC_NOT_VISIBLE);
    if(!editmode) conoutf("operation only allowed in edit mode");
    if(!viewable) conoutf("selection not in view");
    return !editmode || !viewable;
};

///////// selection support /////////////

cube &blockcube(int x, int y, int z, block3 &b, int rgrid) // looks up a world cube, based on coordinates mapped by the block
{
    int d = dimension(b.orient);
    int s[3] = { dimcoord(b.orient)*(b.s[d]-1)*b.grid, y*b.grid, x*b.grid };
    return neighbourcube(b.o.x+s[X(d)], b.o.y+s[Y(d)], b.o.z+s[Z(d)], -z*b.grid, rgrid, b.orient);
};

#define loopxy(b)        loop(y,(b).s[C(dimension((b).orient))]) loop(x,(b).s[R(dimension((b).orient))])
#define loopxyz(b, r, f) { loop(z,(b).s[D(dimension((b).orient))]) loopxy((b)) { cube &c = blockcube(x,y,z,b,r); f; }; }
#define loopselxyz(f)    { makeundo(); loopxyz(sel, sel.grid, f); changed(); }
#define selcube(x, y, z) blockcube(x, y, z, sel, sel.grid)

uchar octatouchblock(block3 &b, int cx, int cy, int cz, int size)
{
    uchar m = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
    if(cz+size <= b.o.z)         m &= 0xF0; // not in a -ve Z octant
    if(cz+size >= b.o.z+b.us(0)) m &= 0x0F; // not in a +ve Z octant
    if(cy+size <= b.o.y)         m &= 0xCC; // not in a -ve Y octant
    if(cy+size >= b.o.y+b.us(1)) m &= 0x33; // etc..
    if(cx+size <= b.o.x)         m &= 0xAA;
    if(cx+size >= b.o.x+b.us(2)) m &= 0x55;
    return m;
};

////////////// cursor ///////////////

int selchildcount=0;

void countselchild(cube *c=worldroot, int cx=0, int cy=0, int cz=0, int size=hdr.worldsize/2)
{
    uchar m = octatouchblock(sel, cx, cy, cz, size);
    loopi(8) if(m&(1<<i))
    {
        ivec o(i, cx, cy, cz, size);
        if(c[i].children) countselchild(c[i].children, o.x, o.y, o.z, size/2);
        else selchildcount++;
    };
};

bool selectcorners = false;
void selcorners(bool isdown) { selectcorners = isdown; editdrag(isdown); };
COMMAND(selcorners, ARG_DOWN);

void cursorupdate()
{
    vec ray(worldpos), v(player->o);
    ray.sub(player->o);
    float m = ray.magnitude();
    ray.div(m);
    float r = raycube(true, player->o, ray, 0, gridsize);

    ray.mul(r);
    v.add(ray);

    lookupcube(int(v.x), int(v.y), int(v.z));
    if(lusize>gridsize)
    {
        lu.x += ((int)v.x-lu.x)/gridsize*gridsize;
        lu.y += ((int)v.y-lu.y)/gridsize*gridsize;
        lu.z += ((int)v.z-lu.z)/gridsize*gridsize;
    }
    else if(gridsize>lusize)
    {
        lu.x &= ~(gridsize-1);
        lu.y &= ~(gridsize-1);
        lu.z &= ~(gridsize-1);
    };
    lusize = gridsize;

    float xi  = v.x + (ray.x/ray.y) * (lu.y - v.y + (ray.y<0 ? gridsize : 0)); // x intersect of xz plane
    float zi  = v.z + (ray.z/ray.y) * (lu.y - v.y + (ray.y<0 ? gridsize : 0)); // z intersect of xz plane
    float zi2 = v.z + (ray.z/ray.x) * (lu.x - v.x + (ray.x<0 ? gridsize : 0)); // z intersect of yz plane
    bool xside = (ray.x<0 && xi<lu.x+gridsize) || (ray.x>0 && xi>lu.x);
    bool zside = (ray.z<0 && zi<lu.z+gridsize) || (ray.z>0 && zi>lu.z);
    bool z2side= (ray.z<0 && zi2<lu.z+gridsize)|| (ray.z>0 && zi2>lu.z);

    if(xside && zside) orient = (ray.y>0 ? O_BACK : O_FRONT);
    else if(z2side) orient = (ray.x>0 ? O_LEFT : O_RIGHT);
    else orient = (ray.z>0 ? O_BOTTOM : O_TOP);

    cur = lu;
    int g2 = gridsize/2;
    cor.x = (int)v.x/g2;
    cor.y = (int)v.y/g2;
    cor.z = (int)v.z/g2;
    int od = dimension(orient);
    int d = dimension(sel.orient);

    if(dragging)
    {
        sel.o.x = min(lastcur.x, cur.x);
        sel.o.y = min(lastcur.y, cur.y);
        sel.o.z = min(lastcur.z, cur.z);
        sel.s.x = abs(lastcur.x-cur.x)/sel.grid+1;
        sel.s.y = abs(lastcur.y-cur.y)/sel.grid+1;
        sel.s.z = abs(lastcur.z-cur.z)/sel.grid+1;

        selcx   = min(cor[R(d)], lastcor[R(d)]);
        selcy   = min(cor[C(d)], lastcor[C(d)]);
        selcxs  = max(cor[R(d)], lastcor[R(d)]);
        selcys  = max(cor[C(d)], lastcor[C(d)]);

        if(!selectcorners)
        {
            selcx &= ~1;
            selcy &= ~1;
            selcxs &= ~1;
            selcys &= ~1;
            selcxs -= selcx-2;
            selcys -= selcy-2;
        }
        else
        {
            selcxs -= selcx-1;
            selcys -= selcy-1;
        };

        selcx  &= 1;
        selcy  &= 1;
        havesel = true;
    }
    else if (!havesel)
    {
        sel.o = lu;
        sel.s.x = sel.s.y = sel.s.z = 1;
        selcx = selcy = 0;
        selcxs = selcys = 2;
        sel.grid = gridsize;
        sel.orient = orient;
        d = od;
    };

    corner = (cor[R(d)]-lu[R(d)]/g2)+(cor[C(d)]-lu[C(d)]/g2)*2;
    selchildcount = 0;
    countselchild();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glLineWidth(1);
    glColor3ub(120,120,120); // cursor
    boxs(od, lu[R(od)], lu[C(od)], lusize, lusize, lu[od]+dimcoord(orient)*lusize);
    if(havesel)
    {
        glColor3ub(20,20,20);   // grid
        loopxy(sel) boxs(d, sel.o[R(d)]+x*sel.grid, sel.o[C(d)]+y*sel.grid, sel.grid, sel.grid, sel.o[d]+dimcoord(sel.orient)*sel.us(d));
        glColor3ub(200,0,0);    // 0 reference
        boxs(d, sel.o[R(d)]-4, sel.o[C(d)]-4, 8, 8, sel.o[d]);
        glColor3ub(200,200,200);// 2D selection box
        boxs(d, sel.o[R(d)]+selcx*g2, sel.o[C(d)]+selcy*g2, selcxs*g2, selcys*g2, sel.o[d]+dimcoord(sel.orient)*sel.us(d));
        glColor3ub(0,0,40);     // 3D selection box
        loopi(6) { d=dimension(i); boxs(d, sel.o[R(d)], sel.o[C(d)], sel.us(R(d)), sel.us(C(d)), sel.o[d]+dimcoord(i)*sel.us(d)); };
    };
    glDisable(GL_BLEND);
};

//////////// ready changes to vertex arrays ////////////

void readyva(block3 &b, cube *c, int cx, int cy, int cz, int size)
{
    uchar m = octatouchblock(b, cx, cy, cz, size);
    loopi(8) if(m&(1<<i))
    {
        ivec o(i, cx, cy, cz, size);
        freeclipplanes(c[i]);
        if(c[i].va)             // removes va s so that octarender will recreate
        {
            destroyva(c[i].va);
            c[i].va = NULL;
        };
        if(c[i].children)
        {
            if(size<=4)
            {
                solidfaces(c[i]);
                discardchildren(c[i]);
            }
            else readyva(b, c[i].children, o.x, o.y, o.z, size/2);
        };
    };
};

void changed()
{
    block3 b = sel;
    loopi(3) b.s[i] *= b.grid;
    b.grid = 1;
    loopi(3)                    // the changed blocks are the selected cubes
    {
        b.o[i] -= 1;
        b.s[i] += 2;
        readyva(b, worldroot, 0, 0, 0, hdr.worldsize/2);
        b.o[i] += 1;
        b.s[i] -= 2;
    };
    octarender();
};

//////////// copy and undo /////////////
cube copycube(cube &src)
{
    cube c = src;
    c.va = NULL;                // src cube is responsible for va destruction
    c.surfaces = NULL;
    c.clip = NULL;
    if(src.children)
    {
        c.children = newcubes(F_EMPTY);
        loopi(8) c.children[i] = copycube(src.children[i]);
    };
    return c;
};

void pastecube(cube &src, cube &dest)
{
    discardchildren(dest);
    dest = copycube(src);
};

block3 *blockcopy(block3 &s, int rgrid)
{
    block3 *b = (block3 *)new uchar[sizeof(block3)+sizeof(cube)*s.size()];
    *b = s;
    cube *q = b->c();
    loopxyz(s, rgrid, *q++ = copycube(c));
    return b;
};

void freeblock(block3 *b)
{
    cube *q = b->c();
    loopi(b->size()) discardchildren(*q++);
    delete[] b;
};

struct undoblock { int *g; block3 *b; };
vector<undoblock> undos;                                // unlimited undo
VARP(undomegs, 0, 1, 10);                                // bounded by n megs

void freeundo(undoblock u)
{
    delete[] u.g;;
    freeblock(u.b);
};

int *selgridmap()                                       // generates a map of the cube sizes at each grid point
{
    int *g = new int[sel.size()];
    loopxyz(sel, -sel.grid, (*g++ = lusize, c));
    return g-sel.size();
};

void pasteundo(undoblock &u)
{
    int *g = u.g;
    cube *s = u.b->c();
    loopxyz(*u.b, *g++, pastecube(*s++, c));
};

void pruneundos(int maxremain)                          // bound memory
{
    int t = 0, p = 0;
    loopvrev(undos)
    {
        cube *q = undos[i].b->c();
        t += undos[i].b->size()*sizeof(int);
        loopj(undos[i].b->size()) t += familysize(*q++)*sizeof(cube);
        if(t>maxremain) freeundo(undos.remove(i)); else p = t;
    };
    //conoutf("undo: %d of %d(%%%d)", p, undomegs<<20, p*100/(undomegs<<20));
};

void makeundo()                                         // stores state of selected cubes before editing
{
    if(lastsel == sel) return;
    undoblock u = { selgridmap(), blockcopy(lastsel=sel, -sel.grid)};
    undos.add(u);
    pruneundos(undomegs<<20);
};

void editundo()                                         // undoes last action
{
    if(noedit()) return;
    if(undos.empty()) { conoutf("nothing more to undo"); return; };
    undoblock u = undos.pop();
    sel = *u.b;
    pasteundo(u);
    freeundo(u);
    changed();
    reorient();
    forcenextundo();
};

block3 *copybuf=NULL;
void copy()
{
    if(noedit()) return;
    if(copybuf) freeblock(copybuf);
    forcenextundo();
    makeundo(); // guard against subdivision
    copybuf = blockcopy(sel, sel.grid);
    editundo();
};

void paste()
{
    if(noedit() || copybuf==NULL) return;
    sel.s = copybuf->s;
    sel.orient = copybuf->orient;
    cube *s = copybuf->c();
    loopselxyz(pastecube(*s++, c));
};

COMMAND(copy, ARG_NONE);
COMMAND(paste, ARG_NONE);
COMMANDN(undo, editundo, ARG_NONE);

///////////// main cube edit ////////////////
int bounded(int n) { return n<0 ? 0 : (n>8 ? 8 : n); };

void pushedge(uchar &edge, int dir, int dc)
{
    int ne = bounded(edgeget(edge, dc)+dir);
    edge = edgeset(edge, dc, ne);
    int oe = edgeget(edge, 1-dc);
    if((dir<0 && dc && oe>ne) || (dir>0 && dc==0 && oe<ne)) edge = edgeset(edge, 1-dc, ne);
};

void editheight(int dir, int mode)
{
    if(noedit() || havesel) return;
    int d = dimension(sel.orient);
    int dc = dimcoord(sel.orient);
    int seldir = dc ? -dir : dir;
    block3 t = sel;

    sel.s[R(d)] = 2;
    sel.s[C(d)] = 2;
    sel.s[D(d)] = 1;
    sel.o[R(d)] += corner&1 ? 0 : -sel.grid;
    sel.o[C(d)] += corner&2 ? 0 : -sel.grid;

    loopselxyz(
        if(c.children) { solidfaces(c); discardchildren(c); };

        if(mode==1)
        {
            loopj(4) pushedge(c.edges[edgeindex(1-x, 1-y, d)], seldir, dc);
            loopj(2) pushedge(c.edges[edgeindex(x, 1-y, d)], seldir, dc);
            loopj(2) pushedge(c.edges[edgeindex(1-x, y, d)], seldir, dc);
            pushedge(c.edges[edgeindex(x, y, d)], seldir, dc);
        }
        else
        {
            uchar &e = c.edges[edgeindex(1-x, 1-y, d)];
            pushedge(e, seldir, dc);
        };

        optiface((uchar *)&c.faces[d], c);
    );
    sel = t;
};

COMMAND(editheight, ARG_2INT);

void editface(int dir, int mode)
{
    if(noedit()) return;
    if(mode==1 && (selcx || selcy || selcxs&1 || selcys&1)) mode = 0;
    int d = dimension(sel.orient);
    int dc = dimcoord(sel.orient);
    int seldir = dc ? -dir : dir;
    if (mode==1)
    {
        int h = sel.o[d]+dc*sel.grid;
        if((dir>0 == dc && h<=0) || (dir<0 == dc && h>=hdr.worldsize)) return;
        if(dir<0) sel.o[d] += sel.grid * seldir;
    };
    if(dc) sel.o[d] += sel.us(d)-sel.grid;
    sel.s[d] = 1;
    loopselxyz(
        if(c.children) solidfaces(c);
        discardchildren(c);
        if (mode==1) { if (dir<0) { solidfaces(c); } else emptyfaces(c); }  // fill command
        else
        {
            uchar *p = (uchar *)&c.faces[d];
            if(mode==2) pushedge(p[corner], seldir, dc);                    // coner command
            else
            {
                loop(mx,2) loop(my,2)                                       // pull/push edges command
                {
                    if(x==0 && mx==0 && selcx) continue;
                    if(y==0 && my==0 && selcy) continue;
                    if(x==sel.s[R(d)]-1 && mx==1 && (selcx+selcxs)&1) continue;
                    if(y==sel.s[C(d)]-1 && my==1 && (selcy+selcys)&1) continue;

                    pushedge(p[mx+my*2], seldir, dc);
                };
            };
            optiface(p, c);
        };
    );
    if (mode==1 && dir>0) sel.o[d] += sel.grid * seldir;
};

COMMAND(editface, ARG_2INT);

void selextend()
{
    if(noedit()) return;
    loopi(3)
    {
        if(cur[i]<sel.o[i])
        {
            sel.s[i] += (sel.o[i]-cur[i])/sel.grid;
            sel.o[i] = cur[i];
        }
        else if(cur[i]>=sel.o[i]+sel.s[i]*sel.grid)
        {
            sel.s[i] = (cur[i]-sel.o[i])/sel.grid+1;
        }
    };

    cursorupdate();
    reorient();
}

COMMAND(selextend, ARG_NONE);

void entmove(int dir, int dist)
{
    if(noedit()) return;
    int e = closestent();
    if(e<0||dir<0||dir>2) return;
    et->getents()[e]->o[dir] += dist;
}

COMMAND(entmove, ARG_2INT);

/////////// texture editing //////////////////

int curtexindex = -1, lasttex = 0;
void tofronttex()                                       // maintain most recently used of the texture lists when applying texture
{
    int c = curtexindex;
    if(c>=0)
    {
        uchar *p = hdr.texlist;
        int t = p[c];
        for(int a = c-1; a>=0; a--) p[a+1] = p[a];
        p[0] = t;
        curtexindex = -1;
    };
};

void edittexcube(cube &c, int tex, int orient)
{
    c.texture[visibleorient(c, orient)] = tex;
    if (c.children) loopi(8) edittexcube(c.children[i], tex, orient);
};

extern int curtexnum;

void edittex(int dir)
{
    if(noedit()) return;
    if(!(lastsel==sel)) tofronttex();
    int i = curtexindex;
    i = i<0 ? 0 : i+dir;
    curtexindex = i = min(max(i, 0), curtexnum-1);
    int t = lasttex = hdr.texlist[i];
    loopselxyz(edittexcube(c, t, sel.orient));
};

COMMAND(edittex, ARG_1INT);

////////// flip and rotate ///////////////
uint edgeinv(uint face) { return face==F_EMPTY ? face : 0x88888888 - (((face&0xF0F0F0F0)>>4)+ ((face&0x0F0F0F0F)<<4)); };
uint rflip(uint face)   { return ((face&0xFF00FF00)>>8) + ((face&0x00FF00FF)<<8); };
uint cflip(uint face)   { return ((face&0xFFFF0000)>>16)+ ((face&0x0000FFFF)<<16); };
uint mflip(uint face)   { return (face&0xFF0000FF) + ((face&0x00FF0000)>>8) + ((face&0x0000FF00)<<8); };

void flipcube(cube &c, int dim)
{
    swap(uchar, c.texture[dim*2], c.texture[dim*2+1]);
    c.faces[D(dim)] = edgeinv(c.faces[D(dim)]);
    c.faces[C(dim)] = rflip(c.faces[C(dim)]);
    c.faces[R(dim)] = cflip(c.faces[R(dim)]);
    if (c.children)
    {
        loopi(8) if (i&octadim(dim)) swap(cube, c.children[i], c.children[i-octadim(dim)]);
        loopi(8) flipcube(c.children[i], dim);
    };
};

void rotatequad(cube &a, cube &b, cube &c, cube &d)
{
    cube t = a; a = b; b = c; c = d; d = t;
};

void rotatecube(cube &c, int dim)   // rotates cube clockwise. see pics in cvs for help.
{
    c.faces[D(dim)] = rflip  (mflip(c.faces[D(dim)]));
    c.faces[C(dim)] = edgeinv(mflip(c.faces[C(dim)]));
    c.faces[R(dim)] = cflip  (mflip(c.faces[R(dim)]));
    swap(uint, c.faces[R(dim)], c.faces[C(dim)]);

    swap(uint, c.texture[2*R(dim)], c.texture[2*C(dim)+1]);
    swap(uint, c.texture[2*C(dim)], c.texture[2*R(dim)+1]);
    swap(uint, c.texture[2*C(dim)], c.texture[2*C(dim)+1]);

    if(c.children)
    {
        int row = octadim(R(dim));
        int col = octadim(C(dim));
        for(int i=0; i<=octadim(dim); i+=octadim(dim)) rotatequad
        (
            c.children[i+row],
            c.children[i],
            c.children[i+col],
            c.children[i+col+row]
        );
        loopi(8) rotatecube(c.children[i], dim);
    };
};

void flip()
{
    if(noedit()) return;
    int zs = sel.s[dimension(sel.orient)];
    makeundo();
    loopxy(sel)
    {
        loop(z,zs) flipcube(selcube(x, y, z), dimension(sel.orient));
        loop(z,zs/2)
        {
            cube &a = selcube(x, y, z);
            cube &b = selcube(x, y, zs-z-1);
            swap(cube, a, b);
        };
    };
    changed();
};

void rotate(int cw)
{
    if(noedit()) return;
    int dim = dimension(sel.orient);
    if(!dimcoord(sel.orient)) cw = -cw;
    int &m = min(sel.s[C(dim)], sel.s[R(dim)]);
    int ss = m = max(sel.s[R(dim)], sel.s[C(dim)]);
    makeundo();
    loop(z,sel.s[D(dim)]) loopi(cw>0 ? 1 : 3)
    {
        loopxy(sel) rotatecube(selcube(x,y,z), dim);
        loop(y,ss/2) loop(x,ss-1-y*2) rotatequad
        (
            selcube(ss-1-y, x+y, z),
            selcube(x+y, y, z),
            selcube(y, ss-1-x-y, z),
            selcube(ss-1-x-y, ss-1-y, z)
        );
    };
    changed();
};

COMMAND(flip, ARG_NONE);
COMMAND(rotate, ARG_1INT);

struct material
{
    const char *name;
    uchar id;
} materials [] =
{
    {"air", MAT_AIR},
    {"water", MAT_WATER},
    {"clip", MAT_CLIP},
    {"glass", MAT_GLASS},
    {"noclip", MAT_NOCLIP},
};

void setmat(cube &c, uchar mat)
{
    if(c.children)
        loopi(8) setmat(c.children[i], mat);
    else
        c.material = mat;
}

void editmat(char *name)
{
    if(noedit()) return;
    loopi(sizeof(materials)/sizeof(material))
    {
        if(!strcmp(materials[i].name, name))
        {
            loopselxyz(setmat(c, materials[i].id));
            return;
        }
    }
    conoutf("unknown material \"%s\"", name);
    return;
}

COMMAND(editmat, ARG_1STR);

