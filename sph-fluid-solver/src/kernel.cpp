#include "kernel.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <string_view>
#include "structures.hpp"
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

double correctionConst = 0.999139;
//double correctionConst = 1;

namespace kernel {
    using namespace structures;

    Kernel::Kernel(config conf) {
        this->conf = conf;
    }

    void Kernel::updateConf(config conf) {
        this->conf = conf;
    }

    void Kernel::initialize(particle* particles) {
        this->particles = particles;
        this->grid.resize(conf.particleCount);
        double gridSize = (conf.h * conf.kernelSupport);
        int yMultiplier = (double(conf.area[0]) / gridSize + 1);
        gridMax = yMultiplier * int(conf.area[1] / gridSize + 1);
        this->gridCount.resize(gridMax + 1);
    }

    void Kernel::completeNeighborSearch() {
        gridNeighborhoodSearch();
    }

    void Kernel::cellNeighborhoodSearch(int i, int gridX, int gridY, int yMultiplier) {
        double gridSize = (conf.h * conf.kernelSupport);
        int gridId = gridY * yMultiplier + gridX;
        if (gridId >= gridMax || gridId < 0) return;
        for (int k = gridCount[gridId]; k < gridCount[gridId + 1]; k++) {
            int j = grid[k];
            double distX = (particles[i].pos[0] - particles[j].pos[0]);
            double distY = (particles[i].pos[1] - particles[j].pos[1]);
            if (distX * distX + distY * distY < conf.kernelSupport * conf.h * conf.kernelSupport * conf.h) {
                particles[i].neighbors.push_back(j);
            }
        }
    }

    void Kernel::gridNeighborhoodSearch() {
        double gridSize = (conf.h * conf.kernelSupport);
        int yMultiplier = (double(conf.area[0]) / gridSize + 1);
        for(int i = 0; i <= gridMax; i++) {
            gridCount[i] = 0;
        }
        for (int i = 0; i < conf.particleCount; i++) {
            particles[i].neighbors.clear();
            int gridX = int(particles[i].pos[0] / gridSize);
            int gridY = int(particles[i].pos[1] / gridSize);
            int gridId = gridY * yMultiplier + gridX;
            if (gridId >= gridMax) std::cout << "Fail\n";
            gridCount[gridId]++;
        }
        for (int i = 1; i < gridMax + 1; i++) {
            gridCount[i] = gridCount[i] + gridCount[i-1]; 
        }
        for (int i = 0; i < conf.particleCount; i++) {
            int gridX = int(particles[i].pos[0] / gridSize);
            int gridY = int(particles[i].pos[1] / gridSize);
            int gridId = gridY * yMultiplier + gridX;
            grid[--gridCount[gridId]] = i;
        }
        for (int i = 0; i < conf.activeParticles; i++) {
            int gridX = int(particles[i].pos[0] / gridSize);
            int gridY = int(particles[i].pos[1] / gridSize);
            cellNeighborhoodSearch(i, gridX - 1, gridY - 1, yMultiplier);
            cellNeighborhoodSearch(i, gridX - 1, gridY, yMultiplier);
            cellNeighborhoodSearch(i, gridX - 1, gridY + 1, yMultiplier);
            cellNeighborhoodSearch(i, gridX, gridY - 1, yMultiplier);
            cellNeighborhoodSearch(i, gridX, gridY, yMultiplier);
            cellNeighborhoodSearch(i, gridX, gridY + 1, yMultiplier);
            cellNeighborhoodSearch(i, gridX + 1, gridY - 1, yMultiplier);
            cellNeighborhoodSearch(i, gridX + 1, gridY, yMultiplier);
            cellNeighborhoodSearch(i, gridX + 1, gridY + 1, yMultiplier);
        }
    }

    void Kernel::quadraticNeighborSearch() {
        for (int i = 0; i < conf.activeParticles; i++) {
            particles[i].neighbors.clear();
        }
        for (int i = 0; i < conf.activeParticles; i++) {
            particles[i].neighbors.push_back(i);
            for (int j = i + 1; j < conf.particleCount; j++) {
                double distX = (particles[i].pos[0] - particles[j].pos[0]);
                double distY = (particles[i].pos[1] - particles[j].pos[1]);
                if (distX * distX + distY * distY < conf.kernelSupport * conf.h * conf.kernelSupport * conf.h) {
                    particles[i].neighbors.push_back(j);
                    particles[j].neighbors.push_back(i);
                }
            }
        }
    }

