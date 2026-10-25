from random import randint
import math
phi = 30
h = 0.5
n = 150 * 8 + 2 * 128 + 2500
dim = 2
k = 5000
m = 1000 * h * h
nu = 0.2
kernelSupport = 2
step = 0.001
acc = [0, 9.81]

"""
def box():
    f = open("boundary", "w+")
    f.write(f"{n} 2 {h} {k} {kernelSupport} 0 {step} {acc[0]} {acc[1]} 600 600\n")
    f.write("0,3.5,300,300,0,0\n")
    for x in range(1, 49):
        x1 = 50 + 10 * x
        f.write(f"1,{m},{x1},{40},0,0\n")
        f.write(f"1,{m},{x1},{50},0,0\n")
        f.write(f"1,{m},{x1},{540},0,0\n")
        f.write(f"1,{m},{x1},{550},0,0\n")
        f.write(f"1,{m},{40},{x1},0,0\n")
        f.write(f"1,{m},{50},{x1},0,0\n")
        f.write(f"1,{m},{540},{x1},0,0\n")
        f.write(f"1,{m},{550},{x1},0,0\n")
    f.close()
    """

def plane():
    f = open("planeSingle", "w+")
    m = 250
    f.write(f"{61} {dim} {h} {k} {nu} {kernelSupport} {1} {step} {acc[0]} {acc[1]} {30 * h} {30 * h}\n")
    f.write(f"0,{m},{15 * h},{19 * h},0,0\n")
    #for x in range(0, 10):
    #    for y in range(0, 10):
    #        f.write(f"0,{m},{(10 + x) * h},{(10 + y) * h},0,0\n")
    for x in range(0, 30):
        x1 = h * x
        f.write(f"1,{m},{x1},{20 * h},0,0\n")
        f.write(f"1,{m},{x1},{21 * h},0,0\n")
    f.close()

def grid():
    dx = 200
    dy = 200
    f = open("angled", "w+")
    f.write(f"{100} 2 {20} {k} {0} {1.9} 0 {step} {acc[0]} {acc[1]} 600 600\n")
    for x in range(0,10):
        for y in range(0, 10):
            x1 = math.cos(phi) * 20 * x + math.sin(phi) * 20 * y + dx
            y1 = math.cos(phi) * 20 * y - math.sin(phi) * 20 * x + dy
            f.write(f"0,3.5,{x1},{y1},0,0\n")
    f.close()

def breakingDam(n):
    f = open("dam", "w+")
    f.write(f"{n + 50 * 50} {dim} {h} {k} {nu} 0 {kernelSupport} 0 {step} {acc[0]} {acc[1]} 80 80\n")
    diff = 5
    m = n * 1000 * 1000
    for x in range(50):
        for y in range(50, 150):
            f.write(f"0, {m},{diff + h * (x + 2)},{diff + h * y},0,0\n")

    for x in range(0, 150):
        f.write(f"1,{m},{0 + diff},{h * x + diff},0,0\n")
        f.write(f"1,{m},{h + diff},{h * x + diff},0,0\n")

        f.write(f"1,{m},{h * x + diff},{h * 150 + diff},0,0\n")
        f.write(f"1,{m},{h * x + diff},{h * 151 + diff},0,0\n")

        f.write(f"1,{m},{h * 150 + diff},{h * (x + 2) + diff},0,0\n")
        f.write(f"1,{m},{h * 151 + diff},{h * (x + 2) + diff},0,0\n")

        f.write(f"1,{m},{h * (x + 2) + diff},{0 + diff},0,0\n")
        f.write(f"1,{m},{h * (x + 2) + diff},{h + diff},0,0\n")
    for x in range(2, 130):
        f.write(f"1,{m},{h * 52 + diff},{h * x + diff},0,0\n")
        f.write(f"1,{m},{h * 53 + diff},{h * x + diff},0,0\n")
    f.close()

def box(n, h):
    f = open("box", "w+")
    f.write(f"{16*(n+1) + n * n} {dim} {h} {k} {nu} {kernelSupport} 0 {step} {acc[0]} {acc[1]} {(2 * n + 10) * h} {(2 * n + 10) * h}\n")
    for i in range(0, n):
        for j in range(0, n):
            f.write(f"0,{m},{(i + 5) * h},{(j + 5 + n) * h},0,0\n")
    for i in range(0, 2 * n + 2):
        f.write(f"1,{m},{(i + 3) * h},{3 * h},0,0\n")
        f.write(f"1,{m},{(i + 3) * h},{4 * h},0,0\n")

        f.write(f"1,{m},{(5 + 2 * n) * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{(6 + 2 * n) * h},{(i + 3) * h},0,0\n")

        f.write(f"1,{m},{(i + 5) * h},{(2 * n + 5) * h},0,0\n")
        f.write(f"1,{m},{(i + 5) * h},{(2 * n + 6) * h},0,0\n")

        f.write(f"1,{m},{3 * h},{(i + 5) * h},0,0\n")
        f.write(f"1,{m},{4 * h},{(i + 5) * h},0,0\n")

