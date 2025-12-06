// Camera.cpp
#include "Camera.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <algorithm>

Camera::Camera(sf::RenderWindow& window, float worldWidth, float worldHeight)
    : m_cameraSpeed(700.f),
    m_zoomLevel(1.f),
    m_targetZoom(1.f),
    m_zoomSpeed(5.f),
    m_edgeScrollSpeed(400.f),
    m_edgeSize(20),
    m_worldWidth(worldWidth),
    m_worldHeight(worldHeight),
    m_windowFocused(true)
{
    m_view = sf::View(sf::FloatRect(0.f, 0.f,
        static_cast<float>(window.getSize().x),
        static_cast<float>(window.getSize().y)));

    // Start zoomed out and centered
    m_zoomLevel = m_targetZoom = 6.5f;
    m_view.setSize(window.getSize().x * m_zoomLevel,
        window.getSize().y * m_zoomLevel);
    centerOnMap();
}

void Camera::handleEvent(const sf::Event& event) {
    // Track focus changes explicitly
    if (event.type == sf::Event::GainedFocus) {
        m_windowFocused = true;
    }
    else if (event.type == sf::Event::LostFocus) {
        m_windowFocused = false;
    }

    // Zoom wheel (handle here to avoid losing it)
    if (event.type == sf::Event::MouseWheelScrolled && m_windowFocused) {
        if (event.mouseWheelScroll.delta > 0)
            m_targetZoom *= 0.9f;
        else
            m_targetZoom *= 1.1f;

        if (m_targetZoom < 0.4f) m_targetZoom = 0.4f;
        if (m_targetZoom > 16.5f) m_targetZoom = 16.5f;
    }

    // You can add other camera-related event handling here if needed
}

void Camera::update(float dt, sf::RenderWindow& window) {
    // Smooth zoom interpolation (always update, even if unfocused,
    // but user input only changes m_targetZoom via handleEvent)
    m_zoomLevel += (m_targetZoom - m_zoomLevel) * dt * m_zoomSpeed;
    m_view.setSize(window.getSize().x * m_zoomLevel,
        window.getSize().y * m_zoomLevel);

    float adjustedSpeed = m_cameraSpeed * m_zoomLevel;

    // Keyboard movement (only when focused)
    if (m_windowFocused) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) m_view.move(0.f, -adjustedSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) m_view.move(0.f, adjustedSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) m_view.move(-adjustedSpeed * dt, 0.f);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) m_view.move(adjustedSpeed * dt, 0.f);
    }

    // Edge scrolling and soft mouse-clamp (only when focused)
    if (m_windowFocused) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        sf::Vector2u winSize = window.getSize();

        if (mousePos.x <= m_edgeSize)
            m_view.move(-m_edgeScrollSpeed * dt * m_zoomLevel, 0.f);
        else if (mousePos.x >= static_cast<int>(winSize.x) - m_edgeSize)
            m_view.move(m_edgeScrollSpeed * dt * m_zoomLevel, 0.f);

        if (mousePos.y <= m_edgeSize)
            m_view.move(0.f, -m_edgeScrollSpeed * dt * m_zoomLevel);
        else if (mousePos.y >= static_cast<int>(winSize.y) - m_edgeSize)
            m_view.move(0.f, m_edgeScrollSpeed * dt * m_zoomLevel);

        // Soft lock: clamp mouse inside the window (but only slightly inside)
        int clampedX = std::max(1, std::min(static_cast<int>(winSize.x) - 2, mousePos.x));
        int clampedY = std::max(1, std::min(static_cast<int>(winSize.y) - 2, mousePos.y));

        if (clampedX != mousePos.x || clampedY != mousePos.y)
            sf::Mouse::setPosition({ clampedX, clampedY }, window);
    }

    // Clamp view with half-screen void allowance (same as your working version)
    sf::Vector2f size = m_view.getSize();
    sf::Vector2f center = m_view.getCenter();

    float halfW = size.x / 2.f;
    float halfH = size.y / 2.f;

    float marginW = halfW; // allow half-screen void
    float marginH = halfH;

    float minX = halfW - marginW;
    float minY = halfH - marginH;
    float maxX = m_worldWidth - halfW + marginW;
    float maxY = m_worldHeight - halfH + marginH;

    if (center.x < minX) center.x = minX;
    if (center.y < minY) center.y = minY;
    if (center.x > maxX) center.x = maxX;
    if (center.y > maxY) center.y = maxY;

    m_view.setCenter(center);
}

void Camera::centerOnMap() {
    m_view.setCenter(m_worldWidth / 2.f, m_worldHeight / 2.f);
}

sf::View& Camera::getView() {
    return m_view;
}
