#include "../src/simulate.hpp"
#include "../src/kernel.hpp"
#include "../src/structures.hpp"

using namespace structures;

config conf1;
auto kernel1 = kernel::Kernel(conf1);
simulate::Simulator simulator1 = simulate::Simulator(conf1);

double upperError = 1.005;

void setup(char* path) {
    simulator1 = simulate::Simulator(conf1);
    conf1 = simulator1.parseFile(path);
    kernel1 = kernel::Kernel(conf1);
    kernel1.initialize(simulator1.getParticleData());
    simulator1.addKernel(&kernel1);
}

void printHeader(char* file) {
    std::ofstream myfile;
    myfile.open (file, std::ios::app);
    myfile << "k,m,v,a,b,e,\n";
    myfile.close();
}

void resetScenario(double speed, int k, double timestep) {
    setup("./data/plane");
    for (int i = 0; i < simulator1.getConf().activeParticles; i++) {
        simulator1.getParticleData()[i].speed = {0, speed};
    }
    conf1.k = k;
    simulator1.updateConf(conf1);
}

bool checkValidBounce(int expectedBounces, int upperLimit, double tolerance) {
    int direction = 1;
    int bounces = 0;
    double maxHeight = 1.0 - simulator1.getParticles()[0].pos[1];
    for (int i = 0; i < upperLimit; i++) {
        simulator1.simulateStep();
        double height = 1.0 - simulator1.getParticles()[0].pos[1];

        //Particle went through the boundary
        if (height < -1.0) {
            std::cout << "Bounds: " << conf1.timestep << ", " << simulator1.getParticles()[0].pos[1] << ": " << height << "\n";
            return false;
        }
        if (direction == -1 && simulator1.getParticles()[0].speed[1] >= 0) {
            if (height > maxHeight * upperError) {
                std::cout << "Explode: " << conf1.timestep << ": " << height << "/" << maxHeight << "\n";
                return false;
            }
            else if (height < maxHeight * tolerance) {
                //std::cout << "Collapse: " << conf1.timestep << ": " << height << "\n";
                //return false;
            }
            else {
                bounces++;
                maxHeight = height; 
            }
        } 
        if (simulator1.getParticles()[0].speed[1] >= 0) direction = 1;
        else direction = -1;
        if (bounces >= expectedBounces) return true;
    }
    std::cout << "Timeout!\n";
    return false;
}

double getEnergyLoss(int timeout) {
    int direction = 1;
    double initialHeight = 1.0 - simulator1.getParticles()[0].pos[1];
    for (int i = 0; i < timeout; i++) {
        simulator1.simulateStep();
        double height = 1.0 - simulator1.getParticles()[0].pos[1];

        //Particle went through the boundary
        if (height < -1.0) {
            std::cout << "Bounds: " << conf1.timestep << ", " << simulator1.getParticles()[0].pos[1] << ": " << height << "\n";
            return -1;
        }
        if (direction == -1 && simulator1.getParticles()[0].speed[1] >= 0) {
            return height / initialHeight;
        } 
        if (simulator1.getParticles()[0].speed[1] >= 0) direction = 1;
        else direction = -1;
    }
    std::cout << "Timeout!\n";
    return -1;
}

double findTimestepBound(double initPos, double startingPoint, double step, double timeout) {
    for (int i = 0; i < 10; i++) {
        double timestep = startingPoint + (10 - i) * step;
        resetScenario(initPos, conf1.k, timestep);
        bool valid = checkValidBounce(5, timeout / step, 0.5);
        if (valid) return timestep;
    }
    return -1;
}

void findMaximumTimestep(double step, double timeout) {
    double initPos = simulator1.getParticles()[0].pos[1];
    double currentGuess = 0;
    //std::cout << "Initial step: " << step << "\n";
    while (step >= 0.0001) {
        //std::cout << "Searching range from " << currentGuess << " to " << currentGuess + 10 * step << "\n";
        double result = findTimestepBound(initPos, currentGuess, step, timeout);
        //std::cout << "Found: " << result << "\n";
        if (result > 0) {
            //std::cout << "Improved guess: " << result << "\n";
            currentGuess = result;
        }
        step = step / 10;
        //std::cout << "Best guess: " << currentGuess << "\n";
    }   
    std::cout << conf1.k << ":" << currentGuess << ",\n";
}