def tube(nx, ny, h, vy):
    f = open("tube", "w+")
    f.write(f"{4*(1.5 * ny + 4) + 4 * nx + nx * ny} {dim} {h} {k} {nu} 0 {kernelSupport} 0 {step} {acc[0]} {acc[1]} {(2 * ny + 10) * h} {(2 * ny + 10) * h}\n")
    for x in range(0, nx):
        for y in range(0, ny):
            f.write(f"0,{m},{(x + 5) * h},{(y + 5 + 0.5 * ny - vy) * h},0,{0}\n")
    for i in range(0, int(1.5 * ny) + 4):
        f.write(f"1,{m},{(5 + nx) * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{(6 + nx) * h},{(i + 3) * h},0,0\n")

        f.write(f"1,{m},{3 * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{4 * h},{(i + 3) * h},0,0\n")
    for i in range(0, nx):
        #continue
        f.write(f"1,{m},{(i + 5) * h},{3 * h},0,0\n")
        f.write(f"1,{m},{(i + 5) * h},{4 * h},0,0\n")

        f.write(f"1,{m},{(i + 5) * h},{(1.5 * ny + 5) * h},0,0\n")
        f.write(f"1,{m},{(i + 5) * h},{(1.5 * ny + 6) * h},0,0\n")

def uTube(nx, ny, h):
    f = open("utube", "w+")

    steps1 = math.pi * (nx + 2.5)
    ang1 = 180 / steps1
    steps2 = math.pi * (nx + 2)
    ang2 = 180 / steps2

    steps3 = math.pi * (0.75 * nx + 0.5)
    ang3 = 180 / steps3
    steps4 = math.pi * (0.75 * nx - 0.5)
    ang4 = 180 / steps4
    print(steps3)
    m = 1000 * h * h
    n = nx * ny + 6 * (ny + 4) + 2 * 2 * nx + 3 * nx + int(steps1) + int(steps2) + 8 + int(steps3) + int(steps4)
    f.write(f"{n} {dim} {h} {k} {nu} {0} {kernelSupport} 0 {step} {acc[0]} {acc[1]} {(2 * nx + 15) * h} {(ny + nx + 15) * h}\n")
    for x in range(0, nx):
        for y in range(0, ny):
            f.write(f"0,{m},{(x + 5) * h},{(y + 5) * h},0,{0}\n")
    for i in range(0, ny + 4):
        f.write(f"1,{m},{(5 + nx) * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{(6 + nx) * h},{(i + 3) * h},0,0\n")

        f.write(f"1,{m},{(7 + 2 * nx) * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{(8 + 2 * nx) * h},{(i + 3) * h},0,0\n")

        f.write(f"1,{m},{3 * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{4 * h},{(i + 3) * h},0,0\n")
    for i in range(0, 2 * nx + 2):
        #continue
        if i != nx and i != nx + 1:
            f.write(f"1,{m},{(i + 5) * h},{3 * h},0,0\n")
            f.write(f"1,{m},{(i + 5) * h},{4 * h},0,0\n")
        if i >= nx * 0.25 and i < nx or i < nx * 1.75 + 2 and i >= nx + 2:
            f.write(f"1,{m},{(i + 5) * h},{(ny + 5) * h},0,0\n")
            f.write(f"1,{m},{(i + 5) * h},{(ny + 6) * h},0,0\n")

    for i in range(0, int(steps1) + 2):
        f.write(f"1,{m},{(5.5 + nx) * h + (nx + 2.5) * h * math.cos(math.radians(ang1 * i))},{(ny + 7) * h + (nx + 2.5) * h * math.sin(math.radians(ang1 * i))},0,0\n")

    for i in range(0, int(steps2) + 2):
        f.write(f"1,{m},{(5.5 + nx) * h + (nx + 1.5) * h * math.cos(math.radians(ang2 * i))},{(ny + 7) * h + (nx + 1.5) * h * math.sin(math.radians(ang2 * i))},0,0\n")

    for i in range(0, int(steps3) + 2):
        f.write(f"1,{m},{(5.5 + nx) * h + (0.75 * nx + 0.5) * h * math.cos(math.radians(ang3 * i))},{(ny + 7) * h + (0.75 * nx + 0.5) * h * math.sin(math.radians(ang3 * i))},0,0\n")

    for i in range(0, int(steps4) + 2):
        f.write(f"1,{m},{(5.5 + nx) * h + (0.75 * nx - 0.5) * h * math.cos(math.radians(ang4 * i))},{(ny + 7) * h + (0.75 * nx - 0.5) * h * math.sin(math.radians(ang4 * i))},0,0\n")

