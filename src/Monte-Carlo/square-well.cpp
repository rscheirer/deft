#include <stdlib.h>
#include <float.h>
#include "Monte-Carlo/square-well.h"
#include "handymath.h"

ball::ball(){
  pos = vector3d();
  R = 1;
  neighbors = 0;
  num_neighbors = 0;
  neighbor_center = vector3d();
}

ball::ball(const ball &p){
  pos = p.pos;
  R = p.R;
  neighbors = p.neighbors;
  num_neighbors = p.num_neighbors;
  neighbor_center = p.neighbor_center;
}

ball ball::operator=(const ball &p){
  pos = p.pos;
  R = p.R;
  neighbors = p.neighbors;
  num_neighbors = p.num_neighbors;
  neighbor_center = p.neighbor_center;
  return *this;
}

move_info::move_info(){
  total_old = 0;
  total = 0;
  working = 0;
  working_old = 0;
  updates = 0;
  informs = 0;
}

vector3d sw_fix_periodic(vector3d v, const double len[3]){
  // for (int i = 0; i < 3; i++){
  //   while (v[i] > len[i])
  //     v[i] -= len[i];
  //   while (v[i] < 0.0)
  //     v[i] += len[i];
  // }
  if (v.x > len[0]) v.x -= len[0];
  else if (v.x < 0) v.x += len[0];
  if (v.y > len[1]) v.y -= len[1];
  else if (v.y < 0) v.y += len[1];
  if (v.z > len[2]) v.z -= len[2];
  else if (v.z < 0) v.z += len[2];
  return v;
}

vector3d periodic_diff(const vector3d &a, const vector3d  &b, const double len[3],
                       const int walls){
  vector3d v = b - a;
  // for (int i = walls; i < 3; i++){
  //   if (len[i] > 0){
  //     while (v[i] > len[i]/2.0)
  //       v[i] -= len[i];
  //     while (v[i] < -len[i]/2.0)
  //       v[i] += len[i];
  //   }
  // }
  if (2 >= walls) {
    if (v.z > 0.5*len[2]) v.z -= len[2];
    else if (v.z < -0.5*len[2]) v.z += len[2];
    if (1 >= walls) {
      if (v.y > 0.5*len[1]) v.y -= len[1];
      else if (v.y < -0.5*len[1]) v.y += len[1];
      if (0 >= walls) {
        if (v.x > 0.5*len[0]) v.x -= len[0];
        else if (v.x < -0.5*len[0]) v.x += len[0];
      }
    }
  }
  return v;
}

int initialize_neighbor_tables(ball *p, int N, double neighbor_R, int max_neighbors,
                               const double len[3], int walls){
  int most_neighbors = 0;
  for (int i = 0; i < N; i++){
    p[i].neighbor_center = p[i].pos;
  }
  for(int i = 0; i < N; i++){
    p[i].neighbors = new int[max_neighbors];
    p[i].num_neighbors = 0;
    for (int j = 0; j < N; j++){
      const bool is_neighbor = (i != j) &&
        (periodic_diff(p[i].pos, p[j].pos, len, walls).normsquared() <
         sqr(p[i].R + p[j].R + neighbor_R));
      if (is_neighbor){
        const int index = p[i].num_neighbors;
        p[i].num_neighbors++;
        if (p[i].num_neighbors > max_neighbors) {
          printf("Found too many neighbors: %d > %d\n", p[i].num_neighbors, max_neighbors);
          return -1;
        }
        p[i].neighbors[index] = j;
      }
    }
    most_neighbors = max(most_neighbors, p[i].num_neighbors);
  }
  return most_neighbors;
}

void update_neighbors(ball &a, int n, const ball *bs, int N,
                      double neighbor_R, const double len[3], int walls, int max_neighbors){
  a.num_neighbors = 0;
  for (int i = 0; i < N; i++){
    if ((i != n) &&
        (periodic_diff(a.pos, bs[i].neighbor_center, len, walls).normsquared()
         < sqr(a.R + bs[i].R + neighbor_R))){
      a.neighbors[a.num_neighbors] = i;
      a.num_neighbors++;
      assert(a.num_neighbors < max_neighbors);
    }
  }
}

inline void add_neighbor(int new_n, ball *p, int id, int max_neighbors){
  int i = p[id].num_neighbors;
  while (i > 0 && p[id].neighbors[i-1] > new_n){
    p[id].neighbors[i] = p[id].neighbors[i-1];
    i --;
  }
  p[id].neighbors[i] = new_n;
  p[id].num_neighbors ++;
  assert(p[id].num_neighbors < max_neighbors);
}

inline void remove_neighbor(int old_n, ball *p, int id){
  int i = p[id].num_neighbors - 1;
  int temp = p[id].neighbors[i];
  while (temp != old_n){
    i --;
    const int temp2 = temp;
    temp = p[id].neighbors[i];
    p[id].neighbors[i] = temp2;
  }
  p[id].num_neighbors --;
}

void inform_neighbors(const ball &new_p, const ball &old_p, ball *p, int n, int max_neighbors){
  int new_index = 0, old_index = 0;
  while (true){
    if (new_index == new_p.num_neighbors){
      for(int i = old_index; i < old_p.num_neighbors; i++)
        remove_neighbor(n, p, old_p.neighbors[i]);
      return;
    }
    if (old_index == old_p.num_neighbors){
      for(int i = new_index; i < new_p.num_neighbors; i++) {
        add_neighbor(n, p, new_p.neighbors[i], max_neighbors);
      }
      return;
    }
    if (new_p.neighbors[new_index] < old_p.neighbors[old_index]){
      add_neighbor(n, p, new_p.neighbors[new_index], max_neighbors);
      new_index ++;
    } else if (old_p.neighbors[old_index] < new_p.neighbors[new_index]){
      remove_neighbor(n, p, old_p.neighbors[old_index]);
      old_index ++;
    } else {
      new_index ++;
      old_index ++;
    }
  }
}

bool overlap(const ball &a, const ball &b, const double len[3], int walls){
  const vector3d ab = periodic_diff(a.pos, b.pos, len, walls);
  return (ab.normsquared() < sqr(a.R + b.R));
}

int overlaps_with_any(const ball &a, const ball *p, const double len[3], int walls){
  for (int i = 0; i < a.num_neighbors; i++){
    if (overlap(a,p[a.neighbors[i]],len,walls))
      return true;
  }
  return false;
}

