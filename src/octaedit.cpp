#include "cube.h"

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

struct block3
{
    int o[3], s[3], grid;
    cube *c()     { return (cube *)(this+1); };
    int size()    { return s[0]*s[1]*s[2]; };
    int ss(int d) { return s[d]*grid; };
    bool operator==(block3 b) { loopi(3) if(o[i]!=b.o[i] || s[i]!=b.s[i]) return false; return true; };
};

block3 sel;
block3 lastsel;

int orient = 0, selorient = 0;
int gridsize = 32;
int corner = 0, cor[3], lastcor[3];

bool editmode = false;

bool havesel = false;
int cx, cy, cz, lastx, lasty, lastz;
int selcx, selcxs, selcy, selcys;
bool dragging = false;

void cancelsel() { havesel = false; };

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
        lastx = cx;
        lasty = cy;
        lastz = cz;
        sel.grid = gridsize;
        selorient = orient;
        loopi(3) lastcor[i] = cor[i];
    };
};

void toggleedit()
{
    if(player1->state==CS_DEAD) return;                 // do not allow dead players to edit to avoid state confusion
    if(!editmode && !allowedittoggle()) return;         // not in most multiplayer modes
    if(!(editmode = !editmode))
    {
        settagareas();                                  // reset triggers to allow quick playtesting
        entinmap(player1);                              // find spawn closest to current floating pos
        player1->timeinair = 0;
    }
    else
    {
        resettagareas();                                // clear trigger areas to allow them to be edited
        player1->health = 100;
        if(m_classicsp) monsterclear();                 // all monsters back at their spawns for editing
        projreset();
    };
    cancelsel();
    keyrepeat(editmode);
};

COMMANDN(edittoggle, toggleedit, ARG_NONE);

bool noedit()
{
    if(!editmode) conoutf("operation only allowed in edit mode");
    return !editmode;
};

void discardchildren(cube &c)
{
    if(c.children)
    {
        if (c.va) { destroyva(c.va); c.va = NULL; };
        solidfaces(c); // FIXME: better mipmap
        freeocta(c.children);
        c.children = NULL;
    };
};

///////// selection support /////////////

#define loopxy(b,d)  loop(y,(b).s[C(d)]) loop(x,(b).s[R(d)])    //loops through the row and columns (defined by d) of given block
#define loopxyz(b,d) loop(z,(b).s[D(d)]) loopxy((b),(d))
#define loopselxy()  loopxy(sel,dimension(selorient))
#define loopselxyz() loopxyz(sel,dimension(selorient))

cube &blockcube(int x, int y, int z, block3 &b, int rgrid, int o) // looks up a world cube, based on coordinates mapped by the block
{
    int d = dimension(o);
    int s[3] = { dimcoord(o)*(b.s[d]-1)*b.grid, y*b.grid, x*b.grid };
    return neighbourcube(b.o[2]+s[X(d)], b.o[1]+s[Y(d)], b.o[0]+s[Z(d)], -z*b.grid, rgrid, o);
};

cube &selcube(int x, int y, int z) { return blockcube(x, y, z, sel, sel.grid, selorient); };

////////////// cursor ///////////////

int selchildcount=0;
void countselchild(cube *c=worldroot, int cx=0, int cy=0, int cz=0, int size=hdr.worldsize/2)
{
    uchar m = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
    if(cz+size <= sel.o[0])           m &= 0xF0; // not in a -ve Z octant
    if(cz+size >= sel.o[0]+sel.ss(0)) m &= 0x0F; // not in a +ve Z octant
    if(cy+size <= sel.o[1])           m &= 0xCC; // not in a -ve Y octant
    if(cy+size >= sel.o[1]+sel.ss(1)) m &= 0x33; // etc..
    if(cx+size <= sel.o[2])           m &= 0xAA;
    if(cx+size >= sel.o[2]+sel.ss(2)) m &= 0x55;
    loopi(8) if(m&(1<<i))
    {
        int x = cx+octacoord(2,i)*size;
        int y = cy+octacoord(1,i)*size;
        int z = cz+octacoord(0,i)*size;
        if(c[i].children) countselchild(c[i].children, x, y, z, size/2);
        else selchildcount++;
    };
};

