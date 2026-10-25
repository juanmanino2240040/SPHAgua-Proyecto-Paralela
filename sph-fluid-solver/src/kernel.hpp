#include "structures.hpp"
#include <map>

#ifndef KERNEL
#define KERNEL

namespace kernel {
    using namespace structures;

    class Kernel {
        public: 
            Kernel(config conf);
            //The particles should not change during the simulation
            void updateConf(config conf);
            void initialize(particle* particles);
            void completeNeighborSearch();
            std::vector<int> specificNeighborSearch(std::vector<double> pos);
            void calculateKernel();
            double getKernelEntry(int i, int j);
            void calculateKernelDerivative();
            std::vector<double> getDerivativeEntry(int i, int j);

        private:
            config conf;
            particle* particles;
            std::vector<int> gridCount;
            std::vector<int> grid;
            int gridMax;
            void quadraticNeighborSearch();
            void cellNeighborhoodSearch(int i, int gridX, int gridY, int yMultiplier);
            void gridNeighborhoodSearch();
            double getFactor();
    };
}

#endif