#include "pch.h"
#include "engine.h"

void boxs(int d, int x, int y, int xs, int ys, int z)
{
    ivec m(d, x,    y,    z);
    ivec n(d, x+xs, y,    z);
    ivec o(d, x+xs, y+ys, z);
    ivec p(d, x,    y+ys, z);

    glBegin(GL_POLYGON);

    glVertex3i(m.x, m.y, m.z);
    glVertex3i(n.x, n.y, n.z);
    glVertex3i(o.x, o.y, o.z);
    glVertex3i(p.x, p.y, p.z);

    glEnd();
};

selinfo sel = { 0 }, lastsel;

int orient = 0;
int gridsize = 8;
ivec cor, lastcor;
ivec cur, lastcur;

bool editmode = false;
bool havesel = false;
bool dragging = false;

void forcenextundo() { lastsel.orient = -1; };
void cancelsel()     { havesel = false; forcenextundo(); };

VARF(gridpower, 2, 3, VVEC_INT-1,
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
    sel.cx = 0;
    sel.cy = 0;
    sel.cxs = sel.s[R[dimension(orient)]]*2;
    sel.cys = sel.s[C[dimension(orient)]]*2;
    sel.orient = orient;
};

VAR(editing,0,0,1);

void toggleedit()
{
    if(player->state!=CS_ALIVE && player->state!=CS_EDITING) return; // do not allow dead players to edit to avoid state confusion
    if(!editmode && !cc->allowedittoggle()) return;         // not in most multiplayer modes
    if(!(editmode = !editmode))
    {
        player->state = CS_ALIVE;
        cl->entinmap(player, false);                        // find spawn closest to current floating pos
    }
    else
    {
        cl->resetgamestate();
        player->state = CS_EDITING;
    };
    cancelsel();
    keyrepeat(editmode);
    editing = editmode;
};

bool noedit()
{
    if(!editmode) { conoutf("operation only allowed in edit mode"); return true; };
    vec o(sel.o.v);
    vec s(sel.s.v);
    s.mul(float(sel.grid) / 2.0f);
    o.add(s);
    float r = float(max(s.x, max(s.y, s.z)));
    bool viewable = (isvisiblesphere(r, o) != VFC_NOT_VISIBLE);
    if(!viewable) conoutf("selection not in view");
    return !viewable;
};

COMMAND(reorient, ARG_NONE);
COMMANDN(edittoggle, toggleedit, ARG_NONE);


///////// selection support /////////////

cube &blockcube(int x, int y, int z, const block3 &b, int rgrid) // looks up a world cube, based on coordinates mapped by the block
{
    ivec s(dimension(b.orient), x*b.grid, y*b.grid, dimcoord(b.orient)*(b.s[dimension(b.orient)]-1)*b.grid);

    return neighbourcube(b.o.x+s.x, b.o.y+s.y, b.o.z+s.z, -z*b.grid, rgrid, b.orient);
};

#define loopxy(b)        loop(y,(b).s[C[dimension((b).orient)]]) loop(x,(b).s[R[dimension((b).orient)]])
#define loopxyz(b, r, f) { loop(z,(b).s[D[dimension((b).orient)]]) loopxy((b)) { cube &c = blockcube(x,y,z,b,r); f; }; }
#define loopselxyz(f)    { makeundo(); loopxyz(sel, sel.grid, f); changed(sel); }
#define selcube(x, y, z) blockcube(x, y, z, sel, sel.grid)

////////////// cursor ///////////////

int selchildcount=0;
ivec origin(0,0,0);

void countselchild(cube *c, ivec &cor, int size)
{
    ivec ss(sel.s);
    ss.mul(sel.grid);
    loopoctabox(cor, size, sel.o, ss)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children) countselchild(c[i].children, o, size/2);
        else selchildcount++;
    };
};

bool selectcorners = false;
void selcorners(bool isdown) { selectcorners = isdown; editdrag(isdown); };
COMMAND(selcorners, ARG_DOWN);

uchar cursorcolor[3] = {120, 120, 120};

