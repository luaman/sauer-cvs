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
    loopi(4) glVertex3i( d==2 ? v[i][0] : v[i][2], 
                         d==0 ? v[i][0] : v[i][1], 
                         d==2 ? v[i][2] : v[i][d^1] );
    glEnd();
};

int orient = 0;
int gridsize = 32;
int corner = 0, cor[3], lastcor[3];

bool editmode = false; 

bool havesel = false;
bool cursorchange = true;
int cx, cy, cz, lastx, lasty, lastz;
int sel[3], sels[3], selgrid, selorient, selcx, selcxs, selcy, selcys;
bool dragging = false;

void cancelsel() { havesel = false; cursorchange = true; };

VARF(gridpower, 2, 5, 16,
{
    gridsize = 1<<gridpower;
    if(gridsize>=hdr.worldsize) gridsize = hdr.worldsize/2;
    cancelsel();
});

void editdrag(bool on)
{
    if(dragging = on)
    {
        lastx = cx;
        lasty = cy;
        lastz = cz;
        selgrid = gridsize;
        selorient = orient;
        loopi(3) lastcor[i] = cor[i];
        cancelsel();
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
        solidfaces(c); // FIXME: better mipmap
        freeocta(c.children);
        c.children = NULL;
    };
};

int lu(int dim) { return (dim==0?luz:(dim==1?luy:lux)); }; 
void cursorupdate()
{
    vec dir(worldpos);
    dir.sub(player1->o);
    float len = dir.magnitude() / (gridsize/8); // depth modifier might need to be tweaked
    int x = (int)(worldpos.x + dir.x/len);
    int y = (int)(worldpos.y + dir.y/len);      
    int z = (int)(worldpos.z + dir.z/len);
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

    int lastorient = orient;
    int xint  = (int)(worldpos.x + (dir.x/dir.y) * (luy - worldpos.y + (dir.y<0 ? gridsize : 0))); // x intersect of xz plane
    int zint  = (int)(worldpos.z + (dir.z/dir.y) * (luy - worldpos.y + (dir.y<0 ? gridsize : 0))); // z intersect of xz plane
    int zint2 = (int)(worldpos.z + (dir.z/dir.x) * (lux - worldpos.x + (dir.x<0 ? gridsize : 0))); // z intersect of yz plane
    if (xint < lux+gridsize && xint > lux && zint < luz+gridsize && zint > luz) orient = (dir.y>0 ? O_FRONT : O_BACK);
    else if (zint2 < luz+gridsize && zint2 > luz) orient = (dir.x>0 ? O_LEFT : O_RIGHT);
    else orient = (dir.z>0 ? O_TOP : O_BOTTOM);
  
    if (lastorient != orient || cx != lux || cy != luy || cz != luz) cursorchange = true;
    int d = dimension(havesel ? selorient : orient);
    int g2 = gridsize/2;
    cor[2] = x/g2;
    cor[1] = y/g2;
    cor[0] = z/g2;
    corner = (cor[rd(d)]-lu(rd(d))/g2)+(cor[cd(d)]-lu(cd(d))/g2)*2;    
    cx = lux;
    cy = luy;
    cz = luz;
    
    if (cor[0]>-1234567) // ie: cursor not in the void
    {
        if(dragging && gridsize==selgrid) 
        {
            sel[2]  = min(lastx, cx);
            sel[1]  = min(lasty, cy);
            sel[0]  = min(lastz, cz);
            sels[2] = abs(lastx-cx)+selgrid;
            sels[1] = abs(lasty-cy)+selgrid;
            sels[0] = abs(lastz-cz)+selgrid;
            selcx   = min(cor[rd(d)], lastcor[rd(d)]);
            selcxs  = max(cor[rd(d)], lastcor[rd(d)])-selcx+1;
            selcy   = min(cor[cd(d)], lastcor[cd(d)]);
            selcys  = max(cor[cd(d)], lastcor[cd(d)])-selcy+1;
            selcx  &= 1;
            selcy  &= 1;
            havesel = true;
        }
        else if (!havesel)
        {
            loopi(3)
            {
                sel[i] = lu(i);
                sels[i] = gridsize;
            };
            selcx = selcy = 0;
            selcxs = selcys = 2;
            selgrid = gridsize;
            selorient = orient;
        };
    };

    d = dimension(orient);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glLineWidth(1);
    //glColor4ub(64,64,255,32);
    glColor3ub(120,120,120);    // cursor
    boxs(d, lu(rd(d)), lu(cd(d)), lusize, lusize, lu(d)+dimcoord(orient)*lusize);

    if(havesel)
    {
        d = dimension(selorient);
        glColor3ub(20,20,20);   // grid
        for(int row = sel[rd(d)]; row<sel[rd(d)]+sels[rd(d)]; row += selgrid) 
        for(int col = sel[cd(d)]; col<sel[cd(d)]+sels[cd(d)]; col += selgrid) 
            boxs(d, row, col, selgrid, selgrid, sel[d]+dimcoord(selorient)*sels[d]);
        glColor3ub(200,0,0);    // 0 reference
        boxs(d, sel[rd(d)]-4, sel[cd(d)]-4, 8, 8, sel[d]);
        glColor3ub(200,200,200);// 2D selection box
        boxs(d, sel[rd(d)]+selcx*g2, sel[cd(d)]+selcy*g2, selcxs*g2, selcys*g2, sel[d]+dimcoord(selorient)*sels[d]);
        glColor3ub(0,0,40);     // 3D selection box
        loopi(6)
        {
            d = dimension(i);
            boxs(d, sel[rd(d)], sel[cd(d)], sels[rd(d)], sels[cd(d)], sel[d]+dimcoord(i)*sels[d]);
        };
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
        loopi(8) c.children[i] = copycube(src.children[i]); 
    };
    return c;
};

