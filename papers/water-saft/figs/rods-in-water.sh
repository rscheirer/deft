#!/bin/sh
#SBATCH --mem-per-cpu=1000
#SBATCH --output rods-%j.out

set -ev

hostname
date

if test -n "$DIAMETER"; then
    time nice -19 papers/water-saft/figs/rods-in-water.mkdat $DIAMETER
else
    time nice -19 papers/water-saft/figs/rods-in-water.mkdat
fi

date