void setcursorcolor(int r, int g, int b)
{
    cursorcolor[0] = max(0, min(r, 255));
    cursorcolor[1] = max(0, min(g, 255));
    cursorcolor[2] = max(0, min(b, 255));
};

COMMANDN(cursorcolor, setcursorcolor, ARG_3INT);

void cursorupdate()
{
    vec ray(worldpos), v(player->o);
    ray.sub(player->o);
    raycubepos(player->o, ray, v, 0, (editmode && showmat ? RAY_EDITMAT : 0) | RAY_SKIPFIRST, gridsize);

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

        sel.cx   = min(cor[R[d]], lastcor[R[d]]);
        sel.cy   = min(cor[C[d]], lastcor[C[d]]);
        sel.cxs  = max(cor[R[d]], lastcor[R[d]]);
        sel.cys  = max(cor[C[d]], lastcor[C[d]]);

        if(!selectcorners)
        {
            sel.cx &= ~1;
            sel.cy &= ~1;
            sel.cxs &= ~1;
            sel.cys &= ~1;
            sel.cxs -= sel.cx-2;
            sel.cys -= sel.cy-2;
        }
        else
        {
            sel.cxs -= sel.cx-1;
            sel.cys -= sel.cy-1;
        };

        sel.cx  &= 1;
        sel.cy  &= 1;
        havesel = true;
    }
    else if(!havesel)
    {
        sel.o = lu;
        sel.s.x = sel.s.y = sel.s.z = 1;
        sel.cx = sel.cy = 0;
        sel.cxs = sel.cys = 2;
        sel.grid = gridsize;
        sel.orient = orient;
        d = od;
    };

    sel.corner = (cor[R[d]]-lu[R[d]]/g2)+(cor[C[d]]-lu[C[d]]/g2)*2;
    selchildcount = 0;
    countselchild(worldroot, origin, hdr.worldsize/2);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glLineWidth(1);
    glColor3ubv(cursorcolor);
    boxs(od, lu[R[od]], lu[C[od]], lusize, lusize, lu[od]+dimcoord(orient)*lusize);
    if(havesel)
    {
        glColor3ub(20,20,20);   // grid
        loopxy(sel) boxs(d, sel.o[R[d]]+x*sel.grid, sel.o[C[d]]+y*sel.grid, sel.grid, sel.grid, sel.o[d]+dimcoord(sel.orient)*sel.us(d));
        glColor3ub(200,0,0);    // 0 reference
        boxs(d, sel.o[R[d]]-4, sel.o[C[d]]-4, 8, 8, sel.o[d]);
        glColor3ub(200,200,200);// 2D selection box
        boxs(d, sel.o[R[d]]+sel.cx*g2, sel.o[C[d]]+sel.cy*g2, sel.cxs*g2, sel.cys*g2, sel.o[d]+dimcoord(sel.orient)*sel.us(d));
        glColor3ub(0,0,40);     // 3D selection box
        loopi(6) { d=dimension(i); boxs(d, sel.o[R[d]], sel.o[C[d]], sel.us(R[d]), sel.us(C[d]), sel.o[d]+dimcoord(i)*sel.us(d)); };
    };
    glDisable(GL_BLEND);
};

//////////// ready changes to vertex arrays ////////////

void readychanges(block3 &b, cube *c, ivec &cor, int size)
{
    loopoctabox(cor, size, b.o, b.s)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
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
                brightencube(c[i]);
            }
            else readychanges(b, c[i].children, o, size/2);
        }
        else brightencube(c[i]);
        freeoctaentities(c[i]);
    };
};

void changed(const block3 &sel)
{
    block3 b = sel;
    loopi(3) b.s[i] *= b.grid;
    b.grid = 1;
    loopi(3)                    // the changed blocks are the selected cubes
    {
        b.o[i] -= 1;
        b.s[i] += 2;
        readychanges(b, worldroot, origin, hdr.worldsize/2);
        b.o[i] += 1;
        b.s[i] -= 2;
    };

    inbetweenframes = false;
    octarender();
    inbetweenframes = true;
    entitiesinoctanodes();
};