def uTube2(x, h):
    f = open("utube2", "w+")
    n = 6 * (x + 4) + (2 * x + 2) * 0.75

    steps1 = math.pi * (x * 0.625 + 2.5)
    ang1 = 180 / steps1

    steps2 = math.pi * (x * 0.625 + 1.5)
    ang2 = 180 / steps2

    steps3 = math.pi * (x * 0.375 + 0.5)
    ang3 = 180 / steps3

    steps4 = math.pi * (x * 0.375 - 0.5)
    ang4 = 180 / steps4
    n = n = x * x + 6 * (x + 4) + 2 * 2 * x + 3 * x + int(steps1) + int(steps2) + 8 + int(steps3) + int(steps4)
    m = 1000 * h * h
    f.write(f"{n} {dim} {h} {k} {nu} {0} {kernelSupport} 0 {step} 0 {acc[0]} {acc[1]} {(2 * x + 15) * h} {(2 * x + 15) * h}\n")

    for ix in range(0, x):
        for iy in range(0, x):
            f.write(f"0,{m},{(ix + 5) * h},{(iy + 5) * h},0,{0}\n")

    for i in range(0, x + 4):
        f.write(f"1,{m},{(i + 3) * h},{(5 + x) * h},0,0\n")
        f.write(f"1,{m},{(i + 3) * h},{(6 + x) * h},0,0\n")

        f.write(f"1,{m},{(i + 3) * h},{(7 + 2 * x) * h},0,0\n")
        f.write(f"1,{m},{(i + 3) * h},{(8 + 2 * x) * h},0,0\n")

        f.write(f"1,{m},{(i + 3) * h},{3 * h},0,0\n")
        f.write(f"1,{m},{(i + 3) * h},{4 * h},0,0\n")
    for i in range(0, 2 * x + 2):
        #continue
        if i != x and i != x + 1:
            f.write(f"1,{m},{3 * h},{(i + 5) * h},0,0\n")
            f.write(f"1,{m},{4 * h},{(i + 5) * h},0,0\n")
        if i < 0.75 * x or i < x * 1.75 + 2 and i >= x + 2:
            f.write(f"1,{m},{(x + 5) * h},{(i + 5) * h},0,0\n")
            f.write(f"1,{m},{(x + 6) * h},{(i + 5) * h},0,0\n")    

    for i in range(0, int(steps1) + 2):
        f.write(f"1,{m},{(x + 7) * h + (x * 0.625 + 2.5) * h * math.sin(math.radians(ang1 * i))},{(5.5 + x * 1.375) * h + (x * 0.625 + 2.5) * h * math.cos(math.radians(ang1 * i))},0,0\n")

    for i in range(0, int(steps2) + 2):
        f.write(f"1,{m},{(x + 7) * h + (x * 0.625 + 1.5) * h * math.sin(math.radians(ang2 * i))},{(5.5 + x * 1.375) * h + (x * 0.625 + 1.5) * h * math.cos(math.radians(ang2 * i))},0,0\n")

    for i in range(0, int(steps3) + 2):
        f.write(f"1,{m},{(x + 7) * h + (x * 0.375 + 0.5) * h * math.sin(math.radians(ang3 * i))},{(5.5 + x * 1.375) * h + (x * 0.375 + 0.5) * h * math.cos(math.radians(ang3 * i))},0,0\n")

    for i in range(0, int(steps4) + 2):
        f.write(f"1,{m},{(x + 7) * h + (x * 0.375 - 0.5) * h * math.sin(math.radians(ang4 * i))},{(5.5 + x * 1.375) * h + (x * 0.375 - 0.5) * h * math.cos(math.radians(ang4 * i))},0,0\n")