cube *copyselcube()
{
    int xs = sels[2]/selgrid;
    int ys = sels[1]/selgrid;
    int zs = sels[0]/selgrid;
    cube *c = (cube *)gp()->alloc(sizeof(cube)*xs*ys*zs);
    loop(z, zs) loop(y, ys) loop(x, xs)
        c[z*xs*ys + y*xs + x] = copycube(lookupcube(sel[2]+x*selgrid, sel[1]+y*selgrid, sel[0]+z*selgrid, selgrid));
    return c;
};

struct undocube { cube* c; int x, y, z, xs, ys, zs, size; };
vector<undocube> undos;                                 // unlimited undo
VAR(undomegs, 0, 1, 10);                                // bounded by n megs

void pruneundos(int maxremain)                          // bound memory
{
    int t = 0;
    loopvrev(undos)
    {
        int volume = undos[i].xs * undos[i].ys * undos[i].zs;
        t += sizeof(undocube);
        loopj(volume) t += familysize(undos[i].c[j])*sizeof(cube);
        if(t>maxremain) 
        { 
            loopj(volume) discardchildren(undos[i].c[j]); 
            gp()->dealloc(undos[i].c, sizeof(cube)*volume); 
            undos.remove(i); 
        };
    };
};

void makeundo()
{
    if (!cursorchange) return;
    undocube u;
    u.x = sel[2];
    u.y = sel[1];
    u.z = sel[0];
    u.xs = sels[2]/selgrid;
    u.ys = sels[1]/selgrid;
    u.zs = sels[0]/selgrid;
    u.size = selgrid;
    u.c = copyselcube();
    undos.add(u);
    pruneundos(undomegs<<20);
};

void editundo()
{
    if(noedit()) return;
    if(undos.empty()) { conoutf("nothing more to undo"); return; };
    undocube u = undos.pop();
    loop(z,u.zs) loop(y,u.ys) loop(x,u.xs)
    {
        cube &c = lookupcube(u.x+x*u.size, u.y+y*u.size, u.z+z*u.size, u.size);
        discardchildren(c);
        c = u.c[z*u.ys*u.xs + y*u.xs + x];
    };
    gp()->dealloc(u.c, sizeof(cube)*u.xs*u.ys*u.zs);
    changed = true; cursorchange = true;
};

cube *copybuf=NULL; int copys[3];
void copy() 
{ 
    if(noedit()) return; 
    if (copybuf) 
    {
        int size = copys[0]*copys[1]*copys[2];
        loopi(size) discardchildren(copybuf[i]); 
        gp()->dealloc(copybuf, sizeof(cube)*size);
    };
    loopi(3) copys[i] = sels[i]/selgrid;
    copybuf = copyselcube(); 
};

