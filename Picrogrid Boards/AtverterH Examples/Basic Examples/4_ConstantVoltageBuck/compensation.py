
import math
from sympy import *
# from sympy.abc import s,w
from sympy.core.numbers import I, pi
from matplotlib import pyplot as plt
from sympy.printing.latex import latex
# import cmath
import numpy as np
from scipy import signal

import control as ct
from sympy import Poly
from sympy.physics.control.lti import TransferFunction
import sympy.physics.control.control_plots as cp

# setting flags
CONVERTER = 'boost' # 'buck', 'boost', 'buckboost'
MODE = 'CC' # 'CV', 'CC'
USEDELAY = True
STEPGAIN = 4 # multiplier for default step response to 1*u(t)

# set these values for the system
tdelay = 1e-3 # digital loop update interrupt interval
VO = 10.0 # desired output voltage (i.e. reference voltage)
IO = .1 # large signal output current operating point
VI = 20.0 # large signal input voltage operating point
C = 100e-6 # output capacitor
L = 47e-6 # main inductor

# calculated values
R = VO/IO # large signal equivalent output resistance
D = float(VO)/VI # large signal duty cycle operating point
Dp = 1 - D # large signal duty cycle complement

s = ct.TransferFunction.s

def plot_bode(system, magindex, phaseindex, title=None, omega=None, dB=True, Hz=True, deg=True):
    if not title:
        title=f'Bode Plot of ${latex(system)}$'
    # get bode data
    mag, phase, freqs = ct.bode(system, omega=omega, dB=dB, Hz=Hz, deg=True, plot=False)
    if deg:
        phase = [np.degrees(p) for p in phase]
    # plot bode plots
    ax_mag = plt.subplot(2, 4, magindex, label='control-bode-magnitude')
    if dB:
        ax_mag.semilogx(freqs, 20 * np.log10(mag))
    else:
        ax_mag.loglog(freqs, mag)
    ax_mag.set_title(title, pad=20)
    ax_mag.grid(True, which="both")
    ax_mag.set_ylabel("Magnitude (dB)" if dB else "Magnitude")
    ax_phase = plt.subplot(2, 4, phaseindex, label='control-bode-phase', sharex=ax_mag)
    ax_phase.semilogx(freqs, phase)
    ax_phase.grid(True, which="both")
    ax_phase.set_ylabel("Phase (deg)" if deg else "Phase (rad)")

# plant transfer function ------------------------------------------------------------

# Table 8.2 on page 315 of Erikson and Maksimovic 2020 edition
if CONVERTER == 'buck':
    Gg0 = D
    Gd0 = VO/D
    w0 = 1/math.sqrt(L*C)
    Q = R*math.sqrt(C/L)
elif CONVERTER == 'boost':
    Gg0 = 1/Dp
    Gd0 = VO/Dp
    w0 = Dp/math.sqrt(L*C)
    Q = Dp*R*math.sqrt(C/L)
    wz = Dp**2*R/L
elif CONVERTER == 'buckboost':
    Gg0 = -D/Dp
    Gd0 = VO/(D*Dp)
    w0 = Dp/math.sqrt(L*C)
    Q = Dp*R*math.sqrt(C/L)
    wz = Dp**2*R/(D*L)
else:
    print('unknown converter type. exiting')
    exit()
if CONVERTER == 'buck':
    Gvd = ct.minreal(Gd0/(1 + s/(Q*w0) + (s/w0)**2)) # duty cycle to output voltage tf
elif CONVERTER == 'boost' or CONVERTER == 'buckboost':
    Gvd = ct.minreal(Gd0*(1 - s/(wz))/(1 + s/(Q*w0) + (s/w0)**2)) # duty cycle to output voltage tf
Gvvi = Gg0/(1 + s/(Q*w0) + (s/w0)**2) # input voltage to output voltage tf (not used here)
# I assume the small signal output current is simply the small signal output voltage divided by
#  the output resistance, but I could be wrong about this
Gid = ct.minreal(Gvd/R) # duty cycle to output current tf

# uncompensated loop transfer function ------------------------------------------------------------

# resistor ladder (output voltage)
ladbot = 10e3
ladtop = 120e3
Gladder = ladbot/(ladbot+ladtop)
# hall effect sensor (output current)
sensitivity = 0.333 # 333 mV/A sensitivity
cfilt = 10e-9 # external filter cap
rfint = 1800 # internal filter resistance
Ghall = sensitivity*(1/(s*cfilt))/(rfint + 1/(s*cfilt))
Ghall = sensitivity
# adc
adcRes = 1024
vcc = 5
Gadc = adcRes/vcc
# pwm
Gpwm = 1/adcRes
# zero order hold (delay) from microcontroller
if USEDELAY:
    num, den = ct.pade(tdelay, n=3) # pade approximation for Gdelay = exp(-s*tdelay)
    Gdelay = ct.TransferFunction(num, den)
    Gzoh = (1 - Gdelay)/(s*tdelay) # zero order hold better approximates microcontroller effect
