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



std::random_device r;
std::default_random_engine e1(r());
static int xRes = 840u;
static int yRes = 840u;
double maxSpeed = 0;
std::vector<double> lost;

structures::config conf;

void printAvgDensity(simulate::Simulator* sim, structures::config conf, sf::RenderWindow* window) {
    std::cout << "Avg density: " << sim->avgDensity << "\n";
}

void runSimulation(std::string input, int duration) {
    auto window = sf::RenderWindow(sf::VideoMode({xRes, yRes}), "CMake SFML Project");
    window.setSize({xRes, yRes});
    window.setPosition({100, 100});
    window.setFramerateLimit(144);
    auto simulator = simulate::Simulator(conf);
    std::cout << "Parse\n";
    conf = simulator.parseFile(input);
    std::cout << "Parsed\n";
    auto renderer = render::Renderer(&window, {xRes, yRes}, conf);
    kernel::Kernel kernel = kernel::Kernel(conf);
    kernel.initialize(simulator.getParticleData());
    simulator.addKernel(&kernel);
    //std::cout << "Tmp: " << kernel.getDerivativeEntry(3, 4)[0] << ", " << kernel.getDerivativeEntry(4, 3)[0] << " \n";
    renderer.initialize(simulator.getParticleData());

    int simulationSteps = int (double(duration) / conf.timestep); 
    std::cout << "Steps: " << simulationSteps << ", " << duration << ", " << conf.timestep << "\n";
    for (int step = 0; step < simulationSteps; step++)
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }
        window.clear(sf::Color::Black);
        std::cout << "Step: " << step << "\n";
        for (int i = 0; i < 1; i++) {
            simulator.simulateStep();
        }
        renderer.renderCircles();
        //printAvgDensity(&simulator, conf, &window);
        window.display();
    }window.close();
}

int main(int argc, char** argv) {
    std::string input = argv[1];
    int simulationSteps = std::stoi(argv[2]);
    runSimulation(input, simulationSteps);
}
