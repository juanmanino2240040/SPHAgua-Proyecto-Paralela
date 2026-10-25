import matplotlib.pyplot as plt
import numpy as np

points = [
    1000,
    1500,
    2000,
    2500,
    3000,
    3500,
    4000,
    4500,
    5000
]

data = [
    1040.83815,
    1027.4506,
    1020.46075,
    1016.5328,
    1013.89675,
    1011.92965,
    1010.7816,
    1010.21655,
    1009.0535
    ]

plt.plot(points, (np.array(data) - 1000) / 10)

plt.xlabel("Stiffness coefficient")
plt.ylabel("Avg density error (%)")
plt.legend()
plt.show()