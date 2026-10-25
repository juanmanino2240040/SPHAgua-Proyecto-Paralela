import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# 0.08
# 0.20

f = open("../debug/density.txt", "r")
lines = f.readlines()
print(len(lines))
f.close()
x1 = []
y1 = []
for line in lines[1:]:
    x1 += [float(line.split(",")[0]) * 0.001]
    y1 += [float(line.split(",")[2])]
f.close()

f = open("../debug/save/box_broken.txt", "r")
lines = f.readlines()
print(len(lines))
f.close()
x2 = []
y2 = []
for line in lines[1:]:
    x2 += [float(line.split(",")[0]) * 0.001]
    y2 += [float(line.split(",")[2])]
f.close()

f = open("../highDensity1/density_35.txt", "r")
lines = f.readlines()
print(len(lines))
f.close()
x3 = []
y3 = []
for line in lines[2000:]:
    x3 += [float(line.split(",")[0]) * 1e-5]
    y3 += [float(line.split(",")[1])]
f.close()


#avg = np.array(y).mean()
#print(avg)
ye = [1000 for x in range(len(x1))]
print(len(x1))
plt.plot(x1, y1)
print(np.array(y1).mean())
#plt.plot(x2, y2, label=r'$\nu=0.25$')
print(np.array(y2).mean())
#plt.plot(x3, y3, label=r'$\nu=0.35$')
print(np.array(y3).mean())
#plt.plot(x1, ye, label=r'$\rho_0$')

plt.xlabel("simulation time (s)")
plt.ylabel(r'CFL number',fontsize=12)
plt.legend()
plt.show()