int lu(int dim) { return (dim==0?luz:(dim==1?luy:lux)); };

void cursorupdate()
{
    vec dir(worldpos);
    dir.sub(player1->o);
    dir.normalize();
    float len = gridsize/8; // depth modifier might need to be tweaked
    int x = (int)(worldpos.x + dir.x*len);
    int y = (int)(worldpos.y + dir.y*len);
    int z = (int)(worldpos.z + dir.z*len);
    lookupcube(x, y, z);

    if(lusize>gridsize)
    {
        lux += (x-lux)/gridsize*gridsize;
        luy += (y-luy)/gridsize*gridsize;
        luz += (z-luz)/gridsize*gridsize;
    }
    else if(gridsize>lusize)
    {
        lux &= ~(gridsize-1);
        luy &= ~(gridsize-1);
        luz &= ~(gridsize-1);
    };
    lusize = gridsize;

    int xi  = (int)(worldpos.x + (dir.x/dir.y) * (luy - worldpos.y + (dir.y<0 ? gridsize : 0))); // x intersect of xz plane
    int zi  = (int)(worldpos.z + (dir.z/dir.y) * (luy - worldpos.y + (dir.y<0 ? gridsize : 0))); // z intersect of xz plane
    int zi2 = (int)(worldpos.z + (dir.z/dir.x) * (lux - worldpos.x + (dir.x<0 ? gridsize : 0))); // z intersect of yz plane
    if (xi < lux+gridsize && xi > lux && zi < luz+gridsize && zi > luz) orient = (dir.y>0 ? O_FRONT : O_BACK);
    else if (zi2 < luz+gridsize && zi2 > luz) orient = (dir.x>0 ? O_LEFT : O_RIGHT);
    else orient = (dir.z>0 ? O_TOP : O_BOTTOM);

    int g2 = gridsize/2;
    cor[2] = x/g2;
    cor[1] = y/g2;
    cor[0] = z/g2;
    cx = lux;
    cy = luy;
    cz = luz;

    int d = dimension(orient);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glLineWidth(1);
    glColor3ub(120,120,120); // cursor
    boxs(d, lu(R(d)), lu(C(d)), lusize, lusize, lu(d)+dimcoord(orient)*lusize);

    if (cor[0]>=0) // ie: cursor not in the void
    {
        if(dragging)
        {
            d = dimension(selorient);
            sel.o[2]  = min(lastx, cx);
            sel.o[1]  = min(lasty, cy);
            sel.o[0]  = min(lastz, cz);
            sel.s[2] = abs(lastx-cx)/sel.grid+1;
            sel.s[1] = abs(lasty-cy)/sel.grid+1;
            sel.s[0] = abs(lastz-cz)/sel.grid+1;
            selcx   = min(cor[R(d)], lastcor[R(d)]);
            selcxs  = max(cor[R(d)], lastcor[R(d)])-selcx+1;
            selcy   = min(cor[C(d)], lastcor[C(d)]);
            selcys  = max(cor[C(d)], lastcor[C(d)])-selcy+1;
            selcx  &= 1;
            selcy  &= 1;
            havesel = true;
        }
        else if (!havesel)
        {
            loopi(3) sel.o[i] = lu(i);
            loopi(3) sel.s[i] = 1;
            selcx = selcy = 0;
            selcxs = selcys = 2;
            sel.grid = gridsize;
            selorient = orient;
        };
    };

    selchildcount = 0;
    countselchild();

    d = dimension(selorient);
    corner = (cor[R(d)]-lu(R(d))/g2)+(cor[C(d)]-lu(C(d))/g2)*2;
    if(havesel)
    {
        glColor3ub(20,20,20);   // grid
        loopselxy() boxs(d, sel.o[R(d)]+x*sel.grid, sel.o[C(d)]+y*sel.grid, sel.grid, sel.grid, sel.o[d]+dimcoord(selorient)*sel.ss(d));
        glColor3ub(200,0,0);    // 0 reference
        boxs(d, sel.o[R(d)]-4, sel.o[C(d)]-4, 8, 8, sel.o[d]);
        glColor3ub(200,200,200);// 2D selection box
        boxs(d, sel.o[R(d)]+selcx*g2, sel.o[C(d)]+selcy*g2, selcxs*g2, selcys*g2, sel.o[d]+dimcoord(selorient)*sel.ss(d));
        glColor3ub(0,0,40);     // 3D selection box
        loopi(6) boxs(d=dimension(i), sel.o[R(d)], sel.o[C(d)], sel.ss(R(d)), sel.ss(C(d)), sel.o[d]+dimcoord(i)*sel.ss(d));
    };
    glDisable(GL_BLEND);
};

