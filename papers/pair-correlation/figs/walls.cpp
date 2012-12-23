// Deft is a density functional package developed by the research
// group of Professor David Roundy
//
// Copyright 2010 The Deft Authors
//
// Deft is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// You should have received a copy of the GNU General Public License
// along with deft; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// Please see the file AUTHORS for a list of authors.

#include <stdio.h>
#include <time.h>
#include "OptimizedFunctionals.h"
#include "equation-of-state.h"
#include "LineMinimizer.h"
#include "ContactDensity.h"
#include "utilities.h"
#include "handymath.h"

// Maximum and spacing values for plotting, saved for use by plot-walls.py
const double zmax = 18;
const double xmax = 10;
const double dx = 0.01;

// Here we set up the lattice.
static double width = 30;
const double dw = 0.001;
const double spacing = 3; // space on each side

double radial_distribution(double gsigma, double r)
{
  // Constants determined by fit to monte-carlo data by papers/contact/figs/plot-ghs2.py
  const double a0 = 1.207,
               a1 = 6.896,
               a2 = .4921,
               a3 = 6.373,
               a4 = 2.046,
               a5 = .3890,
               a6 = 4.670,
               a7 = 3.033;
  return 1 + a0*(gsigma-1)*exp(-a1*r) + a2*(gsigma-1)*sin(a3*r)*exp(-a4*r)
           - a5*(gsigma-1)*(gsigma-1)*sin(a6*r)*exp(-a7*r);
}

double notinwall(Cartesian r) {
  const double z = r.z();
  if (fabs(z) > spacing) {
      return 1;
  }
  return 0;
}

static void took(const char *name) {
  //static clock_t last_time = clock();
  //clock_t t = clock();
  assert(name); // so it'll count as being used...
  //double peak = peak_memory()/1024.0/1024;
  //printf("\t\t%s took %g seconds and %g M memory\n", name, (t-last_time)/double(CLOCKS_PER_SEC), peak);
  //last_time = t;
}

Functional WB = HardSpheresNoTensor(1.0);
//Functional WBm2 = HardSpheresWBm2(1.0);
//Functional WBT = HardSpheresWBFast(1.0);



void z_plot(const char *fname, const Grid &a, const Grid &b) {
  FILE *out = fopen(fname, "w");
  if (!out) {
    fprintf(stderr, "Unable to create file %s!\n", fname);
    // don't just abort?
    return;
  }
  const GridDescription gd = a.description();
  const int x = 0;
  const int y = 0;
  for (int z=0; z<gd.Nz/2; z++) {
    Cartesian here = gd.fineLat.toCartesian(Relative(x,y,z));
    double ahere = a(x,y,z);
    double bhere = b(x,y,z);
    fprintf(out, "%g\t%g\t%g\n", here[2], ahere, bhere);
  }
  // hypothetical loop
  //for (double z=0; z<gd.Lat.a3().z(); z+= 0.015) {
  //  double ahere = a(Cartesian(0,0,z));
  //  printf("a(%g) = %g\n", z, ahere);
  //}
  fclose(out);
}

void plot_pair_distribution(const char *fname, double z0, const Grid &gsigma) {
  FILE *out = fopen(fname, "w");
  if (!out) {
    fprintf(stderr, "Unable to create file %s!\n", fname);
    return;
  }
  double gsigma0 = gsigma(Cartesian(0, 0, z0));
  const GridDescription gd = gsigma.description();
  for (double x = 0; x < xmax - dx/2; x += dx) {
    for (double z1 = 0; z1 < zmax - dx/2; z1 += dx) {
      double gsigma1 = gsigma(Cartesian(0, 0, z1));
      double r = sqrt((z0 - z1)*(z0 - z1) + x*x);
      double g2 = (radial_distribution(gsigma0, r) + radial_distribution(gsigma1, r))/2;
      fprintf(out, "%g\t", g2);
    }
    fprintf(out, "\n");
  }
  fclose(out);
}


