struct PackNode
{
    PackNode *children;
    ushort x, y, w, h;
    bool packed;

    PackNode()
     : children(0), x(0), y(0), w(LM_PACKW), h(LM_PACKH), packed(false)
    {}

    PackNode(ushort x, ushort y, ushort w, ushort h)
     : children(0), x(x), y(y), w(w), h(h), packed(false)
    {}

    ~PackNode()
    {
        if(children)
        {
            children[0].~PackNode();
            children[1].~PackNode();
            gp()->dealloc(children, 2 * sizeof(PackNode));
        }
    }

    bool insert(ushort &tx, ushort &ty, ushort tw, ushort th)
    {
        if(packed || w < tw || h < th)
            return false;
        if(children)
        {
            bool inserted = children[0].insert(tx, ty, tw, th) ||
                            children[1].insert(tx, ty, tw, th);
            packed = children[0].packed && children[1].packed;
            return inserted;    
        }
        if(w == tw && h == th)
        {
            packed = true;
            tx = x;
            ty = y;
            return true;
        }
        
        children = gp()->alloc(2 * sizeof(PackNode));
        if(w - tw > h - th)
        {
            new (children) PackNode(x, y, tw, h);
            new (children + 1) PackNode(x + tw, y, w - tw, h);
        }
        else
        {
            new (children) PackNode(x, y, w, th);
            new (children + 1) PackNode(x, y + th, w, h - th);
        }

        return children[0].insert(tx, ty, tw, th);
    }
};

struct LightMap
{
    uchar data[3 * LM_PACKW * LM_PACKH];
    PackNode packroot;

    LightMap()
     : packroot(LM_PACKW, LM_PACKH)
    {
        memset(data, 0, sizeof(data));
    }

    bool insert(ushort &tx, ushort &ty, uchar *src, ushort tw, ushort th)
    {
        if(!packroot.insert(tx, ty, tw, th))
            return false;
        uchar *dst = data + tx + ty * 3 * LM_PACKW;
        loop(y, th)
        {
            loop(x, tw)
            {
                memcpy(dst, src, 3 * tw);
                dst += 3 * LM_PACKW;
                src += 3 * tw;
            }
        }
    }
        
};

extern vector<LightMap *> lightmaps;