//////////// copy and undo /////////////
cube copycube(cube &src)
{
    cube c = src;
    if (src.children)
    {
        c.children = newcubes(F_EMPTY);
        c.va = NULL;
        loopi(8) c.children[i] = copycube(src.children[i]);
    };
    return c;
};

void pastecube(cube &src, int x, int y, int z, block3 &b, int rgrid)
{
    cube &c = blockcube(x, y, z, b, rgrid, O_TOP);
    discardchildren(c);
    c = copycube(src);
};

block3 *block3copy(block3 &s, int rgrid)
{
    block3 *b = (block3 *)gp()->alloc(sizeof(block3)+sizeof(cube)*s.size());
    *b = s;
    cube *q = b->c();
    loopxyz(s,O_TOP) *q++ = copycube(blockcube(x, y, z, s, rgrid, O_TOP));
    return b;
};

void freeblock3(block3 *b)
{
    cube *q = b->c();
    loopi(b->size()) discardchildren(*q++);
    gp()->dealloc(b, sizeof(block3)+sizeof(cube)*b->size());
};

vector<cube*> changed;
struct undoblock { int *g; block3 *b; };
vector<undoblock> undos;                                // unlimited undo
VAR(undomegs, 0, 1, 10);                                // bounded by n megs

void changedva(int x, int y, int z, int size, block3 &step)
{
    cube *c, *cc = worldroot;
    int g, m, mm = 0, grid = hdr.worldsize;
    loopi(16)
    {
        grid = grid >> 1; mm = mm | grid;
        loopj(8) if (cc[j].va)
        {
            step.o[2]=(x&mm)+grid; step.o[1]=(y&mm)+grid; step.o[0]=(z&mm)+grid;
            break;
        }
        cc = cc + ((x & grid)?1:0) + ((y & grid)?2:0) + ((z & grid)?4:0);
        if (cc->va || !i) { c = cc; g = grid; m = mm; };
        if (!cc->children || grid<=size) break;
        cc = cc->children;
    };
    if (!c->va)
    {
        c->va = newva(x & m, y & m, z & m, g);
        step.o[2]=(x&m)+g; step.o[1]=(y&m)+g; step.o[0]=(z&m)+g;
    };
    if (!c->va->changed) { c->va->changed = true; changed.add(c); };
};

void allchanged()
{
    block3 dummy;
    int size = hdr.worldsize / 2;
    vaclearc(worldroot);
    changed.setsize(0);
    loopi(8) changedva((i&1)?size:0, (i&2)?size:0, (i&4)?size:0, hdr.worldsize, dummy);
};