bool in_cell(const ball &p, const double len[3], const int walls){
  for (int i = 0; i < walls; i++){
    if (p.pos[i]-p.R < 0.0 || p.pos[i]+p.R > len[i])
      return false;
  }
  return true;
}

ball random_move(const ball &p, double move_scale, const double len[3]){
  ball temp = p;
  temp.pos = sw_fix_periodic(temp.pos + vector3d::ran(move_scale), len);
  return temp;
}

int count_interactions(int id, ball *p, double interaction_distance,
                       double len[3], int walls, int sticky_wall){
  int interactions = 0;
  for(int i = 0; i < p[id].num_neighbors; i++){
    if(periodic_diff(p[id].pos, p[p[id].neighbors[i]].pos,
                     len, walls).normsquared()
       <= uipow(interaction_distance,2))
      interactions++;
  }
  // if sticky_wall is true, then there is an attractive slab right
  // near the -z wall.
  if (sticky_wall && p[id].pos.z < -0.5*len[2] + p[id].R) {
    interactions++;
  }
  return interactions;
}

int count_all_interactions(ball *balls, int N, double interaction_distance,
                           double len[3], int walls) {
  // Count initial number of interactions
  // Sum over i < k for all |ball[i].pos - ball[k].pos| < interaction_distance
  int interactions = 0;
  for(int i = 0; i < N; i++) {
    for(int j = 0; j < balls[i].num_neighbors; j++) {
      if(i < balls[i].neighbors[j]
         && periodic_diff(balls[i].pos,
                          balls[balls[i].neighbors[j]].pos,
                          len, walls).norm()
         <= interaction_distance)
        interactions++;
    }
  }
  return interactions;
}

int new_max_entropy_state(long *energy_histogram, double *ln_energy_weights,
                      int energy_levels){
  int max_entropy_state = 0;
  for (int i = 0; i < energy_levels; i++) {
    if (log(energy_histogram[i]) - ln_energy_weights[i]
        > (log(energy_histogram[max_entropy_state])
           - ln_energy_weights[max_entropy_state])) {
      max_entropy_state = i;
    }
  }
  return max_entropy_state;
}

vector3d fcc_pos(int n, int m, int l, double x, double y, double z, double a){
  return a*vector3d(n+x/2,m+y/2,l+z/2);
}

int max_balls_within(double distance){ // distances are in units of ball radius
  distance += 1e-10; // add a tiny, but necessary margin of error
  double a = 2*sqrt(2); // fcc lattice constant
  int c = int(ceil(distance/a)); // number of cubic fcc cells to go out from center
  int num = -1; // number of balls within a given radius; don't count the center ball

  int xs[] = {0,1,1,0};
  int ys[] = {0,1,0,1};
  int zs[] = {0,0,1,1};
  for(int n  = -c; n <= c; n++){
    for(int m = -c; m <= c; m++){
      for(int l = -c; l <= c; l++){
        for(int k = 0; k < 4; k++)
          num += (fcc_pos(n,m,l,xs[k],ys[k],zs[k],a).norm() <= distance);
      }
    }
  }
  return num;
}

// sw_simulation methods

void sw_simulation::reset_histograms(){

  moves.total = 0;
  moves.working = 0;

  for(int i = 0; i < energy_levels; i++){
    energy_histogram[i] = 0;
    optimistic_samples[i] = 0;
    pessimistic_samples[i] = 0;
    pessimistic_observation[i] = false;
    walkers_up[i] = 0;
  }
}

void sw_simulation::move_a_ball(bool use_transition_matrix) {
  int id = moves.total % N;
  moves.total++;
  const int old_interaction_count =
    count_interactions(id, balls, interaction_distance, len, walls, sticky_wall);
  ball temp = random_move(balls[id], translation_scale, len);
  // If we're out of the cell or we overlap, this is a bad move!
  if (!in_cell(temp, len, walls) || overlaps_with_any(temp, balls, len, walls)){
    transitions(energy, 0) += 1; // update the transition histogram
    end_move_updates();
    return;
  }
  const bool get_new_neighbors =
    (periodic_diff(temp.pos, temp.neighbor_center, len, walls).normsquared()
     > sqr(neighbor_R/2.0));
  if (get_new_neighbors){
    // If we've moved too far, then the overlap test may have given a false
    // negative. So we'll find our new neighbors, and check against them.
    // If we still don't overlap, then we'll have to update the tables
    // of our neighbors that have changed.
    temp.neighbors = new int[max_neighbors];
    update_neighbors(temp, id, balls, N, neighbor_R, len, walls, max_neighbors);
    moves.updates++;
    // However, for this check (and this check only), we don't need to
    // look at all of our neighbors, only our new ones.
    // fixme: do this!
    //int *new_neighbors = new int[max_neighbors];

    if (overlaps_with_any(temp, balls, len, walls)) {
      // turns out we overlap after all.  :(
      delete[] temp.neighbors;
      transitions(energy, 0) += 1; // update the transition histogram
      end_move_updates();
      return;
    }
  }
  // Now that we know that we are keeping the new move (unless the
  // weights say otherwise), and after we have updated the neighbor
  // tables if needed, we can compute the new interaction count.
  ball pid = balls[id]; // save a copy
  balls[id] = temp; // temporarily update the position
  const int new_interaction_count =
    count_interactions(id, balls, interaction_distance, len, walls, sticky_wall);
  balls[id] = pid;
  // Now we can check whether we actually want to do this move based on the
  // new energy.
  const int energy_change = new_interaction_count - old_interaction_count;
  transitions(energy, energy_change) += 1; // update the transition histogram
  double Pmove = 1;
  if (use_transition_matrix) {
    if (energy_change < 0) { // "Interactions" are decreasing, so energy is increasing.
      const double betamax = 1.0/min_T;
      const double Pmin = exp(energy_change*betamax);
      /* I note that Swendson 1999 uses essentially this method
           *after* two stages of initialization. */
      long tup_norm = 0;
      long tdown_norm = 0;
      for (int de=-biggest_energy_transition; de<=biggest_energy_transition; de++) {
        tup_norm += transitions(energy, de);
        tdown_norm += transitions(energy+energy_change, de);
      }
      /* The "conservativeness" parameter below allows us to adjust
         how soon the tmmc method becomes confident in its estimation
         of the transition matrix.  We add "conservativeness" to the
         number of times that we think we have seen the system go
         either up or down.  This means when we have poor statistics,
         we default towards allowing transitions, which can help avoid
         getting "stuck" at low energies, due to an incorrect first
         guess as to how probable the downward transition is. */
      const double conservativeness = 16;
      const double tup = (transitions(energy, energy_change)+conservativeness)
                         / double(tup_norm+conservativeness);
      const double tdown = (transitions(energy+energy_change,-energy_change)+conservativeness)
                           / double(tdown_norm+conservativeness);
      if (tdown < tup) {
        Pmove = tdown/tup;
        if (!(Pmove > Pmin)) Pmove = Pmin;
      }
    }
    // printf("Pmove %g  with E %d and dE %d   from (%ld, %ld)\n",
    //        Pmove, energy, energy_change,
    //        transitions(energy+energy_change, -energy_change),
    //        transitions(energy, energy_change));
  } else {
    const double lnPmove =
      ln_energy_weights[energy + energy_change] - ln_energy_weights[energy];
    if (lnPmove < 0) Pmove = exp(lnPmove);
  }
  if (Pmove < 1) {
    if (random::ran() > Pmove) {
      // We want to reject this move because it is too improbable
      // based on our weights.
      if (get_new_neighbors) delete[] temp.neighbors;
      end_move_updates();
      return;
    }
  }
  if (get_new_neighbors) {
    // Okay, we've checked twice, just like Santa Clause, so we're definitely
    // keeping this move and need to tell our neighbors where we are now.
    temp.neighbor_center = temp.pos;
    inform_neighbors(temp, balls[id], balls, id, max_neighbors);
    moves.informs++;
    delete[] balls[id].neighbors;
  }
  balls[id] = temp; // Yay, we have a successful move!
  moves.working++;
  energy += energy_change;

  if(energy_change != 0) energy_change_updates(energy_change);

  end_move_updates();
}

