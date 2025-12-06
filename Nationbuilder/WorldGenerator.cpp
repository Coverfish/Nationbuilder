#include "WorldGenerator.hpp"
#include <cmath>
#include <random>
#include <algorithm>
#include <functional>
#include <ctime>

// === Utility Noise Functions (value/interpolated noise) ===
static float noise(int xi, int yi, unsigned int seed)
{
    // integer-based deterministic hashing noise (0..1)
    unsigned int n = static_cast<unsigned int>(xi) * 374761393u + static_cast<unsigned int>(yi) * 668265263u + seed * 982451653u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    return static_cast<float>(n & 0x7fffffff) / 2147483647.0f;
}

static float smoothNoise(float x, float y, unsigned int seed)
{
    float corners = (noise(static_cast<int>(x) - 1, static_cast<int>(y) - 1, seed) + noise(static_cast<int>(x) + 1, static_cast<int>(y) - 1, seed) +
        noise(static_cast<int>(x) - 1, static_cast<int>(y) + 1, seed) + noise(static_cast<int>(x) + 1, static_cast<int>(y) + 1, seed)) / 16.0f;
    float sides = (noise(static_cast<int>(x) - 1, static_cast<int>(y), seed) + noise(static_cast<int>(x) + 1, static_cast<int>(y), seed) +
        noise(static_cast<int>(x), static_cast<int>(y) - 1, seed) + noise(static_cast<int>(x), static_cast<int>(y) + 1, seed)) / 8.0f;
    float center = noise(static_cast<int>(x), static_cast<int>(y), seed) / 4.0f;
    return corners + sides + center;
}

static float interpolate(float a, float b, float t)
{
    float ft = t * 3.1415927f;
    float f = (1 - std::cos(ft)) * 0.5f;
    return a * (1 - f) + b * f;
}

static float interpolatedNoise(float x, float y, unsigned int seed)
{
    int intX = static_cast<int>(std::floor(x));
    float fracX = x - intX;
    int intY = static_cast<int>(std::floor(y));
    float fracY = y - intY;

    float v1 = smoothNoise(static_cast<float>(intX), static_cast<float>(intY), seed);
    float v2 = smoothNoise(static_cast<float>(intX + 1), static_cast<float>(intY), seed);
    float v3 = smoothNoise(static_cast<float>(intX), static_cast<float>(intY + 1), seed);
    float v4 = smoothNoise(static_cast<float>(intX + 1), static_cast<float>(intY + 1), seed);

    float i1 = interpolate(v1, v2, fracX);
    float i2 = interpolate(v3, v4, fracX);
    return interpolate(i1, i2, fracY);
}

float WorldGenerator::perlin(float x, float y, int seed) const
{
    float total = 0.0f;
    float persistence = 0.5f;
    int octaves = 6;

    for (int i = 0; i < octaves; ++i)
    {
        float freq = std::pow(2.0f, static_cast<float>(i));
        float amp = std::pow(persistence, static_cast<float>(i));
        total += interpolatedNoise(x * freq, y * freq, static_cast<unsigned int>(seed)) * amp;
    }
    return total;
}

// === Constructor ===
WorldGenerator::WorldGenerator(int width_, int height_, unsigned int seed_)
    : width(width_), height(height_), m_seed(seed_)
{
    if (m_seed == 0) {
        // use time-based seed if none provided
        m_seed = static_cast<unsigned int>(std::time(nullptr));
    }
    map.resize(height, std::vector<TileInfo>(width));
}

