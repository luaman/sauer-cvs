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
    ivec o, s;
    int grid;
    cube *c()     { return (cube *)(this+1); };
    int size()    { return s.x*s.y*s.z; };
    int us(int d) { return s[d]*grid; };
    bool operator==(block3 &b) { return o==b.o && s==b.s && grid==b.grid; };
};

block3 sel;
block3 lastsel;

int orient = 0, selorient = 0;
int gridsize = 32;
int corner = 0;
ivec cor, lastcor;
ivec cur, lastcur;
int selcx, selcxs, selcy, selcys;

bool editmode = false;
bool havesel = false;
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
        lastcur = cur;
        lastcor = cor;
        sel.grid = gridsize;
        selorient = orient;
    };
};

void reorient()
{
    selcx = 0;
    selcy = 0;
    selcxs = sel.s[R(dimension(orient))]*2;
    selcys = sel.s[C(dimension(orient))]*2;
    selorient = orient;
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

COMMAND(reorient, ARG_NONE);
COMMANDN(edittoggle, toggleedit, ARG_NONE);

bool noedit()
{
    if(!editmode) conoutf("operation only allowed in edit mode");
    return !editmode;
};

void discardchildren(cube &c)
{
    if(c.va) destroyva(c.va);
    c.va = NULL;
    if(c.children)
    {
        solidfaces(c); // FIXME: better mipmap
        freeocta(c.children);
        c.children = NULL;
    };
};

///////// selection support /////////////

#define loopxy(b,d)  loop(y,(b).s[C(d)]) loop(x,(b).s[R(d)])    //loops through the row and columns (defined by d) of given block
#define loopxyz(b)   loop(z,(b).s[D(0)]) loopxy((b),0)
#define loopselxy()  loopxy(sel, dimension(selorient))
#define loopselxyz() loop(z, sel.s[D(dimension(selorient))]) loopxy(sel, dimension(selorient))

cube &blockcube(int x, int y, int z, block3 &b, int rgrid, int o=O_TOP) // looks up a world cube, based on coordinates mapped by the block
{
    int d = dimension(o);
    int s[3] = { dimcoord(o)*(b.s[d]-1)*b.grid, y*b.grid, x*b.grid };
    return neighbourcube(b.o.x+s[X(d)], b.o.y+s[Y(d)], b.o.z+s[Z(d)], -z*b.grid, rgrid, o);
};

cube &selcube(int x, int y, int z) { return blockcube(x, y, z, sel, sel.grid, selorient); };

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

void cursorupdate()
{
    vec dir(worldpos);
    dir.sub(player1->o);
    dir.normalize();

    int x = (int)(worldpos.x +(dir.x<0?-2:2));
    int y = (int)(worldpos.y +(dir.y<0?-2:2));
    int z = (int)(worldpos.z +(dir.z<0?-2:2));
    lookupcube(x, y, z);

    if(lusize>gridsize)
    {
        lu.x += (x-lu.x)/gridsize*gridsize;
        lu.y += (y-lu.y)/gridsize*gridsize;
        lu.z += (z-lu.z)/gridsize*gridsize;
    }
    else if(gridsize>lusize)
    {
        lu.x &= ~(gridsize-1);
        lu.y &= ~(gridsize-1);
        lu.z &= ~(gridsize-1);
    };
    lusize = gridsize;

    int xi  = (int)(worldpos.x + (dir.x/dir.y) * (lu.y - worldpos.y + (dir.y<0 ? gridsize : 0))); // x intersect of xz plane
    int zi  = (int)(worldpos.z + (dir.z/dir.y) * (lu.y - worldpos.y + (dir.y<0 ? gridsize : 0))); // z intersect of xz plane
    int zi2 = (int)(worldpos.z + (dir.z/dir.x) * (lu.x - worldpos.x + (dir.x<0 ? gridsize : 0))); // z intersect of yz plane
    if (xi < lu.x+gridsize && xi > lu.x && zi < lu.z+gridsize && zi > lu.z) orient = (dir.y>0 ? O_FRONT : O_BACK);
    else if (zi2 < lu.z+gridsize && zi2 > lu.z) orient = (dir.x>0 ? O_LEFT : O_RIGHT);
    else orient = (dir.z>0 ? O_TOP : O_BOTTOM);

    int g2 = gridsize/2;
    cur = lu;
    cor.x = x/g2;
    cor.y = y/g2;
    cor.z = z/g2;

    int d = dimension(orient);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glLineWidth(1);
    glColor3ub(120,120,120); // cursor
    boxs(d, lu[R(d)], lu[C(d)], lusize, lusize, lu[d]+dimcoord(orient)*lusize);

    if (cor[0]>=0) // ie: cursor not in the void
    {
        if(dragging)
        {
            d = dimension(selorient);
            sel.o.x = min(lastcur.x, cur.x);
            sel.o.y = min(lastcur.y, cur.y);
            sel.o.z = min(lastcur.z, cur.z);
            sel.s.x = abs(lastcur.x-cur.x)/sel.grid+1;
            sel.s.y = abs(lastcur.y-cur.y)/sel.grid+1;
            sel.s.z = abs(lastcur.z-cur.z)/sel.grid+1;
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
            sel.o = lu;
            sel.s.x = sel.s.y = sel.s.z = 1;
            selcx = selcy = 0;
            selcxs = selcys = 2;
            sel.grid = gridsize;
            selorient = orient;
        };
    };

    selchildcount = 0;
    countselchild();

    d = dimension(selorient);
    corner = (cor[R(d)]-lu[R(d)]/g2)+(cor[C(d)]-lu[C(d)]/g2)*2;
    if(havesel)
    {
        glColor3ub(20,20,20);   // grid
        loopselxy() boxs(d, sel.o[R(d)]+x*sel.grid, sel.o[C(d)]+y*sel.grid, sel.grid, sel.grid, sel.o[d]+dimcoord(selorient)*sel.us(d));
        glColor3ub(200,0,0);    // 0 reference
        boxs(d, sel.o[R(d)]-4, sel.o[C(d)]-4, 8, 8, sel.o[d]);
        glColor3ub(200,200,200);// 2D selection box
        boxs(d, sel.o[R(d)]+selcx*g2, sel.o[C(d)]+selcy*g2, selcxs*g2, selcys*g2, sel.o[d]+dimcoord(selorient)*sel.us(d));
        glColor3ub(0,0,40);     // 3D selection box
        loopi(6) boxs(d=dimension(i), sel.o[R(d)], sel.o[C(d)], sel.us(R(d)), sel.us(C(d)), sel.o[d]+dimcoord(i)*sel.us(d));
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
        if(c[i].va)             // removes va s so that octarender will recreate
        {
            destroyva(c[i].va);
            c[i].va = NULL;
        };
        if(c[i].children) readyva(b, c[i].children, o.x, o.y, o.z, size/2);
    };
};

void changed(bool internal = false)
{
    int of = internal ? 0 : 1;
    int sf = internal ? 0 : 2;
    block3 b = sel;
    loopi(3) b.s[i] *= b.grid;
    b.grid = 1;
    loopi(internal ? 1 : 3)     // the changed blocks are the selected cubes
    {                           // plus, if not internal, any cubes just 1 adjacent unit grid away from the sel block
        b.o[i] -= of;
        b.s[i] += sf;
        readyva(b, worldroot, 0, 0, 0, hdr.worldsize/2);
        b.o[i] += of;
        b.s[i] -= sf;
    };
    octarender();
};

//////////// copy and undo /////////////
cube copycube(cube &src)
{
    cube c = src;
    c.va = NULL;                // src cube is responsible for va destruction
    if (src.children)
    {
        c.children = newcubes(F_EMPTY);
        loopi(8) c.children[i] = copycube(src.children[i]);
    };
    return c;
};

void pastecube(cube &src, int x, int y, int z, block3 &b, int rgrid)
{
    cube &c = blockcube(x, y, z, b, rgrid);
    discardchildren(c);
    c = copycube(src);
};

block3 *block3copy(block3 &s, int rgrid)
{
    block3 *b = (block3 *)gp()->alloc(sizeof(block3)+sizeof(cube)*s.size());
    *b = s;
    cube *q = b->c();
    loopxyz(s) *q++ = copycube(blockcube(x, y, z, s, rgrid));
    return b;
};

void freeblock3(block3 *b)
{
    cube *q = b->c();
    loopi(b->size()) discardchildren(*q++);
    gp()->dealloc(b, sizeof(block3)+sizeof(cube)*b->size());
};

struct undoblock { int *g; block3 *b; };
vector<undoblock> undos;                                // unlimited undo
VAR(undomegs, 0, 1, 10);                                // bounded by n megs

void freeundo(undoblock u)
{
    gp()->dealloc(u.g, sizeof(int)*u.b->size());
    freeblock3(u.b);
};

int *selgridmap()                                       // generates a map of the cube sizes at each grid point
{
    int *g = (int *)gp()->alloc(sizeof(int)*sel.size());
    loopxyz(sel)
    {
        blockcube(x, y, z, sel, -sel.grid);
        *g++ = lusize;
    };
    return g-sel.size();
};

void pasteundo(undoblock &u)
{
    int *g = u.g;
    cube *s = u.b->c();
    loopxyz(*u.b) pastecube(*s++, x, y, z, *u.b, *g++);
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
    conoutf("undo: %d of %d(%%%d)", p, undomegs<<20, p*100/(undomegs<<20));
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
    undoblock u = undos.pop();
    pasteundo(u);
    sel = *u.b;
    freeundo(u);
    changed();
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
    sel.s = copybuf->s;
    cube *s = copybuf->c();
    makeundo();
    loopxyz(sel) pastecube(*s++, x, y, z, sel, sel.grid);
    changed();
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
    if(dc) sel.o[d] += sel.us(d)-sel.grid;
    sel.s[d] = 1;
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
    changed();
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
    changed(true); // Don't update neighbors
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
    changed(); // Update neighbirs since vertex lighting shared
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
    changed();
};

void rotate(int cw)
{
    if(noedit()) return;
    int dim = dimension(selorient);
    if(!dimcoord(selorient)) cw = -cw;
    int &m = min(sel.s[C(dim)], sel.s[R(dim)]);
    int ss = m = max(sel.s[R(dim)], sel.s[C(dim)]);
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
    changed();
};

COMMAND(flip, ARG_NONE);
COMMAND(rotate, ARG_1INT);
