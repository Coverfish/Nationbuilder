#include <SFML/Graphics.hpp>
#include "Map.hpp"
#include "Camera.hpp"

int main() {
    sf::RenderWindow window(sf::VideoMode::getDesktopMode(),
        "Nation Builder Engine", sf::Style::Fullscreen);
    window.setFramerateLimit(300);

    const int TILE_SIZE = 32;
    const int MAP_WIDTH = 1067;
    const int MAP_HEIGHT = 600;

    // Create map (with configurable cache tile size)
    Map map(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE, 1024);

    Camera camera(window, map.getWorldWidth(), map.getWorldHeight());
    camera.centerOnMap();

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape)
                window.close();

            camera.handleEvent(event);

            // Debug overlays: H = height, M = moisture, T = tectonic belts
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::H) {
                    map.toggleHeightOverlay();
                }
                else if (event.key.code == sf::Keyboard::M) {
                    map.toggleMoistureOverlay();
                }
                else if (event.key.code == sf::Keyboard::T) {
                    map.toggleTectonicsOverlay();
                }
            }
        }

        camera.update(dt, window);

        window.clear(sf::Color(40, 40, 50));
        window.setView(camera.getView());

        map.draw(window, camera.getZoomLevel());

        window.display();
    }

    return 0;
}



