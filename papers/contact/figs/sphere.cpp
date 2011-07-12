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
#include "Functionals.h"
#include "equation-of-state.h"
#include "LineMinimizer.h"
#include "ContactDensity.h"
#include "utilities.h"
#include "handymath.h"

const double nm = 18.8972613;
// Here we set up the lattice.
double diameter = 6;
double N = 4;
bool using_default_diameter = true;

double notinwall(Cartesian r) {
  const double z = r.z();
  const double y = r.y();
  const double x = r.x();
  if (sqrt(sqr(z)+sqr(y)+sqr(x)) < diameter/2) {
      return 1; 
  }
  return 0;
}

static void took(const char *name) {
  static clock_t last_time = clock();
  clock_t t = clock();
  assert(name); // so it'll count as being used...
  printf("\t\t%s took %g seconds\n", name, (t-last_time)/double(CLOCKS_PER_SEC));
  last_time = t;
}

void radial_plot(const char *fname, const Grid &a, const Grid &b, const Grid &c, const Grid &d) {
  FILE *out = fopen(fname, "w");
  if (!out) {
    fprintf(stderr, "Unable to create file %s!\n", fname);
    // don't just abort?
    return;
  }
  const GridDescription gd = a.description();
  const int x = 0;
  const int z = 0;
  for (int y=0; y<gd.Ny/2; y++) {
    Cartesian here = gd.fineLat.toCartesian(Relative(x,y,z));
    double ahere = a(x,y,z);
    double bhere = b(x,y,z);
    double chere = c(x,y,z);
    double dhere = d(x,y,z);
    fprintf(out, "%g\t%g\t%g\t%g\t%g\n", here[1], ahere, bhere, chere, dhere);
  }
  fclose(out);
}