void paste() 
{   
    if(noedit() || copybuf==NULL) return; 
    loopi(3) loopi(3) sels[i] = copys[i]*selgrid;
    makeundo();
    loop(z,copys[0]) loop(y,copys[1]) loop(x,copys[2])
    {
        cube &c = lookupcube(sel[2]+x*selgrid, sel[1]+y*selgrid, sel[0]+z*selgrid, selgrid);
        discardchildren(c);
        c = copycube(copybuf[z*copys[1]*copys[2] + y*copys[2] + x]);
    };
    changed = true; cursorchange = true;
};

COMMAND(copy, ARG_NONE);
COMMAND(paste, ARG_NONE);
COMMANDN(undo, editundo, ARG_NONE);

///////////// main cube edit ////////////////
#define EDITINIT if(noedit()) return; makeundo(); changed = true; cursorchange = false;

int bounded(int n) { return n<0 ? 0 : (n>8 ? 8 : n); };

void pushedge(uchar &edge, int dir, int dc)
{
    int ne = bounded(edgeget(edge, dc)+dir);
    edge = edgeset(edge, dc, ne);   
    int oe = edgeget(edge, 1-dc);
    if((dir<0 && dc && oe>ne) || (dir>0 && dc==0 && oe<ne)) edge = edgeset(edge, 1-dc, ne);
};

void optiface(uchar *p, cube &c)
{
    loopi(4) if(edgeget(p[i], 0)!=edgeget(p[i], 1)) return;
    emptyfaces(c);
};


cube &selcube(int row, int col, int depth)
{
    switch(dimension(selorient))
    {   
        case 0:  return neighbourcube(sel[2]+row*selgrid, sel[1]+col*selgrid, sel[0]+dimcoord(selorient)*(sels[0]-selgrid), -depth*selgrid, selgrid, selorient);
        case 1:  return neighbourcube(sel[2]+row*selgrid, sel[1]+dimcoord(selorient)*(sels[1]-selgrid), sel[0]+col*selgrid, -depth*selgrid, selgrid, selorient);
        default: return neighbourcube(sel[2]+dimcoord(selorient)*(sels[2]-selgrid), sel[1]+row*selgrid, sel[0]+col*selgrid, -depth*selgrid, selgrid, selorient);
    };
};

void editface(int dir, int mode)
{
    if(noedit()) return;
    int seldir = dimcoord(selorient) ? -dir : dir;
    int xs = sels[rd(dimension(selorient))]/selgrid;
    int ys = sels[cd(dimension(selorient))]/selgrid;
    if (mode==1 && dir<0) sel[dimension(selorient)] += selgrid * seldir;
    makeundo();
    loop(x,xs) loop(y,ys)
    {
        cube &c = selcube(x, y, 0);
        discardchildren(c);
        
        if (mode==1) { if (dir<0) { solidfaces(c); } else emptyfaces(c); } // FIX : UNDO 
        else 
        {
            uchar *p = (uchar *)&c.faces[dimension(selorient)];
            if(mode==2) pushedge(p[corner], seldir, dimcoord(selorient));
            else
            {
                loop(mx,2) loop(my,2)
                {
                    if(x==0 && mx==0 && selcx) continue;
                    if(y==0 && my==0 && selcy) continue;
                    if(x==xs-1 && mx==1 && (selcx+selcxs)&1) continue;
                    if(y==ys-1 && my==1 && (selcy+selcys)&1) continue;
                    
                    pushedge(p[mx+my*2], seldir, dimcoord(selorient));
                };
            };
            optiface(p, c);
        };
    };
    changed = true; cursorchange = false;
    if (mode==1 && dir>0) sel[dimension(selorient)] += selgrid * seldir;
    if (mode==1) cursorchange = true;
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
    if(noedit()) return; makeundo();
    if(cursorchange) tofronttex();
    int i = curtexindex;
    i = i<0 ? 0 : i+dir;
    curtexindex = i = min(max(i, 0), 255);
    loop(r, sels[rd(dimension(selorient))]/selgrid) 
    loop(c, sels[cd(dimension(selorient))]/selgrid)
        edittexcube(selcube(r,c,0), lasttex = hdr.texlist[i], selorient);
    changed = true; cursorchange = false;
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
    EDITINIT; 
    int xs = sels[rd(dimension(selorient))]/selgrid;
    int ys = sels[cd(dimension(selorient))]/selgrid;
    int ds = sels[dimension(selorient)]/selgrid; 
    loop(d,ds) loop(x,xs) loop(y,ys) lightcube(selcube(x, y, d), r*litepower, g*litepower, b*litepower); 
};
COMMAND(lite, ARG_3INT);

