#!/usr/bin/env python2
from __future__ import division
import os, numpy, sys, re

if len(sys.argv) != 5:
    print "usage:  python2 %s ww ff min_T N" % sys.argv[0]
    exit(1)

# switch to deft project directory and build SWMC
filepath = os.path.abspath(__file__)
deft_dir = re.sub('deft/.*','deft',filepath)
paper_dir = re.sub('histogram/.*','histogram',filepath)
data_dir = paper_dir+'/data'
os.chdir(deft_dir)
os.system('fac square-well-monte-carlo')

def run_golden(ww, ff, min_T, N, method):
    out_fname = 'golden-%s-N%d-ff%.0f-ww%.0f' % (method, N, ff*100, ww*100)
    iterations = 10000000000
    min_samples = 10000
    cmd = ("srun --mem=600 -J %s time nice -19 ./square-well-monte-carlo --ww %g --ff %g --min_T %g --N %d --%s --iterations %d --min_samples %d --filename_suffix golden > %s.out 2>&1 &"
           % (out_fname, ww, ff, min_T, N, method, iterations, min_samples,
              data_dir+'/'+out_fname))
    print(cmd)
    os.system(cmd)

ww = float(sys.argv[1])
ff = float(sys.argv[2])
min_T = float(sys.argv[3])
N = int(sys.argv[4])
method = 'tmmc'

run_golden(ww, ff, min_T, N, method)
