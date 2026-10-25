#include <SFML/Graphics.hpp>
#include <string>
#include <iostream>
#include <filesystem>
#include "simulate.hpp"
#include "structures.hpp"
#include "render.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <sstream>
#include <string> 

using namespace structures;
namespace fs = std::filesystem;
config conf;

static int xRes = 1000u;
static int yRes = 1000u;

void saveScreen(sf::RenderTexture* texture, fs::path path) {
    auto modified = path.replace_extension(".png");
    texture->getTexture().copyToImage().saveToFile(modified);
}

int main(int argc, char** argv)
{
    char* path = argv[1];
    
    sf::RenderTexture texture;


    for (const auto & entry : fs::directory_iterator(path)) {
        std::cout << entry.path() << std::endl;
        fs::path path = entry.path();
        auto modified = path.replace_extension(".png");
        if (entry.path().extension() == ".txt" && entry.path().filename().string() != "density.txt" && entry.path().filename().string() != "cfl.txt" 
            && entry.path().filename().string() != "speed.txt" && !fs::exists(modified)) {
            auto simulate = simulate::Simulator(conf);
            conf = simulate.parseFile(entry.path().u8string());
            auto renderer = render::Renderer(&texture, {xRes, yRes}, conf);
            std::cout << "Res: " << xRes << ", " << yRes << "\n"; 
            texture.resize({xRes, yRes});
            texture.clear(sf::Color::Black);
            renderer.updateConf(conf);
            renderer.initialize(simulate.getParticleData());
            renderer.renderCircles();
            texture.display();
            saveScreen(&texture, entry.path());
        }
    }
}