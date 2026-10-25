#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#ifndef STRUCT
#define STRUCT
namespace structures {
    typedef struct particle {
        bool isStationary;
        double restDensity;
        double density;
        double pressure;
        double mass;
        std::vector<double> pos;
        std::vector<double> speed;
        std::vector<double> acc;
        std::vector<char> rgb;
        std::vector<int> neighbors;
        std::vector<double> kernel;
        std::vector<double> kernelDerivX;
        std::vector<double> kernelDerivY;
    } particle;

    typedef struct config {
        int particleCount;
        int activeParticles;
        int dim;
        double h;
        double k;
        double nu;
        double gamma;
        double kernelSupport;
        bool fastRender;
        double timestep;
        std::vector<double> g;
        std::vector<double> area;
    } config;

    double getSquaredDistance(std::vector<double> a, std::vector<double> b, int dim);

    double getDistance(std::vector<double> a, std::vector<double> b, int dim);
}

#endif