void sw_simulation::end_move_updates(){
   // update iteration counter, energy histogram, and walker counters
  if(moves.total % N == 0) iteration++;
  energy_histogram[energy]++;
  if(pessimistic_observation[min_important_energy]) walkers_up[energy]++;
}

void sw_simulation::energy_change_updates(int energy_change){
  // If we're at or above the state of max entropy, we have not yet observed any energies
  if(energy <= max_entropy_state){
    for(int i = max_entropy_state; i < energy_levels; i++)
      pessimistic_observation[i] = false;
  } else if (!pessimistic_observation[energy]) {
    // If we have not yet observed this energy, now we have!
    pessimistic_observation[energy] = true;
    pessimistic_samples[energy]++;
  }

  if (energy_change > 0) optimistic_samples[energy]++;

  // Update observation of minimum energy
  if(energy > min_energy_state) min_energy_state = energy;
}

int sw_simulation::simulate_energy_changes(int num_moves) {
  int num_moved = 0, num_up = 0;
  while (num_moved < num_moves) {
    int old_energy = energy;
    move_a_ball();
    if (energy != old_energy) {
      num_moved++;
      if (energy > old_energy) num_up++;
    }
  }
  return num_up;
}

void sw_simulation::flush_weight_array(){
  // floor weights above state of max entropy
  for (int i = 0; i < max_entropy_state; i++)
    ln_energy_weights[i] = ln_energy_weights[max_entropy_state];
  // substract off minimum weight from the entire array to reduce numerical error
  double min_weight = ln_energy_weights[0];
  for (int i = 1; i < max_entropy_state; i++)
    min_weight = min(min_weight, ln_energy_weights[i]);
  for (int i = 0; i < energy_levels; i++)
    ln_energy_weights[i] -= min_weight;
  return;
}

double sw_simulation::fractional_sample_error(double T, bool optimistic_sampling){
  double error_times_Z = 0;
  double Z = 0;
  const double *ln_dos = compute_ln_dos(sim_dos_type);
  for(int i = max_entropy_state; i <= min_energy_state; i++){
    const double boltz = exp(ln_dos[i]+(i-min_energy_state)/T);
    Z += boltz;
    // fixme: what should we do if samples=0?
    if(optimistic_sampling)
      error_times_Z += boltz/sqrt(max(optimistic_samples[i],1));
    else
      error_times_Z += boltz/sqrt(max(pessimistic_samples[i],1));
  }
  delete[] ln_dos;
  return error_times_Z/Z;
}

double* sw_simulation::compute_ln_dos(dos_types dos_type){

  double *ln_dos = new double[energy_levels]();

  if(dos_type == histogram_dos){
    for(int i = max_entropy_state; i < energy_levels; i++){
      if(energy_histogram[i] != 0){
        ln_dos[i] = log(energy_histogram[i]) - ln_energy_weights[i];
      }
      else ln_dos[i] = -DBL_MAX;
    }
  }

  else if(dos_type == transition_dos){

    const int energies_observed = min_energy_state+1;
    double *TD_over_D = new double[energies_observed];

    // initialize a flat density of states with unit norm
    for (int i = 0; i < energies_observed; i++) {
      ln_dos[i] = 0;
      TD_over_D[i] = 1.0/energies_observed;
    }

    // now find the eigenvector of our transition matrix via the power iteration method
    bool done = false;
    int iters = 0;
    while(!done){
      iters++;
      // set D_n = T*D_{n-1}
      for (int i = 0; i < energies_observed; i++) {
        if (TD_over_D[i] > 0) ln_dos[i] += log(TD_over_D[i]);
        TD_over_D[i] = 0;
      }

      // compute T*D_n
      for (int i = 0; i < energies_observed; i++) {
        double norm = 0;
        for (int de = -biggest_energy_transition; de <= biggest_energy_transition; de++)
          norm += transitions(i, de);
        if(norm){
          for (int de = max(-i, -biggest_energy_transition);
               de <= min(energies_observed-i-1, biggest_energy_transition); de++) {
            TD_over_D[i+de] += exp(ln_dos[i] - ln_dos[i+de])*transitions(i,de)/norm;
          }
        }
      }

      /* The following deals with the fact that we may not have a
         perfectly normalized transition matrix.  I'm not sure why this
         happens, but the net result is that at equilibrium all of the D
         values drop by the same factor.  I deal with this by finding
         the average ratio (which ought to be around 1) and then
         dividing all TD_over_D factors by this ratio, which corresponds
         to a normalization shift.  This enables the algorithm to work
         with high precision, where an unnormalized T (for whatever
         cause) could previously cause the algorithm to never converge.. */
      double norm = 0, count = 0;
      for (int i = 0; i < energies_observed; i++) {
        if (energy_histogram[i]) {
          norm += TD_over_D[i];
          count += 1;
        }
      }
      norm /= count;
      for (int i = 0; i < energies_observed; i++) {
        TD_over_D[i] /= norm;
      }

      // check whether T*D_n (i.e. D_{n+1}) is close enough to D_n for us to quit
      done = true;
      for (int i = max_entropy_state+1; i < energies_observed; i++){
        if (energy_histogram[i]) {
          const double precision = fabs(TD_over_D[i] - 1);
          if (precision > fractional_dos_precision){
            done = false;
            if(iters % 1000000 == 0){
              printf("After %i iterations, failed at energy %i with value %.16g and "
                     "newvalue %.16g and ratio %g and norm %g.\n",
                     iters, i, ln_dos[i], ln_dos[i] + log(TD_over_D[i]), TD_over_D[i], norm);
              fflush(stdout);
            }
            break;
          }
        }
      }
    }
  }
  else {
    printf("We don't know what dos type we have!\n");
    exit(1);
  }
  return ln_dos;
}