void run_walls(double eta, const char *name, Functional fhs) {
  // Generates a data file for the pair distribution function, for filling fraction eta
  // and distance of first sphere from wall of z0. Data saved in a table such that the
  // columns are x values and rows are z1 values.

  Functional f = OfEffectivePotential(fhs + IdealGas());
  double mu = find_chemical_potential(f, 1, eta/(4*M_PI/3));
  f = OfEffectivePotential(fhs + IdealGas()
                           + ChemicalPotential(mu));

  const double zmax = width + 2*spacing;
  Lattice lat(Cartesian(dw,0,0), Cartesian(0,dw,0), Cartesian(0,0,zmax));
  GridDescription gd(lat, 0.01);

  Grid potential(gd);
  Grid constraint(gd);
  constraint.Set(notinwall);
  f = constrain(constraint, f);

  potential = (eta*constraint + 1e-4*eta*VectorXd::Ones(gd.NxNyNz))/(4*M_PI/3);
  potential = -potential.cwise().log();

  const double approx_energy = (fhs + IdealGas() + ChemicalPotential(mu))(1, eta/(4*M_PI/3))*dw*dw*width;
  const double precision = fabs(approx_energy*1e-4);
  //printf("Minimizing to %g absolute precision...\n", precision);

  Minimizer min = Precision(precision,
                            PreconditionedConjugateGradient(f, gd, 1,
                                                            &potential,
                                                            QuadraticLineMinimizer));
  for (int i=0;min.improve_energy(false) && i<100;i++) {
  }
  took("Doing the minimization");

  Grid density(gd, EffectivePotentialToDensity()(1, gd, potential));
  //printf("# per area is %g at filling fraction %g\n", density.sum()*gd.dvolume/dw/dw, eta);

  char *plotname = (char *)malloc(1024);
  Grid gsigma(gd, Correlation_A2(1.0)(1, gd, density));

  sprintf(plotname, "papers/pair-correlation/figs/walls%s-%04.2f.dat", name, eta);
  z_plot(plotname, density, gsigma);

  for (double z0 = 1; z0 < 18; z0 += 2) {
    sprintf(plotname, "papers/pair-correlation/figs/walls%s-pair-%04.2f-%g.dat", name, eta, z0);
    plot_pair_distribution(plotname, z0, gsigma);
  }
    free(plotname);

  {
    GridDescription gdj = density.description();
    double sep =  gdj.dz*gdj.Lat.a3().norm();
    int div = gdj.Nz;
    int mid = int (div/2.0);
    double Ntot_per_A = 0;
    double mydist = 0;

    for (int j=0; j<mid; j++){
      Ntot_per_A += density(0,0,j)*sep;
      mydist += sep;
    }

    double Extra_per_A = Ntot_per_A - eta/(4.0/3.0*M_PI)*width/2;

    FILE *fout = fopen("papers/pair-correlation/figs/wallsfillingfracInfo.txt", "a");
    fprintf(fout, "walls%s-%04.2f.dat  -  If you want to match the bulk filling fraction of figs/walls%s-%04.2f.dat, than the number of extra spheres per area to add is %04.10f.  So you'll want to multiply %04.2f by your cavity volume and divide by (4/3)pi.  Then add %04.10f times the Area of your cavity to this number\n",
	    name, eta, name, eta, Extra_per_A, eta, Extra_per_A);

    int wallslen = 20;
    double Extra_spheres =  (eta*wallslen*wallslen*wallslen/(4*M_PI/3) + Extra_per_A*wallslen*wallslen);
    fprintf (fout, "For filling fraction %04.02f and walls of length %d you'll want to use %.0f spheres.\n\n", eta, wallslen, Extra_spheres);

    fclose(fout);
  }

  {
    //double peak = peak_memory()/1024.0/1024;
    //double current = current_memory()/1024.0/1024;
    //printf("Peak memory use is %g M (current is %g M)\n", peak, current);
  }

  took("Plotting stuff");
}

int main(int, char **) {
  FILE *fout = fopen("papers/pair-correlation/figs/wallsfillingfracInfo.txt", "w");
  fclose(fout);
  FILE *out = fopen("papers/pair-correlation/figs/constants.dat", "w");
  if (!out) {
    fprintf(stderr, "Unable to create file constants.dat!\n");
    return 1;
  }
  fprintf(out, "%g\t%g\t%g", zmax, xmax, dx);
  fclose(out);

  for (double eta = 0.1; eta < 0.6; eta += 0.1) {
    run_walls(eta, "WB", WB);
    //run_walls(eta, "WBT", WBT);
    //run_walls(eta, "WBm2", WBm2);
  }
  // Just create this file so make knows we have run.
  if (!fopen("papers/pair-correlation/figs/walls.dat", "w")) {
    printf("Error creating walls.dat!\n");
    return 1;
  }
  return 0;
}