//////////// copy and undo /////////////
cube copycube(cube &src)
{
    cube c = src;
    c.va = NULL;                // src cube is responsible for va destruction
    c.surfaces = NULL;
    c.clip = NULL;
    c.ents = NULL;
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

block3 *blockcopy(const block3 &s, int rgrid)
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
    delete[] u.g;
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
    if(lastsel==sel) return;
    lastsel=sel;
    if(multiplayer(false)) return;
    undoblock u = { selgridmap(), blockcopy(lastsel, -sel.grid)};
    undos.add(u);
    pruneundos(undomegs<<20);
};

void editundo()                                         // undoes last action
{
    if(noedit() || multiplayer()) return;
    if(undos.empty()) { conoutf("nothing more to undo"); return; };
    undoblock u = undos.pop();
    sel.o = u.b->o;
    sel.s = u.b->s;
    sel.grid = u.b->grid;
    sel.orient = u.b->orient;
    pasteundo(u);
    freeundo(u);
    changed(sel);
    reorient();
    forcenextundo();
};

block3 *copybuf=NULL;
void copy()
{
    if(noedit() || multiplayer()) return;
    if(copybuf) freeblock(copybuf);
    forcenextundo();
    makeundo(); // guard against subdivision
    copybuf = blockcopy(block3(sel), sel.grid);
    editundo();
};

void paste()
{
    if(noedit() || multiplayer() || copybuf==NULL) return;
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

void linkedpush(cube &c, int d, int x, int y, int dc, int dir)
{
    ivec v, p;
    getcubevector(c, d, x, y, dc, v);

    loopi(2) loopj(2)
    {
        getcubevector(c, d, i, j, dc, p);
        if(v==p)
            pushedge(cubeedge(c, d, i, j), dir, dc);
    };
};

void mpeditheight(int dir, int mode, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_HEIGHT, dir, mode);
    int d = dimension(sel.orient);
    int dc = dimcoord(sel.orient);
    int seldir = dc ? -dir : dir;

    ivec o = sel.o;
    sel.s[R[d]] = 2;
    sel.s[C[d]] = 2;
    sel.s[D[d]] = 1;
    sel.o[R[d]] -= sel.corner&1 ? 0 : sel.grid;
    sel.o[C[d]] -= sel.corner&2 ? 0 : sel.grid;

    int solid = 0, solidground = 0;
    loopxy(sel)
    {
        if(dir<0)
        {
            if(blockcube(x, y, 0, sel, -sel.grid).faces[d]==F_SOLID)
                solid++;
            if(blockcube(x, y, 1, sel, -sel.grid).faces[d]==F_SOLID)
                solidground++;
        }
        else if(!isempty(blockcube(x, y, -1, sel, -sel.grid)))
            return;
    };

    if(solid==4)
    {
        sel.o[D[d]] += sel.grid*seldir;
    };

    loopselxyz(
        if(c.children) { solidfaces(c); discardchildren(c); };
        if(solid==4 || (isempty(c) && solidground==4))
            c.faces[R[d]] = c.faces[C[d]] = F_SOLID;
        else if(isempty(c)) continue;

        uint bak = c.faces[d];
        linkedpush(c, d, 1-x, 1-y, dc, seldir);

        optiface((uchar *)&c.faces[d], c);
        if(!isvalidcube(c))
            c.faces[d] = bak;
    );
    sel.o = o;
};

void editheight(int dir, int mode)
{
    if(noedit()) return;
    mpeditheight(dir, mode, sel, true);
};

COMMAND(editheight, ARG_2INT);

