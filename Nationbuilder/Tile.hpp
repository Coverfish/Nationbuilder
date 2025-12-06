#pragma once
#include <SFML/Graphics.hpp>

enum class TileType {
    Water,
    Grass,
    Forest,
    Hill,
    Mountain,
    Beach,
    River,
    Lake,
    Desert,
    Jungle,
    Snow,
    SnowHill,
    Tundra,
    Swamp
};

struct TileInfo {
    TileType type = TileType::Water;
    float elevation = 0.0f;
    float moisture = 0.0f;
    float tectonic = 0.0f;  // tectonic uplift strength (for debug overlay)
};
