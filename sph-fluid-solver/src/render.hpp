#include <SFML/Graphics.hpp>
#include "simulate.hpp"
#include "structures.hpp"
#include <vector>
#ifndef RENDER
#define RENDER

namespace render {
    using namespace structures;
    class Renderer {
    public:
        Renderer(sf::RenderTarget* window, std::vector<int> res, config conf);
        void initialize(particle* particles);
        void updateConf(config conf);
        void renderParticles();
        void renderCircles();
    private:
        std::vector<int> transformPoint(std::vector<double> pos);
        int transformSize(double size);
        config conf;
        int scale;
        std::vector<int> offset;
        std::vector<int> res;
        sf::RenderTarget* window;
        particle* particles;
        std::vector<sf::Vertex> vertices;
        void addParticleRender(simulate::particle element, std::vector<sf::Vertex>* render);
    };
}

#endif