// === Main generation: elevation, moisture, biomes, rivers/lakes ===
std::vector<std::vector<TileInfo>> WorldGenerator::generate()
{
    // base noise scales
    const float baseScale = 0.015f;
    const float continentScale = 0.004f;  // very low frequency = big continents
    const float moistureScale = 0.025f;

    std::vector<std::vector<float>> elevation(height, std::vector<float>(width));
    std::vector<std::vector<float>> moisture(height, std::vector<float>(width));

    // extra uplift used for mountains / rain shadow
    std::vector<std::vector<float>> tectonicBoost(height, std::vector<float>(width, 0.0f));

    // clean “where is the plate boundary” mask for debug overlay
    std::vector<std::vector<float>> tectonicMask(height, std::vector<float>(width, 0.0f));

    // ----------------------------------------------------------------
    // 1) Build elevation and moisture using a mix of noises
    // ----------------------------------------------------------------
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float nx = static_cast<float>(x) * baseScale;
            float ny = static_cast<float>(y) * baseScale;

            // medium-scale terrain detail
            float base = perlin(nx + (m_seed & 0xFFFF),
                ny + ((m_seed >> 16) & 0xFFFF),
                static_cast<int>(m_seed ^ 0xA5A5));

            float sub = perlin(nx * 1.7f + (m_seed & 0x7FFF),
                ny * 1.7f + ((m_seed >> 8) & 0x7FFF),
                static_cast<int>(m_seed ^ 0x5A5A));

            // ridged mountains
            float rnx = nx * 2.8f;
            float rny = ny * 2.8f;
            float ridgeNoise = perlin(rnx + 123.456f,
                rny - 78.9f,
                static_cast<int>(m_seed ^ 0xFF00)); // 0..1
            ridgeNoise = 2.0f * ridgeNoise - 1.0f;     // -1..1
            ridgeNoise = 1.0f - std::fabs(ridgeNoise); // inverted abs
            float ridge = ridgeNoise * ridgeNoise;      // sharpen ridges

            // very low-frequency "continent" noise
            float cx = static_cast<float>(x) * continentScale;
            float cy = static_cast<float>(y) * continentScale;
            float continent = perlin(cx + 1000.0f,
                cy - 1000.0f,
                static_cast<int>(m_seed ^ 0x1234));

            // optional radial falloff so edges tend to be ocean
            float dx = (static_cast<float>(x) - static_cast<float>(width) * 0.5f)
                / (static_cast<float>(width) * 0.5f);
            float dy = (static_cast<float>(y) - static_cast<float>(height) * 0.5f)
                / (static_cast<float>(height) * 0.5f);
            float radial = 1.0f - std::min(1.0f, std::sqrt(dx * dx + dy * dy));

            // combine pieces into final elevation
            float e = base * 0.40f
                + sub * 0.20f
                + continent * 0.40f
                + ridge * 0.15f   // less ridged noise
                + radial * 0.05f;  // slightly weaker edge boost

            elevation[y][x] = e;

            // moisture: mix of several frequencies so forests aren't one big blob
            float mx = nx * (1.5f * moistureScale);
            float my = ny * (1.5f * moistureScale);

            float m1 = perlin(mx + 200.0f, my - 200.0f,
                static_cast<int>(m_seed ^ 0x3333));
            float m2 = perlin(mx * 2.3f + 400.0f, my * 2.3f + 50.0f,
                static_cast<int>(m_seed ^ 0x8888));
            float m3 = perlin(mx * 4.1f - 100.0f, my * 4.1f + 600.0f,
                static_cast<int>(m_seed ^ 0x9999));

            // weighted blend: large-scale + medium + small details
            float m = m1 * 0.6f + m2 * 0.3f + m3 * 0.1f;
            moisture[y][x] = m;
        }
    }

    // ----------------------------------------------------------------
    // 2) Normalize & smooth elevation and moisture
    // ----------------------------------------------------------------
    auto normalize = [&](std::vector<std::vector<float>>& grid) {
        float lo = 1e9f;
        float hi = -1e9f;
        for (auto& r : grid)
            for (float v : r) { lo = std::min(lo, v); hi = std::max(hi, v); }

        float range = (hi - lo);
        if (range <= 1e-6f) range = 1.0f;

        for (auto& r : grid)
            for (float& v : r) v = (v - lo) / range;
        };

    normalize(elevation);
    normalize(moisture);

    auto smooth = [&](std::vector<std::vector<float>>& grid) {
        auto copy = grid;
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float sum = 0.0f;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        sum += copy[y + dy][x + dx];
                grid[y][x] = sum / 9.0f;
            }
        }
        };

    // a couple of light blurs
    smooth(elevation);
    smooth(moisture);
    smooth(elevation);

    // ----------------------------------------------------------------
    // 2.5) Tectonic-style mountain belts with many curved plates
    // ----------------------------------------------------------------
    {
        // Roughly like Earth: several major + minor plates
        const int numPlates = 14;  // increase for more, decrease for fewer

        struct Plate {
            float x;
            float y;
            float sizeFactor; // >1 = bigger plate, <1 = smaller
        };

        std::mt19937 rng(m_seed ^ 0xBEEF1234u);
        std::uniform_real_distribution<float> randX(0.0f, static_cast<float>(width));
        std::uniform_real_distribution<float> randY(0.0f, static_cast<float>(height));
        std::uniform_real_distribution<float> randSize(0.6f, 1.4f); // vary plate size

        std::vector<Plate> plates(numPlates);
        for (int i = 0; i < numPlates; ++i) {
            plates[i].x = randX(rng);
            plates[i].y = randY(rng);
            plates[i].sizeFactor = randSize(rng);
        }

        // For each tile: closest plate (in a warped, noisy coordinate space)
        std::vector<std::vector<int>> plateId(height,
            std::vector<int>(width, 0));

        const float warpScale = 0.02f; // lower = smoother, higher = more wiggly
        const float warpAmp = 15.0f; // how far (in tiles) to bend boundaries

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {

                // Domain warping: bend space with Perlin so borders curve
                float wx = static_cast<float>(x);
                float wy = static_cast<float>(y);

                float n1 = perlin(wx * warpScale, wy * warpScale,
                    static_cast<int>(m_seed ^ 0x9999));
                float n2 = perlin(wx * warpScale + 1000.0f, wy * warpScale - 500.0f,
                    static_cast<int>(m_seed ^ 0xABCD));

                wx += (n1 * 2.0f - 1.0f) * warpAmp;
                wy += (n2 * 2.0f - 1.0f) * warpAmp;

                float bestScore = 1e9f;
                int   bestIdx = 0;

                for (int i = 0; i < numPlates; ++i) {
                    float dx = wx - plates[i].x;
                    float dy = wy - plates[i].y;
                    float d2 = dx * dx + dy * dy;

                    // scale distance so some plates are larger/smaller
                    float s = plates[i].sizeFactor;
                    d2 /= (s * s);

                    if (d2 < bestScore) {
                        bestScore = d2;
                        bestIdx = i;
                    }
                }

                plateId[y][x] = bestIdx;
            }
        }

        // Compute plate areas so we can ignore tiny "microplates"
        std::vector<int> plateArea(numPlates, 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int id = plateId[y][x];
                if (id >= 0 && id < numPlates) {
                    plateArea[id]++;
                }
            }
        }

        int avgArea = (width * height) / std::max(1, numPlates);
        // plates smaller than about half the average will NOT get big mountain belts
        int minPlateAreaForMountains = avgArea / 2;

        // ------------------------------------------------------------
        // Build tectonicMask: every place where plateId changes, then
        // thicken it so it shows up as a 4–5 tile wide belt.
        // This is ONLY for the debug view (does not affect terrain).
        // ------------------------------------------------------------

        // First, a 1-tile-wide boundary map.
        std::vector<std::vector<unsigned char>> boundary(
            height, std::vector<unsigned char>(width, 0));

        for (int y = 1; y < height - 1; ++y)
        {
            for (int x = 1; x < width - 1; ++x)
            {
                int myPlate = plateId[y][x];

                bool onBoundary = false;
                const int dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

                for (int d = 0; d < 4 && !onBoundary; ++d)
                {
                    int nx = x + dirs[d][0];
                    int ny = y + dirs[d][1];

                    if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                        continue;

                    if (plateId[ny][nx] != myPlate)
                        onBoundary = true;
                }

                if (onBoundary)
                    boundary[y][x] = 1;
            }
        }

        // Then thicken the boundary into tectonicMask by looking
        // up to 2 tiles away from each cell. That gives us roughly
        // 4–5 tiles of visible plate boundary in the overlay.
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                bool nearBoundary = false;

                for (int dy = -2; dy <= 2 && !nearBoundary; ++dy)
                {
                    for (int dx = -2; dx <= 2 && !nearBoundary; ++dx)
                    {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                            continue;

                        if (boundary[ny][nx])
                            nearBoundary = true;
                    }
                }

                if (nearBoundary)
                    tectonicMask[y][x] = 1.0f;
            }
        }

        // Add a broader ridge on plate boundaries,
        // and remember where tectonics boosted the height.
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int myPlate = plateId[y][x];

                // skip tiny microplates so they don't form circular belts
                if (plateArea[myPlate] < minPlateAreaForMountains)
                    continue;

                int differentCount = 0;
                bool hasBigNeighbor = false;

                // look up to 2 tiles away -> thicker belts
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;

                        int nbPlate = plateId[ny][nx];
                        if (nbPlate != myPlate) {
                            ++differentCount;
                            // only count it as a "real" boundary if the neighbor
                            // is also a reasonably large plate
                            if (plateArea[nbPlate] >= minPlateAreaForMountains) {
                                hasBigNeighbor = true;
                            }
                        }
                    }
                }

                // not near a meaningful boundary between two big plates
                if (differentCount == 0 || !hasBigNeighbor)
                    continue;

                // more neighbors from other plates => closer to the core of the belt
                float boundaryFactor = std::min(1.0f, differentCount / 8.0f);

                // --- NEW: mark boundary in tectonicMask for debug overlay ---
                // this happens regardless of elevation (so you see it under the ocean too)
                tectonicMask[y][x] = std::max(tectonicMask[y][x], boundaryFactor);

                float e = elevation[y][x];

                // only bump tiles that are already somewhat high
                if (e < 0.50f || e > 0.95f)
                    continue;

                // noisy ridge height
                float n = noise(x, y, m_seed ^ 0x7777u); // 0..1
                float extra = (0.06f + 0.18f * n) * boundaryFactor; // 0.06..0.24 scaled
                elevation[y][x] += extra;
                tectonicBoost[y][x] += extra; // remember tectonic uplift
            }
        }

        // Re-normalize and lightly smooth after adding belts
        normalize(elevation);
        smooth(elevation);

        // Blur uplift a bit for nicer ranges (overlay uses tectonicMask, not this)
        smooth(tectonicBoost);  // affects mountains / rain shadow
        // no smoothing of tectonicMask here – we already thickened it explicitly
    }

    // ----------------------------------------------------------------
    // 2.6) Climate shaping: latitude bands + mountain rain shadows
    // ----------------------------------------------------------------
    {
        // 1) Latitude-based moisture: wetter near equator, drier toward poles.
        //    y = 0 is "north", y = height-1 is "south"; equator ~ middle.
        for (int y = 0; y < height; ++y)
        {
            float lat = static_cast<float>(y) / std::max(1, height - 1); // 0..1
            float distFromEquator = std::fabs(lat - 0.5f);               // 0 at eq.

            // Basic pattern: wettest at equator, gradually drier toward poles.
            float latFactor = 0.5f + distFromEquator * 0.8f;
            if (latFactor < 0.35f) latFactor = 0.35f;
            if (latFactor > 1.0f)  latFactor = 1.0f;

            for (int x = 0; x < width; ++x)
            {
                moisture[y][x] *= latFactor;
            }
        }

        // 2) Orographic rain shadow: assume prevailing wind from west -> east.
        //    West slopes stay wetter, leeward (east) side of big ranges gets drier.
        for (int y = 0; y < height; ++y)
        {
            float shadow = 0.0f;

            for (int x = 0; x < width; ++x)
            {
                float e = elevation[y][x];
                float tect = tectonicBoost[y][x];

                // If we hit a significant mountain belt, increase "shadow".
                if (tect > 0.03f && e > 0.65f)
                {
                    shadow += tect * 3.0f;              // how strong the shadow starts
                    if (shadow > 2.0f) shadow = 2.0f;   // clamp
                }

                // Apply shadow: reduce moisture behind mountains.
                // (0.0 = no effect, 2.0 = strong drying)
                moisture[y][x] -= shadow * 0.35f;

                // Shadow gradually weakens as we move east.
                shadow *= 0.96f;
            }
        }

        // Moisture may have gone outside 0..1; renormalize and lightly smooth.
        normalize(moisture);
        smooth(moisture);
        }

    // ----------------------------------------------------------------
    // 3) First pass: mark ocean vs land (no beaches yet)
    // ----------------------------------------------------------------
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float e = elevation[y][x];
            float m = moisture[y][x];

            map[y][x].elevation = e;
            map[y][x].moisture = m;
            map[y][x].tectonic = tectonicMask[y][x];  // overlay only cares about boundary mask

            if (e < 0.42f) {                 // was 0.35f
                map[y][x].type = TileType::Water;  // deep ocean / sea
            }
            else {
                map[y][x].type = TileType::Grass;  // provisional "generic land"
            }
        }
    }

    // ----------------------------------------------------------------
    // 4) Second pass: 1-tile thick beaches along coasts
    // ----------------------------------------------------------------
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (map[y][x].type != TileType::Grass)
                continue; // only land can become beach

            bool nearWater = false;
            for (int dy = -1; dy <= 1 && !nearWater; ++dy) {
                for (int dx = -1; dx <= 1 && !nearWater; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                        continue;
                    if (map[ny][nx].type == TileType::Water)
                        nearWater = true;
                }
            }

            if (nearWater) {
                map[y][x].type = TileType::Beach;
            }
        }
    }

    // 5) Third pass: inland biomes
        //    Here we take the normalized elevation + moisture and a simple
        //    latitude-based temperature model and turn all remaining generic
        //    "Grass" tiles into proper biomes (desert, jungle, tundra, snow, ...).

        // Precompute a very simple temperature field: 0 = freezing polar, 1 = hot equator.
        // y = 0 is "north", y = height-1 is "south"; equator lives roughly in the middle.
    std::vector<std::vector<float>> temperature(height, std::vector<float>(width, 0.0f));
    for (int y = 0; y < height; ++y)
    {
        float lat = static_cast<float>(y) / std::max(1, height - 1);      // 0..1
        float distFromEquator = std::fabs(lat - 0.5f) * 2.0f;             // 0 at eq, 1 at poles
        float baseTemp = 1.0f - distFromEquator * 0.8f;                     // hot at eq, cold at poles

        for (int x = 0; x < width; ++x)
        {
            float e = elevation[y][x]; // 0..1
            float t = baseTemp - e * 0.6f;                                // colder with height
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            temperature[y][x] = t;
        }
    }

    // Assign biome tiles for any land that is still plain "Grass".
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (map[y][x].type != TileType::Grass)
                continue; // leave Water/Beach/River/Lake/etc. as they are

            float e = elevation[y][x];
            float m = moisture[y][x];
            float tect = tectonicBoost[y][x];
            float temp = temperature[y][x];

            // Small noise on the moisture to avoid perfectly straight biome borders.
            float jitter = noise(x, y, m_seed ^ 0xBABAu) * 0.08f - 0.04f; // -0.04..0.04
            float mJ = m + jitter;
            if (mJ < 0.0f) mJ = 0.0f;
            if (mJ > 1.0f) mJ = 1.0f;

            TileType biome = TileType::Grass;

            // --- Cold & polar regions: snow + tundra bands ---
            // Strongly reduced: only the very coldest tiles are snow/tundra.
            if (temp < 0.04f)
            {
                // Proper polar snow caps
                if (e > 0.75f && tect > 0.02f)
                {
                    biome = TileType::Mountain;   // tall, very cold mountains
                }
                else if (e > 0.58f)
                {
                    biome = TileType::SnowHill;   // snowy hills / ridges
                }
                else
                {
                    biome = TileType::Snow;       // flat snow fields
                }
            }
            else if (temp < 0.12f)
            {
                // Narrow tundra band around the poles.
                if (e > 0.75f && tect > 0.02f)
                {
                    biome = TileType::Mountain;
                }
                else
                {
                    biome = TileType::Tundra;
                }
            }
            else
            {
                // --- Temperate + tropical belts ---

                // Tropics / subtropics: wider band
                if (temp > 0.55f)
                {
                    // Much more desert in the hot belt
                    // 0.0 .. 0.45 -> desert
                    // 0.45 .. 0.65 -> savanna / grass
                    // 0.65 .. 1.0 -> jungle
                    if (mJ < 0.45f)
                        biome = TileType::Desert;
                    else if (mJ > 0.65f)
                        biome = TileType::Jungle;
                    else
                        biome = TileType::Grass;   // savanna / mixed
                }
                else
                {
                    // Temperate: still allow decent desert/steppe area
                    // 0.0 .. 0.35 -> desert / steppe
                    // 0.35 .. 0.70 -> grass
                    // 0.70 .. 1.0 -> forest
                    if (mJ < 0.35f)
                        biome = TileType::Desert;   // steppe / cold desert feeling
                    else if (mJ > 0.70f)
                        biome = TileType::Forest;   // classic temperate forests
                    else
                        biome = TileType::Grass;    // plains / farmland
                }

                // Highlands in these warmer belts: hills + mountains override base biome.
                if (e > 0.58f && e < 0.75f)
                {
                    biome = TileType::Hill;
                }
                else if (e >= 0.75f)
                {
                    if (tect > 0.02f)
                        biome = TileType::Mountain;
                    else
                        biome = TileType::Hill;
                }
            }

            map[y][x].type = biome;
        }
    }

    // 5.5) Swamps: very wet, warm lowlands touching rivers / lakes / coasts.
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            TileType t = map[y][x].type;
            if (t == TileType::Water || t == TileType::River || t == TileType::Lake)
                continue; // already open water
            if (t == TileType::Beach || t == TileType::Mountain)
                continue; // keep beaches & peaks as they are

            float temp = temperature[y][x];
            float e = elevation[y][x];
            float m = moisture[y][x];

            // Only in relatively warm & very wet tiles near sea level.
            if (temp < 0.40f || temp > 0.90f)
                continue;
            if (m < 0.70f)
                continue;
            if (e < 0.43f || e > 0.60f)
                continue;

            bool nearWater = false;
            for (int dy = -1; dy <= 1 && !nearWater; ++dy)
            {
                for (int dx = -1; dx <= 1 && !nearWater; ++dx)
                {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                        continue;
                    TileType nt = map[ny][nx].type;
                    if (nt == TileType::Water || nt == TileType::River || nt == TileType::Lake)
                        nearWater = true;
                }
            }

            if (nearWater)
            {
                map[y][x].type = TileType::Swamp;
            }
        }
    }

    // ----------------------------------------------------------------
    // 6) Rivers & lakes: use flow accumulation to build networks
    // ----------------------------------------------------------------
    unsigned int rngSeed = m_seed ^ 0xdeadbeefu;
    addRiversAndLakes(elevation, rngSeed);

    return map;
}