void plot_grids_yz_directions(const char *fname, const Grid &a, const Grid &b, 
			    const Grid &c) {
  FILE *out = fopen(fname, "w");
  if (!out) {
    fprintf(stderr, "Unable to create file %s!\n", fname);
    // don't just abort?
    return;
  }
  const GridDescription gd = a.description();
  const int x = 0;
  //const int y = gd.Ny/2;
  for (int y=-gd.Ny/2; y<=gd.Ny/2; y++) {
    for (int z=-gd.Nz/2; z<=gd.Nz/2; z++) {
      Cartesian here = gd.fineLat.toCartesian(Relative(x,y,z));
      double ahere = a(here);
      double bhere = b(here);
      double chere = c(here);
      fprintf(out, "%g\t%g\t%g\t%g\t%g\t%g\n", here[0], here[1], here[2], 
	      ahere, bhere, chere);
    }
    fprintf(out,"\n");
 }  
  fclose(out);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    if (sscanf(argv[1], "%lg", &diameter) != 1) {
      printf("Got bad diameter argument: %s\n", argv[1]);
      return 1;
    }
    if (sscanf(argv[2], "%lg", &N) != 1) {
      printf("Got bad N argument: %s\n", argv[2]);
      return 1;
    }
    using_default_diameter = false;
    printf("Diameter is %g hard spheres, and it holds %g of them\n", diameter, N);
  }

  char *datname = (char *)malloc(1024);
  sprintf(datname, "papers/contact/figs/sphere-%04.1f-%02.0f-energy.dat", diameter, N);
  
  FILE *o = fopen(datname, "w");

  const double myvolume = M_PI*diameter*diameter*diameter/6;
  const double meandensity = N/myvolume;

  Functional f = OfEffectivePotential(HardSpheresNoTensor(1.0) + IdealGas());
  double mu = 2 + find_chemical_potential(f, 1, meandensity);
  f = OfEffectivePotential(HardSpheresNoTensor(1.0) + IdealGas()
                           + ChemicalPotential(mu));

  const double xmax = diameter + 4;
  Lattice lat(Cartesian(xmax,0,0), Cartesian(0,xmax,0), Cartesian(0,0,xmax));
  GridDescription gd(lat, 0.1);
    
  Grid potential(gd);
  Grid constraint(gd);
  constraint.Set(notinwall);
  took("Setting the constraint");

  f = constrain(constraint, f);
  //constraint.epsNativeSlice("papers/contact/figs/sphere-constraint.eps",
  // 			      Cartesian(0,xmax,0), Cartesian(0,0,xmax), 
  // 			      Cartesian(0,xmax/2,xmax/2));
  //printf("Constraint has become a graph!\n");
  
  potential = meandensity*constraint + 1e-4*meandensity*VectorXd::Ones(gd.NxNyNz);
  potential = -potential.cwise().log();
    
  Minimizer min = Precision(1e-6, 
                            PreconditionedConjugateGradient(f, gd, 1, 
                                                            &potential,
                                                            QuadraticLineMinimizer));
  Grid density(gd, EffectivePotentialToDensity()(1, gd, potential));
    
  const int numiters = 25;

  double mumax = mu, mumin = mu, Nnow = 0, dmu = 4.0/N;
  while (Nnow < N) {
    mumax = mumin;
    if (mumin > dmu) {
      mumin -= dmu;
      dmu *= 2;
    } else if (mumin > 0) {
      mumin = -mumin;
    } else {
      mumin *= 2;
    }

    f = constrain(constraint, OfEffectivePotential(HardSpheresNoTensor(1.0) + IdealGas()
                                                   + ChemicalPotential(mumin)));
    min.minimize(f, gd);
    for (int i=0;i<numiters && min.improve_energy(false);i++) {
      density = EffectivePotentialToDensity()(1, gd, potential);
      Nnow = density.sum()*gd.dvolume;
      //printf("Nnow is %g vs %g\n", Nnow, N);
      fflush(stdout);
      
      //density.epsNativeSlice("papers/contact/figs/sphere.eps", 
      //                       Cartesian(0,xmax,0), Cartesian(0,0,xmax), 
      //                       Cartesian(0,xmax/2,xmax/2));
      
      //sleep(3);
    }
    printf("mu %g gives N %g\n", mumin, Nnow);
    took("Finding N from mu");
  }
  printf("mu is between %g and %g\n", mumin, mumax);

  while (fabs(N/Nnow-1) > 1e-3) {
    mu = 0.5*(mumin + mumax);

    f = constrain(constraint, OfEffectivePotential(HardSpheresNoTensor(1.0) + IdealGas()
                                                   + ChemicalPotential(mu)));
    min.minimize(f, gd);
    for (int i=0;i<numiters && min.improve_energy(false);i++) {
      density = EffectivePotentialToDensity()(1, gd, potential);
      Nnow = density.sum()*gd.dvolume;
      //printf("Nnow is %g vs %g\n", Nnow, N);
      fflush(stdout);
      
      //density.epsNativeSlice("papers/contact/figs/sphere.eps", 
      //                       Cartesian(0,xmax,0), Cartesian(0,0,xmax), 
      //                       Cartesian(0,xmax/2,xmax/2));
      
      //sleep(3);
    }
    printf("Nnow is %g vs %g with mu %g\n", Nnow, N, mu);
    took("Finding N from mu");
    if (Nnow > N) {
      mumin = mu;
    } else {
      mumax = mu;
    }
  }
  printf("N final is %g (vs %g) with mu = %g\n", Nnow, N, mu);

  double energy = min.energy();
  printf("Energy is %.15g\n", energy);

  double mean_contact_density = ContactDensitySimplest(1.0).integral(1, density)/myvolume;
  
  fprintf(o, "%g\t%.15g\t%.15g\n", diameter, energy, mean_contact_density);
  
  char *plotname = (char *)malloc(1024);
  sprintf(plotname, "papers/contact/figs/sphere-%04.1f-%02.0f.dat", diameter, N);
  Grid energy_density(gd, f(1, gd, potential));
  Grid contact_density(gd, ContactDensitySimplest(1.0)(1, gd, density));
  Grid n0(gd, ShellConvolve(1)(1, density));
  Grid wu_contact_density(gd, FuWuContactDensity(1.0)(1, gd, density));
  plot_grids_yz_directions(plotname, density, energy_density, contact_density);
  sprintf(plotname, "papers/contact/figs/sphere-radial-%04.1f-%02.0f.dat", diameter, N);
  radial_plot(plotname, density, energy_density, contact_density, wu_contact_density);
  free(plotname);
  density.epsNativeSlice("papers/contact/figs/sphere.eps", 
                         Cartesian(0,xmax,0), Cartesian(0,0,xmax), 
                         Cartesian(0,xmax/2,xmax/2));
  
  double peak = peak_memory()/1024.0/1024;
  printf("Peak memory use is %g M\n", peak);
  took("Plotting stuff");
  
  fclose(o);
}
