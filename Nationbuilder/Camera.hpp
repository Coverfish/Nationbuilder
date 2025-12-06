#pragma once
#include <SFML/Graphics.hpp>

class Camera {
public:
    Camera(sf::RenderWindow& window, float worldWidth, float worldHeight);
    void handleEvent(const sf::Event& event);
    void update(float dt, sf::RenderWindow& window);
    void centerOnMap();
    sf::View& getView();
    float getZoomLevel() const { return m_zoomLevel; }

private:
    sf::View m_view;
    float m_cameraSpeed;
    float m_zoomLevel;
    float m_targetZoom;
    float m_zoomSpeed;
    float m_edgeScrollSpeed;
    int m_edgeSize;
    float m_worldWidth, m_worldHeight;
    bool m_windowFocused;
};