// === Rivers & Lakes ===
// Uses flow-direction + accumulation so rivers form networks and reach the sea
void WorldGenerator::addRiversAndLakes(const std::vector<std::vector<float>>& elevation,
    unsigned int rngSeed)
{
    struct Vec2 { int x; int y; };

    // flowDir: for each tile, where water flows next
    std::vector<std::vector<Vec2>> flowDir(height,
        std::vector<Vec2>(width, { 0, 0 }));

    // flowAcc: how many tiles drain through this tile
    std::vector<std::vector<int>> flowAcc(height,
        std::vector<int>(width, 0));

    // ------------------------------------------------------------
    // 1) Compute flow direction (steepest descent)
    // ------------------------------------------------------------
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // ocean tiles: no downhill, they are sinks
            if (map[y][x].type == TileType::Water) {
                flowDir[y][x] = { x, y };
                continue;
            }

            int bestX = x;
            int bestY = y;
            float best = elevation[y][x];

            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                        continue;

                    float e = elevation[ny][nx];
                    if (e < best) {
                        best = e;
                        bestX = nx;
                        bestY = ny;
                    }
                }
            }

            flowDir[y][x] = { bestX, bestY };
        }
    }

    // ------------------------------------------------------------
    // 2) Flow accumulation
    // ------------------------------------------------------------
    int maxSteps = width + height + 10;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int cx = x;
            int cy = y;

            for (int step = 0; step < maxSteps; ++step)
            {
                Vec2 d = flowDir[cy][cx];
                if (d.x == cx && d.y == cy)
                    break; // local sink or ocean

                flowAcc[cy][cx]++;

                cx = d.x;
                cy = d.y;

                if (cx < 0 || cy < 0 || cx >= width || cy >= height)
                    break;

                if (map[cy][cx].type == TileType::Water) {
                    flowAcc[cy][cx]++;
                    break; // reached ocean
                }
            }
        }
    }

    // ------------------------------------------------------------
