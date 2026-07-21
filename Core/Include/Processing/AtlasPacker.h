#pragma once
#include <SFML/Graphics/Image.hpp>
#include <memory>

namespace StudioCore {

class Project;

class AtlasPacker {
public:
    static sf::Image PackUniformGrid(const Project& project, int padding);
};

}