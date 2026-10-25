## Overview
This projects provides a C++ implementation for a particle based fluid solver with Smoothed Particle Hydrodynamics.

## Building and running the solver
To build the entire project, run

```
cmake -S . -B build
cmake --build build
```

This builds multiple binaries in the directory ./build/bin. 
To simulate and visualize a scene from an input file in real time for t seconds of simulated time, run
```
./build/bin/main <input.txt> <t>
```

To simulate a scene without visualization, run
```
./build/bin/plain <input.txt> <t> <x> <out>
```
This will simulate the scene provided in input.txt for t seconds of simulated time and save the state of the simulation in out every x seconds

To replay a simulated scene, run
```
./build/bin/replay <input> <x> <y>
```
This will visualize the simulation saved in input. It will try to visualize a scene for every xth timestep up to timestep y.

To generate and store visualizations of a simulated scene, run 
```
./build/bin/animate <input>
```
This will save a visualization of every input file in folder input in the same folder.

## Input format
The fluid solver requires an input file that stores the initial position and velocity of every particle and also all additional parameters that are required for the simulation. The first line should give all neccessary parameters in the form
```
<n> 2 <h> <k> <nu> <h_k> <r> <t> <g_x> <g_y> <b_x> <b_y>
``` 
where n is the number of particles, h is the sample size, h_k is the kernel support (should be set to 2*h), g_x and g_y are the gravity and b_x and b_y are the boundaries of the simulation. k, nu and t represent the stiffness coefficient, viscosity parameter and timestep of the simulation. The variable r determines the visualization of the scene. For r=0, particles are rendered as circles, for r=1 a less accurate but slightly faster visualization is used.
The remaining lines give the positions of all particles of the simulation in the form 
```
<s> <m> <x> <y> <v_y> <v_y>
```
The variable s states if the particle is a fluid or boundary particle, m is the mass and x, y, v_x and v_y provide the initial position and velocity of the particle.

The folder data provides input files for some basic scenarios, the file data/grid.py can also generate more scenarios with flexible dimensions

## Source code
The main logic of the fluid solver is implemented in three classes. The class Kernel in src/kernel.cpp performs the neighborhood search and calculates the kernel and kernel derivative of all fluid particles and their neighbors. 
The class Simulator in simulate.cpp holds the data of all particles and executes the simulation loop. It calculates the accelerations of all fluid particle according to the Navier-Stokes equation and performs the update step with the Euler-Cromer update scheme. The computation of the acceleration is split into three parts with pressure accelerations, viscous accelerations and gravity. Additionally, the Simulator also checks for particles that leave the defined area of the simulation and removes them.
The class Renderer computes a transformation of the simulated space to the screen that is available for rendering and renders the scene.