
import math
from sympy import *
from sympy.abc import s,w
from sympy.core.numbers import I, pi
from matplotlib import pyplot as plt
from sympy.printing.latex import latex
# import cmath
import numpy as np

import control as ct
from sympy import Poly
from sympy.physics.control.lti import TransferFunction
import sympy.physics.control.control_plots as cp

converter = 'buck'
VO = 10.0
IO = 2.0
VI = 20.0
R = VO/IO
C = 100e-6
L = 47e-6
tdelay = 1e-3 # digital loop update interrupt interval

Vref = VO
D = float(VO)/VI
ROWS = 2
COLS = 4

def plot_bode(system, magindex, phaseindex, title=None,
        initial_exp=-5, final_exp=5, grid=True, show_axes=False,
        show=False, freq_unit='Hz', phase_unit='deg', **kwargs):
    if not title:
        title=f'Bode Plot of ${latex(system)}$'
    plt.subplot(ROWS, COLS, magindex)
    mag = cp.bode_magnitude_plot(system, initial_exp=initial_exp, final_exp=final_exp,
        show=False, grid=grid, show_axes=show_axes, freq_unit=freq_unit, **kwargs)
    mag.title(title + ' Mag', pad=20)
    plt.subplot(ROWS, COLS, phaseindex)
    phase = cp.bode_phase_plot(system, initial_exp=initial_exp, final_exp=final_exp,
        show=show, grid=grid, show_axes=show_axes, freq_unit=freq_unit,
        phase_unit=phase_unit, **kwargs)
    phase.title(title + ' Ph')
    return fig

# def plot_bode_manual(system, magindex, phaseindex, title=None,
#         initial_exp=-5, final_exp=5, grid=True, show_axes=False,
#         show=False, freq_unit='Hz', phase_unit='deg', **kwargs):
#     if not title:
#         title=f'Bode Plot of ${latex(system)}$'
#     if freq_unit == 'Hz':
#         repl = I*w*2*pi
#     else:
#         repl = I*w
#     freqResp = system.subs(s,repl)
#     numPoints = 10*(final_exp-initial_exp)
#     wvalue = np.logspace(initial_exp, final_exp, numPoints)
#     mag = np.zeros(numPoints)
#     phase = np.zeros(numPoints)
#     for n in range(0, numPoints):
#         freqSub = freqResp.subs(w, wvalue[n])
#         mag[n] = 20*log(Abs(freqSub), 10)
#         phase[n] = arg(freqSub)
#     if phase_unit == 'deg':
#         phase = phase*180/math.pi
#     plt.subplot(ROWS, COLS, magindex)
#     magPlot = plt.plot(wvalue, mag)
#     plt.xscale('log')
#     plt.title(title + ' Mag(dB)', pad=20)
#     plt.subplot(ROWS, COLS, phaseindex)
#     phasePlot = plt.plot(wvalue, phase)
#     plt.xscale('log')
#     plt.title(title + ' Ph(deg)', pad=20)

# Table 8.2 on page 315 of Erikson and Maksimovic 2020 edition
if converter == 'buck':
    Gg0 = D
    Gd0 = VO/D
    w0 = 1/math.sqrt(L*C)
    Q = R*math.sqrt(C/L)

# plant transfer function
if converter == 'buck':
    Gvd0 = simplify(Gd0/(1 + s/(Q*w0) + (s/w0)**2)) # duty cycle to output voltage tf
    Gvd = TransferFunction.from_rational_expression(Gvd0)
Gvvi0 = simplify(Gg0/(1 + s/(Q*w0) + (s/w0)**2)) # input voltage to output voltage tf
Gvvi = TransferFunction.from_rational_expression(Gvvi0) # input voltage to output voltage tf


print(Gvd)

# uncompensated loop transfer function
# resistor ladder
rladbot = 10e3
# cladbot = 1/(s*1e-9)
ladtop = 120e3
# ladbot = rladbot*cladbot/(rladbot+cladbot)
ladbot = rladbot
Gladder = simplify(ladbot/(ladbot+ladtop))
# adc
adcRes = 1024
vcc = 5
Gadc = adcRes/vcc
# pwm
Gpwm = 1/adcRes
# delay - pade approximation for Gdelay = exp(-s*tdelay)
AVGWINDOW = 1
Gdelay = 0
for n in range(1,AVGWINDOW+1):
    num, den = ct.pade(n*tdelay, n=5)
    Gdelay = 1/AVGWINDOW*(Poly(num, s)/Poly(den, s))
