VAR(dbgts, 0, 0, 1);
VAR(tsswap, 0, 1, 1);

struct tristrip
{
    enum
    {
        // must be larger than all other indices
        REMOVED = 0xFFFE,
        UNUSED  = 0xFFFF
    };

    struct triangle
    {
        ushort v[3], n[3];

        bool link(ushort neighbor, ushort old = UNUSED)
        {
            loopi(3) if(n[i]==old) { n[i] = neighbor; return true; };
            return false;
        };    
   
        int numlinks() { int num = 0; loopi(3) if(n[i]!=UNUSED) num++; return num; };

        bool hasvert(ushort idx) { loopi(3) if(v[i]==idx) return true; return false; };
    };

    vector<triangle> triangles;
    vector<ushort> connectivity[4];
    vector<uchar> nodes;

    void addtriangles(const ushort *triidxs, int numtris)
    {
        if(dbgts) conoutf("addtriangles: tris = %d, inds = %d", numtris, numtris*3);
        loopi(numtris)
        {
            triangle &tri = triangles.add();
            loopj(3) 
            {
                tri.v[j] = *triidxs++;
                tri.n[j] = UNUSED;
            };
            if(tri.v[0]==tri.v[1] || tri.v[1]==tri.v[2] || tri.v[2]==tri.v[0])
            {
                if(dbgts) conoutf("degenerate triangle");
                triangles.drop();
            };
        };
    };

    struct edge
    {
        ushort from, to;

        edge() {};
        edge(ushort from, ushort to) : from(min(from, to)), to(max(from, to)) {};

        void link(ushort tri)
        {
            if(tri < from)
            {
                to = from;
                from = tri;
            } 
            else to = tri; 
        };
    };

    void findconnectivity()
    {
        hashtable<edge, edge> edges;
        nodes.setsizenodelete(0);
        loopv(triangles)
        {
            triangle &tri = triangles[i];
            loopj(3) { while(nodes.length()<=tri.v[j]) nodes.add(0); nodes[tri.v[j]]++; };
            loopj(3)
            {
                edge e(tri.v[j], tri.v[j==2 ? 0 : j+1]);
                edge *conn = edges.access(e);
                if(conn) 
                { 
                    if(conn->to==UNUSED && tri.link(conn->from))
                    {
                        if(triangles[conn->from].link(i)) conn->link(i); 
                        else
                        {
                            tri.link(UNUSED, conn->from);
                        };
                    };
                }
                else edges[e] = edge(i, UNUSED); 
            };
        };
        loopi(4) connectivity[i].setsizenodelete(0);
        loopv(triangles) connectivity[triangles[i].numlinks()].add(i);
    };

    void removeconnectivity(ushort i)
    {
        triangle &tri = triangles[i];
        loopj(3) if(tri.n[j]!=UNUSED)
        {
            triangle &neighbor = triangles[tri.n[j]];
            if(neighbor.n[0]==REMOVED) continue;
            int conn = neighbor.numlinks();
            bool linked = false;
            loopk(3) if(neighbor.n[k]==i) { linked = true; neighbor.n[k] = UNUSED; break; };
            if(linked)
            {
                connectivity[conn].replacewithlast(tri.n[j]);
                connectivity[conn-1].add(tri.n[j]);
            };
        };
    };

    bool remaining()
    {
        loopi(4) if(!connectivity[i].empty()) return true;
        return false;
    };

    ushort leastconnected()
    {
        loopi(4) if(!connectivity[i].empty()) return connectivity[i].pop();
        return UNUSED;
    };

    int findedge(const triangle &from, const triangle &to, ushort v = UNUSED)
    {
        loopi(3) loopj(3)
        {
            if(from.v[i]==to.v[j] && from.v[i]!=v) return i;
        }
        return -1;
    };

    void removenodes(int i)
    {
        loopj(3) nodes[triangles[i].v[j]]--;
    };

    ushort nexttriangle(triangle &tri, bool &nextswap, ushort v1 = UNUSED, ushort v2 = UNUSED)
    {
        ushort next = UNUSED;
        int nextscore = 777;
        nextswap = false;
        loopi(3) if(tri.n[i]!=UNUSED)
        {
            triangle &nexttri = triangles[tri.n[i]]; 
            if(nexttri.n[0]==REMOVED) continue;
            int score = nexttri.numlinks();
            bool swap = false;
            if(v1 != UNUSED) 
            {
                if(!nexttri.hasvert(v1))
                {
                    swap = true;
                    score += nodes[v2] > nodes[v1] ? 1 : -1;
                }
                else score += nodes[v1] > nodes[v2] ? 1 : -1;
                if(!tsswap && swap) continue;
                score += swap ? 1 : -1;
            };
            if(score < nextscore) { next = tri.n[i]; nextswap = swap; nextscore = score; };
        };
        tri.n[0] = REMOVED;
        if(next!=UNUSED) 
        {
            connectivity[triangles[next].numlinks()].replacewithlast(next);
            removenodes(next);
        };
        return next;
    };

    void buildstrip(vector<ushort> &strip, bool reverse = false)
    {
        ushort prev = leastconnected();
        if(prev==UNUSED) return;
        removenodes(prev);
        removeconnectivity(prev);
        bool doswap;
        ushort cur = nexttriangle(triangles[prev], doswap);
        if(cur==UNUSED)
        {
            loopi(3) strip.add(triangles[prev].v[reverse && i>=1 ? 3-i : i]);
            return;
        };
        removeconnectivity(cur);
        int from = findedge(triangles[prev], triangles[cur]);
        int to = findedge(triangles[prev], triangles[cur], triangles[prev].v[from]);
        if(from+1!=to) swap(int, from, to); 
        strip.add(triangles[prev].v[(to+1)%3]);
        if(reverse) swap(int, from, to);
        strip.add(triangles[prev].v[from]);
        strip.add(triangles[prev].v[to]);

        while(cur!=UNUSED)
        {
            prev = cur;
            ushort prev1 = strip.last(), prev2 = strip[strip.length()-2];
            cur = nexttriangle(triangles[prev], doswap, prev1, prev2);
            if(cur!=UNUSED) removeconnectivity(cur);
            if(doswap) strip.add(prev2);
            ushort v;
            loopi(3)
            {
                v = triangles[prev].v[i];
                if(v!=prev1 && v!=prev2) break;
            };
            strip.add(v);
        };
    };

    void buildstrips(vector<ushort> &strips, bool degen)
    {
        int numtris = 0, numstrips = 0;
        while(remaining())
        {
            vector<ushort> strip;
            bool reverse = degen && !strips.empty() && (strips.length()&1);
            buildstrip(strip, reverse);
            numstrips++;
            numtris += strip.length()-2;
            if(!strips.empty())
            {
                if(degen) { strips.dup(); strips.add(strip[0]); }
                else strips.add(UNUSED);
            };
            loopv(strip) strips.add(strip[i]);
        };
        if(dbgts) conoutf("strips = %d, tris = %d, inds = %d, merges = %d", numstrips, numtris, numtris + numstrips*2, (degen ? 2 : 1)*(numstrips-1));
    };
};

static inline uint hthash(const tristrip::edge &x) { return x.from^x.to; };
static inline bool htcmp(const tristrip::edge &x, const tristrip::edge &y) { return x.from==y.from && x.to==y.to; };
            
