#include <SFML/Graphics.hpp>
#include "simulate.hpp"
#include "render.hpp"
#include <ctime> 
#include <vector>
#include <iostream>
#include <fstream>
#include <random>
#include "structures.hpp"
#include "kernel.hpp"
#include <format>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <sstream>
#include <string> 



std::random_device r;
std::default_random_engine e1(r());
static int xRes = 540u;
static int yRes = 750u;
double maxSpeed = 0;
std::vector<double> lost;

structures::config conf;

void writeDensity(char* path, int timestep, double density) {
    std::ofstream myfile;
    myfile.open (path, std::ios::app);
    myfile << timestep << "," << density << "\n";
    myfile.close();
}

void run(char* path, int step, int limit, char* output) {
    auto window = sf::RenderWindow(sf::VideoMode({xRes, yRes}), "CMake SFML Project");
    window.setSize({xRes, yRes});
    window.setPosition({100, 100});
    window.setFramerateLimit(144);

    for (int i = step; i < limit; i = i + step) 
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }
        window.clear();

        std::stringstream files;
        files << path << "/step" << i << ".txt";
        std::string file = files.str();

        auto simulator = simulate::Simulator(conf);
        std::cout << "Start paring\n";
        conf = simulator.parseFile(file);
        std::cout << "Parsed\n";
        auto kernel = kernel::Kernel(conf);
        kernel.initialize(simulator.getParticleData());
        simulator.addKernel(&kernel);
        auto renderer = render::Renderer(&window, {xRes, yRes}, conf);
        renderer.initialize(simulator.getParticleData());
        renderer.renderCircles();
        int measure = 0;
        if (measure) {
            simulator.simulateStep();
            double densitySum = 0;
            for (int i = 0; i < conf.activeParticles; i++) {
                densitySum += simulator.getParticles()[i].density;
            }
            std::cout << densitySum / conf.activeParticles << "/" << simulator.getParticleData()[0].restDensity << "\n";
            writeDensity(output, i, densitySum / conf.activeParticles);
        }

        window.display();
    }
    window.close();
}

int main(int argc, char** argv) {
    char* path = argv[1];
    int step = std::stoi(argv[2]);
    int limit = std::stoi(argv[3]);
    char* out = "";
    if (argc > 4) {
        char* out = argv[4];
    }
    run(path, step, limit, out);
}
