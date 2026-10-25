import matplotlib.pyplot as plt

data = {
    0.0: 63,
    0.5: 65,
    1.0: 68,
    1.5: 74,
    2.0: 83,
    2.5: 93,
    3.0: 106,
    3.5: 122,
    4.0: 139,
    4.5: 159,
    5.0: 181,
    5.5: 205,
    6.0: 231,
    6.5: 260,
    7.0: 291,
    7.5: 324,
    8.0: 360,
    8.5: 398,
    9.0: 438,
    9.5: 480,
    10.0: 525,
    11.0: 621,
    12.0: 726,
    13.0: 840,
    14.0: 964,
    15.0: 1096
}

data2 = {
    0.0: 192,
    1.0: 191,
    1.5: 190,
    2.0: 195,
    3.0: 211,
    4.0: 235,
    5.0: 266,
    6.0: 313,
    7.0: 375,
    8.0: 450,
    9.0: 538,
    10: 638,
    11: 748,
    12: 870,
    13: 1003,
    14: 1147,
    15: 1301
}

# Falling from height h, nu = 0
# timestep 0.001
dataSpeed = {
    0.0: 62550,
    1.0: 67460,
    2.0: 81870,
    3.0: 105410,
    4.0: 137910,
    5.0: 179350,
    6.0: 229800,
    7.0: 289230,
    8.0: 357630,
    9.0: 435110,
    10.0: 521660
}

# timestep 0.0001
dataSpeed2 = {
    0.0: 30280,
    1.0: 56490,
    2.0: 77140,
    3.0: 103210,
    4.0: 136840,
    5.0: 178780,
    6.0: 229380,
    7.0: 288860,
    8.0: 357310,
    9.0: 434800,
    10.0: 521340

}

#timestep 0.00001
dataSpeed3 = {
    0:30260,
    1:56480,
    2:77130,
    3:103200,
    4:136830,
    5:178750,
    6:229350
}

dataSpeed4 = {
    0:62550,
    1:67440,
    2:81830,
    3:105340,
    4:137820,
    5:179230,
    6:229640,
    7:289040,
    8:357460,
    9:434970,
    10:521510
}

dataTube = {
    0: 204000,
    1: 205000,
    2: 223000,
    5: 263000,
    8: 341000,
    10: 428000
}

fig, (ax1) = plt.subplots(1, 1, layout='constrained')
# log x axis

ax1.grid()
ax1.grid(which="minor", color="0.9")
# ax1.set_title("Minimum stiffness for valid boundary handling")
#ax1.plot(data.keys(), data.values())
ax1.plot(data.keys(), data.values(), label="Single particle")
ax1.plot(data2.keys(), data2.values(), label="Particle block")
#ax1.plot(dataSpeed3.keys(), dataSpeed3.values(), label="t=0.00001")
#ax1.plot(range(0, 6), [4329 * x * x + 3577 * x + 52670 for x in range(0,6)], label="prediction")

#ax1.plot(dataTube.keys(), dataTube.values(), label="Tube")
#ax1.plot(range(0, 6), [19327 + 89722 * x for x in range(0,6)] )
plt.xlabel("v (m/s)")
plt.ylabel("k")
ax1.legend()
plt.show()