else:
    Gzoh = 1
# uncompensated loop tf
if MODE == 'CV':
    GloopUC = ct.minreal(Gladder*Gadc*Gzoh*Gpwm*Gvd)
elif MODE == 'CC':
    GloopUC = ct.minreal(Ghall*Gadc*Gzoh*Gpwm*Gid)
else:
    print('unknown mode of operation. exiting')
    exit()

# compensator design ------------------------------------------------------------

if USEDELAY: # custom compensator with low-frequency dominant pole to overcome delay effects
    Gcomp = 1/(s*tdelay) # phase margin 81°
    # Gcomp = 1/(s*tdelay)*(1+s/200)/(1+s/1000)*.5 # phase margin 119°
    # Gcomp = 1/(s*tdelay)/(1+s/300)*2 # phase margin 27°
else: # compensator design for second order system (E&M 2020 p383)
    pm = 70
    f0 = w0/(2*math.pi)
    fc = f0*5 # arbitrarily pick a crossover frequency 5x above the resonant frequency
    fz = fc*math.sqrt((1-math.sin(math.radians(pm)))/(1+math.sin(math.radians(pm))))
    fp = fc*math.sqrt((1+math.sin(math.radians(pm)))/(1-math.sin(math.radians(pm))))
    fl = fz/10 # arbitrarily set lag compenation pole 10x below crossover frequency
    Tu0 = GloopUC.dcgain()
    Gc0 = (fc/f0)**2*(1/Tu0)*math.sqrt(fz/fp)
    print([fz, fp, f0, Tu0, Gc0])
    Gcomp = Gc0*(1+s/(2*math.pi*fz))/(1+s/(2*math.pi*fp)) # lead comp for phase margin
    Gcomp = Gcomp*(1 + (2*math.pi*fl)/s) # add a lag comp factor for high DC gain
# compensated loop gain = uncompensated loop gain * compensator gain
GloopC = ct.minreal(GloopUC*Gcomp)

# bode plotting ------------------------------------------------------------

fig, ax = plt.subplots(2, 4, figsize=(15,6))
freqs = np.logspace(start=0, stop=7, num=1000) # create frequency array
plot_bode(Gvd, 1, 1 + 4, title='Plant(s)', omega=freqs)
plot_bode(GloopUC, 2, 2 + 4, title='GloopUC(s)', omega=freqs)
plot_bode(Gcomp, 3, 3 + 4, title='Gcomp(s)', omega=freqs)
plot_bode(GloopC, 4, 4 + 4, title='GloopC(s)', omega=freqs)
# print phase margin of compensated loop gain
gm, pm, wcg, wcp = ct.margin(GloopC)
print("\nphase margin " + str(pm) + '\n')
fig.tight_layout()
plt.show()

# forward gain and total system gain ------------------------------------------------------------

if MODE == 'CV':
    Gprescale = Gladder*Gadc # prescale Vref by Gladder*Gadc to express voltage as 10-bit value
    GforUC = Gpwm*Gvd # forward gain path for uncompensated system
    GforC = Gcomp*Gpwm*Gvd # forward gain path for compensated system
elif MODE == 'CC':
    Gprescale = Ghall*Gadc # prescale Iref by Ghall*Gadc to express current as 10-bit value
    GforUC = Gpwm*Gid # forward gain path for uncompensated system
    GforC = Gcomp*Gpwm*Gid # forward gain path for compensated system
else:
    print('unknown mode of operation. exiting')
    exit()

TotalUC = ct.minreal(Gprescale*GforUC/(1+GloopUC)) # total system gain of uncompensated system
TotalC = ct.minreal(Gprescale*GforC/(1+GloopC)) # total system gain of compensated system

# step response plotting ------------------------------------------------------------

t = np.linspace(0, 10e-2, 10000) # create time array
t, yout = ct.step_response(STEPGAIN*TotalC, T=t)
plt.plot(t, yout)
plt.grid()
plt.show()

# equivalent discrete compensator ------------------------------------------------------------

print(Gcomp)
# discritization methods: "bilinear", "euler", "backward_diff", "gbt"
GcompD = Gcomp.sample(tdelay, method='backward_diff')
print(GcompD)
# optionally use a power of 2 multiplier to avoid floating point math on Atmega
multiplier = 8
print("coefficient multipler: " + str(multiplier))
numMult = [np.round(n).astype(int) for n in np.multiply(GcompD.num, multiplier)]
denMult = [np.round(n).astype(int) for n in np.multiply(GcompD.den, multiplier)]
print(*np.squeeze(numMult), sep=", ")
print(*np.squeeze(denMult), sep=", ")
# copy these coefficients directly into compNum and compDen in Atmega code



