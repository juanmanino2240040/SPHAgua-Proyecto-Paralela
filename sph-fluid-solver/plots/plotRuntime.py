x = [
    100,
    400,
    900,
    2500,
    6400,
    10000,
    150*150,
    180*180,
    200*200
]

tSim = [
    0.887,
    1.499,
    2.769,
    6.218,
    14.910,
    23.399,
    48.340,
    67.892,
    74.863
]

import matplotlib.pyplot as plt
#plt.plot(k, tInv)
plt.plot(x, tSim, label="water column")
plt.scatter([1568, 5000, 5000, 9622, 20000], [0.7, 2.4, 2.3, 5, 12.3], color='red', label="scenarios")

plt.xlabel("Fluid particles")
plt.ylabel("Runtime (s)")
plt.legend()
plt.show()