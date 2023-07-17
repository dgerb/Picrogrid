from sympy.abc import s
import sympy as sp
from sympy.physics.control import lti
from sympy.physics.control import control_plots

import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

# 20.0/(4.7e-9*s**2 + 9.4e-6*s + 1)
num = [20]
den = [4.7e-9, 9.4e-6, 1]
upper_limit = 1e-2

# sympy continuous
symCont = sp.Poly(num, s)/sp.Poly(den, s)
print('')
print(symCont)
symContTF = lti.TransferFunction(symCont, 1, s)
# plt.plot(t, yspc, 'b')
# plt.xlabel('Time [s]')
# plt.ylabel('Amplitude')
# plt.title('Sympy step response')
# plt.grid()
# plt.show()

# scipy continuous
t = np.linspace(0, upper_limit, 100)
t, y = signal.step((num, den), T=t)
plt.plot(t, y, 'b')

# scipy c2d discrete
print(t)
Ts = t[1] - t[0]
d_system = signal.cont2discrete((num, den), Ts, method='bilinear')
numd = np.squeeze(d_system[0])
dend = np.squeeze(d_system[1])
print('')
print(d_system)
td, y = signal.dstep((numd, dend, t[1] - t[0]), t=t)
plt.plot(td, np.squeeze(y), 'r')
t, y = signal.step((numd, dend), T=t)
plt.plot(t, np.squeeze(y), 'g')
plt.xlabel('Time [s]')
plt.ylabel('Amplitude')
plt.title('Scipy step response')
plt.grid()
plt.show()

# sympy discrete
t, yspc = control_plots.step_response_numerical_data(symContTF, lower_limit=0, upper_limit=upper_limit)
plt.plot(t, yspc, 'b')
symDis = sp.simplify(sp.Poly(numd, s)/sp.Poly(dend, s))
print('')
print(symDis)
symDisTF = lti.TransferFunction(symDis, 1, s)
t, yspd = control_plots.step_response_numerical_data(symDisTF, lower_limit=0, upper_limit=upper_limit)
plt.plot(t, yspd, 'r')
plt.xlabel('Time [s]')
plt.ylabel('Amplitude')
plt.title('Sympy step response')
plt.grid()
plt.show()

print('')

# conclusion: cont2discrete returns the z transform coefficients that give a
# discrete tf equivalent to the input continuous tf. these coefficients must be used with
# z tf, cannot be subbed into laplace tf

