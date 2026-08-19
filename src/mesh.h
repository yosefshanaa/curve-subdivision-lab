// mesh.h — general polygon mesh + the adjacency information subdivision needs.
//
// Faces are arbitrary vertex-index loops in counter-clockwise order, because
// the subdivision schemes do not agree on face degree: Catmull-Clark turns
// anything into quads, Loop and Butterfly want triangles, and Doo-Sabin
// produces n-gons around every old vertex.
#pragma once

#include <string>
#include <vector>

#include "vecmath.h"

namespace sl {

struct Mesh {
    std::vector<Vec3> V;                 // vertex positions
    std::vector<std::vector<int>> F;     // faces: CCW loops of vertex indices

    // Optional per-vertex colour (terrain elevation ramp, scheme comparison
    // heat maps). Empty means "use the material albedo".
    std::vector<Vec3> vertexColor;

    // Derived by computeNormals(); cleared whenever geometry changes.
    std::vector<Vec3> faceNormal;
    std::vector<Vec3> vertexNormal;

    int numVerts() const { return int(V.size()); }
    int numFaces() const { return int(F.size()); }

    // Newell's method — correct even for the slightly non-planar quads that
    // subdivision produces.
    Vec3 computeFaceNormal(int f) const;
    Vec3 faceCentroid(int f) const;
    double faceArea(int f) const;

    void computeNormals();               // face normals + area-weighted vertex normals
    void clear();

    // Centre on the bounding-box centre and scale so the bounding sphere has
    // the given radius. Framing then behaves the same for every base cage,
    // whether it is a cube (corners far out) or an icosahedron (nearly round).
    void normalizeToRadius(double targetRadius = 1.0);
    void bounds(Vec3& lo, Vec3& hi) const;
    Vec3 centroid() const;

    bool allFacesAreTriangles() const;
    bool allFacesAreQuads() const;
};

// ------------------------------------------------------------------ topology

struct Topology {
    struct Edge {
        int a = -1, b = -1;   // endpoints, stored with a < b
        int f0 = -1, f1 = -1; // incident faces; f1 == -1 means a boundary edge
        bool boundary() const { return f1 < 0; }
    };

    std::vector<Edge> edges;
    std::vector<std::vector<int>> vertEdges;  // edge indices around each vertex
    std::vector<std::vector<int>> vertFaces;  // face indices around each vertex
    std::vector<std::vector<int>> faceEdges;  // per face, edge for corner i -> i+1
    std::vector<char> vertBoundary;           // 1 if the vertex lies on a boundary
    std::vector<char> nonManifold;            // 1 if an edge had > 2 incident faces

    int numEdges() const { return int(edges.size()); }
    int edgeIndex(int a, int b) const;        // -1 if the edge does not exist
    int valence(int v) const { return int(vertEdges[v].size()); }
    int otherEnd(int e, int v) const { return edges[e].a == v ? edges[e].b : edges[e].a; }

    // For a boundary vertex, the two neighbours reached along boundary edges.
    bool boundaryNeighbours(int v, int& n0, int& n1) const;

    // The two vertices opposite edge e in its incident triangles (Loop/Butterfly).
    // Returns how many were found (0, 1 for a boundary edge, or 2).
    int oppositeVerts(const Mesh& m, int e, int& o0, int& o1) const;

    // Neighbours of v in cyclic order around the one-ring, starting from a
    // boundary edge when v is on the boundary. Needed by Butterfly's
    // extraordinary-vertex stencil.
    std::vector<int> orderedNeighbours(int v) const;

    bool hasNonManifold = false;
};

Topology buildTopology(const Mesh& m);

// ------------------------------------------------------------------- stats

struct MeshStats {
    int verts = 0, edges = 0, faces = 0;
    int euler = 0;                 // V - E + F
    int boundaryEdges = 0;
    int triangles = 0, quads = 0, ngons = 0;
    int extraordinary = 0;         // valence != 4 (quad mesh) or != 6 (tri mesh)
    int minValence = 0, maxValence = 0;
    double avgValence = 0;
    double minEdgeLen = 0, maxEdgeLen = 0, avgEdgeLen = 0;
};

MeshStats computeStats(const Mesh& m, const Topology& t);

// --------------------------------------------------------------- rendering

// A triangle handed to the rasteriser: positions plus per-corner normals.
struct RTri {
    Vec3 p[3];
    Vec3 n[3];
    Vec3 c[3] = {Vec3(1, 1, 1), Vec3(1, 1, 1), Vec3(1, 1, 1)};
    int face = 0;
};

// Fan-triangulate every face. `smooth` picks per-vertex normals (Gouraud/Phong)
// over per-face normals (flat shading).
std::vector<RTri> triangulate(const Mesh& m, bool smooth);

// ------------------------------------------------------------- base meshes

enum class BaseMesh {
    Cube, Tetrahedron, Octahedron, Icosahedron, Torus,
    Plane, Cylinder, LBlock, Cross, Pyramid, Count
};

const char* baseMeshName(BaseMesh b);
Mesh makeBaseMesh(BaseMesh b);
bool parseBaseMesh(const std::string& name, BaseMesh& out);

// Every quad face split into two triangles (for the triangle-only schemes).
Mesh triangulateMesh(const Mesh& m);

}  // namespace sl