void addchanged(block3 &b, bool internal = false)
{
    static uint badmask;
    if (!internal)
    {
        badmask = hdr.worldsize;
        loopi(5) badmask = badmask | badmask << (1 << i);
    };
    loopi(3) if((b.o[i] | (b.o[i]+b.s[i]*b.grid-1)) & badmask) return;

    block3 bb;
    int ze=b.o[0]+b.s[0]*b.grid;
    int ye=b.o[1]+b.s[1]*b.grid;
    int xe=b.o[2]+b.s[2]*b.grid;
    for(int z=b.o[0], zn=ze; z<ze; z=zn, zn=ze)
        for(int y=b.o[1], yn=ye; y<ye; y=yn, yn=ye)
            for(int x=b.o[2], xn=xe; x<xe; x=xn, xn=xe)
    {
        changedva(x, y, z, b.grid, bb);
        xn = min(xn, bb.o[2]);
        yn = min(yn, bb.o[1]);
        zn = min(zn, bb.o[0]);
    };

    if (!internal)
    {
        bb.grid = 1;
        bb.o[2] = b.o[2]; bb.s[2] = b.s[2] * b.grid;
        bb.o[1] = b.o[1]; bb.s[1] = b.s[1] * b.grid;
        bb.o[0] = b.o[0] - 1; bb.s[0] = 1;
        addchanged(bb, true);
        bb.o[0] = b.o[0] + b.s[0] * b.grid;
        addchanged(bb, true);
        bb.o[0] = b.o[0]; bb.s[0] = b.s[0] * b.grid;
        bb.o[1] = b.o[1] - 1; bb.s[1] = 1;
        addchanged(bb, true);
        bb.o[1] = b.o[1] + b.s[1] * b.grid;
        addchanged(bb, true);
        bb.o[1] = b.o[1]; bb.s[1] = b.s[1] * b.grid;
        bb.o[2] = b.o[2] - 1; bb.s[2] = 1;
        addchanged(bb, true);
        bb.o[2] = b.o[2] + b.s[2] * b.grid;
        addchanged(bb, true);
    };
};

void freeundo(undoblock u)
{
    gp()->dealloc(u.g, sizeof(int)*u.b->size());
    freeblock3(u.b);
};

int *selgridmap()                                       // generates a map of the leaf cube sizes at each grid point
{
    int *g = (int *)gp()->alloc(sizeof(int)*sel.size());
    int *i = g;
    loopxyz(sel, O_TOP)
    {
        blockcube(x, y, z, sel, -sel.grid, O_TOP);
        *i++ = lusize;
    };
    return g;
};

void pasteundo(undoblock &u)
{
    int *g = u.g;
    cube *s = u.b->c();
    loopxyz(*u.b, O_TOP) pastecube(*s++, x, y, z, *u.b, *g++);
    freeundo(u);
};

void pruneundos(int maxremain)                          // bound memory
{
    int t = 0;
    loopvrev(undos)
    {
        cube *q = undos[i].b->c();
        t += undos[i].b->size()*sizeof(int);
        loopj(undos[i].b->size()) t += familysize(*q++)*sizeof(cube);
        if(t>maxremain) freeundo(undos.remove(i));
    };
    conoutf("undo: %d of %d(%%%d)", t, undomegs<<20, t*100/(undomegs<<20));
};

void makeundo()                                         // stores state of selected cubes before editing
{
    if(lastsel == sel) return;
    undoblock u = { selgridmap(), block3copy(lastsel=sel, -sel.grid)};
    undos.add(u);
    pruneundos(undomegs<<20);
};

void editundo()                                         // undoes last action
{
    if(noedit()) return;
    if(undos.empty()) { conoutf("nothing more to undo"); return; };
    loopi(3)
    {
        lastsel.o[i] = undos.last().b->o[i];
        lastsel.s[i] = undos.last().b->s[i];
    };
    lastsel.grid = undos.last().b->grid;
    pasteundo(undos.pop());
    addchanged(lastsel);
    lastsel.s[0]=0;                                     // next edit should save state again
};

block3 *copybuf=NULL;
void copy()
{
    if(noedit()) return;
    if(copybuf) freeblock3(copybuf);
    lastsel.s[0]=0;                                     // force undo to guard against subdivision
    makeundo();
    copybuf = block3copy(sel, sel.grid);
    editundo();
};

