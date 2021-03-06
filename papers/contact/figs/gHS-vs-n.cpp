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
#include <cassert>
#include "ContactDensity.h"
#include "handymath.h"

int main(int, char **) { 
  FILE *o = fopen("papers/contact/figs/gHS-vs-n.dat", "w");
  assert(o);

  Functional ghs_A = gSigmaA(1.0);
  Functional ghs_S = gSigmaS(1.0);
  double mykT = 1.0e-30; // has no effect here!

  for (double eta=0.0001; eta<=0.6; eta *= 1.01) {
    double gg = (1 - eta/2)/ipow(1-eta, 3);
    double n = eta/(4*M_PI/3);
    double s = ghs_S(mykT, n);
    double a = ghs_A(mykT, n);
    double carnghs = (1-eta/2)/uipow(1-eta,3);
    fprintf(o, "%g\t%g\t%g\t%g\t%g\n", eta, gg, carnghs, s, a);
    fflush(o);
  }
  fclose(o);
}