def roundBox(n, h):
    f = open("round", "w+")
    f.write(f"{11534} {dim} {h} {k} {nu} 0 {kernelSupport} 0 {step} {acc[0]} {acc[1]} {(2 * n + 10) * h} {(2 * n + 10) * h}\n")
    m = h * h * 1000
    for x in range(n):
        yMin = min(int(205 - 38 / math.sqrt(2) + x), 2 * n + 5)
        for y in range(105, yMin):
            f.write(f"0,{m},{(x + 5) * h},{(y) * h},0,0\n")
    
    for i in range(0, 2 * n + 2):
        f.write(f"1,{m},{(i + 3) * h},{3 * h},0,0\n")
        f.write(f"1,{m},{(i + 3) * h},{4 * h},0,0\n")

        f.write(f"1,{m},{(5 + 2 * n) * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{(6 + 2 * n) * h},{(i + 3) * h},0,0\n")

        f.write(f"1,{m},{(i + 5) * h},{(2 * n + 5) * h},0,0\n")
        f.write(f"1,{m},{(i + 5) * h},{(2 * n + 6) * h},0,0\n")

        f.write(f"1,{m},{3 * h},{(i + 5) * h},0,0\n")
        f.write(f"1,{m},{4 * h},{(i + 5) * h},0,0\n")

    for i in range(36):
        f.write(f"1,{m},{(30 - 1 / math.sqrt(2) * i) * h},{(5 + 1 / math.sqrt(2) * i) * h},0,0\n")
        f.write(f"1,{m},{(30 - 1 / math.sqrt(2) * i) * h},{(205 - 1 / math.sqrt(2) * i) * h},0,0\n")
        f.write(f"1,{m},{(180 + 1 / math.sqrt(2) * i) * h},{(5 + 1 / math.sqrt(2) * i) * h},0,0\n")
        f.write(f"1,{m},{(180 + 1 / math.sqrt(2) * i) * h},{(205 - 1 / math.sqrt(2) * i) * h},0,0\n")

    for i in range(-1, 37):
        f.write(f"1,{m},{(30 - 1 / math.sqrt(2) * (i - 1)) * h},{(5 + 1 / math.sqrt(2) * (i + 1)) * h},0,0\n")
        f.write(f"1,{m},{(30 - 1 / math.sqrt(2) * (i - 1)) * h},{(205 - 1 / math.sqrt(2) * (i + 1)) * h},0,0\n")
        f.write(f"1,{m},{(180 + 1 / math.sqrt(2) * (i - 1)) * h},{(5 + 1 / math.sqrt(2) * (i + 1)) * h},0,0\n")
        f.write(f"1,{m},{(180 + 1 / math.sqrt(2) * (i - 1)) * h},{(205 - 1 / math.sqrt(2) * (i + 1)) * h},0,0\n")

def colliding(n, h):
    f = open("collide", "w+")
    f.write(f"{4 * (3 * n + 2) + 4 * (2 * n+2) + 2 * n * n} {dim} {h} {k} {nu} 0 {kernelSupport} 0 {step} {acc[0]} {acc[1]} {(3 * n + 10) * h} {(2 * n + 10) * h}\n")
    m = 1000 * h * h

    for ix in range(0, n):
        for iy in range(0, n):
            f.write(f"0,{m},{(ix + 5) * h},{(iy + 5 + n) * h},0,{0}\n")
            f.write(f"0,{m},{(ix + 5 + 2 * n) * h},{(iy + 5 + n) * h},0,{0}\n")

    for i in range(0, 3 * n + 2):
        f.write(f"1,{m},{(i + 3) * h},{3 * h},0,0\n")
        f.write(f"1,{m},{(i + 3) * h},{4 * h},0,0\n")

        f.write(f"1,{m},{(i + 5) * h},{(2 * n + 5) * h},0,0\n")
        f.write(f"1,{m},{(i + 5) * h},{(2 * n + 6) * h},0,0\n")

    for i in range(0, 2 * n + 2):

        f.write(f"1,{m},{(5 + 3 * n) * h},{(i + 3) * h},0,0\n")
        f.write(f"1,{m},{(6 + 3 * n) * h},{(i + 3) * h},0,0\n")

        f.write(f"1,{m},{3 * h},{(i + 5) * h},0,0\n")
        f.write(f"1,{m},{4 * h},{(i + 5) * h},0,0\n")
    

#uTube(28, 56, 0.5)
#plane()       
#uTube2(28, 0.25)
box(100, 0.5)
#tube(200,200,0.5,0)
#grid()
#roundBox(100, 0.1)
#breakingDam(100)
#colliding(40, 0.25)