void paste()
{
    if(noedit() || copybuf==NULL) return;
    loopi(3) sel.s[i] = copybuf->s[i];
    makeundo();
    cube *s = copybuf->c();
    loopxyz(sel, O_TOP) pastecube(*s++, x, y, z, sel, sel.grid);
    addchanged(sel);
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

void editface(int dir, int mode)
{
    if(noedit()) return;
    int d = dimension(selorient);
    int dc = dimcoord(selorient);
    int seldir = dc ? -dir : dir;
    if (mode==1)
    {
        int h = sel.o[d]+dc*sel.grid;
        if((dir>0 == dc && h<=0) || (dir<0 == dc && h>=hdr.worldsize)) return;
        if(dir<0) sel.o[d] += sel.grid * seldir;
    };
    int ts = sel.s[D(dimension(selorient))];
    sel.s[D(dimension(selorient))] = 1;
    makeundo();
    loopselxy()
    {
        cube &c = selcube(x, y, 0);
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
    };
    addchanged(sel);
    sel.s[D(dimension(selorient))] = ts;
    if (mode==1 && dir>0) sel.o[d] += sel.grid * seldir;
};

COMMAND(editface, ARG_2INT);

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

void edittexcube(cube &c, int tex, int texorient)
{
    c.texture[texorient] = tex;
    if (c.children) loopi(8) edittexcube(c.children[i], tex, texorient);
};

void edittex(int dir)
{
    if(noedit()) return;
    if(!(lastsel==sel)) tofronttex();
    int i = curtexindex;
    i = i<0 ? 0 : i+dir;
    curtexindex = i = min(max(i, 0), 255);
    makeundo();
    loopselxyz() edittexcube(selcube(x,y,z), lasttex = hdr.texlist[i], selorient);
    addchanged(sel, true); // Don't update neighbors
};

COMMAND(edittex, ARG_1INT);

VAR(litepower, 1, 10, 255);
void lightcube(cube &c, int dr, int dg, int db)
{
    c.colour[0] = (uchar) (c.colour[0]+dr<0 ? 0 : (c.colour[0]+dr>255 ? 255 : c.colour[0]+dr));
    c.colour[1] = (uchar) (c.colour[1]+dg<0 ? 0 : (c.colour[1]+dg>255 ? 255 : c.colour[1]+dg));
    c.colour[2] = (uchar) (c.colour[2]+db<0 ? 0 : (c.colour[2]+db>255 ? 255 : c.colour[2]+db));
    if (c.children) loopi(8) lightcube(c.children[i],dr,dg,db);
};

void lite(int r, int g, int b)
{
    if(noedit()) return;
    makeundo();
    loopselxyz() lightcube(selcube(x, y, z), r*litepower, g*litepower, b*litepower);
    addchanged(sel); // Update neighbirs since vertex lighting shared
};
COMMAND(lite, ARG_3INT);

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
        loopi(8) if (i&octamask(dim)) swap(cube, c.children[i], c.children[i-octamask(dim)]);
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
        int row = octamask(R(dim));
        int col = octamask(C(dim));
        for(int i=0; i<=octamask(dim); i+=octamask(dim)) rotatequad
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
    int ds = sel.s[dimension(selorient)];
    makeundo();
    loopselxy()
    {
        loop(d,ds) flipcube(selcube(x, y, d), dimension(selorient));
        loop(d,ds/2)
        {
            cube &a = selcube(x, y, d);
            cube &b = selcube(x, y, ds-d-1);
            swap(cube, a, b);
        };
    };
    addchanged(sel);
};

void rotate(int cw)
{
    if(noedit()) return;
    int dim = dimension(selorient);
    if(!dimcoord(selorient)) cw = -cw;
    int ss = min(sel.s[R(dim)], sel.s[C(dim)]) = max(sel.s[R(dim)], sel.s[C(dim)]);
    makeundo();
    loop(d,sel.s[D(dim)]) loopi(cw>0 ? 1 : 3)
    {
        loopselxy() rotatecube(selcube(x,y,d), dim);
        loop(y,ss/2) loop(x,ss-1-y*2) rotatequad
        (
            selcube(ss-1-y, x+y, d),
            selcube(x+y, y, d),
            selcube(y, ss-1-x-y, d),
            selcube(ss-1-x-y, ss-1-y, d)
        );
    };
    addchanged(sel); // Is this correct?
};

COMMAND(flip, ARG_NONE);
COMMAND(rotate, ARG_1INT);
