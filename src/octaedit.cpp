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
cube *last = NULL;
int gridsize = 32;
int corner = 0, corx, cory;

bool editmode = false; 

bool havesel = false;
int cx, cy, cz, lastx, lasty, lastz, lastcorx, lastcory;
int sel[3], sels[3], selgrid, selorient, selcx, selcxs, selcy, selcys;
bool dragging = false;

VARF(gridpower, 2, 5, 16,
{
    gridsize = 1<<gridpower;
    if(gridsize>=hdr.worldsize) gridsize = hdr.worldsize/2;
    havesel = false;
});

void cancelsel() { havesel = false; };

void editdrag(bool on)
{
    if(dragging = on)
    {
        lastx = cx;
        lasty = cy;
        lastz = cz;
        selgrid = gridsize;
        selorient = orient;
        lastcorx = corx;
        lastcory = cory;
        havesel = false;
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
    havesel = false;
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
    int yawo[] = { O_LEFT, O_FRONT, O_RIGHT, O_BACK }; 
    if(player1->pitch>45) orient = O_TOP;
    else if(player1->pitch<-45) orient = O_BOTTOM;
    else orient = yawo[(((int)player1->yaw-45)/90)&3];

    int d = dimension(orient);
    int x = (int)worldpos.x;
    int y = (int)worldpos.y;
    int z = (int)worldpos.z;

    switch(orient)
    {
        case O_TOP:    z+=4; break;
        case O_BOTTOM: z-=4; break;
        case O_FRONT:  y+=4; break;
        case O_BACK:   y-=4; break;
        case O_LEFT:   x+=4; break;
        case O_RIGHT:  x-=4; break;
    };
    last = &lookupcube(x, y, z, gridsize);
    int g2 = gridsize/2;
    corx = (d==2?y:x)/g2;
    cory = (d==0?y:z)/g2;
    corner = (corx-lu(rd(d))/g2)+(cory-lu(cd(d))/g2)*2;    
    cx = lux;
    cy = luy;
    cz = luz;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    //glColor4ub(64,64,255,32);
    glColor3ub(120,120,120);
    glLineWidth(1);
    boxs(d, lu(rd(d)), lu(cd(d)), lusize, lusize, lu(d)+dimcoord(orient)*lusize);
    
    if(dragging && gridsize==selgrid && orient==selorient)
    {
        sel[2]  = min(lastx, cx);
        sel[1]  = min(lasty, cy);
        sel[0]  = min(lastz, cz);
        sels[2] = abs(lastx-cx)+selgrid;
        sels[1] = abs(lasty-cy)+selgrid;
        sels[0] = abs(lastz-cz)+selgrid;
        selcx   = min(corx, lastcorx);
        selcxs  = max(corx, lastcorx)-selcx+1;
        selcy   = min(cory, lastcory);
        selcys  = max(cory, lastcory)-selcy+1;
        havesel = true;
        selcx  &= 1;
        selcy  &= 1;
    }; 
    
    if(havesel)
    {
        glColor3ub(0,120,0);
        d = dimension(selorient);
        for(int row = sel[rd(d)]; row<sel[rd(d)]+sels[rd(d)]; row += selgrid) 
        for(int col = sel[cd(d)]; col<sel[cd(d)]+sels[cd(d)]; col += selgrid) 
            boxs(d, row, col, selgrid, selgrid, sel[d]+dimcoord(selorient)*sels[d]);
        glColor3ub(60,60,60);
        //conoutf("%d %d ", selcxs*g2, selcys*g2);
        boxs(d, sel[rd(d)]+selcx*g2, sel[cd(d)]+selcy*g2, selcxs*g2, selcys*g2, sel[d]+dimcoord(selorient)*sels[d]);
    };
    glDisable(GL_BLEND);
};

/////////////////////////////////////////

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

struct undo { cube c; int x, y, z, size; };
vector<undo> undos;                                     // unlimited undo
VAR(undomegs, 0, 1, 10);                                // bounded by n megs

void pruneundos(int maxremain)                          // bound memory
{
    int t = 0;
    loopvrev(undos)
    {
        t += familysize(undos[i].c)*sizeof(cube) + sizeof(int)*4;
        if(t>maxremain) { discardchildren(undos[i].c); undos.remove(i); };
    };
};

void makeundo()
{
    undo u = { copycube(*last), lux, luy, luz, lusize };
    undos.add(u);
    pruneundos(undomegs<<20);
};

void editundo()
{
    if(noedit()) return;
    if(undos.empty()) { conoutf("nothing more to undo"); return; };
    undo u = undos.pop();
    cube &dest = lookupcube(u.x, u.y, u.z, u.size); 
    discardchildren(dest);
    dest = copycube(u.c);
    discardchildren(u.c);
    changed = true;
};

cube copybuf;
void copy() { if(noedit()) return; discardchildren(copybuf); copybuf = copycube(*last); };
void paste() 
{ 
    if(noedit()) return; makeundo();
    discardchildren(*last);
    *last = copycube(copybuf);
    changed = true; 
};

COMMAND(copy, ARG_NONE);
COMMAND(paste, ARG_NONE);
COMMANDN(undo, editundo, ARG_NONE);

////////////////////////////////////////

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

void editface(int dir, int mode)
{
    if(noedit()) return; makeundo(); // FIXME: Not adequate
    if(havesel)
    {
        int xs = sels[rd(dimension(selorient))]/selgrid;
        int ys = sels[cd(dimension(selorient))]/selgrid;
        if(dimcoord(selorient)) dir *= -1;
        loop(x,xs) loop(y,ys)
        {
            cube *c;
            switch(dimension(selorient))
            {   
                case 0: c = &lookupcube(sel[2]+x*selgrid, sel[1]+y*selgrid, sel[0], selgrid); break;
                case 1: c = &lookupcube(sel[2]+x*selgrid, sel[1], sel[0]+y*selgrid, selgrid); break;
                case 2: c = &lookupcube(sel[2], sel[1]+x*selgrid, sel[0]+y*selgrid, selgrid); break;
            };
            discardchildren(*c);
           
            uchar *p = (uchar *)&c->faces[dimension(selorient)];         
            loop(mx,2) loop(my,2)
            {
                if(x==0 && mx==0 && selcx) continue;
                if(y==0 && my==0 && selcy) continue;
                if(x==xs-1 && mx==1 && (selcx+selcxs)&1) continue;
                if(y==ys-1 && my==1 && (selcy+selcys)&1) continue;
                
                pushedge(p[mx+my*2], dir, dimcoord(selorient));
            };       
            
            optiface(p, *c);
        };
    }
    else
    {
        cube &c = lookupcube(lux, luy, luz, lusize);
        discardchildren(c);
                
        if(isentirelysolid(c) && dir<0)
        {
            cube &o = neighbourcube2(lux, luy, luz, lusize, lusize, orient);
            
            switch(mode)
            {
                case 1: discardchildren(o); solidfaces(o); break;
            };
        }
        else
        {
            if(dimcoord(orient)) dir *= -1;
            uchar *p = (uchar *)&c.faces[dimension(orient)];
            
            switch(mode)
            {
                case 0: loopi(4) pushedge(p[i], dir,   dimcoord(orient)); break;
                case 1: loopi(4) pushedge(p[i], dir*8, dimcoord(orient)); break;
                case 2:          pushedge(p[corner], dir, dimcoord(orient)); break;
            };
        
            optiface(p, c);
        };
    };
    changed = true;  
};

COMMAND(editface, ARG_2INT);

//////////////////////////////////////

int curtexindex = -1, lasttex = 0, lastorient = 0;
cube *lasttexcube = NULL;
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

void edittexcube(cube &c, int tex)
{
    c.texture[dimension(orient)==2 ? orient : opposite(orient)] = tex;  //FIXME ?? : see neighbourcube and octarender
    if (c.children) loopi(8) edittexcube(c.children[i], tex);
};

void edittex(int dir)
{
    if(noedit()) return; makeundo();
    if(last!=lasttexcube || lastorient!=orient) { tofronttex(); lasttexcube = last; lastorient = orient; };
    int i = curtexindex;
    i = i<0 ? 0 : i+dir;
    curtexindex = i = min(max(i, 0), 255);
    edittexcube(*last, lasttex = hdr.texlist[i]);
    changed = true;
};

COMMAND(edittex, ARG_1INT);

///////////////////////////////////////

#define edgeinv(face)  ( 0x88888888 - (((face&0xF0F0F0F0)>>4)+ ((face&0x0F0F0F0F)<<4)) )
#define rflip(face)    ( ((face&0xFF00FF00)>>8) + ((face&0x00FF00FF)<<8) )
#define cflip(face)    ( ((face&0xFFFF0000)>>16)+ ((face&0x0000FFFF)<<16) )
#define mflip(face)    ( (face&0xFF0000FF) + ((face&0x00FF0000)>>8) + ((face&0x0000FF00)<<8) )
#define rcflip(face,r) ( r>0 ? rflip(face) : cflip(face) )
#define rcd(dim,r)     ( r>0 ? rd(dim) : cd(dim) )

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

void rotatecube(cube &c, int dim, int cw)
{
    c.faces[dim]          = dim==1 ? rcflip (mflip(c.faces[dim]),-cw)          : rcflip (mflip(c.faces[dim]),cw);
    c.faces[rcd(dim,-cw)] = dim==1 ? rcflip (mflip(c.faces[rcd(dim,-cw)]),-cw) : edgeinv(c.faces[rcd(dim,-cw)]);
    c.faces[rcd(dim,cw)]  = dim==1 ? edgeinv(mflip(c.faces[rcd(dim,cw)]))      : rcflip (c.faces[rcd(dim,cw)],2-dim);
    uint t; swap(c.faces[rd(dim)], c.faces[cd(dim)], t);

    swap(c.texture[2*rd(dim)],     c.texture[2*cd(dim) + (dim?0:1)], t);
    swap(c.texture[2*rd(dim)+1],   c.texture[2*cd(dim) + (dim?1:0)], t);
    swap(c.texture[2*rcd(dim,cw)], c.texture[2*rcd(dim,cw)+1], t);

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

void flip()         { if(noedit()) return; makeundo(); flipcube(*last, dimension(orient)); changed = true; };
void rotate(int cw) { if(noedit()) return; makeundo(); rotatecube(*last, dimension(orient), dimcoord(orient)?cw:-cw); changed = true; };

COMMAND(flip, ARG_NONE);
COMMAND(rotate, ARG_1INT);
