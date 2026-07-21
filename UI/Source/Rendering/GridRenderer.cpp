#include "Rendering/GridRenderer.h"

namespace StudioUI {

GridRenderer::GridRenderer() = default;

void GridRenderer::SetCellSize(int width, int height) {
    if (width > 0 && height > 0) {
        m_cellWidth = width;
        m_cellHeight = height;
    }
}

void GridRenderer::Render(sf::RenderWindow& window, const sf::FloatRect& imageBounds) {
    if (imageBounds.width <= 0 || imageBounds.height <= 0) return;

    sf::VertexArray lines(sf::Lines);

    // Vertical lines
    for (float x = 0; x <= imageBounds.width; x += m_cellWidth) {
        lines.append(sf::Vertex(sf::Vector2f(x, 0), m_gridColor));
        lines.append(sf::Vertex(sf::Vector2f(x, imageBounds.height), m_gridColor));
    }

    // Horizontal lines
    for (float y = 0; y <= imageBounds.height; y += m_cellHeight) {
        lines.append(sf::Vertex(sf::Vector2f(0, y), m_gridColor));
        lines.append(sf::Vertex(sf::Vector2f(imageBounds.width, y), m_gridColor));
    }

    window.draw(lines);
}

}