    std::vector<int> Kernel::specificNeighborSearch(std::vector<double> pos) {
        std::vector<int> neighbors;
        for (int i = 0; i < conf.particleCount; i++) {
            double dist2 = getSquaredDistance(particles[i].pos, pos, conf.dim);
            if (dist2 < std::pow(conf.kernelSupport * conf.h, 2)) {
                neighbors.push_back(i);
                particles[i].rgb = {char(200), 0, 0};
            } else {
                particles[i].rgb = {char(200), char(200), char(200)};
            }
        }
        return neighbors;
    }

    void Kernel::calculateKernel() {
        double alpha = 5 / (14 * M_PI * pow(conf.h, 2));
        for (int i = 0; i < conf.activeParticles; i++) {
            particles[i].kernel.resize(particles[i].neighbors.size());
            for (int k = 0; k < particles[i].kernel.size(); k++) {
                int j = particles[i].neighbors[k];
                double distX = (particles[i].pos[0] - particles[j].pos[0]) / conf.h;
                double distY = (particles[i].pos[1] - particles[j].pos[1]) / conf.h;
                double q = std::sqrt(distX * distX + distY * distY);
                double q1 = std::max(0.0, 1 - q);
                double q2 = std::max(0.0, 2 - q); 
                double kernelVal = correctionConst * alpha * (q2 * q2 * q2 - 4 * q1 * q1 * q1);
                particles[i].kernel[k] = kernelVal;
            }
        }
    }

    void Kernel::calculateKernelDerivative() {
        double alpha = 5 / (14 * M_PI * pow(conf.h, 2));
        for (int i = 0; i < conf.activeParticles; i++) {
            particles[i].kernelDerivX.resize(particles[i].neighbors.size());
            particles[i].kernelDerivY.resize(particles[i].neighbors.size());
            for (int k = 0; k < particles[i].neighbors.size(); k++) {
                int j = particles[i].neighbors[k];
                double distX = (particles[i].pos[0] - particles[j].pos[0]) / conf.h;
                double distY = (particles[i].pos[1] - particles[j].pos[1]) / conf.h;
                double dist = std::sqrt(distX * distX + distY * distY);
                double q1 = std::max(0.0, 1 - dist);
                double q2 = std::max(0.0, 2 - dist);
                double inner = -3 * q2 * q2 + 12 * q1 * q1;
                double common = 0;
                //Ignore elements with really low distance to avoid underflow
                if (dist > 0.001) {
                    common = alpha * inner / (dist * conf.h * conf.h);
                }
                particles[i].kernelDerivX[k] = correctionConst * (particles[i].pos[0] - particles[j].pos[0]) * common;
                particles[i].kernelDerivY[k] = correctionConst * (particles[i].pos[1] - particles[j].pos[1]) * common;
            }
        } 
    }

    double Kernel::getFactor() {
        if (conf.dim == 1) return 1 / (6 * conf.h);
        else if (conf.dim == 2) return 5 / (14 * M_PI * pow(conf.h, 2));
        else if (conf.dim == 2) return 1 / (4 * M_PI * pow(conf.h, 3));
        else {
            throw "Maximum number of dimensions is 3";
        };
    }

    //Discontinued function, only used for legacy tests
    double Kernel::getKernelEntry(int i, int j) {
        //Kernel is symmetric
        for (int k = 0; k < particles[i].neighbors.size(); k++) {
            if (particles[i].neighbors[k] == j) {
                return particles[i].kernel[k];
            }
        }
        std::cout << "Failed to find " << i << ", " << j << "\n";
        return 0.0;
    }

    //Discontinued function, only used for legacy tests
    std::vector<double> Kernel::getDerivativeEntry(int i, int j) {
        std::vector<double> basic;
        basic.resize(conf.dim);
        //Derivative is antisymmetric
        auto p = particles[i];
        for (int k = 0; k < p.neighbors.size(); k++) {
            if (p.neighbors[k] == j) {
                basic[0] = p.kernelDerivX[k];
                basic[1] = p.kernelDerivY[k];
            }
        }
        return basic;
    }
}