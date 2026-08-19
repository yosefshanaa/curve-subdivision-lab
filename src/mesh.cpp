#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

namespace sl {

// ------------------------------------------------------------------- Mesh

Vec3 Mesh::computeFaceNormal(int f) const {
    const std::vector<int>& face = F[f];
    Vec3 n(0, 0, 0);
    int n_ = int(face.size());
    for (int i = 0; i < n_; i++) {
        const Vec3& c = V[face[i]];
        const Vec3& d = V[face[(i + 1) % n_]];
        n.x += (c.y - d.y) * (c.z + d.z);
        n.y += (c.z - d.z) * (c.x + d.x);
        n.z += (c.x - d.x) * (c.y + d.y);
    }
    return normalize(n);
}

Vec3 Mesh::faceCentroid(int f) const {
    Vec3 c(0, 0, 0);
    for (int vi : F[f]) c += V[vi];
    return F[f].empty() ? c : c / double(F[f].size());
}

double Mesh::faceArea(int f) const {
    const std::vector<int>& face = F[f];
    int n = int(face.size());
    if (n < 3) return 0.0;
    Vec3 acc(0, 0, 0);
    for (int i = 1; i + 1 < n; i++)
        acc += cross(V[face[i]] - V[face[0]], V[face[i + 1]] - V[face[0]]);
    return 0.5 * length(acc);
}

void Mesh::computeNormals() {
    faceNormal.assign(F.size(), Vec3(0, 0, 0));
    vertexNormal.assign(V.size(), Vec3(0, 0, 0));
    for (size_t f = 0; f < F.size(); f++) {
        faceNormal[f] = computeFaceNormal(int(f));
        double w = faceArea(int(f));
        for (int vi : F[f]) vertexNormal[vi] += faceNormal[f] * w;
    }
    for (size_t v = 0; v < V.size(); v++) {
        Vec3 n = vertexNormal[v];
        vertexNormal[v] = (lengthSq(n) > 1e-24) ? normalize(n) : Vec3(0, 1, 0);
    }
}

void Mesh::clear() {
    V.clear(); F.clear(); vertexColor.clear(); faceNormal.clear(); vertexNormal.clear();
}

void Mesh::bounds(Vec3& lo, Vec3& hi) const {
    if (V.empty()) { lo = hi = Vec3(0, 0, 0); return; }
    lo = hi = V[0];
    for (const Vec3& p : V) { lo = vmin(lo, p); hi = vmax(hi, p); }
}

Vec3 Mesh::centroid() const {
    Vec3 c(0, 0, 0);
    for (const Vec3& p : V) c += p;
    return V.empty() ? c : c / double(V.size());
}

void Mesh::normalizeToRadius(double targetRadius) {
    if (V.empty()) return;
    Vec3 lo, hi;
    bounds(lo, hi);
    Vec3 mid = (lo + hi) * 0.5;
    double r = 0;
    for (const Vec3& p : V) r = std::max(r, length(p - mid));
    double s = (r > 1e-12) ? (targetRadius / r) : 1.0;
    for (Vec3& p : V) p = (p - mid) * s;
}

bool Mesh::allFacesAreTriangles() const {
    for (const auto& f : F) if (f.size() != 3) return false;
    return !F.empty();
}

bool Mesh::allFacesAreQuads() const {
    for (const auto& f : F) if (f.size() != 4) return false;
    return !F.empty();
}

// --------------------------------------------------------------- Topology

Topology buildTopology(const Mesh& m) {
    Topology t;
    t.vertEdges.assign(m.V.size(), {});
    t.vertFaces.assign(m.V.size(), {});
    t.faceEdges.assign(m.F.size(), {});
    t.vertBoundary.assign(m.V.size(), 0);

    std::map<std::pair<int, int>, int> lookup;
    auto getEdge = [&](int a, int b) -> int {
        std::pair<int, int> key(std::min(a, b), std::max(a, b));
        auto it = lookup.find(key);
        if (it != lookup.end()) return it->second;
        Topology::Edge e;
        e.a = key.first;
        e.b = key.second;
        t.edges.push_back(e);
        int idx = int(t.edges.size()) - 1;
        lookup[key] = idx;
        t.vertEdges[key.first].push_back(idx);
        t.vertEdges[key.second].push_back(idx);
        return idx;
    };

    for (size_t f = 0; f < m.F.size(); f++) {
        const std::vector<int>& face = m.F[f];
        int n = int(face.size());
        t.faceEdges[f].resize(n);
        for (int i = 0; i < n; i++) {
            int e = getEdge(face[i], face[(i + 1) % n]);
            t.faceEdges[f][i] = e;
            if (t.edges[e].f0 < 0)        t.edges[e].f0 = int(f);
            else if (t.edges[e].f1 < 0)   t.edges[e].f1 = int(f);
            else                          t.hasNonManifold = true;
        }
        for (int vi : face) {
            auto& vf = t.vertFaces[vi];
            if (std::find(vf.begin(), vf.end(), int(f)) == vf.end()) vf.push_back(int(f));
        }
    }

    for (const Topology::Edge& e : t.edges) {
        if (e.boundary()) {
            t.vertBoundary[e.a] = 1;
            t.vertBoundary[e.b] = 1;
        }
    }
    return t;
}

int Topology::edgeIndex(int a, int b) const {
    int lo = std::min(a, b);
    for (int e : vertEdges[lo])
        if ((edges[e].a == a && edges[e].b == b) || (edges[e].a == b && edges[e].b == a)) return e;
    return -1;
}

bool Topology::boundaryNeighbours(int v, int& n0, int& n1) const {
    n0 = n1 = -1;
    for (int e : vertEdges[v]) {
        if (!edges[e].boundary()) continue;
        int o = otherEnd(e, v);
        if (n0 < 0)      n0 = o;
        else if (n1 < 0) n1 = o;
    }
    return n0 >= 0 && n1 >= 0;
}

int Topology::oppositeVerts(const Mesh& m, int e, int& o0, int& o1) const {
    o0 = o1 = -1;
    int found = 0;
    const Edge& ed = edges[e];
    for (int f : {ed.f0, ed.f1}) {
        if (f < 0) continue;
        for (int vi : m.F[f]) {
            if (vi != ed.a && vi != ed.b) {
                (found == 0 ? o0 : o1) = vi;
                found++;
                break;
            }
        }
    }
    return found;
}

std::vector<int> Topology::orderedNeighbours(int v) const {
    std::vector<int> result;
    const std::vector<int>& faces = vertFaces[v];
    if (faces.empty()) return result;

    // Walk face-to-face across shared edges. Starting from a boundary edge (when
    // one exists) makes the walk terminate instead of looping.
    int startEdge = -1;
    for (int e : vertEdges[v])
        if (edges[e].boundary()) { startEdge = e; break; }
    if (startEdge < 0) startEdge = vertEdges[v].empty() ? -1 : vertEdges[v][0];
    if (startEdge < 0) return result;

    int curEdge = startEdge;
    int curFace = edges[curEdge].f0;
    std::vector<char> seenEdge(edges.size(), 0);

    for (size_t guard = 0; guard < vertEdges[v].size() + 2; guard++) {
        if (curEdge < 0 || seenEdge[curEdge]) break;
        seenEdge[curEdge] = 1;
        result.push_back(otherEnd(curEdge, v));
        if (curFace < 0) break;

        // The other edge of curFace that also touches v.
        int nextEdge = -1;
        for (int e : faceEdges[curFace]) {
            if (e == curEdge) continue;
            if (edges[e].a == v || edges[e].b == v) { nextEdge = e; break; }
        }
        if (nextEdge < 0) break;
        int nextFace = (edges[nextEdge].f0 == curFace) ? edges[nextEdge].f1 : edges[nextEdge].f0;
        curEdge = nextEdge;
        curFace = nextFace;
    }

    // Fall back to unordered adjacency if the walk missed anything.
    if (result.size() != vertEdges[v].size()) {
        result.clear();
        for (int e : vertEdges[v]) result.push_back(otherEnd(e, v));
    }
    return result;
}

// ------------------------------------------------------------------ stats

MeshStats computeStats(const Mesh& m, const Topology& t) {
    MeshStats s;
    s.verts = m.numVerts();
    s.faces = m.numFaces();
    s.edges = t.numEdges();
    s.euler = s.verts - s.edges + s.faces;

    for (const auto& e : t.edges) {
        if (e.boundary()) s.boundaryEdges++;
        double L = length(m.V[e.a] - m.V[e.b]);
        s.avgEdgeLen += L;
        if (s.minEdgeLen == 0 || L < s.minEdgeLen) s.minEdgeLen = L;
        s.maxEdgeLen = std::max(s.maxEdgeLen, L);
    }
    if (s.edges) s.avgEdgeLen /= s.edges;

    for (const auto& f : m.F) {
        if (f.size() == 3)      s.triangles++;
        else if (f.size() == 4) s.quads++;
        else                    s.ngons++;
    }

    // "Extraordinary" is scheme-relative: valence 4 is regular on a quad mesh,
    // valence 6 on a triangle mesh.
    int regular = (s.triangles > s.quads) ? 6 : 4;
    long long valSum = 0;
    for (int v = 0; v < s.verts; v++) {
        int val = t.valence(v);
        valSum += val;
        if (v == 0) s.minValence = s.maxValence = val;
        s.minValence = std::min(s.minValence, val);
        s.maxValence = std::max(s.maxValence, val);
        if (!t.vertBoundary[v] && val != regular) s.extraordinary++;
    }
    if (s.verts) s.avgValence = double(valSum) / s.verts;
    return s;
}

int orientationDefects(const Mesh& m) {
    std::map<std::pair<int, int>, int> seen;
    for (const auto& f : m.F) {
        int n = int(f.size());
        for (int i = 0; i < n; i++) seen[{f[i], f[(i + 1) % n]}]++;
    }
    int defects = 0;
    for (const auto& kv : seen) {
        if (kv.second != 1) defects++;                     // same direction twice
        auto rev = seen.find({kv.first.second, kv.first.first});
        if (rev != seen.end() && rev->second != kv.second) defects++;
    }
    return defects;
}

// -------------------------------------------------------------- rendering

std::vector<RTri> triangulate(const Mesh& m, bool smooth) {
    std::vector<RTri> out;
    if (m.faceNormal.size() != m.F.size() || m.vertexNormal.size() != m.V.size()) return out;
    out.reserve(m.F.size() * 2);
    const bool haveColor = m.vertexColor.size() == m.V.size();

    for (size_t f = 0; f < m.F.size(); f++) {
        const std::vector<int>& face = m.F[f];
        int n = int(face.size());
        if (n < 3) continue;
        Vec3 fn = m.faceNormal[f];

        if (n <= 4) {
            // Fan from corner 0 — exact for triangles, fine for near-planar quads.
            for (int i = 1; i + 1 < n; i++) {
                RTri t;
                t.face = int(f);
                int idx[3] = {face[0], face[i], face[i + 1]};
                for (int k = 0; k < 3; k++) {
                    t.p[k] = m.V[idx[k]];
                    t.n[k] = smooth ? m.vertexNormal[idx[k]] : fn;
                    if (haveColor) t.c[k] = m.vertexColor[idx[k]];
                }
                out.push_back(t);
            }
        } else {
            // n-gons (Doo-Sabin vertex faces) fan from the centroid, which stays
            // inside even when the polygon is not convex.
            Vec3 c = m.faceCentroid(int(f));
            Vec3 cn(0, 0, 0);
            Vec3 ccol(0, 0, 0);
            for (int vi : face) {
                cn += m.vertexNormal[vi];
                if (haveColor) ccol += m.vertexColor[vi];
            }
            cn = (lengthSq(cn) > 1e-24) ? normalize(cn) : fn;
            ccol /= double(n);
            for (int i = 0; i < n; i++) {
                int a = face[i], b = face[(i + 1) % n];
                RTri t;
                t.face = int(f);
                t.p[0] = c;      t.n[0] = smooth ? cn : fn;
                t.p[1] = m.V[a]; t.n[1] = smooth ? m.vertexNormal[a] : fn;
                t.p[2] = m.V[b]; t.n[2] = smooth ? m.vertexNormal[b] : fn;
                if (haveColor) { t.c[0] = ccol; t.c[1] = m.vertexColor[a]; t.c[2] = m.vertexColor[b]; }
                out.push_back(t);
            }
        }
    }
    return out;
}

Mesh triangulateMesh(const Mesh& m) {
    Mesh out;
    out.V = m.V;
    for (size_t fi = 0; fi < m.F.size(); fi++) {
        const std::vector<int>& f = m.F[fi];
        int n = int(f.size());
        if (n < 3) continue;
        if (n == 3) { out.F.push_back(f); continue; }
        if (n == 4) {
            // Split along the shorter diagonal for better-shaped triangles.
            double d02 = lengthSq(m.V[f[0]] - m.V[f[2]]);
            double d13 = lengthSq(m.V[f[1]] - m.V[f[3]]);
            if (d02 <= d13) {
                out.F.push_back({f[0], f[1], f[2]});
                out.F.push_back({f[0], f[2], f[3]});
            } else {
                out.F.push_back({f[1], f[2], f[3]});
                out.F.push_back({f[1], f[3], f[0]});
            }
            continue;
        }
        int c = int(out.V.size());
        out.V.push_back(m.faceCentroid(int(fi)));
        for (int i = 0; i < n; i++) out.F.push_back({c, f[i], f[(i + 1) % n]});
    }
    out.computeNormals();
    return out;
}

// ------------------------------------------------------------- base meshes

const char* baseMeshName(BaseMesh b) {
    switch (b) {
        case BaseMesh::Cube:        return "Cube";
        case BaseMesh::Tetrahedron: return "Tetrahedron";
        case BaseMesh::Octahedron:  return "Octahedron";
        case BaseMesh::Icosahedron: return "Icosahedron";
        case BaseMesh::Torus:       return "Torus";
        case BaseMesh::Plane:       return "Open plane";
        case BaseMesh::Cylinder:    return "Open cylinder";
        case BaseMesh::LBlock:      return "L-block";
        case BaseMesh::Cross:       return "Cross";
        case BaseMesh::Pyramid:     return "Pyramid";
        default:                    return "?";
    }
}

bool parseBaseMesh(const std::string& name, BaseMesh& out) {
    std::string n;
    for (char c : name) if (c != '-' && c != '_' && c != ' ') n += char(std::tolower(c));
    struct { const char* key; BaseMesh v; } table[] = {
        {"cube", BaseMesh::Cube},           {"tetrahedron", BaseMesh::Tetrahedron},
        {"tetra", BaseMesh::Tetrahedron},   {"octahedron", BaseMesh::Octahedron},
        {"octa", BaseMesh::Octahedron},     {"icosahedron", BaseMesh::Icosahedron},
        {"icosa", BaseMesh::Icosahedron},   {"torus", BaseMesh::Torus},
        {"plane", BaseMesh::Plane},         {"cylinder", BaseMesh::Cylinder},
        {"lblock", BaseMesh::LBlock},       {"cross", BaseMesh::Cross},
        {"pyramid", BaseMesh::Pyramid},
    };
    for (auto& t : table)
        if (n == t.key) { out = t.v; return true; }
    return false;
}

namespace {

// Build a mesh from a "voxel" set: emit only the faces that are not shared
// between two solid cells, which yields a closed, manifold quad surface.
Mesh polycube(const std::vector<Vec3>& cells) {
    auto key = [](int x, int y, int z) {
        return ((long long)(x + 64) << 20) | ((long long)(y + 64) << 10) | (long long)(z + 64);
    };
    std::map<long long, bool> solid;
    for (const Vec3& c : cells) solid[key(int(c.x), int(c.y), int(c.z))] = true;

    Mesh m;
    std::map<long long, int> vmap;
    auto vert = [&](int x, int y, int z) {
        long long k = key(x, y, z);
        auto it = vmap.find(k);
        if (it != vmap.end()) return it->second;
        int id = int(m.V.size());
        m.V.push_back(Vec3(x, y, z));
        vmap[k] = id;
        return id;
    };

    const int dirs[6][3] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
    for (const Vec3& c : cells) {
        int cx = int(c.x), cy = int(c.y), cz = int(c.z);
        for (int d = 0; d < 6; d++) {
            int nx = cx + dirs[d][0], ny = cy + dirs[d][1], nz = cz + dirs[d][2];
            if (solid.count(key(nx, ny, nz))) continue;  // interior face: skip

            // The four corners of that cell face, wound CCW as seen from outside.
            int q[4][3];
            switch (d) {
                case 0: q[0][0]=cx+1;q[0][1]=cy;  q[0][2]=cz;   q[1][0]=cx+1;q[1][1]=cy+1;q[1][2]=cz;
                        q[2][0]=cx+1;q[2][1]=cy+1;q[2][2]=cz+1; q[3][0]=cx+1;q[3][1]=cy;  q[3][2]=cz+1; break;
                case 1: q[0][0]=cx;  q[0][1]=cy;  q[0][2]=cz+1; q[1][0]=cx;  q[1][1]=cy+1;q[1][2]=cz+1;
                        q[2][0]=cx;  q[2][1]=cy+1;q[2][2]=cz;   q[3][0]=cx;  q[3][1]=cy;  q[3][2]=cz;   break;
                case 2: q[0][0]=cx;  q[0][1]=cy+1;q[0][2]=cz+1; q[1][0]=cx+1;q[1][1]=cy+1;q[1][2]=cz+1;
                        q[2][0]=cx+1;q[2][1]=cy+1;q[2][2]=cz;   q[3][0]=cx;  q[3][1]=cy+1;q[3][2]=cz;   break;
                case 3: q[0][0]=cx;  q[0][1]=cy;  q[0][2]=cz;   q[1][0]=cx+1;q[1][1]=cy;  q[1][2]=cz;
                        q[2][0]=cx+1;q[2][1]=cy;  q[2][2]=cz+1; q[3][0]=cx;  q[3][1]=cy;  q[3][2]=cz+1; break;
                case 4: q[0][0]=cx;  q[0][1]=cy;  q[0][2]=cz+1; q[1][0]=cx+1;q[1][1]=cy;  q[1][2]=cz+1;
                        q[2][0]=cx+1;q[2][1]=cy+1;q[2][2]=cz+1; q[3][0]=cx;  q[3][1]=cy+1;q[3][2]=cz+1; break;
                default:q[0][0]=cx;  q[0][1]=cy+1;q[0][2]=cz;   q[1][0]=cx+1;q[1][1]=cy+1;q[1][2]=cz;
                        q[2][0]=cx+1;q[2][1]=cy;  q[2][2]=cz;   q[3][0]=cx;  q[3][1]=cy;  q[3][2]=cz;   break;
            }
            m.F.push_back({vert(q[0][0],q[0][1],q[0][2]), vert(q[1][0],q[1][1],q[1][2]),
                           vert(q[2][0],q[2][1],q[2][2]), vert(q[3][0],q[3][1],q[3][2])});
        }
    }
    return m;
}

}  // namespace

Mesh makeBaseMesh(BaseMesh b) {
    Mesh m;
    switch (b) {
        case BaseMesh::Cube: {
            m.V = {{-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
                   {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1}};
            m.F = {{0,3,2,1}, {4,5,6,7}, {0,1,5,4}, {1,2,6,5}, {2,3,7,6}, {3,0,4,7}};
            break;
        }
        case BaseMesh::Tetrahedron: {
            m.V = {{1,1,1}, {1,-1,-1}, {-1,1,-1}, {-1,-1,1}};
            m.F = {{0,1,2}, {0,3,1}, {0,2,3}, {1,3,2}};
            break;
        }
        case BaseMesh::Octahedron: {
            m.V = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
            m.F = {{0,2,4}, {2,1,4}, {1,3,4}, {3,0,4},
                   {2,0,5}, {1,2,5}, {3,1,5}, {0,3,5}};
            break;
        }
        case BaseMesh::Icosahedron: {
            const double t = (1.0 + std::sqrt(5.0)) / 2.0;
            m.V = {{-1,t,0},{1,t,0},{-1,-t,0},{1,-t,0},
                   {0,-1,t},{0,1,t},{0,-1,-t},{0,1,-t},
                   {t,0,-1},{t,0,1},{-t,0,-1},{-t,0,1}};
            m.F = {{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
                   {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
                   {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
                   {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
            break;
        }
        case BaseMesh::Torus: {
            const int NU = 12, NV = 8;
            const double R = 1.0, r = 0.42;
            for (int i = 0; i < NU; i++) {
                double u = 2 * PI * i / NU;
                for (int j = 0; j < NV; j++) {
                    double v = 2 * PI * j / NV;
                    m.V.push_back(Vec3((R + r * std::cos(v)) * std::cos(u),
                                       r * std::sin(v),
                                       (R + r * std::cos(v)) * std::sin(u)));
                }
            }
            auto id = [&](int i, int j) { return ((i % NU) * NV) + (j % NV); };
            for (int i = 0; i < NU; i++)
                for (int j = 0; j < NV; j++)
                    m.F.push_back({id(i,j), id(i+1,j), id(i+1,j+1), id(i,j+1)});
            break;
        }
        case BaseMesh::Plane: {
            // Open quad grid with a raised middle: shows the boundary rules.
            const int N = 4;
            for (int i = 0; i <= N; i++)
                for (int j = 0; j <= N; j++) {
                    double x = -1.0 + 2.0 * i / N, z = -1.0 + 2.0 * j / N;
                    double y = 0.75 * std::exp(-3.0 * (x * x + z * z));
                    m.V.push_back(Vec3(x, y, z));
                }
            auto id = [&](int i, int j) { return i * (N + 1) + j; };
            for (int i = 0; i < N; i++)
                for (int j = 0; j < N; j++)
                    m.F.push_back({id(i,j), id(i,j+1), id(i+1,j+1), id(i+1,j)});
            break;
        }
        case BaseMesh::Cylinder: {
            // Tube with both ends open — two boundary loops.
            const int NU = 10, NV = 3;
            for (int i = 0; i < NU; i++) {
                double u = 2 * PI * i / NU;
                for (int j = 0; j <= NV; j++)
                    m.V.push_back(Vec3(std::cos(u), -1.0 + 2.0 * j / NV, std::sin(u)));
            }
            auto id = [&](int i, int j) { return (i % NU) * (NV + 1) + j; };
            for (int i = 0; i < NU; i++)
                for (int j = 0; j < NV; j++)
                    m.F.push_back({id(i,j), id(i+1,j), id(i+1,j+1), id(i,j+1)});
            break;
        }
        case BaseMesh::LBlock: {
            std::vector<Vec3> cells;
            for (int x = 0; x < 3; x++) cells.push_back(Vec3(x, 0, 0));
            for (int y = 1; y < 3; y++) cells.push_back(Vec3(0, y, 0));
            m = polycube(cells);
            break;
        }
        case BaseMesh::Cross: {
            std::vector<Vec3> cells = {{0,0,0}, {1,0,0}, {-1,0,0},
                                       {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
            m = polycube(cells);
            break;
        }
        case BaseMesh::Pyramid: {
            m.V = {{-1,-0.7,-1}, {1,-0.7,-1}, {1,-0.7,1}, {-1,-0.7,1}, {0,1.1,0}};
            m.F = {{0,3,2,1}, {0,1,4}, {1,2,4}, {2,3,4}, {3,0,4}};
            break;
        }
        default: break;
    }
    m.normalizeToRadius(1.0);
    m.computeNormals();
    return m;
}

}  // namespace sl
