#include "vector3d.h"
#pragma once

struct ball {
  vector3d pos;
  double R;
  int *neighbors;
  int num_neighbors;
  vector3d neighbor_center;

  ball();
  ball(const ball &p);

  ball operator=(const ball &p);
};

// Struct to store all information about an appempt to move one ball
struct move_info {
  long total_old;
  long total;
  long working_old;
  long working;
  int updates;
  int informs;
  int old_count;
  int new_count;
  move_info();
};

// This should store all information needed to run a simulation.  Thus
// we can just pass this struct around to functions that run the
// simulation.  We do not maintain here any of the histograms except
// the energy_histogram, since only that histogram is needed to
// actually run the simulation.  Collecting "real" data is another
// matter.
struct sw_simulation {
  long iteration; // the current iteration number

  /* The following describe the current state of the system. */

  ball *balls;
  int interactions; // i.e. the current energy

  /* The following are constant parameters that describe the physical
     system, but do not change as we simulate. */

  int N; // number of balls
  double len[3]; // the size of the cell
  int walls; // should this be an enum?
  double interaction_distance; // the distance over which balls interact

  /* The following are constant parameters that describe the details
     of how we run the computation, and do not change as we
     simulate. */

  // max_neighbors is the upper limit to the maximum number of
  // neighbors a ball could have.  It is needed as an imput to
  // move_a_ball.
  int max_neighbors;
  double neighbor_R; // radius of our neighbor sphere
  double translation_distance; // scale for how far to move balls
  double dr; // small change in radius used to compute pressure
  int energy_levels; // total number of energy levels

  /* The following accumulate results of the simulation.  Although
     ln_energy_weights is a constant except during initialization. */

  int state_of_max_entropy, max_observed_interactions;
  move_info moves;
  long *energy_histogram;
  double *ln_energy_weights;

  /* The following keep track of how many times we have walked
     between the a given energy and the state of max entropy */

  bool *seeking_energy;
  long *round_trips;
  long actual_round_trips() const { // return the number of round trips to minimum energy
    for (int i=energy_levels-1;i>=state_of_max_entropy;i--) if (round_trips[i]) return round_trips[i];
    return 0;
  };
  int max_interactions() const { // return the maximum observed number of interactions
    for (int i=energy_levels-1;i>=0;i--) if (energy_histogram[i]) return i;
    return 0;
  };

  /* The following deal with the "optimized ensemble" approach and
     keep track of walkers. */
  long *walkers_up, *walkers_total;

  // move just one ball, and update the energy, and update the
  // energy_histogram.
  void move_a_ball();

  // update round trip and walker info
  void update_state_search_info();

  // print the timings (if the time is right)
  void print_timings(int timings_period) const;

  // iterate long enough to find the max entropy state and initialize
  // the translation distance. return most probable energy
  int initialize_max_entropy_and_translation_distance(double acceptance_goal = 0.4);

  // iterate enough times for the energy to change n times.  Return
  // the number of "up" moves.
  int simulate_energy_changes(int num_moves);

  // initialize the weight array using the Gaussian approximation.
  // Returns the width of the gaussian used.
  double initialize_gaussian(double scale = 100.0);

  // initialize the weight array using the specified temperature.
  void initialize_canonical(double kT);

  // improve the weight array using the Wang-Landau method.
  void initialize_wang_landau(double wl_factor, double wl_threshold, double wl_cutoff);
};

// Modulates v to within the periodic boundaries of the cell
vector3d sw_fix_periodic(vector3d v, const double len[3]);

// Return the vector pointing from a to b, accounting for periodic boundaries
vector3d periodic_diff(const vector3d &a, const vector3d  &b,
                          const double len[3], int walls);

// Create and initialize the neighbor tables for all balls (p).
// Returns the maximum number of neighbors that any ball has,
// or -1 if that number is larger than max_neighbors.
int initialize_neighbor_tables(ball *p, int N, double neighborR,
                               int max_neighbors, const double len[3],
                               int walls);

