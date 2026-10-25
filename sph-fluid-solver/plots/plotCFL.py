import os
import math
import matplotlib.pyplot as plt



def getCFL(path, min, max, step, timestep):
    cfl = []
    for i in range(min, max, step):
        f = open(f"../{path}/step{i}.txt")
        lines = f.readlines()[1:]
        maxSpeed = 0
        for line in lines:
            speed = math.sqrt(float(line.split(",")[4]) ** 2 + float(line.split(",")[5]) ** 2)
            if speed > maxSpeed:
                maxSpeed = speed

        cfl += [timestep * maxSpeed / 0.25]
    return cfl

cfl1 = getCFL("box5", 500, 1000000, 500, 2e-5)
#cfl2 = getCFL("tubeDensity5", 500, 1000000, 500, 2e-5)
#cfl3 = getCFL("tubeDensity7", 0, 100000000, 5000, 2e-7)

plt.plot(range(500, 1000000,500), cfl1)
#plt.plot(range(500, 1000000,500), cfl2)
#plt.plot(range(0, 1000000,50), cfl3)

plt.show()