void mpeditface(int dir, int mode, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_FACE, dir, mode);
    if(mode==1 && (sel.cx || sel.cy || sel.cxs&1 || sel.cys&1)) mode = 0;
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
        if (mode==1)
            { if (dir<0) { solidfaces(c); } else emptyfaces(c); }  // fill command
        else
        {
            uint bak = c.faces[d];
            uchar *p = (uchar *)&c.faces[d];

            if(mode==2)
                linkedpush(c, d, sel.corner&1, sel.corner>>1, dc, seldir); // coner command
            else
            {
                loop(mx,2) loop(my,2)                                       // pull/push edges command
                {
                    if(x==0 && mx==0 && sel.cx) continue;
                    if(y==0 && my==0 && sel.cy) continue;
                    if(x==sel.s[R[d]]-1 && mx==1 && (sel.cx+sel.cxs)&1) continue;
                    if(y==sel.s[C[d]]-1 && my==1 && (sel.cy+sel.cys)&1) continue;
                    if(p[mx+my*2] != ((uchar *)&bak)[mx+my*2]) continue;

                    linkedpush(c, d, mx, my, dc, seldir);
                };
            };

            optiface(p, c);
            if(!isvalidcube(c))
            {
                uint newbak = c.faces[d];
                uchar *m = (uchar *)&bak;
                uchar *n = (uchar *)&newbak;
                loopk(4) if(n[k] != m[k]) // try to find partial solution
                {
                    c.faces[d] = bak;
                    c.edges[d*4+k] = n[k];
                    if(isvalidcube(c))
                        m[k] = n[k];
                };
                c.faces[d] = bak;
            };
        };
    );
    if (mode==1 && dir>0)
        sel.o[d] += sel.grid * seldir;
};

void editface(int dir, int mode)
{
    if(noedit()) return;
    mpeditface(dir, mode, sel, true);
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

/////////// texture editing //////////////////

int curtexindex = -1, lasttex = 0;
int texpaneltimer = 0;

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

selinfo repsel;
int reptex = -1, reporient = -1;

void edittexcube(cube &c, int tex, int orient, bool &findrep)
{
    if(orient<0) loopi(6) c.texture[i] = tex;
    else
    {
        int i = visibleorient(c, orient);
        if(findrep)
        {
            if(reptex < 0)
            {
                reptex = c.texture[i];
                reporient = orient;
            }
            else if(reptex != c.texture[i]) findrep = false;
        };
        c.texture[i] = tex;
    };
    if(c.children) loopi(8) edittexcube(c.children[i], tex, orient, findrep);
};

extern int curtexnum;
VAR(allfaces, 0, 0, 1);

void mpedittex(int tex, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        cl->edittrigger(sel, EDIT_TEX, tex, allfaces);
        if(allfaces || !(repsel == sel)) reptex = reporient = -1;
        repsel = sel;
    };
    bool findrep = local && !allfaces && reptex < 0;
    loopselxyz(edittexcube(c, tex, allfaces ? -1 : sel.orient, findrep));
};

void edittex(int dir)
{
    if(noedit()) return;
    texpaneltimer = 5000;
    if(!(lastsel==sel)) tofronttex();
    int i = curtexindex;
    i = i<0 ? 0 : i+dir;
    curtexindex = i = min(max(i, 0), curtexnum-1);
    int t = lasttex = hdr.texlist[i];
    mpedittex(t, allfaces, sel, true);
};

void gettex()
{
    if(noedit()) return;
    loopxyz(sel, sel.grid, curtexindex = c.texture[sel.orient]);
    loopi(curtexnum) if(hdr.texlist[i]==curtexindex)
    {
        curtexindex = i;
        tofronttex();
        return;
    };
};

COMMAND(edittex, ARG_1INT);
COMMAND(gettex, ARG_NONE);

void replacetexcube(cube &c, int oldtex, int newtex, int orient)
{
    int i = visibleorient(c, orient);
    if(c.texture[i] == oldtex) c.texture[i] = newtex;
    if(c.children) loopi(8) replacetexcube(c.children[i], oldtex, newtex, orient);
};

void mpreplacetex(int oldtex, int newtex, int orient, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_REPLACE, oldtex, newtex, orient);
    loopselxyz(replacetexcube(c, oldtex, newtex, orient));
};

void replace()
{
    if(noedit()) return;
    if(reptex < 0) { conoutf("can only replace after a texture edit"); return; };
    mpreplacetex(reptex, lasttex, reporient, sel, true);
};

COMMAND(replace, ARG_NONE);

