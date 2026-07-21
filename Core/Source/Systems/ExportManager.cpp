#include "Systems/ExportManager.h"
#include "Processing/AtlasPacker.h"

namespace StudioCore {

sf::Image ExportManager::GeneratePreview(const Project& project, int padding) const {
    return AtlasPacker::PackUniformGrid(project, padding);
}

bool ExportManager::ExportPNG(const Project& project, const std::string& filePath, int padding) const {
    sf::Image img = AtlasPacker::PackUniformGrid(project, padding);
    if (img.getSize().x > 0 && img.getSize().y > 0) {
        return img.saveToFile(filePath);
    }
    return false;
}

}