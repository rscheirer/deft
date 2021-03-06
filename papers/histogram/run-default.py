#!/usr/bin/env python2
from __future__ import division
import os, numpy, sys, re

if len(sys.argv) not in [6,7]:
    print "usage:  python2 %s ww ff min_T N method [seed]" % sys.argv[0]
    exit(1)

# switch to deft project directory and build SWMC
filepath = os.path.abspath(__file__)
deft_dir = re.sub('deft/.*','deft',filepath)
paper_dir = re.sub('histogram/.*','histogram',filepath)
data_dir = paper_dir+'/data/'
os.chdir(deft_dir)
os.system('fac square-well-monte-carlo')

def run_default(ww, ff, min_T, N, method, seed):
    out_fname = '%s-N%d-ff%.0f-ww%.0f-s%.3i' % (method, N, ff*100, ww*100, seed)
    iterations = 1e5*N*N

    if method == 'cfw':
        method = 'transitions_input_filename ' + \
                 os.path.abspath(data_dir + 'periodic-ww%.2f-ff%.2f-N%i'%(ww,ff,N) + \
                                 '-tmmc-golden-transitions.dat')

    cmd = ("srun --mem=600 -J %s time nice -19 ./square-well-monte-carlo --ww %g --ff %g --min_T %g --N %d --%s --seed %i --iterations %d > %s.out 2>&1 &"
           % (out_fname, ww, ff, min_T, N, method, seed, iterations, data_dir+out_fname))
    print(cmd)
    os.system(cmd)

ww = float(sys.argv[1])
ff = float(sys.argv[2])
min_T = float(sys.argv[3])
N = int(sys.argv[4])
method = sys.argv[5]
try:
    seed = int(sys.argv[6])
except:
    seed = 0

run_default(ww, ff, min_T, N, method, seed)
