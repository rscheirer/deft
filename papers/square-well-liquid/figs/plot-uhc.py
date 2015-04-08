#!/usr/bin/python2
import matplotlib, sys
if 'show' not in sys.argv:
    matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy

import styles

if len(sys.argv) not in [5,6]:
    print 'useage: %s ww ff N versions show' % sys.argv[0]
    exit(1)

ww = float(sys.argv[1])
#arg ww = [1.3, 1.5, 2.0, 3.0]

ff = float(sys.argv[2])
#arg ff = [0.3]

# note: speficic HC should be independent of N, but we have to choose one
N = float(sys.argv[3])
#arg N = range(5,30)+range(30,50,5)+range(50,100,10)+range(100,201,20)

versions = eval(sys.argv[4])
#arg versions = [["nw","wang_landau","robustly_optimistic","gaussian","optimized_ensemble","kT0.4","kT0.5","tmmc"]]

# input: ["data/periodic-ww%04.2f-ff%04.2f-N%i-%s-%s.dat" % (ww, ff, N, version, data) for version in versions for data in ["E","lnw"]]

Tmin = 0.2
reference_method = "wang_landau"

Tmax = 2 # fixme: make this an input?
T_bins = 1e3
dT = Tmax/T_bins
T_range = numpy.arange(dT,Tmax,dT)
Tmin_i = int(Tmin/Tmax*T_bins)

# make figure with axes labeled using scientific notation
def sci_fig(handle):
    fig = plt.figure(handle)
    ax = fig.add_subplot(1,1,1)
    fmt = matplotlib.ticker.ScalarFormatter(useMathText=True)
    fmt.set_powerlimits((-2,3))
    fmt.set_scientific(True)
    ax.yaxis.set_major_formatter(fmt)
    ax.xaxis.set_major_formatter(fmt)
    return fig, ax

fig_u, ax_u = sci_fig('u')
plt.title('Specific internal energy for $\lambda=%g$, $\eta=%g$, and $N=%i$' % (ww, ff, N))

fig_hc, ax_hc = sci_fig('hc')
plt.title('Specific heat capacity for $\lambda=%g$, $\eta=%g$, and $N=%i$' % (ww, ff, N))

fig_f, ax_f = sci_fig('f')
plt.title('Specific free energy for $\lambda=%g$, $\eta=%g$, and $N=%i$' % (ww, ff, N))

# make dictionaries which we can index by method name
U = {} # internal energy
CV = {} # heat capacity
F = {} # free energy


for version in versions:
    # energy histogram file; indexed by [-energy,counts]z
    e_hist = numpy.loadtxt(
        "data/periodic-ww%04.2f-ff%04.2f-N%i-%s-E.dat" % (ww, ff, N, version), ndmin=2)
    # weight histogram file; indexed by [-energy,ln(weight)]
    lnw_hist = numpy.loadtxt(
        "data/periodic-ww%04.2f-ff%04.2f-N%i-%s-lnw.dat" % (ww, ff, N, version), ndmin=2)

    energy = -e_hist[:,0] # array of energies
    lnw = lnw_hist[e_hist[:,0].astype(int),1] # look up the lnw for each actual energy
    ln_dos = numpy.log(e_hist[:,1]) - lnw

    Z = numpy.zeros(len(T_range)) # partition function
    U[version] = numpy.zeros(len(T_range)) # internal energy
    CV[version] = numpy.zeros(len(T_range)) # heat capacity
    F[version] = numpy.zeros(len(T_range)) # free energy

    for i in range(len(T_range)):
       # ln_dos_boltz = ln_dos - energy/T_range[i]
        ln_dos_boltz = ln_dos - energy/T_range[i] #                added
        maxA = ln_dos_boltz.max()
        dos_boltz = numpy.exp(ln_dos_boltz - maxA)#           by
        Z[i] = sum(dos_boltz) #                                       brenden
        F[version][i] = -T_range[i]*maxA - T_range[i]*numpy.log(Z[i])#       for reasons
       # dos_boltz = numpy.exp(ln_dos_boltz - ln_dos_boltz.max())
       # Z[i] = sum(dos_boltz)
       # U[version][i] = sum(energy*dos_boltz)/Z[i]
       # CV[version][i] = sum((energy/T_range[i])**2*dos_boltz)/Z[i] - \
        #                 (sum(energy/T_range[i]*dos_boltz)/Z[i])**2
       # F[version][i] = energy.min() +  T_range[i]*numpy.log(sum(numpy.exp(dos_boltz -(energy - energy.min())/T_range[i])))


#    plt.figure('u')
#    plt.plot(T_range,U[version]/N,styles.plot(version),label=styles.title(version))

#    plt.figure('hc')
#    plt.plot(T_range,CV[version]/N,styles.plot(version),label=styles.title(version))

    plt.figure('f')
    plt.plot(T_range,F[version]/N,styles.plot(version),label=styles.title(version))

#plt.figure('u')
#plt.xlabel('$kT/\epsilon$')
#plt.ylabel('$U/N\epsilon$')
#plt.legend(loc='best')
#plt.tight_layout(pad=0.2)
#plt.savefig("figs/periodic-ww%02.0f-ff%02.0f-N%i-u.pdf" % (ww*100, ff*100, N))

#plt.figure('hc')
#plt.ylim(0)
#plt.xlabel('$kT/\epsilon$')
#plt.ylabel('$C_V/Nk$')
#plt.legend(loc='best')
#plt.tight_layout(pad=0.2)
#plt.savefig("figs/periodic-ww%02.0f-ff%02.0f-N%i-hc.pdf" % (ww*100, ff*100, N))

plt.figure('f')
plt.xlabel('$kT/\epsilon$')
plt.ylabel('$F/N\epsilon$')
plt.legend(loc='best')
plt.tight_layout(pad=0.2)
plt.savefig("figs/periodic-ww%02.0f-ff%02.0f-N%i-f.pdf" %(ww*100, ff*100, N))

#error_data = open("figs/error-table-ww%02.0f-ff%02.0f-%i.dat" % (ww*100, ff*100, N), "w")
#error_data.write("# method u_error cv_error\n")
#for version in versions:
#    u_error = max(abs(U[version][Tmin_i:] - U[reference_method][Tmin_i:]))/N
#    cv_error = max(abs(CV[version][Tmin_i:] - CV[reference_method][Tmin_i:]))/N
#    error_data.write("%s %g %g\n" % (version, u_error, cv_error))
#error_data.close()

if 'show' in sys.argv:
    plt.show()
