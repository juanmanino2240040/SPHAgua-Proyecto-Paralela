import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def printFile(path, ax1):
    content = pd.read_csv(path)
    x = content["n"]
    fps = content["t"]
    ax1.semilogx(x, fps, label=path)


fig, (ax1) = plt.subplots(1, 1, layout='constrained')
# log x axis
printFile("../random", ax1)
printFile("../random2", ax1)
printFile("../random3", ax1)
printFile("../random4", ax1)
ax1.set(title='semilogx')
ax1.grid()
ax1.grid(which="minor", color="0.9")
ax1.legend()
plt.show()