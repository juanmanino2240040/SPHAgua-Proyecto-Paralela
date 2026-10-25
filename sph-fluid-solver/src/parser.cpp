#include "parser.hpp"

namespace parser {
    Parser::Parser() {}

    bool Parser::open(std::string path) {
        file.open(path, std::ios::in);
        if (file.is_open()) {
            std::string sa;
            getline(file, sa);
            auto tokens = split(sa, ' ');
            if (tokens.size() < 8) {
                std::cout << "Header should have at least 8 tokens\n";
                throw "Invalid line";
            }
            conf.particleCount = std::stoi(tokens[0]);
            conf.dim = std::stoi(tokens[1]);
            if (conf.dim != 2) throw "Only support dimensions 2.";
            conf.h = std::stod(tokens[2]);
            conf.k = std::stod(tokens[3]);
            conf.nu = std::stod(tokens[4]);
            conf.kernelSupport = std::stod(tokens[5]);
            conf.fastRender = std::stoi(tokens[6]);
            conf.timestep = std::stod(tokens[7]);
            if (tokens.size() != 2 * conf.dim + 8) {
                std::cout << "Header should have 8 + 2 * " << conf.dim << " tokens\n";
                throw "Invalid line";
            }
            for (int i = 0; i < conf.dim; i++) {
                conf.g.push_back(std::stod(tokens[8 + i]));
                conf.area.push_back(std::stoi(tokens[8 + conf.dim + i]));
            }
        }
        return file.is_open();
    }

    void Parser::close() {
        file.close();
    }

    //Returns a partial configuration with all the information parsed from the file
    config Parser::getParsedConfig() {
        return conf;
    }

    void Parser::parseNextParticle(particle* target) {
        std::string sa;
        getline(file, sa);
        auto tokens = split(sa, ',');
        if (tokens.size() != 2 * conf.dim + 2) {
            std::cout << "Particle: " << sa << ", " << tokens.size() << "\n";
            throw "Invalid line";
        }
        target->isStationary = std::stoi(tokens[0]);
        target->mass = std::stod(tokens[1]);
        for (int i = 0; i < conf.dim; i++) {
            target->pos.push_back(std::stod(tokens[i + 2]));
            target->speed.push_back(std::stod(tokens[conf.dim + i + 2]));
        }
    }

    std::vector<std::string> Parser::split(std::string line, char delim) {
        std::vector<std::string> tokens;
        auto start = 0U;
        auto end = line.find(delim);
        while (end != std::string::npos) {
            tokens.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(delim, start);
        }
        tokens.push_back(line.substr(start, end - start));
        return tokens;
    }
}