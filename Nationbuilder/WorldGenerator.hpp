#pragma once
#include <vector>
#include "Tile.hpp"
#include <cstdint>

// World generator: elevation, moisture, rivers & lakes.
// Now accepts a seed so every run can produce a different full world.
class WorldGenerator
{
public:
    // width/height in tiles. seed==0 => uses time-based seed internally.
    WorldGenerator(int width, int height, unsigned int seed = 0);

    // Generates and returns a 2D map of TileInfo
    std::vector<std::vector<TileInfo>> generate();

private:
    int width;
    int height;
    unsigned int m_seed;
    std::vector<std::vector<TileInfo>> map;

    // noise helpers
    float perlin(float x, float y, int seed = 0) const;

    // river/lake builder
    void addRiversAndLakes(const std::vector<std::vector<float>>& elevation, unsigned int rngSeed);
};
