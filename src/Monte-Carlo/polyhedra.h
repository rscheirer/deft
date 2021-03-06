#include "vector3d.h"
#pragma once

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

const int NONE=0;
const int CUBE=1;
const int TETRAHEDRON=2;
const int TRUNCATED_TETRAHEDRON=3;
const int CUBOID=4;

struct poly_shape {
  // faces are unit vectors normal to the actual faces. In the case of
  // parallel faces, only one of them is used

  // edges are unit vectors to the center of the edges
  int nvertices;
  int nfaces;
  int nedges;
  vector3d *vertices;
  vector3d *faces;
  vector3d *edges;
  double volume;
  char *name;
  int type;

  poly_shape();
  explicit poly_shape(const char *set_name, double ratio=1);
  ~poly_shape();

private:
  DISALLOW_COPY_AND_ASSIGN(poly_shape);
};

// Note: once assigned, the poly_shape of a polyhedron is never
// cleared. It is expected that there are only a few poly_shapes with
// many polyhedra pointing to each
struct polyhedron {
  vector3d pos;
  rotation rot;
  double R;
  const poly_shape *mypoly;
  int *neighbors;
  int num_neighbors;
  vector3d neighbor_center;

  polyhedron();
  polyhedron(const polyhedron &p);

  polyhedron operator=(const polyhedron &p);
};

struct counter {
  long totalmoves;
  long workingmoves;
  long informs;
  long updates;

  counter() {
    totalmoves = workingmoves = informs = updates = 0; }
  counter(const long n_tmoves, const long n_wmoves, const long new_informs, const long new_updates) {
    totalmoves = n_tmoves; workingmoves = n_wmoves; informs = new_informs; updates = new_updates; }
  counter(const counter &c) {
    totalmoves = c.totalmoves; workingmoves = c.workingmoves; informs = c.informs; updates = c.updates; }
  counter operator+(const counter &c) const {
    return counter(totalmoves+c.totalmoves, workingmoves+c.workingmoves, informs+c.informs, updates+c.updates); }
  counter operator+=(const counter &c) {
    totalmoves += c.totalmoves; workingmoves += c.workingmoves; informs += c.informs; updates += c.updates;
    return *this; }
};

// struct polyhedron; fixme: forward declare

// Create and initialize the neighbor tables for polyhedra p.  Returns
// the maximum number of neighbors that any polyhedron has, or -1 if
// that number is larger than max_neighbors.
int initialize_neighbor_tables(polyhedron *p, int N, double neighborR,
                               int max_neighbors, const double periodic[3]);

// Find's the neighbors of a by comparing a's position to the center of
// everyone else's neighborsphere, where id is the index of a in p.
void update_neighbors(polyhedron &a, int id, const polyhedron *p, int N, double neighborR, const double periodic[3]);

// Removes n from the neighbor table of anyone neighboring oldp but not newp.
// Adds n to the neighbor table of anyone neighboring newp but not oldp.
// Keeps the neighbor tables sorted.
void inform_neighbors(const polyhedron &newp, const polyhedron &oldp, int id, polyhedron *p, int N);

// moves v to inside the cell
vector3d fix_periodic(vector3d v, const double len[3]);

// Return the vector pointing from a to b, accounting for periodic
// boundaries
vector3d periodic_diff(const vector3d &a, const vector3d  &b, const double periodic[3]);


// Overlap functions use the seperating axis theorem. Not the fastest algorithm,
// but simple to implement and understand.
// Theorem: Two convex objects do not overlap iff there exists a line onto which their
// 1d projections do not overlap.
// In three dimensions, if such a line exists, then the normal line to one of the
// faces of one of the shapes will be such a line.

// Check whether two polyhedra overlap
// Note: this function (intentionally) does not make use of neighbor tables
// It is slow, and should only be used for debugging purposes
bool overlap(const polyhedron &a, const polyhedron &b, const double periodic[3], double dr=0);

// Check whether polyhedron overlaps with any of its neighbors in p.
// If count is true, it will return the total number of overlaps, otherwise
// it returns 1 if there is at least one overlap, 0 if there are none.
// If dr is nonzero, then each polyhedron is treated as having a radius R + dr
int overlaps_with_any(const polyhedron &a, const polyhedron *p, const double periodic[3],
                      bool count=false, double dr=0);

// Return true if p doesn't intersect walls
bool in_cell(const polyhedron &p, const double walls[3], bool real_walls, double dr=0);

// Move and rotate the polyhedron by a random amount, in a gaussian distribution with
// respective standard deviations dist and angwidth
polyhedron random_move(const polyhedron &original, double dist, double angwidth, const double len[3]);

// Attempt to move polyhedron of id in p, while paying attention to
// collisions and walls
// Returns a counter that indicates:
//   if the move was successful
//   if the neighbor table was updated, and
//   if, after updating the neighbor table, neighbors were informed
counter move_one_polyhedron(int id, polyhedron *p, int N, const double periodic[3],
                         const double walls[3], bool real_walls, double neighborR,
                        double dist, double angwidth, int max_neighbors, double dr);
