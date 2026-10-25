import math
import os

path = "./tube001"
files = os.listdir(path)
speed = 0
vx = 0
vy = 0
for file in files:
    if (file.split(".")[1] == "png"): continue
    f = open(f"./{path}/{file}")
    lines = f.readlines()
    for line in lines[1:]:
        try:
            boundary = line.split(",")[0]
            xpos = float(line.split(",")[2])
            ypos = float(line.split(",")[3])
            xspeed = float(line.split(",")[4])
            yspeed = float(line.split(",")[5])
        except:
            print(file, line)
        if xpos > 34 or ypos > 34 or xpos < 1 or ypos < 1:
            #continue
            print(file, line, "out of bounds")
        else:
            if xspeed > vx and xpos < 32 and ypos < 32:
                vx = xspeed
                #print(file, line)
            if yspeed > vy and ypos < 32 and xpos < 32:
                vy = yspeed
            if math.sqrt(xspeed * xspeed + yspeed * yspeed) > speed:
                speed = math.sqrt(xspeed * xspeed + yspeed * yspeed)
                #print(file, line)
print(speed, vx, vy)