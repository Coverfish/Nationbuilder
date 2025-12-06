#include "Map.hpp"
#include "Tile.hpp"
#include <cmath>
#include <algorithm>

// Constructor
Map::Map(int width, int height, int tileSize, int cacheTileSize)
    : m_width(width),
    m_height(height),
    m_tileSize(tileSize),
    m_cacheTileSize(cacheTileSize),
    m_vertices(sf::Quads),
    m_cacheCols(0),
    m_cacheRows(0),
    m_cacheInitialized(false),
    m_cacheColorMode(false),
    m_debugOverlay(DebugOverlay::None)
{
    generate();
    loadTextures();
    m_vertices = buildVertices();
}

// Draw the visible part of the map
void Map::draw(sf::RenderWindow& window, float zoomLevel) {
    // If a debug overlay is active, always use color-only mode
    bool colorOnly =
        shouldUseColorOnly(zoomLevel) ||
        (m_debugOverlay != DebugOverlay::None);

    if (!m_cacheInitialized)
        initializeCacheGrid();

    smartRebuildVisibleCache(window, colorOnly);

    sf::View view = window.getView();
    sf::FloatRect visible(
        view.getCenter().x - view.getSize().x / 2.f,
        view.getCenter().y - view.getSize().y / 2.f,
        view.getSize().x, view.getSize().y
    );

    int startGX = std::max(0, static_cast<int>(visible.left / m_cacheTileSize) - 1);
    int endGX = std::min(m_cacheCols, static_cast<int>((visible.left + visible.width) / m_cacheTileSize) + 1);
    int startGY = std::max(0, static_cast<int>(visible.top / m_cacheTileSize) - 1);
    int endGY = std::min(m_cacheRows, static_cast<int>((visible.top + visible.height) / m_cacheTileSize) + 1);

    for (int gy = startGY; gy < endGY; ++gy) {
        for (int gx = startGX; gx < endGX; ++gx) {
            int idx = gy * m_cacheCols + gx;
            if (idx >= 0 && idx < static_cast<int>(m_cacheTiles.size())) {
                CacheTile& tile = *m_cacheTiles[idx];
                if (tile.valid)
                    window.draw(tile.sprite);
            }
        }
    }
}

float Map::getWorldWidth() const { return static_cast<float>(m_width * m_tileSize); }
float Map::getWorldHeight() const { return static_cast<float>(m_height * m_tileSize); }

void Map::invalidateWorldRect(const sf::IntRect& rect) {
    if (!m_cacheInitialized) return;

    for (auto& t : m_cacheTiles) {
        if (t && t->worldRect.intersects(rect))
            t->valid = false;
    }
}

void Map::invalidateAllCache() {
    for (auto& t : m_cacheTiles)
        if (t) t->valid = false;
}

void Map::setDebugOverlay(DebugOverlay mode)
{
    if (m_debugOverlay == mode)
        return;

    m_debugOverlay = mode;
    // Rebuild cached render textures with the new colors
    invalidateAllCache();
}

void Map::toggleHeightOverlay()
{
    if (m_debugOverlay == DebugOverlay::Height)
        setDebugOverlay(DebugOverlay::None);
    else
        setDebugOverlay(DebugOverlay::Height);
}

void Map::toggleMoistureOverlay()
{
    if (m_debugOverlay == DebugOverlay::Moisture)
        setDebugOverlay(DebugOverlay::None);
    else
        setDebugOverlay(DebugOverlay::Moisture);
}

void Map::toggleTectonicsOverlay()
{
    if (m_debugOverlay == DebugOverlay::Tectonics)
        setDebugOverlay(DebugOverlay::None);
    else
        setDebugOverlay(DebugOverlay::Tectonics);
}
