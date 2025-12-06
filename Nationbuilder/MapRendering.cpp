#include "Map.hpp"
#include "Tile.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

// ------------------------------------------------------------
// Load textures for each tile type
// ------------------------------------------------------------
void Map::loadTextures() {
    struct Entry { TileType type; const char* file; };
    std::vector<Entry> list = {
        { TileType::Grass,    "assets/tiles/grass.png" },
        { TileType::Water,    "assets/tiles/water.png" },
        { TileType::Forest,   "assets/tiles/forest.png" },
        { TileType::Hill,     "assets/tiles/hill.png" },
        { TileType::Mountain, "assets/tiles/mountain.png" },

        // coast & basic water
        { TileType::Beach,    "assets/tiles/beach.png" },  
        { TileType::River,    "assets/tiles/river.png" },
        { TileType::Lake,     "assets/tiles/lake.png" },

        // climate / biome tiles (adjust filenames to match your sprites!)
        { TileType::Desert,   "assets/tiles/desert.png" },
        { TileType::Jungle,   "assets/tiles/jungle.png" },
        { TileType::Snow,     "assets/tiles/snowFields.png" },
        { TileType::SnowHill, "assets/tiles/snowHill.png" },
        { TileType::Tundra,   "assets/tiles/tundra.png" },
        { TileType::Swamp,    "assets/tiles/swamp.png" }
    };

    for (auto& e : list) {
        sf::Texture tex;
        if (!tex.loadFromFile(e.file))
            continue;

        tex.setSmooth(false);
        m_textures[e.type] = std::move(tex);

        sf::Sprite spr(m_textures[e.type]);
        spr.setScale(
            static_cast<float>(m_tileSize) / m_textures[e.type].getSize().x,
            static_cast<float>(m_tileSize) / m_textures[e.type].getSize().y
        );
        m_sprites[e.type] = spr;
    }
}

// ------------------------------------------------------------
// Build vertex array for color-only rendering
// ------------------------------------------------------------
sf::VertexArray Map::buildVertices() {
    sf::VertexArray vertices(sf::Quads, m_width * m_height * 4);
    int i = 0;

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            sf::Color color = getRenderColorForTile(x, y);
            float fx = static_cast<float>(x * m_tileSize);
            float fy = static_cast<float>(y * m_tileSize);

            vertices[i + 0].position = { fx, fy };
            vertices[i + 1].position = { fx + m_tileSize, fy };
            vertices[i + 2].position = { fx + m_tileSize, fy + m_tileSize };
            vertices[i + 3].position = { fx, fy + m_tileSize };

            vertices[i + 0].color = color;
            vertices[i + 1].color = color;
            vertices[i + 2].color = color;
            vertices[i + 3].color = color;

            i += 4;
        }
    }
    return vertices;
}