////////// flip and rotate ///////////////
uint dflip(uint face) { return face==F_EMPTY ? face : 0x88888888 - (((face&0xF0F0F0F0)>>4)+ ((face&0x0F0F0F0F)<<4)); };
uint cflip(uint face) { return ((face&0xFF00FF00)>>8) + ((face&0x00FF00FF)<<8); };
uint rflip(uint face) { return ((face&0xFFFF0000)>>16)+ ((face&0x0000FFFF)<<16); };
uint mflip(uint face) { return (face&0xFF0000FF) + ((face&0x00FF0000)>>8) + ((face&0x0000FF00)<<8); };

void flipcube(cube &c, int dim)
{
    swap(uchar, c.texture[dim*2], c.texture[dim*2+1]);
    c.faces[D[dim]] = dflip(c.faces[D[dim]]);
    c.faces[C[dim]] = cflip(c.faces[C[dim]]);
    c.faces[R[dim]] = rflip(c.faces[R[dim]]);
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
    c.faces[D[dim]] = cflip (mflip(c.faces[D[dim]]));
    c.faces[C[dim]] = dflip (mflip(c.faces[C[dim]]));
    c.faces[R[dim]] = rflip (mflip(c.faces[R[dim]]));
    swap(uint, c.faces[R[dim]], c.faces[C[dim]]);

    swap(uint, c.texture[2*R[dim]], c.texture[2*C[dim]+1]);
    swap(uint, c.texture[2*C[dim]], c.texture[2*R[dim]+1]);
    swap(uint, c.texture[2*C[dim]], c.texture[2*C[dim]+1]);

    if(c.children)
    {
        int row = octadim(R[dim]);
        int col = octadim(C[dim]);
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

void mpflip(selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_FLIP);
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
    changed(sel);
};

void flip()
{
    if(noedit()) return;
    mpflip(sel, true);
};

void mprotate(int cw, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_ROTATE, cw);
    int dim = dimension(sel.orient);
    if(!dimcoord(sel.orient)) cw = -cw;
    int &m = min(sel.s[C[dim]], sel.s[R[dim]]);
    int ss = m = max(sel.s[R[dim]], sel.s[C[dim]]);
    makeundo();
    loop(z,sel.s[D[dim]]) loopi(cw>0 ? 1 : 3)
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
    changed(sel);
};

void rotate(int cw)
{
    if(noedit()) return;
    mprotate(cw, sel, true);
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
};

void mpeditmat(int matid, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_MAT, matid);
    loopselxyz(setmat(c, matid));
};

void editmat(char *name)
{
    if(noedit()) return;
    loopi(sizeof(materials)/sizeof(material))
    {
        if(!strcmp(materials[i].name, name))
        {
            mpeditmat(materials[i].id, sel, true);
            return;
        }
    }
    conoutf("unknown material \"%s\"", name);
};

COMMAND(editmat, ARG_1STR);

void render_texture_panel()
{
    if((texpaneltimer -= curtime)>0 && editmode)
    {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        int y = 50, gap = 10;

        loopi(7)
        {
            int s = (i == 3 ? 285 : 220), ti = curtexindex+i-3;
            if(ti>=0 && ti<256)
            {
                Texture *tex = lookuptexture(hdr.texlist[ti]);
                float sx = min(1, tex->xs/(float)tex->ys), sy = min(1, tex->ys/(float)tex->xs);
                glBindTexture(GL_TEXTURE_2D, tex->gl);
                glColor4f(0, 0, 0, texpaneltimer/1000.0f);
                int x = (i == 3 ? 2070 : 2100), r = s;
                loopj(2)
                {
                    glBegin(GL_QUADS);
                    glTexCoord2f(0.0,    0.0);    glVertex2f(x,   y);
                    glTexCoord2f(1.0/sx, 0.0);    glVertex2f(x+r, y);
                    glTexCoord2f(1.0/sx, 1.0/sy); glVertex2f(x+r, y+r);
                    glTexCoord2f(0.0,    1.0/sy); glVertex2f(x,   y+r);
                    glEnd();
                    xtraverts += 4;
                    glColor4f(1.0, 1.0, 1.0, texpaneltimer/1000.0f);
                    r -= 10;
                    x += 5;
                    y += 5;
                };
            }
            y += s+gap;
        }
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}
