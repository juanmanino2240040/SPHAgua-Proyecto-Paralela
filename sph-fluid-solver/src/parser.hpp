#include <iostream>
#include <fstream>
#include <string>
#include "structures.hpp"
#ifndef PARSER
#define PARSER

namespace parser {
    using namespace structures;
    class Parser {
        public:
            Parser();
            bool open(std::string path);
            config getParsedConfig();
            void parseNextParticle(particle* target);
            void close();
        private:
            std::fstream file;
            config conf;
            std::vector<std::string> split(std::string, char delim);
    };
}

#endif