double *sw_simulation::compute_walker_density_using_transitions(double *sample_rate) {
  double *ln_downwalkers = new double[energy_levels]();
  double *ln_dos = compute_ln_dos(transition_dos);

  const int energies_observed = min_energy_state+1;
  double *TD_over_D = new double[energies_observed];
  double norm = 1, oldnorm = 0;

  // initialize a flat density of states with unit norm
  for (int i = 0; i < energies_observed; i++) {
    ln_downwalkers[i] = 0;
    TD_over_D[i] = 1.0/energies_observed;
  }

  // now find the eigenvector of our transition matrix via the power iteration method
  bool done = false;
  int iters = 0;
  while(!done){
    iters++;
    // set D_n = T*D_{n-1}
    double max_downwalker = -1e300;
    for (int i = 0; i < energies_observed; i++) {
      if (TD_over_D[i] > 0) ln_downwalkers[i] += log(TD_over_D[i]);
      TD_over_D[i] = 0;
      if (ln_downwalkers[i] > max_downwalker) max_downwalker = ln_downwalkers[i];
    }
    // Set the maximum value of downwalkers to 1 (set max
    // ln_downwalkers to 0).  This is intended to reduce problems due
    // to roundoff error which could accumulate if the mean value of
    // the log drifts far from 0.
    for (int i = 0; i < energies_observed; i++) {
      ln_downwalkers[i] -= max_downwalker;
    }

    for (int i = 0; i < max_entropy_state; i++) {
      // For energies above the max_entropy_state, assume a flat set
      // of weights, and compute the number of "downwalkers" relative
      // to the number at the max_entropy_state using the density of
      // states.
      ln_downwalkers[i] = ln_downwalkers[max_entropy_state] + ln_dos[i] - ln_dos[max_entropy_state];
    }
    // compute T*D_n with energies less than min_important_energy set
    // to zero, since we're only looking at downwalkers.
    for (int i = max_entropy_state; i < min_important_energy; i++) {
      double norm = 0;
      for (int de = -biggest_energy_transition; de <= biggest_energy_transition; de++) {
        if (i+de < energy_levels && i+de >= 0) {
          if (de < 0 && ln_downwalkers[i] > ln_downwalkers[i+de]) {
            norm += transitions(i, de)*exp(ln_energy_weights[i] - ln_energy_weights[i+de]);
          } else {
            norm += transitions(i, de);
          }
        }
      }
      if (norm) {
        for (int de = max(-i, -biggest_energy_transition);
             de <= min(energies_observed-i-1, biggest_energy_transition); de++) {
          if (transitions(i,de)) {
            double change;
            if (de < 0 && ln_downwalkers[i] > ln_downwalkers[i+de]) {
              change = exp(ln_downwalkers[i] - ln_downwalkers[i+de]
                           + ln_energy_weights[i] - ln_energy_weights[i+de])
                *transitions(i,de)/norm;
            } else {
              change = exp(ln_downwalkers[i] - ln_downwalkers[i+de])
                *transitions(i,de)/norm;
            }
            if (change != change) {
              printf("ln_downwalkers[i] == %g\n", ln_downwalkers[i]);
              printf("ln_downwalkers[i+de] == %g\n", ln_downwalkers[i+de]);
              printf("ln_energy_weights[i] == %g\n", ln_energy_weights[i]);
              printf("ln_energy_weights[i+de] == %g\n", ln_energy_weights[i+de]);
              printf("norm == %g\n", norm);
              printf("transitions(i,de) == %ld\n", transitions(i,de));
              printf("change is nan at i=%d and de=%d\n", i, de);
              exit(1);
            }
            TD_over_D[i+de] += change;
          }
        }
      }
    }

    /* The following deals with the fact that we will not have a
       normalized matrix since we eliminate transitions below the
       min_important_energy. */
    norm = 0;
    double count = 0;
    for (int i = 0; i < energies_observed; i++) {
      if (energy_histogram[i]) {
        norm += TD_over_D[i];
        count += 1;
      }
    }
    norm /= count;
    if (norm != norm) {
      for (int i=0; i<energies_observed;i++) {
        printf("TD_over_D[%5d] = %10g ln_downwalkers[%5d] = %10g\n",
               i, TD_over_D[i], i, ln_downwalkers[i]);
      }
      printf("nan in norm with count %g and norm %g\n", count, norm);
      exit(1);
    }
    for (int i = 0; i < energies_observed; i++) {
      TD_over_D[i] /= norm;
    }

    // check whether T*D_n (i.e. D_{n+1}) is close enough to D_n for us to quit
    done = true;
    // If the norm is exactly the same as it was last time, then we
    // have presumably reached equilibrium.
    if (oldnorm != norm) {
      for (int i = max_entropy_state+1; i < energies_observed; i++){
        if (energy_histogram[i]) {
          const double precision = fabs(TD_over_D[i] - 1);
          if (precision > fractional_dos_precision){
            done = false;
            if(iters % 1000000 == 0){
              printf("After %i iterations, failed at energy %i with value %.16g"
                     " and newvalue %.16g and ratio %g and norm %g.\n",
                     iters, i, ln_downwalkers[i],
                     ln_downwalkers[i] + log(TD_over_D[i]),
                     TD_over_D[i], norm);
              fflush(stdout);
            }
            break;
          }
        }
      }
    }
    oldnorm = norm;
    if (iters > 1000000) {
      printf("Eventually giving up at %d iters to avoid infinite loop\n", iters);
      done = true;
    }
  }
  if (norm == 1) {
    printf("Found sample rate of infinity\n");
    if (sample_rate) *sample_rate = 1e100;
  } else {
    printf("Found sample rate of %g from norm %.16g\n", 1.0/(1 - norm), norm);
    if (sample_rate) *sample_rate = 1.0/(1 - norm);
  }
  delete[] ln_dos;
  return ln_downwalkers;
}