// 3) Choose river sources: high elevation + good accumulation
//     (lots of sources, prefer the biggest basins)
// ------------------------------------------------------------
    std::vector<Vec2> candidates;
    candidates.reserve(width* height / 5);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (map[y][x].type == TileType::Water)
                continue; // no rivers starting in ocean

            float e = elevation[y][x];
            int   acc = flowAcc[y][x];

            // looser thresholds -> many more possible river sources
            if (e > 0.50f && acc > 25) {
                candidates.push_back({ x, y });
            }
        }
    }

    if (candidates.empty())
        return;

    // Prefer tiles with the highest accumulated flow first,
    // so we get long, main rivers across continents.
    auto score = [&](const Vec2& v) {
        return flowAcc[v.y][v.x];
        };
    std::sort(candidates.begin(), candidates.end(),
        [&](const Vec2& a, const Vec2& b) {
            return score(a) > score(b);  // highest flow first
        });

    int cells = width * height;
    // Many more rivers overall
    int targetRivers = cells / 8000;        // ~80 on a 640k-cell map
    if (targetRivers < 40)  targetRivers = 40;
    if (targetRivers > 120) targetRivers = 120;

    // Shuffle the top chunk a bit so they don't all run parallel
    std::mt19937 rng(rngSeed);
    std::size_t topCount = std::min<std::size_t>(
        candidates.size(),
        static_cast<std::size_t>(targetRivers * 3));
    std::shuffle(candidates.begin(),
        candidates.begin() + topCount,
        rng);

    // ------------------------------------------------------------
    // 4) Trace rivers from sources downstream to the sea
    // ------------------------------------------------------------
    int riversCreated = 0;

    for (Vec2 src : candidates)
    {
        if (riversCreated >= targetRivers)
            break;

        int cx = src.x;
        int cy = src.y;

        // skip if this source is already on a river/ocean
        if (map[cy][cx].type == TileType::Water ||
            map[cy][cx].type == TileType::River)
            continue;

        bool madeRiver = false;

        for (int step = 0; step < maxSteps * 2; ++step)
        {
            if (map[cy][cx].type == TileType::Water) {
                // reached ocean
                break;
            }

            if (map[cy][cx].type == TileType::River) {
                // joined an existing river
                break;
            }

            if (map[cy][cx].type == TileType::Lake) {
                // already a lake: stop here
                break;
            }

            // carve the river channel
            map[cy][cx].type = TileType::River;
            madeRiver = true;

            Vec2 d = flowDir[cy][cx];
            if (d.x == cx && d.y == cy) {
                // trapped: create a small lake around this sink
                for (int ly = -1; ly <= 1; ++ly)
                {
                    for (int lx = -1; lx <= 1; ++lx)
                    {
                        int nx = cx + lx;
                        int ny = cy + ly;
                        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                            continue;

                        if (map[ny][nx].type != TileType::Water) {
                            map[ny][nx].type = TileType::Lake;
                        }
                    }
                }
                break;
            }

            cx = d.x;
            cy = d.y;
        }

        if (madeRiver)
            ++riversCreated;
    }
}