//Search the space of (startingPoint, startingPoint + 10 * step] with precision step
//for the lowest stiffness that stops the particle
int findStiffnessBound(double initSpeed, int startingPoint, int step) {
    for (int i = 1; i <= 10; i++) {
        resetScenario(initSpeed, startingPoint + i * step, conf1.timestep);
        //std::cout << "Testing k=" << startingPoint + i * step << "\n";
        bool valid = true;
        std::cout << "k: " << simulator1.getConf().k << "\n";
        //printf("Running simulator wit %f - %i\n", simulator1.getConf().k, startingPoint + i * step);
        for (int s = 0; s < 50; s++) {
            for (int i = 0; i < 50; i++) {
                simulator1.simulateStep();
            }
            //std::cout << "Step: " << s << "\n";
            //The particle phased through the barrier
            for (int i = 0; i < simulator1.getConf().activeParticles; i++) {
                if (simulator1.getParticles()[i].pos[1] > 11) {
                    //std::cout << "Out of bounds: " << s << ", " << simulator1.getParticles()[i].pos[0] << ", " << simulator1.getParticles()[i].pos[1] << "\n";
                    //printf("Out of bounds %i\n", startingPoint + i * step);
                    s = 15000;
                    valid = false;
                }
            }
            if (simulator1.getConf().activeParticles < 0.2 * simulator1.getConf().activeParticles) {
                return startingPoint + i * step;
            }
            //The particle bounced
            //Only for single particle
            /*if (simulator1.getParticles()[0].speed[1] < -0.1) {
                return startingPoint + i * step;
            }*/
        }
        //The particle did not bounce but also did not phase through the barrier
        if (valid) {
            return startingPoint + i * step;
        }
    }
    return -1;
}

int findMinimumStiffness(double initSpeed, int guess) {
    int step = std::pow(10, std::floor(std::log10(guess)));
    printf("Guess: %i-%i\n", guess, step);
    int currentGuess = 0;
    while (step >= 1) {
        std::cout << "Searching range from " << currentGuess << " to " << currentGuess + 10 * step << "\n";
        int result = findStiffnessBound(initSpeed, currentGuess, step);
        std::cout << "Found valid k: " << result << "\n";
        if (result < 0) {
            std::cout << "Could not find k between " << currentGuess << " and " << currentGuess + 10 * step << "\n";
            return -1;
        }
        else if (step > 1){
            //Guess should always be slightly to low
            currentGuess = result - step;
            step = step / 10;
        } else {
            currentGuess = result;
            step = step / 10;
        }
        std::cout << "Best guess: " << currentGuess << "\n";
    }  
    return currentGuess; 
}

void plotEnergyLoss() {
    for (int i = 0; i <= 400; i++) {
        setup("./data/plane");
        conf1.nu = i * 0.0025;
        simulator1.updateConf(conf1);
        std::cout << conf1.nu << ":" << getEnergyLoss(1500) << ",\n";
    }
}

void resetBulk(double speed, int k, double timestep) {
    for (int i = 0; i < conf1.activeParticles; i++) {
        simulator1.getParticleData()[i].speed = {0, speed};
    }
    conf1.k = k;
    conf1.timestep = timestep;
    simulator1.updateConf(conf1);
}

void isValidStiffness(double speed, double stiffness) {
    setup("./data/tube");
    resetBulk(speed, stiffness, 0.0001);
    for (int  i = 0; i < 15000; i++) {
        simulator1.simulateStep();
        if (i % 100 == 0) {
            std::cout << i << "\n";
        }
    }
    for (int j = 0; j < conf1.activeParticles; j++) {
        particle p = simulator1.getParticles()[j];
        if (p.pos[0] > 34 || p.pos[1] > 34 || p.pos[0] < 1 || p.pos[1] < 1) {
            std::cout << "Fail: " << j << " " << p.pos[0] << " " << p.pos[1] << "\n";
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) throw "Fail\n";
    setup("./data/plane");
    int result = findMinimumStiffness(std::stod(argv[1]), std::stoi(argv[2]));
    if (argc > 3) {
        std::ofstream myfile;
        myfile.open (argv[3], std::ios::app);
        myfile << conf1.timestep << "," << argv[1] << "," << result << "\n";
    myfile.close();
    }
    //plotEnergyLoss();
    //isValidStiffness(std::stod(argv[1]), std::stod(argv[2]));
}