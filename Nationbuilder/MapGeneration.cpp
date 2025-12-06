#include "Map.hpp"
#include "WorldGenerator.hpp"
#include <ctime>

// ------------------------------------------------------------
// Generate terrain using the new WorldGenerator
// ------------------------------------------------------------
void Map::generate() {
    // generate with time-based seed so every debug/run produces a new full world.
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    WorldGenerator generator(m_width, m_height, seed);
    m_tiles = generator.generate();
}
