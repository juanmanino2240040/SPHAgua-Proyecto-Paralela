import matplotlib.pyplot as plt

# 0.08
# 0.20

f = open("../debug/save/dam_50_instable", "r")
lines = f.readlines()
print(len(lines))
f.close()
x1 = []
y1 = []
y2 = []
for line in lines[1:]:
    x1 += [float(line.split(",")[0]) / 1000]
    y1 += [float(line.split(",")[1])]
    y2 += [float(line.split(",")[2])]
f.close()

#avg = np.array(y).mean()
#print(avg)
plt.plot(x1, y1, label="fluid acc")
plt.plot(x1, y2, label="border acc")

plt.xlabel("Avg acceleration")
plt.xlabel("Simulation time (s)")
plt.legend()
plt.show()