int sw_simulation::set_min_important_energy(){

  const double *ln_dos = compute_ln_dos(sim_dos_type);

  /* Look for a the highest significant energy at which the slope in ln_dos is 1/min_T */
  for(int i = max_entropy_state+1; i < min_energy_state; i++){
    if(ln_dos[i] - ln_dos[i+1] > 1.0/min_T){
      // ratcheting of min_important_energy is done here
      if(i >= min_important_energy) min_important_energy = i;
      delete[] ln_dos;
      return min_important_energy;
    }
  }
  /* If we never found a slope of 1/min_T, just use the lowest energy we've seen */
  if(min_energy_state > min_important_energy) min_important_energy = min_energy_state;

  delete[] ln_dos;
  return min_important_energy;
}

bool sw_simulation::finished_initializing(bool be_verbose) {

  if(end_condition == optimistic_sample_error
     || end_condition == pessimistic_sample_error){

    const bool optimistic_sampling = end_condition == optimistic_sample_error;
    return fractional_sample_error(min_T,optimistic_sampling) <= sample_error;

  } else if(end_condition == optimistic_min_samples
            || end_condition == pessimistic_min_samples) {
    set_min_important_energy();

    if(end_condition == optimistic_min_samples){
      if (be_verbose) {
        long num_to_go = 0, energies_unconverged = 0;
        int lowest_problem_energy = 0, highest_problem_energy = min_energy_state;
        for (int i = min_important_energy; i > max_entropy_state; i--){
          if (optimistic_samples[i] < min_samples) {
            num_to_go += min_samples - optimistic_samples[i];
            energies_unconverged += 1;
            if (i > lowest_problem_energy) lowest_problem_energy = i;
            if (i < highest_problem_energy) highest_problem_energy = i;
          }
        }
        printf("[%9ld] Have %ld samples to go (at %ld energies; %ld ps at %d)\n",
               iteration, num_to_go, energies_unconverged,
               pessimistic_samples[min_important_energy], min_important_energy);
        printf("       <%d - %d> has samples <%ld(%ld) - %ld(%ld)>/%d (current energy %d)\n",
               lowest_problem_energy, highest_problem_energy,
               optimistic_samples[lowest_problem_energy],
               pessimistic_samples[lowest_problem_energy],
               optimistic_samples[highest_problem_energy],
               pessimistic_samples[highest_problem_energy], min_samples, energy);
        fflush(stdout);
      }
      for(int i = min_important_energy; i > max_entropy_state; i--){
        if (optimistic_samples[i] < min_samples) {
          return false;
        }
      }
      return true;
    } else { // if end_condition == pessimistic_min_samples
      if (be_verbose) {
        long energies_unconverged = 0;
        int highest_problem_energy = min_energy_state;
        for (int i = min_important_energy; i > max_entropy_state; i--){
          if (pessimistic_samples[i] < min_samples) {
            energies_unconverged += 1;
            if (i < highest_problem_energy) highest_problem_energy = i;
          }
        }
        printf("[%9ld] Have %ld energies to go\n", iteration, energies_unconverged);
        printf("       <%d - %d> has samples <%ld(%ld) - %ld(%ld)>/%d (current energy %d)\n",
               min_important_energy, highest_problem_energy,
               pessimistic_samples[min_important_energy],
               optimistic_samples[min_important_energy],
               pessimistic_samples[highest_problem_energy],
               optimistic_samples[highest_problem_energy], min_samples, energy);
        fflush(stdout);
      }
      return pessimistic_samples[min_important_energy] >= min_samples;
    }
  }

  else if(end_condition == flat_histogram){
    int hist_min = int(1e20);
    int hist_total = 0;
    int most_weighted_energy = 0;
    for(int i = max_entropy_state; i <= min_energy_state; i++){
      hist_total += energy_histogram[i];
      if(energy_histogram[i] < hist_min) hist_min = energy_histogram[i];
      if(ln_energy_weights[i] > ln_energy_weights[most_weighted_energy])
        most_weighted_energy = i;
    }
    const double hist_mean = hist_total/(min_energy_state-max_entropy_state);
    /* First, make sure that the histogram is sufficiently flat. In addition,
       make sure we didn't just get stuck at the most heavily weighted energy. */
    return hist_min >= flatness*hist_mean && energy != most_weighted_energy;
  }

  else if(end_condition == init_iter_limit) {
    if (be_verbose) {
      static int last_percent = 0;
      int percent = (100*iteration)/init_iters;
      if (percent > last_percent) {
        printf("%2d%% done (%ld/%ld iterations)\n",
               percent, long(iteration), long(init_iters));
        last_percent = percent;
        fflush(stdout);
      }
    }
    return iteration >= init_iters;
  }

  printf("We are asking whether we are finished initializing without "
         "a valid end condition!\n");
  exit(1);
}

bool sw_simulation::reached_iteration_cap(){
  return end_condition == init_iter_limit && iteration >= init_iters;
}

