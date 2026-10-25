import matplotlib.pyplot as plt

f = open("../data/dump.txt")
densities = []
for line in f.readlines():
    split = line.split(",")
    if (len(split) == 1):
        continue
    split = split[1].split(" ")
    densities += [float(split[1])]

f.close()
points = [i for i in range(len(densities))]
plt.plot(range(len(densities)), densities)

plt.show()