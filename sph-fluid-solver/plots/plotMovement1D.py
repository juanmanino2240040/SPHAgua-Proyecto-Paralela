import matplotlib.pyplot as plt

coords = []
path = "../planeChaotic"
x = range(0, 4001, 10)
for i in x:
    f = open(f"{path}/step{i}.txt")
    header = f.readline()
    p1 = f.readline()
    vals = p1.split(",")
    coords += [5 - float(vals[3])]

fig, (ax1) = plt.subplots(1, 1, layout='constrained')
# log x axis

ax1.grid()
ax1.grid(which="minor", color="0.9")
ax1.plot(x, coords)
#ax1.plot(data2.keys(), data2.values())
ax1.legend()
plt.show()