int sw_simulation::initialize_max_entropy(double acceptance_goal) {
  printf("Moving to most probable state.\n");
  int num_moves = 500;
  const double mean_allowance = 1.0;
  const int starting_iterations = iteration;
  int attempts_to_go = 4; // always try this many times, to get translation_scale right.
  double mean = 0, variance = 0;
  int last_energy = 0;
  int counted_in_mean;
  while (abs(last_energy-energy) > max(2,mean_allowance*sqrt(variance)) ||
         attempts_to_go >= 0) {
    attempts_to_go -= 1;
    last_energy = energy;
    simulate_energy_changes(num_moves);
    mean = 0;
    counted_in_mean = 0;
    for (int i = 0; i < energy_levels; i++) {
      counted_in_mean += energy_histogram[i];
      mean += i*energy_histogram[i];
    }
    mean /= counted_in_mean;
    variance = 0;
    for (int i = 0; i < energy_levels; i++) {
      variance += (i-mean)*(i-mean)*energy_histogram[i];
      energy_histogram[i] = 0; // clear it out for the next attempt
    }
    variance /= counted_in_mean;

    num_moves *= 2;
  }
  printf("Took %ld iterations to find most probable state: %d with width %.2g\n",
         iteration - starting_iterations, max_entropy_state, sqrt(variance));
  return int(mean + 0.5);
}

void sw_simulation::initialize_translation_distance(double acceptance_goal) {
  printf("Tuning translation distance.\n");
  const int max_tries = 5;
  const int num_moves = 100*N*N;
  const int starting_iterations = iteration;
  double dscale = 0.1;
  double acceptance_rate = 0;
  for (int num_tries=0;
       fabs(acceptance_rate - acceptance_goal) > 0.05 && num_tries < max_tries;
       num_tries++) {
    // ---------------------------------------------------------------
    // Fine-tune translation scale to reach acceptance goal
    // ---------------------------------------------------------------
    for (int i=0;i<num_moves;i++) move_a_ball();
    acceptance_rate =
      double(moves.working-moves.working_old)/(moves.total-moves.total_old);
    moves.working_old = moves.working;
    moves.total_old = moves.total;
    if (acceptance_rate < acceptance_goal)
      translation_scale /= 1+dscale;
    else
      translation_scale *= 1+dscale;
    // hokey heuristic for tuning dscale
    const double closeness = fabs(acceptance_rate - acceptance_goal)/acceptance_rate;
    if(closeness > 0.5) dscale *= 2;
    else if(closeness < dscale*2 && dscale > 0.01) dscale/=2;
  }
  printf("Took %ld iterations to find acceptance rate of %.2g with translation scale %.2g\n",
         iteration - starting_iterations,
         acceptance_rate, translation_scale);
}

// initialize the weight array using the specified temperature.
void sw_simulation::initialize_canonical(double T, int reference) {
  for(int i=reference+1; i < energy_levels; i++){
    ln_energy_weights[i] = ln_energy_weights[reference] + (i-reference)/T;
  }
}

// initialize the weight array using the Wang-Landau method.
void sw_simulation::initialize_wang_landau(double wl_factor, double wl_fmod,
                                           double wl_threshold, double wl_cutoff) {
  int weight_updates = 0;
  bool done = false;
  if(!manual_min_e) min_important_energy = default_min_e();
  while (!done) {

    if(manual_min_e){
      // If we have a manually set minimum energy, don't allow going lower
      initialize_canonical(-1e-2,min_important_energy);
    } else {
      // Otherwise, just use the minimum energy we've seen as the minimum important energy
      min_important_energy = min_energy_state;
    }

    for (int i=0; i < N*energy_levels && !reached_iteration_cap(); i++) {
      move_a_ball();
      ln_energy_weights[energy] -= wl_factor;
    }

    // compute variation in energy histogram
    int highest_hist_i = 0; // the most commonly visited energy
    int lowest_hist_i = 0; // the least commonly visited energy
    double highest_hist = 0; // highest histogram value
    double lowest_hist = 1e200; // lowest histogram value
    double total_counts = 0; // total counts in energy histogram
    for(int i = max_entropy_state+1; i <= min_important_energy; i++){
      if(energy_histogram[i] > 0){
        total_counts += energy_histogram[i];
        if(energy_histogram[i] > highest_hist){
          highest_hist = energy_histogram[i];
          highest_hist_i = i;
        }
        if(energy_histogram[i] < lowest_hist){
          lowest_hist = energy_histogram[i];
          lowest_hist_i = i;
        }
      }
    }
    double hist_mean = (double)total_counts / (min_important_energy - max_entropy_state);
    const double variation = hist_mean/lowest_hist - 1;

    // print status text for testing purposes
    if(printing_allowed()){
      printf("\nWL weight update: %i\n",weight_updates++);
      printf("  WL factor: %g\n",wl_factor);
      printf("  count variation: %g\n", variation);
      printf("  highest/lowest histogram energies (values): %d (%.2g) / %d (%.2g)\n",
             highest_hist_i, highest_hist, lowest_hist_i, lowest_hist);
      printf("  minimum_energy_state: %d,  max_entropy_state: %d,  energies visited: %d\n",
             min_energy_state, max_entropy_state, min_energy_state-max_entropy_state);
      printf("  current energy: %d\n", energy);
      printf("  hist_mean: %g,  total_counts: %g\n", hist_mean, total_counts);
    }

    // check whether our histogram is flat enough to update wl_factor
    if (variation > 0 && variation < wl_threshold) {
      wl_factor /= wl_fmod;
      flush_weight_array();
      for (int i = 0; i < energy_levels; i++) {
        if (energy_histogram[i] > 0) energy_histogram[i] = 1;
      }

      // repeat until terminal condition is met,
      // and make sure we're not stuck at a newly introduced minimum energy state
      if (wl_factor < wl_cutoff && energy != min_energy_state
          && end_condition != init_iter_limit) {
        printf("Took %ld iterations and %i updates to initialize with Wang-Landau method.\n",
               iteration, weight_updates);
        done = true;
      }
    }
    if(end_condition == init_iter_limit) done = reached_iteration_cap();
  }
  initialize_canonical(min_T,min_important_energy);
}

