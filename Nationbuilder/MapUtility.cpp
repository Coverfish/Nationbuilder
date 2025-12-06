#include "Map.hpp"
#include "Tile.hpp"
#include <SFML/Graphics/Color.hpp>

namespace {
    inline float clamp01(float v) {
        if (v < 0.f) return 0.f;
        if (v > 1.f) return 1.f;
        return v;
    }

    sf::Color lerpColor(const sf::Color& a, const sf::Color& b, float t) {
        t = clamp01(t);
        auto lerp = [t](unsigned char ca, unsigned char cb) -> unsigned char {
            float v = ca + (cb - ca) * t;
            if (v < 0.f) v = 0.f;
            if (v > 255.f) v = 255.f;
            return static_cast<unsigned char>(std::round(v));
            };
        return sf::Color(
            lerp(a.r, b.r),
            lerp(a.g, b.g),
            lerp(a.b, b.b),
            255
        );
    }

    // Quantize to N discrete steps (you want 10)
    sf::Color quantizedGradient(float value,
        const sf::Color& low,
        const sf::Color& high,
        int steps = 10)
    {
        value = clamp01(value);
        if (steps < 2) return low;

        float scaled = value * static_cast<float>(steps - 1);
        int index = static_cast<int>(std::round(scaled));
        float t = static_cast<float>(index) / static_cast<float>(steps - 1);
        return lerpColor(low, high, t);
    }
}

// ------------------------------------------------------------
// Color lookup per tile type (used for zoomed-out / overview rendering)
// ------------------------------------------------------------
sf::Color Map::getTileColor(TileType type) const {
    switch (type) {
    case TileType::Water:     return sf::Color(0, 0, 200);
    case TileType::Lake:      return sf::Color(0, 50, 220);    // a lighter blue
    case TileType::River:     return sf::Color(0, 80, 255);    // bright blue

    case TileType::Grass:     return sf::Color(0, 180, 0);
    case TileType::Forest:    return sf::Color(0, 100, 0);

    case TileType::Hill:      return sf::Color(170, 120, 60);  // warm brown hills
    case TileType::Mountain:  return sf::Color(130, 130, 130); // cool grey mountains

    case TileType::Beach:     return sf::Color(210, 180, 60);  // yellowish sand
    case TileType::Desert:    return sf::Color(235, 220, 140); // bright, dry sand
    case TileType::Jungle:    return sf::Color(0, 140, 40);    // deep, saturated green

    case TileType::Snow:      return sf::Color(240, 245, 255); // almost white
    case TileType::SnowHill:  return sf::Color(220, 230, 240); // slightly darker snow
    case TileType::Tundra:    return sf::Color(160, 170, 120); // muted olive / brownish
    case TileType::Swamp:     return sf::Color(30, 80, 40);    // dark greenish wetlands

    default:                  return sf::Color(255, 0, 255);   // fallback magenta
    }
}

// ------------------------------------------------------------
// Debug overlays: height, moisture & tectonics color ramps
// ------------------------------------------------------------
sf::Color Map::getHeightDebugColor(float elevation) const
{
    // Yellow (low) -> blood red (high)
    sf::Color low(255, 255, 0); // bright yellow
    sf::Color high(160, 0, 0);  // dark red
    return quantizedGradient(elevation, low, high, 10);
}

sf::Color Map::getMoistureDebugColor(float moisture) const
{
    // Light pink (dry) -> dark purple (wet)
    sf::Color low(255, 200, 220); // light pink
    sf::Color high(80, 0, 120);   // dark purple
    return quantizedGradient(moisture, low, high, 10);
}

sf::Color Map::getTectonicDebugColor(float tectonic) const
{
    // Blood red tectonic belts: dark red at edges, bright red at core.
    sf::Color low(120, 0, 0);   // dark blood red
    sf::Color high(255, 0, 0);  // bright red

    // tectonic is usually a small bump (~0..0.25), scale up to 0..1
    float v = clamp01(tectonic * 5.0f);
    return quantizedGradient(v, low, high, 10);
}

sf::Color Map::getRenderColorForTile(int x, int y) const
{
    const TileInfo& info = m_tiles[y][x]; // has type, elevation, moisture, tectonic

    // Base biome color (what you normally see)
    sf::Color base = getTileColor(info.type);

    // No overlay? Just return normal color.
    if (m_debugOverlay == DebugOverlay::None)
        return base;

    // Debug color (height / moisture / tectonics)
    sf::Color overlay;
    if (m_debugOverlay == DebugOverlay::Height) {
        overlay = getHeightDebugColor(info.elevation);
    }
    else if (m_debugOverlay == DebugOverlay::Moisture) {
        overlay = getMoistureDebugColor(info.moisture);
    }
else { // DebugOverlay::Tectonics
    // tectonicMask is 0 outside belts, >0 on the belt after smoothing
    if (info.tectonic <= 0.0f)
        return base;

    overlay = getTectonicDebugColor(info.tectonic);
}

    // Blend factor: 0 = only biome, 1 = only overlay.
    // Stronger tint for tectonics so belts are clearly visible.
    const float t = (m_debugOverlay == DebugOverlay::Tectonics) ? 0.85f : 0.6f;

    auto mix = [t](unsigned char a, unsigned char b) -> unsigned char {
        float f = a + (b - a) * t;
        if (f < 0.f)   f = 0.f;
        if (f > 255.f) f = 255.f;
        return static_cast<unsigned char>(std::round(f));
        };

    return sf::Color(
        mix(base.r, overlay.r),
        mix(base.g, overlay.g),
        mix(base.b, overlay.b),
        255
    );
}

// ------------------------------------------------------------
// Zoom threshold for switching between pixel-color and texture mode
// ------------------------------------------------------------
bool Map::shouldUseColorOnly(float zoomLevel) const {
    // Higher zoom = color mode (for map overview)
    return zoomLevel > 2.0f;
}