////////// flip and rotate ///////////////
int edgeinv(int face)       { return 0x88888888 - (((face&0xF0F0F0F0)>>4)+ ((face&0x0F0F0F0F)<<4)); };
int rflip(int face)         { return ((face&0xFF00FF00)>>8) + ((face&0x00FF00FF)<<8); };
int cflip(int face)         { return ((face&0xFFFF0000)>>16)+ ((face&0x0000FFFF)<<16); };
int mflip(int face)         { return (face&0xFF0000FF) + ((face&0x00FF0000)>>8) + ((face&0x0000FF00)<<8); };
int rcflip(int face, int r) { return r>0 ? rflip(face) : cflip(face); };
int rcd(int dim, int r)     { return r>0 ? rd(dim) : cd(dim); };

void flipcube(cube &c, int dim)
{
    uchar t; swap(c.texture[dim*2], c.texture[dim*2+1], t);
    loop(d,3)
    {
        if (d==dim) c.faces[d] = edgeinv(c.faces[d]);
        else if ((d<dim)==(dim!=1)) c.faces[d] = rflip(c.faces[d]);
        else c.faces[d] = cflip(c.faces[d]);
    };
    if (c.children)
    {
        loopi(8) if (i&(1<<(2-dim))) { cube tc; swap(c.children[i], c.children[i-(1<<(2-dim))], tc); }
        loopi(8) flipcube(c.children[i], dim);
    };
};

void rotatecube(cube &c, int dim, int cw)   // swapping stuff to mimic rotation -- kinda like a rubiks cube!
{                                           // not too hard to understand if you can visualize it. see pics in cvs for help
    c.faces[dim]          = dim==1 ? rcflip (mflip(c.faces[dim]),-cw)          : rcflip (mflip(c.faces[dim]),cw);
    c.faces[rcd(dim,-cw)] = dim==1 ? rcflip (mflip(c.faces[rcd(dim,-cw)]),-cw) : edgeinv(c.faces[rcd(dim,-cw)]);
    c.faces[rcd(dim,cw)]  = dim==1 ? edgeinv(mflip(c.faces[rcd(dim,cw)]))      : rcflip (c.faces[rcd(dim,cw)],2-dim);
    uint t; swap(c.faces[rd(dim)], c.faces[cd(dim)], t);

    swap(c.texture[2*rd(dim)],     c.texture[2*cd(dim)+(dim==1?0:1)], t);
    swap(c.texture[2*rd(dim)+1],   c.texture[2*cd(dim)+(dim==1?1:0)], t);
    swap(c.texture[2*rcd(dim,-cw)], c.texture[2*rcd(dim,-cw)+1], t);

    if (c.children)
    {
        int row = dim==1 ? (1<<rd(dim)) : (1<<(2-rd(dim)));
        int col = dim==1 ? (1<<cd(dim)) : (1<<(2-cd(dim)));
        for(int i=0; i<=1<<(2-dim); i+=1<<(2-dim))
        {
            cube tc;
            swap(c.children[i],              c.children[i+row], tc);
            swap(c.children[i+col],          c.children[i+row+col], tc);
            swap(c.children[i+(cw>0?0:col)], c.children[i+row+(cw>0?col:0)], tc);
        };
        loopi(8) rotatecube(c.children[i], dim, cw);
    };
};

void flip()         
{ 
    EDITINIT;
    int xs = sels[rd(dimension(selorient))]/selgrid;
    int ys = sels[cd(dimension(selorient))]/selgrid;
    int ds = sels[dimension(selorient)]/selgrid; 
    loop(d,ds) loop(x,xs) loop(y,ys) flipcube(selcube(x, y, d), dimension(selorient));
    loop(d,ds/2) loop(x,xs) loop(y,ys) 
    {
        cube &a = selcube(x, y, d), t;
        cube &b = selcube(x, y, ds-d-1);
        swap(a,b,t);
    };
};

void rotate(int cw) { EDITINIT; rotatecube(lookupcube(lux,luy,luz,lusize), dimension(orient), dimcoord(orient)?cw:-cw); };

COMMAND(flip, ARG_NONE);
COMMAND(rotate, ARG_1INT);