// initialize the weight array using the optimized ensemble method.
void sw_simulation::initialize_optimized_ensemble(int first_update_iterations,
                                                  int oe_update_factor){
  int weight_updates = 0;
  long update_iters = first_update_iterations;
  double tiny = 1e-3;
  do {
    reset_histograms();

    const long started_at = iteration;
    // simulate for a while
    while (pessimistic_samples[min_important_energy] < 2 && !reached_iteration_cap()) {
      for(long i = 0; i < N*update_iters && !reached_iteration_cap(); i++) move_a_ball();

      if (pessimistic_samples[min_important_energy]) {
        printf("Optimized ensemble sees %ld samples (%.3g iters per sample)\n",
               pessimistic_samples[min_important_energy],
               (iteration-started_at)/double(pessimistic_samples[min_important_energy]));
      } else {
        printf("Optimized ensemble sees %ld low-energy samples\n",
               pessimistic_samples[min_important_energy]);
      }
      update_iters *= oe_update_factor; // simulate for longer next time
    }

    // There's no point doing updating our weights with a partial set
    // of samples, since this could as easily make things worse as
    // better.
    if (reached_iteration_cap()) {
      printf("Optimized ensemble is quitting after %d updates (mie: %d).\n",
             weight_updates, min_important_energy);
      return;
    }

    // Find the minimum energy we've seen in this iteration.
    int min_hist = energy_levels-1;
    for(int i = min_hist; i > max_entropy_state; i--){
      if(energy_histogram[i] > 0){
        min_hist = i;
        break;
      }
    }

    // Update weight array
    for(int i = max_entropy_state; i < min_hist; i++){
      const int top = i-1;
      const int bottom = i+1;
      const int dE = bottom - top;
      double df = double(walkers_up[top]) / energy_histogram[top]
        - (double(walkers_up[bottom]) / energy_histogram[bottom]);
      double walker_density = max(energy_histogram[i],tiny)/moves.total;
      /* Floor df/dE and walker_density at tiny nonzero values.
         If df is not positive or a nan, we forget df/dE and let the walker_density
         term take care of weight corrections */
      if(df <= 0 || df != df) df = dE;
      ln_energy_weights[i] += 0.5*(log(df/dE) - log(walker_density));
    }
    // Set canonical weights at unseen energies
    initialize_canonical(min_T,min_hist);

    flush_weight_array();

    printf("Optimized ensemble update: %i (%ld iters)\n", weight_updates, update_iters);
    for(int e = max_entropy_state; e <= min_energy_state; e++){
      printf("h[%3d] = %10ld,  ps[%3d] = %10ld,  lnw[%3d]: %g\n",
             e, energy_histogram[e], e, pessimistic_samples[e], e, ln_energy_weights[e]);
    }
    fflush(stdout);
    weight_updates++;

  } while(!finished_initializing());
}

void sw_simulation::initialize_simple_flat(int flat_update_factor){
  int weight_updates = 0;
  long num_moves = exp(1/min_T)*N*energy_levels;
  do {
    set_min_important_energy();

    // update weight array
    for (int e = max_entropy_state; e <= min_important_energy; e++) {
      if (energy_histogram[e]) {
        ln_energy_weights[e] -= log(energy_histogram[e]);
      }
    }
    if(min_energy_state > min_important_energy){
      initialize_canonical(min_T,min_important_energy);
    }
    flush_weight_array();
    weight_updates++;

    // Now reset the calculation!
    reset_histograms();

    for (long j = 0; j < num_moves && !reached_iteration_cap(); j++) move_a_ball();

    // There's no point doing updating our weights with a partial set
    // of samples, since this could as easily make things worse as
    // better.
    if (reached_iteration_cap()) {
      printf("Simple flat is quitting after %d updates (mine: %d).\n",
             weight_updates, min_important_energy);
      return;
    }

    if (printing_allowed()) {
      printf("simple flat status update:\n");
      for (int e = max_entropy_state; e <= min_energy_state; e++) {
        printf("h[%3d] = %10ld,  os[%3d] = %10ld,  lnw[%3d]: %g\n",
               e, energy_histogram[e], e, optimistic_samples[e], e, ln_energy_weights[e]);
      }
      printf("min_important_energy: %i\n",min_important_energy);
    }
    num_moves *= flat_update_factor;

  } while (!finished_initializing());

  flush_weight_array();
  set_min_important_energy();
  initialize_canonical(min_T,min_important_energy);
}

/* This method implements the optimized ensemble using the transition
   matrix information.  This should give an identical set of weights
   to the optimized_ensemble approach, provided each approach have
   sufficient statistics. */
void sw_simulation::optimize_weights_using_transitions() {
  // Assume that we already *have* a reasonable set of weights (with
  // which to compute the diffusivity), and that we have already
  // defined the min_important_energy.
  const double *ln_dos = compute_ln_dos(transition_dos);

  for (int attempt=0; attempt<10; attempt++) {

    double *lnw = new double[energy_levels+2*biggest_energy_transition];
    for (int i=0;i<energy_levels;i++) {
      lnw[biggest_energy_transition+i] = ln_energy_weights[i];
    }

    double inv_diffusivity = 1;
    for(int i = max_entropy_state; i <= min_important_energy; i++) {
      double norm = 0, mean_sqr_de = 0, mean_de = 0;
      for (int de=-biggest_energy_transition;de<=biggest_energy_transition;de++) {
        double T = transitions(i, de)*exp(lnw[biggest_energy_transition+i+de]
                                          -lnw[biggest_energy_transition+i]);
        norm += T;
        mean_sqr_de += T*double(de)*de;
        mean_de += T*double(de);
      }
      if (norm) {
        mean_sqr_de /= norm;
        mean_de /= norm;
        //printf("%4d: <de> = %15g   <de^2> = %15g\n", i, mean_de, mean_sqr_de);
        inv_diffusivity = 1.0/fabs(mean_sqr_de - mean_de*mean_de);
      } else {
        printf("Craziness at energy %d!!!\n", i);
      }
      /* This is a different way of computing it than is done by Trebst,
         Huse and Troyer, but uses the formula that they derived at the
         end of Section IIA (and expressed in words).  The main
         difference is that we compute the diffusivity here *directly*
         rather than inferring it from the walker gradient.  */
      ln_energy_weights[i] = -ln_dos[i] + 0.5*log(inv_diffusivity);
    }
    initialize_canonical(min_T,min_important_energy);
    double ln_max = ln_energy_weights[max_entropy_state];
    for (int i=0;i<max_entropy_state;i++) {
      ln_energy_weights[i] = 0;
    }
    for (int i=max_entropy_state;i<energy_levels;i++) {
      ln_energy_weights[i] -= ln_max;
    }
    for (int i=max_entropy_state+1;i<min_important_energy;i++) {
      double old_w = lnw[biggest_energy_transition+i]
        - lnw[biggest_energy_transition+max_entropy_state];
      double new_w = ln_energy_weights[i];
      if (fabs(old_w - new_w) > 1e-4) {
        printf("attempt %d:   lnw[%d] %15g -> %15g\n",
               attempt, i, old_w, new_w);
      }
      fflush(stdout);
    }
    delete[] lnw;
  }
  delete[] ln_dos;
}

