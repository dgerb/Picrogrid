
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

CONVERTER = 'buck'
USEDELAY = True
tdelay = 1e-3 # digital loop update interrupt interval
VO = 10.0 # desired output voltage (equal to Vref)
IO = 2.0 # large signal output current operating point
VI = 20.0 # large signal input voltage operating point
C = 100e-6 # output capacitor
L = 47e-6 # main inductor

Vref = VO
R = VO/IO
D = float(VO)/VI
ROWS = 2
COLS = 4

s = ct.TransferFunction.s

def plot_bode(system, magindex, phaseindex, title=None, omega=None, dB=True, Hz=True, deg=True):
    if not title:
        title=f'Bode Plot of ${latex(system)}$'
    # get bode data
    mag, phase, freqs = ct.bode(system, omega=omega, dB=dB, Hz=Hz, deg=True, plot=False)
    if deg:
        phase = [np.degrees(p) for p in phase]
    # plot bode plots
    ax_mag = plt.subplot(ROWS, COLS, magindex, label='control-bode-magnitude')
    if dB:
        ax_mag.semilogx(freqs, 20 * np.log10(mag))
    else:
        ax_mag.loglog(freqs, mag)
    ax_mag.set_title(title, pad=20)
    ax_mag.grid(True, which="both")
    ax_mag.set_ylabel("Magnitude (dB)" if dB else "Magnitude")
    ax_phase = plt.subplot(ROWS, COLS, phaseindex, label='control-bode-phase', sharex=ax_mag)
    ax_phase.semilogx(freqs, phase)
    ax_phase.grid(True, which="both")
    ax_phase.set_ylabel("Phase (deg)" if deg else "Phase (rad)")
    # print margins
    gm, pm, wcg, wcp = ct.margin(system)


# Table 8.2 on page 315 of Erikson and Maksimovic 2020 edition
if CONVERTER == 'buck':
    Gg0 = D
    Gd0 = VO/D
    w0 = 1/math.sqrt(L*C)
    Q = R*math.sqrt(C/L)

# plant transfer function
if CONVERTER == 'buck':
    Gvd = ct.minreal(Gd0/(1 + s/(Q*w0) + (s/w0)**2)) # duty cycle to output voltage tf
Gvvi = Gg0/(1 + s/(Q*w0) + (s/w0)**2) # input voltage to output voltage tf
# uncompensated loop transfer function
# resistor ladder
ladbot = 10e3
ladtop = 120e3
Gladder = ladbot/(ladbot+ladtop)
# adc
adcRes = 1024
vcc = 5
Gadc = adcRes/vcc
# pwm
Gpwm = 1/adcRes
# delay - pade approximation for Gdelay = exp(-s*tdelay)
if USEDELAY:
    AVGWINDOW = 1
    Gdelay = 0
    for n in range(1,AVGWINDOW+1):
        num, den = ct.pade(n*tdelay, n=5)
        Gdelay = Gdelay + ct.TransferFunction(num, den)
    Gdelay = Gdelay/AVGWINDOW
    Gzoh = (1 - Gdelay)/(s*tdelay)
else:
    Gzoh = 1
# uncompensated loop tf
GloopUC = ct.minreal(Gladder*Gadc*Gzoh*Gpwm*Gvd)
# compensator
if not USEDELAY: # E&M compensator design for second order system
    pm = 70
    fc = 5000
    fz = fc*math.sqrt((1-math.sin(math.radians(pm)))/(1+math.sin(math.radians(pm))))
    fp = fc*math.sqrt((1+math.sin(math.radians(pm)))/(1-math.sin(math.radians(pm))))
    fl = fz/10
    f0 = w0/(2*math.pi)
    Tu0 = GloopUC.dcgain()
    Gc0 = (fc/f0)**2*(1/Tu0)*math.sqrt(fz/fp)
    print([fz, fp, f0, Tu0, Gc0])
    Gcomp = Gc0*(1+s/(2*math.pi*fz))/(1+s/(2*math.pi*fp)) # lead comp for phase margin
    Gcomp = Gcomp*(1 + (2*math.pi*fl)/s) # add a lag comp factor for high DC gain
else: # custom compensator with low-frequency dominant pole to overcome delay effects
    Gcomp = 1*(1 + (2*math.pi*50)/s)/((1+s/(2*math.pi*200))**2) # arbitrary compensator
GloopC = ct.minreal(GloopUC*Gcomp)

# plotting
fig, ax = plt.subplots(ROWS, COLS, figsize=(15,6))
freqs = np.logspace(start=0, stop=7, num=1000)
plot_bode(Gvd, 1, 1 + COLS, title='Gvd(s)', omega=freqs)
plot_bode(GloopUC, 2, 2 + COLS, title='GloopUC(s)', omega=freqs)
plot_bode(Gcomp, 3, 3 + COLS, title='Gcomp(s)', omega=freqs)
plot_bode(GloopC, 4, 4 + COLS, title='GloopC(s)', omega=freqs)
fig.tight_layout()
plt.show()

Gv2adc = Gladder*Gadc # prescale Vref by Gladder*Gadc to express voltage as 10-bit value
GforUC = Gpwm*Gvd # forward gain path for uncompensated system
TotalUC = ct.minreal(GforUC/(1+GloopUC)) # total feedback gain of uncompensated system
GforC = Gcomp*Gpwm*Gvd # forward gain path for compensated system
TotalC = ct.minreal(Gv2adc*GforC/(1+GloopC))

t = np.linspace(0, 10e-2, 10000)
t, yout = ct.step_response(TotalC, T=t)
plt.plot(t, yout)
plt.grid()
plt.show()

# report discrete compensator
print(Gcomp)
GcompD = Gcomp.sample(tdelay, method='bilinear')
print(GcompD)
multiplier = 256
print("coefficient multipler: " + str(multiplier))
num10 = [np.round(n).astype(int) for n in np.multiply(GcompD.num, multiplier)]
den10 = [np.round(n).astype(int) for n in np.multiply(GcompD.den, multiplier)]
print(*np.squeeze(num10), sep=", ")
print(*np.squeeze(den10), sep=", ")


