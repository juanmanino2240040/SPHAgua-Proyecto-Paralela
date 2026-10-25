#include <iostream>
#include <cmath>
#include "structures.hpp"

namespace structures {
    /**
     * We use squared distance for distance comparisons to avoid explicitly
     * taking the square root
     */
    double getSquaredDistance(std::vector<double> a, std::vector<double> b, int dim) {
        double dist2 = 0;
        for (int k = 0; k < dim; k++) {
            dist2 = dist2 + std::pow(a[k] - b[k], 2);
        }
        return dist2;
    }

    double getDistance(std::vector<double> a, std::vector<double> b, int dim) {
        return std::sqrt(getSquaredDistance(a, b, dim));
    }
}