// update the weight array using transitions
void sw_simulation::update_weights_using_transitions() {
  const double *ln_dos = compute_ln_dos(transition_dos);
  for(int i = 0; i < energy_levels; i++) {
    ln_energy_weights[i] = -ln_dos[i];
  }
  delete[] ln_dos;
}

// initialization with tmmc
void sw_simulation::initialize_transitions() {
  int check_how_often = exp(1/min_T)*energy_levels; // avoid wasting time if we are done
  do {
    for (int i = 0; i < check_how_often && !reached_iteration_cap(); i++) move_a_ball(true);
    check_how_often += exp(1/min_T)*energy_levels; // try a little harder next time...
  } while(!finished_initializing(printing_allowed()));

  update_weights_using_transitions();
  flush_weight_array();
  set_min_important_energy();
  initialize_canonical(min_T,min_important_energy);
}

// initialize by reading transition matrix from file
void sw_simulation::initialize_transitions_file(char *transitions_input_filename){
  // open the transition matrix data file as read-only
  FILE *transitions_infile = fopen(transitions_input_filename,"r");

  // spit out an error and exist if the data file does not exist
  if(transitions_infile == NULL){
    printf("Cannot find transition matrix input file: %s\n",transitions_input_filename);
    exit(254);
  }

  const int line_len = 1000;
  char line[line_len];
  int min_de;

  // gather and verify metadata
  while(fgets(line,line_len,transitions_infile) != NULL){

    /* check that the metadata in the transition matrix file
       agrees with our simulation parameters */

    // check well width agreement
    if(strstr(line,"# well_width:") != NULL){
      double file_ww;
      if (sscanf(line,"%*s %*s %lf",&file_ww) != 1) {
        printf("Unable to read well-width properly from \"%s\"\n", line);
        exit(1);
      }
      if (file_ww != well_width) {
        printf("The well width in the transition matrix file metadata (%g) disagrees "
               "with that requested for this simulation (%g)!\n", file_ww, well_width);
        exit(232);
      }
    }

    // check filling fraction agreement
    if(strstr(line,"# ff:") != NULL){
      double file_ff;
      if (sscanf(line,"%*s %*s %lf", &file_ff) != 1) {
        printf("Unable to read ff properly from '%s'\n", line);
        exit(1);
      }
      if(file_ff != filling_fraction){
        printf("The filling fraction in the transition matrix file metadata (%g) disagrees "
               "with that requested for this simulation (%g)!\n", file_ff, filling_fraction);
        exit(233);
      }
    }

    // check N agreement
    if(strstr(line,"# N:") != NULL){
      int file_N;
      if (sscanf(line,"%*s %*s %i", &file_N) != 1) {
        printf("Unable to read N properly from '%s'\n", line);
        exit(1);
      }
      if(file_N != N){
        printf("The number of spheres in the transition matrix file metadata (%i) disagrees "
               "with that requested for this simulation (%i)!\n", file_N, N);
        exit(234);
      }
    }

    // check that we have a sufficiently small min_T in the data file
    if(strstr(line,"# min_T:") != NULL){
      double file_min_T;
      if (sscanf(line,"%*s %*s %lf", &file_min_T) != 1) {
        printf("Unable to read min_T properly from '%s'\n", line);
        exit(1);
      }
      if(file_min_T > min_T){
        printf("The minimum temperature in the transition matrix file metadata (%g) is "
               "larger than that requested for this simulation (%g)!\n", file_min_T, min_T);
        exit(234);
      }
    }

    /* find the minimum de in the transition matrix as stored in the file.
       the line we match here should be the last commented line in the data file,
       so we break out of the metadata loop after storing min_de */
    if(strstr(line,"# energy\t") != NULL){
      if (sscanf(line,"%*s %*s %d", &min_de) != 1) {
        printf("Unable to read min_de properly from '%s'\n", line);
        exit(1);
      }
      break;
    }
  }

  // read in transition matrix
  /* when we hit EOF, we won't know until after trying to scan past it,
     so we loop while true and enforce a break condition */
  int e, min_e = 0;
  while(true){
    if (fscanf(transitions_infile,"%i",&e) != 1) {
      break;
    }
    if (!min_e) min_e = e;
    for(int de = min_de; de <= -min_de; de++){
      if (fscanf(transitions_infile,"%li",&transitions(e,de)) != 1) {
        break;
      }
    }
  }
  min_energy_state = e;

  // we are done with the data file
  fclose(transitions_infile);

  // pretend we have seen the energies for which we have transition data
  for(int i = min_e; i <= min_energy_state; i++) energy_histogram[i] = 1;

  // now construct the actual weight array
  /* FIXME: it appears that we are reading in the data file properly,
     but for some reason we are still not getting proper weights */
  update_weights_using_transitions();
  flush_weight_array();
  set_min_important_energy();
  initialize_canonical(min_T,min_important_energy);
}

double sw_simulation::estimate_trip_time(int E1, int E2) {
  double oldmin = min_important_energy;
  double oldmax = max_entropy_state;

  if (E1 > E2) {
    min_important_energy = E1;
    max_entropy_state = E2;
  } else {
    min_important_energy = E2;
    max_entropy_state = E1;
  }
  double rate = 0;
  delete[] compute_walker_density_using_transitions(&rate);

  min_important_energy = oldmin;
  max_entropy_state = oldmax;
  return rate;
}

bool sw_simulation::printing_allowed(){
  const double time_skip = 3; // seconds
  static int every_so_often = 0;
  static int how_often = 1;
  // clock can be expensive, so this is a heuristic to reduce our use of it.
  if (++every_so_often % how_often == 0) {
    const clock_t time_now = clock();
    if(time_now-last_print_time > time_skip*double(CLOCKS_PER_SEC)){
      how_often = 1+ 2*every_so_often/3; // our simple heuristic
      last_print_time = time_now;
      every_so_often = 0;
      fflush(stdout); // flushing once a second will be no problem and can be helpful
      return true;
    }
  }
  return false;
}

