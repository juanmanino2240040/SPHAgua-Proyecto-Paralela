import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
# Timestep: 0.001

f = open("../debug/tmp/density_5000.txt", "r")
lines = f.readlines()
print("lines: ", len(lines))
f.close()
x1 = []
y1 = []
for line in lines[200:]:
    x1 += [float(line.split(",")[0]) * 1e-05]
    y1 += [float(line.split(",")[1])]
f.close()
print(np.array(y1).mean())


x = [1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000]
y = [0.09, 0.12, 0.15, 0.17, 0.19, 0.2, 0.21, 0.24, 0.25]

fig, (ax1) = plt.subplots(1, 1, layout='constrained')
# log x axis

#ax1.grid()
#ax1.grid(which="minor", color="0.9")
ax1.plot(x, y)
ax1.legend()
ax1.set_xlabel("Stiffness coefficient k")
ax1.set_ylabel(r"Viscosity parameter $\nu$")
plt.show()