Gdelay = simplify(Gdelay)
Gdelay = simplify((1 - Gdelay)/(s*tdelay))
# Gdelay = simplify(1/(1+s*tdelay/2))
# Gdelay = 1
# uncompensated loop tf
GloopUC0 = simplify(Gladder*Gadc*Gdelay*Gpwm*Gvd0)
GloopUC = TransferFunction.from_rational_expression(GloopUC0)
# compensator
if False:
    pm = 70
    fc = 5000
    fz = fc*math.sqrt((1-math.sin(math.radians(pm)))/(1+math.sin(math.radians(pm))))
    fp = fc*math.sqrt((1+math.sin(math.radians(pm)))/(1-math.sin(math.radians(pm))))
    fl = fz/10
    f0 = w0/(2*math.pi)
    Tu0 = Abs(GloopUC0.subs(s, I*0.001))
    Gc0 = (fc/f0)**2*(1/Tu0)*math.sqrt(fz/fp)
    print(fz)
    print(fp)
    print(f0)
    print(Tu0)
    print(Gc0)
    Gcomp0 = Gc0*(1+s/(2*math.pi*fz))/(1+s/(2*math.pi*fp)) # lead comp
    Gcomp0 = Gcomp0*(1 + (2*math.pi*fl)/s) # add lag comp to get lead lag
else:
    Gcomp0 = 1*(1 + (2*math.pi*50)/s)/((1+s/(2*math.pi*200))**2) # arbitrary compensator
Gcomp = TransferFunction.from_rational_expression(Gcomp0)
GloopC0 = simplify(GloopUC0*Gcomp0)
GloopC = TransferFunction.from_rational_expression(GloopC0)

# plotting
fig, ax = plt.subplots(ROWS, COLS, figsize=(15,6))
# plot_bode_manual(Gvd, 1, 4, title='Gvd(s)', initial_exp=1, final_exp=7)
# plot_bode_manual(GloopUC, 2, 5, title='GloopUC(s)', initial_exp=1, final_exp=7)
plot_bode(Gvd, 1, 1 + COLS, title='Gvd(s)', initial_exp=0, final_exp=7)
plot_bode(GloopUC, 2, 2 + COLS, title='GloopUC(s)', initial_exp=0, final_exp=7)
plot_bode(Gcomp, 3, 3 + COLS, title='Gcomp(s)', initial_exp=0, final_exp=7)
plot_bode(GloopC, 4, 4 + COLS, title='GloopC(s)', initial_exp=0, final_exp=7)
fig.tight_layout()
plt.show()

GforUC0 = Gpwm*Gvd0
TotalUC0 = simplify(GforUC0/(1+GloopUC0))
TotalUC = TransferFunction.from_rational_expression(TotalUC0)
GforC0 = Gcomp0*Gpwm*Gvd0
TotalC0 = simplify(GforC0/(1+GloopC0))
TotalC = TransferFunction.from_rational_expression(Gladder*Gadc*TotalC0) # prescale Vref by Gladder*Gadc to express voltage as 10-bit value
# TotalC0 = simplify(1/(1+GloopC0))
# TotalC = TransferFunction.from_rational_expression(TotalC0) # prescale Vref by Gladder*Gadc to express voltage as 10-bit value
# fig, ax = plt.subplots(2, 1, figsize=(6,8))
# plt.subplot(2, 1, 1)
# cp.step_response_plot(TotalC, lower_limit=0, upper_limit=1e-2, prec = 1000, show=False)
# (t, y) = cp.step_response_numerical_data(TotalC, lower_limit=0, upper_limit=1e-2, prec = 1000)
# plt.subplot(2, 1, 2)
# T = np.linspace(t[0], t[-1], 10*len(t))

T = np.linspace(0, 50e-2, 10000)
num = TotalC.num
den = TotalC.den
numArr = Poly(num, s).all_coeffs()
numArr = np.squeeze(numArr).tolist()
numArr = [float(n) for n in numArr]
denArr = Poly(den, s).all_coeffs()
denArr = np.squeeze(denArr).tolist()
denArr = [float(n) for n in denArr]
cttf = ct.TransferFunction(numArr, denArr)
T, yout = ct.step_response(cttf, T=T)
plt.plot(T, yout)
plt.grid()
plt.show()
