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

#include "Minimizer.h"
#include <stdio.h>

void MinimizerInterface::print_info(const char *prefix) const {
  f.print_iteration(prefix, iter);
  printf("%sEnergy = %.16g\n", prefix, energy());
  printf("%sGradient = %g\n", prefix, grad().norm());
}
