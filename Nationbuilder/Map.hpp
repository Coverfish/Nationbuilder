#pragma once

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <vector>
#include <memory>
#include "Tile.hpp"

class Map {
public:
    // Debug overlay modes for visualization tools.
    enum class DebugOverlay {
        None,
        Height,
        Moisture,
        Tectonics
    };

    Map(int width, int height, int tileSize, int cacheTileSize);

    // Draw the visible part of the map
    void draw(sf::RenderWindow& window, float zoomLevel);

    // World size in pixels
    float getWorldWidth() const;
    float getWorldHeight() const;

    // Generate tiles via WorldGenerator
    void generate();

    // Cache invalidation
    void invalidateWorldRect(const sf::IntRect& rect);
    void invalidateAllCache();

    // Debug overlays
    void setDebugOverlay(DebugOverlay mode);
    void toggleHeightOverlay();
    void toggleMoistureOverlay();
    void toggleTectonicsOverlay();
    DebugOverlay getDebugOverlay() const { return m_debugOverlay; }

private:
    struct CacheTile {
        sf::RenderTexture texture;
        sf::Sprite        sprite;
        sf::IntRect       worldRect;
        bool              valid = false;
    };

    // Map dimensions (in tiles)
    int m_width;
    int m_height;
    int m_tileSize;

    // Cache (render texture) tiling
    int m_cacheTileSize;
    int m_cacheCols;
    int m_cacheRows;
    bool m_cacheInitialized;
    bool m_cacheColorMode;

    // Tile data
    std::vector<std::vector<TileInfo>> m_tiles;

    // Optional legacy vertex array (used by buildVertices)
    sf::VertexArray m_vertices;

    // Textures & sprites per tile type
    std::unordered_map<TileType, sf::Texture> m_textures;
    std::unordered_map<TileType, sf::Sprite>  m_sprites;

    // Render cache tiles
    std::vector<std::unique_ptr<CacheTile>> m_cacheTiles;

    // Current debug overlay
    DebugOverlay m_debugOverlay;

    // Cache helpers
    void initializeCacheGrid();
    void rebuildCacheTile(int gridX, int gridY, bool colorMode);
    void smartRebuildVisibleCache(sf::RenderWindow& window, bool colorMode);

    // Rendering helpers
    void loadTextures();
    sf::VertexArray buildVertices();

    sf::Color getTileColor(TileType type) const;
    sf::Color getHeightDebugColor(float elevation) const;
    sf::Color getMoistureDebugColor(float moisture) const;
    sf::Color getTectonicDebugColor(float tectonic) const;
    sf::Color getRenderColorForTile(int x, int y) const;
    bool      shouldUseColorOnly(float zoomLevel) const;
};

