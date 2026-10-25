#include "../src/structures.hpp"
#include "../src/simulate.hpp"
#include "../src/kernel.hpp"

int main(int argc, char** argv) {
    std::map<int, std::vector<int>> grid;
    grid[0].push_back(3);

    structures::config conf = {0, true, 2, 0, 2, {}, {}};

    auto simulator = simulate::Simulator(conf);
    conf = simulator.parseFile("./data/regular");
    kernel::Kernel kernel = kernel::Kernel(conf);
    std::cout << "Parsed\n";
    kernel.initialize(simulator.getParticleData());
    std::cout << "Initialized\n";
    kernel.completeNeighborSearch();
    simulator.addKernel(&kernel);
    std::cout << "Step\n";
    simulator.simulateStep();
    std::cout << "Stepped\n";
    for (int i = 0; i < simulator.getParticles().size(); i++) {
        if (i % 10 != 0 && i % 10 != 9 && i > 9 && i < 90) {
            std::cout << i << ": " << simulator.getParticles()[i].density << ", " << simulator.getParticles()[i].restDensity << "\n";
        }
    }
}