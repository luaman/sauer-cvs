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

int lu(int dim) { return (dim==0?luz:(dim==1?luy:lux)); };  // ! 4|\/| h4XoR
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
    last = &lookupcube(x, y, z);
  
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

bool noedit()
{
    if(!editmode) conoutf("operation only allowed in edit mode");
    return !editmode;
};

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

void discardchildren(cube &c)
{
    if(c.children)
    {
        solidfaces(c); // FIXME: better mipmap
        freeocta(c.children);
        c.children = NULL;
    };
};

void editface(int dir, int mode)
{
    if(noedit()) return;
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

#define swap(a,b,t) { t=a; a=b; b=t; }
#define edgeinv(face) ( 0x88888888 - (((face&0xF0F0F0F0)>>4)+ ((face&0x0F0F0F0F)<<4)) )
#define rflip(face)   ( ((face&0xFF00FF00)>>8) + ((face&0x00FF00FF)<<8) )
#define cflip(face)   ( ((face&0xFFFF0000)>>16)+ ((face&0x0000FFFF)<<16) )
#define mflip(face)   ( (face&0xFF0000FF) + ((face&0x00FF0000)>>8) + ((face&0x0000FF00)<<8) )
void recurseflip(cube &c, int dim)
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
        loopi(8) recurseflip(c.children[i], dim);
    };
};
void flipcube() { if(noedit()) return; recurseflip(lookupcube(lux,luy,luz,lusize), dimension(orient)); changed = true; };
COMMAND(flipcube, ARG_NONE);

#define rcflip(face,r) ( r>0 ? rflip(face) : cflip(face) )
#define rcd(dim,r)     ( r>0 ? rd(dim) : cd(dim) )
void recurserotate(cube &c, int dim, int cw)
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
        loopi(8) recurserotate(c.children[i], dim, cw);
    };
};
void rotatecube(int cw) { if(noedit()) return; recurserotate(lookupcube(lux,luy,luz,lusize), dimension(orient), dimcoord(orient)?cw:-cw); changed = true; };
COMMAND(rotatecube, ARG_1INT);

cube copy;
void recursecopy(cube &c, cube &src)
{
    c = src; 
    if (src.children) 
    { 
        c.children = newcubes(F_EMPTY); 
        loopi(8) recursecopy(c.children[i], src.children[i]); 
    };
};

void copycube() { if(noedit()) return; discardchildren(copy); recursecopy(copy, lookupcube(lux,luy,luz,lusize)); };
void pastecube() 
{ 
    if(noedit()) return; 
    cube &dest = lookupcube(lux,luy,luz,lusize);
    discardchildren(dest);
    recursecopy(dest, copy);
    changed = true; 
};

COMMAND(copycube, ARG_NONE);
COMMAND(pastecube, ARG_NONE);

