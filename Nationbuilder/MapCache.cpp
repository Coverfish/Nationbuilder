#include "Map.hpp"
#include "Tile.hpp"
#include <algorithm>
#include <cmath>

// ------------------------------------------------------------
// Initialize cache grid (render textures)
// ------------------------------------------------------------
void Map::initializeCacheGrid() {
    int worldW = m_width * m_tileSize;
    int worldH = m_height * m_tileSize;

    m_cacheCols = static_cast<int>(std::ceil(worldW / static_cast<float>(m_cacheTileSize)));
    m_cacheRows = static_cast<int>(std::ceil(worldH / static_cast<float>(m_cacheTileSize)));

    m_cacheTiles.clear();
    m_cacheTiles.reserve(m_cacheCols * m_cacheRows);

    for (int gy = 0; gy < m_cacheRows; ++gy) {
        for (int gx = 0; gx < m_cacheCols; ++gx) {
            auto tile = std::make_unique<CacheTile>();
            tile->worldRect = sf::IntRect(
                gx * m_cacheTileSize,
                gy * m_cacheTileSize,
                std::min(m_cacheTileSize, worldW - gx * m_cacheTileSize),
                std::min(m_cacheTileSize, worldH - gy * m_cacheTileSize)
            );
            tile->valid = false;
            m_cacheTiles.push_back(std::move(tile));
        }
    }

    m_cacheInitialized = true;
}

// ------------------------------------------------------------
// Rebuild a single cache tile (color or texture mode)
// ------------------------------------------------------------
void Map::rebuildCacheTile(int gridX, int gridY, bool colorMode) {
    if (!m_cacheInitialized) return;
    if (gridX < 0 || gridY < 0 || gridX >= m_cacheCols || gridY >= m_cacheRows) return;

    int index = gridY * m_cacheCols + gridX;
    CacheTile& tile = *m_cacheTiles[index];

    unsigned int texW = static_cast<unsigned int>(tile.worldRect.width);
    unsigned int texH = static_cast<unsigned int>(tile.worldRect.height);

    if (texW == 0 || texH == 0) {
        tile.valid = false;
        return;
    }

    if (!tile.texture.create(texW, texH)) {
        tile.valid = false;
        return;
    }

    tile.texture.clear(sf::Color::Transparent);

    int startTileX = tile.worldRect.left / m_tileSize;
    int endTileX = std::min(m_width, (tile.worldRect.left + tile.worldRect.width) / m_tileSize + 1);
    int startTileY = tile.worldRect.top / m_tileSize;
    int endTileY = std::min(m_height, (tile.worldRect.top + tile.worldRect.height) / m_tileSize + 1);

    if (colorMode) {
        // Draw flat colors
        for (int y = startTileY; y < endTileY; ++y) {
            for (int x = startTileX; x < endTileX; ++x) {
                sf::RectangleShape rect({ (float)m_tileSize, (float)m_tileSize });
                rect.setPosition(
                    static_cast<float>(x * m_tileSize - tile.worldRect.left),
                    static_cast<float>(y * m_tileSize - tile.worldRect.top)
                );
                rect.setFillColor(getRenderColorForTile(x, y));
                tile.texture.draw(rect);
            }
        }
    }
    else {
        // Draw textured tiles
        for (int y = startTileY; y < endTileY; ++y) {
            for (int x = startTileX; x < endTileX; ++x) {
                TileType type = m_tiles[y][x].type;
                auto it = m_sprites.find(type);
                if (it != m_sprites.end()) {
                    sf::Sprite sprite = it->second;
                    sprite.setPosition(
                        static_cast<float>(x * m_tileSize - tile.worldRect.left),
                        static_cast<float>(y * m_tileSize - tile.worldRect.top)
                    );
                    tile.texture.draw(sprite);
                }
                else {
                    sf::RectangleShape rect({ (float)m_tileSize, (float)m_tileSize });
                    rect.setPosition(
                        static_cast<float>(x * m_tileSize - tile.worldRect.left),
                        static_cast<float>(y * m_tileSize - tile.worldRect.top)
                    );
                    rect.setFillColor(getTileColor(type));
                    tile.texture.draw(rect);
                }
            }
        }
    }

    tile.texture.display();
    tile.sprite.setTexture(tile.texture.getTexture());
    tile.sprite.setPosition(
        static_cast<float>(tile.worldRect.left),
        static_cast<float>(tile.worldRect.top)
    );
    tile.valid = true;
}

// ------------------------------------------------------------
// Smart rebuild only visible cache tiles
// ------------------------------------------------------------
void Map::smartRebuildVisibleCache(sf::RenderWindow& window, bool colorMode) {
    if (!m_cacheInitialized)
        initializeCacheGrid();

    // If color mode changed, invalidate all
    if (colorMode != m_cacheColorMode) {
        for (auto& t : m_cacheTiles)
            if (t) t->valid = false;
    }

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
                if (!tile.valid)
                    rebuildCacheTile(gx, gy, colorMode);
            }
        }
    }

    m_cacheColorMode = colorMode;
}