// Find's the neighbors of a by comparing a's position to the center of
// everyone else's neighborsphere, where id is the index of a in p.
void update_neighbors(ball &a, int id, const ball *p, int N,
                      double neighborR, const double len[3], int walls);

// Add ball new_n to the neighbor table of ball id
void add_neighbor(int new_n, ball *p, int id);

// Remove ball old_n from the neighbor table of ball id
void remove_neighbor(int old_n, ball *p, int id);

// Removes p from the neighbor table of anyone neighboring old_p.
// Adds p to the neighbor table of anyone neighboring new_p.
void inform_neighbors(const ball &new_p, const ball &old_p, ball *p);

// Check whether two balls overlap
bool overlap(const ball &a, const ball &b, const double len[3],
             int walls, double dr = 0);

// Check whether ball a overlaps with any of its neighbors in p.
// If count is true, it will return the total number of overlaps, otherwise constant
// it returns 1 if there is at least one overlap, 0 if there are none.
// If dr is nonzero, then each ball is treated as having a radius R + dr
int overlaps_with_any(const ball &a, const ball *p, const double len[3],
                      int walls, double dr=0);

// Return true if p doesn't intersect walls
bool in_cell(const ball &p, const double len[3], const int walls,
             double dr = 0);

// Move the ball by a random amount, in a gaussian distribution with
// respective standard deviations dist and angwidth
ball random_move(const ball &original, double size, const double len[3]);

// Attempt to move ball of id in p, while paying attention to collisions and walls
// Returns the sum of:
// 1 if the move was successful
// 2 if the neighbor table was updated
// 4 if, after updating the neighbor table, neighbors were informed
void move_one_ball(int id, ball *p, int N, double len[3], int walls,
                   double neighborR, double translation_distance,
                   double interaction_distance, int max_neighbors, double dr,
                   move_info *move, int interactions, double *ln_energy_weights);

// Count the number of interactions a given ball has
int count_interactions(int id, ball *p, double interaction_distance,
                       double len[3], int walls);

// Count the interactions of all the balls
int count_all_interactions(ball *balls, int N, double interaction_distance,
                           double len[3], int walls);

// Find index of max entropy point
int max_entropy_index(long *energy_histogram, double *ln_energy_weights,
                      int energy_levels);

// Flatten weights beyond max entropy point and reset energy histogram
void flush_arrays(long *energy_histogram, double *ln_energy_weights,
                  int energy_levels, bool flush_weights);

void flat_hist(long *energy_histogram, double *ln_energy_weights,
               int energy_levels);

void walker_hist(long *energy_histogram, double *ln_energy_weights,
                 int energy_levels, long *walkers_up, long *walkers_total,
                 move_info *moves);

// Variation in histogram values
double count_variation(long *energy_histogram, double *ln_energy_weights,
                       int energy_levels);

// Consider an fcc lattice broken into cubic cells with a ball at the center of each cell
//   and a ball on the center of each edge
// Choosing one cell as the "center", index each cell by n, m, and l,
//   along the x, y, and z axis respectively
// For example, a cell three cells to the left, one cell forward, and five cells down
//    from the "center" cell would be indexed n = 3, m = 1, l = -5
// Denote the sides of a given cell by values of x, y, and z which may be -1, 0, or 1
// For example, the "forward right" side of a cell has x = 1, y = 1, z = 0,
//    while the "top left" side would be x = -1, y = 0, z = 1
// This function finds the position of the ball located at n,m,l,x,y,z,
//    assuming cells have a side length of a
vector3d pos_at(int n, int m, int l, double x, double y, double z, double a);

// This function finds the maximum number of balls within a given distance
//   distance should be normalized to (divided by) ball radius
int max_balls_within(double radius);

// Upper limit on the maximum number of interacions N balls with well width ww could have
// Makes a spherical droplet with N balls, and counts the interactions in that droplet
// fixme: currently wrong because it doesn't consider the fact that opposite ends of
//    the droplet interact in a periodic cell
int maximum_interactions(int N, double interaction_distance, double neighbor_R,
                         int max_neighbors, double len[3]);
