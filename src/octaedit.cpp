#include "cube.h"

void boxzs(int x, int y, int xs, int ys, int z)
{
    glBegin(GL_POLYGON);
    glVertex3i(x,    z, y);
    glVertex3i(x+xs, z, y);
    glVertex3i(x+xs, z, y+ys);
    glVertex3i(x,    z, y+ys);
    glEnd();
};

void boxys(int x, int y, int xs, int ys, int z)
{
    glBegin(GL_POLYGON);
    glVertex3i(x,    y   , z);
    glVertex3i(x+xs, y   , z);
    glVertex3i(x+xs, y+ys, z);
    glVertex3i(x,    y+ys, z);
    glEnd();
};

void boxxs(int x, int y, int xs, int ys, int z)
{
    glBegin(GL_POLYGON);
    glVertex3i( z,y   , x   );
    glVertex3i( z,y   , x+xs);
    glVertex3i( z,y+ys, x+xs);
    glVertex3i( z,y+ys, x   );
    glEnd();
};


int orient = 0;
cube *last = NULL;
int gridsize = 32;
int corner = 0, corx, cory;

bool editmode = false; 

bool havesel = false;
int cx, cy, cz, lastx, lasty, lastz, lastcorx, lastcory;
int selx, sely, selz, selxs, selys, selzs, selgrid, selorient, selcx, selcxs, selcy, selcys;
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

void cursorupdate()
{
    int yawo[] = { O_LEFT, O_FRONT, O_RIGHT, O_BACK }; 
    if(player1->pitch>45) orient = O_TOP;
    else if(player1->pitch<-45) orient = O_BOTTOM;
    else orient = yawo[(((int)player1->yaw-45)/90)&3];

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

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    //glColor4ub(64,64,255,32);
    glColor3ub(120,120,120);
    glLineWidth(1);
    int g2 = gridsize/2;
    switch(dimension(orient))
    {
        case 0: boxzs(lux, luy, lusize, lusize, luz+dimcoord(orient)*lusize); corx = x/g2; cory = y/g2; corner = (corx-lux/g2)+(cory-luy/g2)*2; break;
        case 1: boxys(lux, luz, lusize, lusize, luy+dimcoord(orient)*lusize); corx = x/g2; cory = z/g2; corner = (corx-lux/g2)+(cory-luz/g2)*2; break;
        case 2: boxxs(luy, luz, lusize, lusize, lux+dimcoord(orient)*lusize); corx = y/g2; cory = z/g2; corner = (corx-luy/g2)+(cory-luz/g2)*2; break;
    };

    cx = lux;
    cy = luy;
    cz = luz;
    
    if(dragging && gridsize==selgrid && orient==selorient)
    {
        selx = min(lastx, cx);
        sely = min(lasty, cy);
        selz = min(lastz, cz);
        selxs = abs(lastx-cx)+selgrid;
        selys = abs(lasty-cy)+selgrid;
        selzs = abs(lastz-cz)+selgrid;
        selcx  = min(corx, lastcorx);
        selcxs = max(corx, lastcorx)-selcx+1;
        selcy  = min(cory, lastcory);
        selcys = max(cory, lastcory)-selcy+1;
        havesel = true;
        selcx &= 1;
        selcy &= 1;
    }; 
    
    if(havesel)
    {
        glColor3ub(20,20,20);
        switch(dimension(selorient))
        {
            case 0: { for(int x = selx; x<selx+selxs; x += selgrid) for(int y = sely; y<sely+selys; y += selgrid) boxzs(x, y, selgrid, selgrid, selz+dimcoord(selorient)*selzs); }; break;
            case 1: { for(int x = selx; x<selx+selxs; x += selgrid) for(int z = selz; z<selz+selzs; z += selgrid) boxys(x, z, selgrid, selgrid, sely+dimcoord(selorient)*selys); }; break;
            case 2: { for(int y = sely; y<sely+selys; y += selgrid) for(int z = selz; z<selz+selzs; z += selgrid) boxxs(y, z, selgrid, selgrid, selx+dimcoord(selorient)*selxs); }; break;
        };
        glColor3ub(60,60,60);
        //conoutf("%d %d ", selcxs*g2, selcys*g2);
        switch(dimension(selorient))
        {
            case 0: boxzs(selx+selcx*g2, sely+selcy*g2, selcxs*g2, selcys*g2, selz+dimcoord(selorient)*selzs); break;
            case 1: boxys(selx+selcx*g2, selz+selcy*g2, selcxs*g2, selcys*g2, sely+dimcoord(selorient)*selys); break;
            case 2: boxxs(sely+selcx*g2, selz+selcy*g2, selcxs*g2, selcys*g2, selx+dimcoord(selorient)*selxs); break;
        };
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
        int xs, ys;
        switch(dimension(selorient))
        {
            case 0: xs = selxs/selgrid; ys = selys/selgrid; break;
            case 1: xs = selxs/selgrid; ys = selzs/selgrid; break;
            case 2: xs = selys/selgrid; ys = selzs/selgrid; break;
        };
        loop(x,xs) loop(y,ys)
        {
            cube *c;
            switch(dimension(selorient))
            {
                case 0: c = &lookupcube(selx+x*selgrid, sely+y*selgrid, selz, selgrid); break;
                case 1: c = &lookupcube(selx+x*selgrid, sely, selz+y*selgrid, selgrid); break;
                case 2: c = &lookupcube(selx, sely+x*selgrid, selz+y*selgrid, selgrid); break;
            };
            discardchildren(*c);

            if(dimcoord(selorient)) dir *= -1;
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
