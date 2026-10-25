import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def printFile(path, ax1):
    # k,m,v,a,b,e,
    content = pd.read_csv(path)
    print(content['m'])
    content["e"] = content["e"] * content["e"]
    group = content.groupby('k').agg({'m':'mean', 
                                      'v':'min', 
                                      'a': 'mean',
                                      'b': 'mean',
                                      'e': ['mean', 'max']})
    print(group[('m', 'mean')])
    valid = group[group[('v', 'min')] == 1]
    ax1.plot(valid.index, valid[('e', 'mean')], label=f'{valid["m"].min()}-avg')
    ax1.plot(valid.index, valid[('e', 'max')], label=f'{valid["m"].min()}-max')


fig, (ax1) = plt.subplots(1, 1, layout='constrained')
# log x axis
printFile("../errorAcc5", ax1)
#printFile("../errorDump", ax1, 1)
#printFile("../errorDump2", ax1, 1)
#printFile("../errorDump3", ax1, 10)
#printFile("../massError", ax1, 2)
#printFile("../massError2", ax1, 5)
ax1.grid()
ax1.grid(which="minor", color="0.9")
ax1.legend()
plt.show()