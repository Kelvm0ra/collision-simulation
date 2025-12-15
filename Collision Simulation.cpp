#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <random>
#include <memory>
#include <sstream>
#include <iomanip>

// Struktur untuk merepresentasikan bola
struct Ball {
    sf::CircleShape shape;
    sf::Vector2f velocity;
    float radius;

    Ball(float x, float y, float r, sf::Color color, sf::Vector2f vel) {
        radius = r;
        shape.setRadius(radius);
        shape.setPosition(sf::Vector2f(x, y));
        shape.setFillColor(color);
        velocity = vel;
    }

    void update(float deltaTime, sf::Vector2u windowSize) {
        shape.move(velocity * deltaTime);

        sf::Vector2f pos = shape.getPosition();
        if (pos.x <= 0 || pos.x + 2 * radius >= windowSize.x) {
            velocity.x = -velocity.x;
        }
        if (pos.y <= 0 || pos.y + 2 * radius >= windowSize.y) {
            velocity.y = -velocity.y;
        }

        if (pos.x < 0) shape.setPosition(sf::Vector2f(0, pos.y));
        if (pos.x + 2 * radius > windowSize.x) shape.setPosition(sf::Vector2f(windowSize.x - 2 * radius, pos.y));
        if (pos.y < 0) shape.setPosition(sf::Vector2f(pos.x, 0));
        if (pos.y + 2 * radius > windowSize.y) shape.setPosition(sf::Vector2f(pos.x, windowSize.y - 2 * radius));
    }

    sf::Vector2f getCenter() const {
        return shape.getPosition() + sf::Vector2f(radius, radius);
    }
};

// Enum untuk memilih algoritma
enum CollisionAlgorithm {
    BRUTE_FORCE,
    QUADTREE
};

// Fungsi helper untuk cek intersects
bool intersects(const sf::FloatRect& a, const sf::FloatRect& b) {
    return a.position.x < b.position.x + b.size.x && a.position.x + a.size.x > b.position.x &&
           a.position.y < b.position.y + b.size.y && a.position.y + a.size.y > b.position.y;
}

// Struktur untuk Quadtree
struct Quadtree {
    sf::FloatRect bounds;
    std::vector<Ball*> balls;
    std::unique_ptr<Quadtree> children[4];
    int capacity;
    bool divided;

    Quadtree(sf::FloatRect b, int cap) : bounds(b), capacity(cap), divided(false) {}

    void subdivide() {
        float x = bounds.position.x;
        float y = bounds.position.y;
        float w = bounds.size.x / 2;
        float h = bounds.size.y / 2;

        children[0] = std::make_unique<Quadtree>(sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(w, h)), capacity);
        children[1] = std::make_unique<Quadtree>(sf::FloatRect(sf::Vector2f(x + w, y), sf::Vector2f(w, h)), capacity);
        children[2] = std::make_unique<Quadtree>(sf::FloatRect(sf::Vector2f(x, y + h), sf::Vector2f(w, h)), capacity);
        children[3] = std::make_unique<Quadtree>(sf::FloatRect(sf::Vector2f(x + w, y + h), sf::Vector2f(w, h)), capacity);

        divided = true;
    }

    bool insert(Ball* ball) {
        if (!bounds.contains(ball->getCenter())) return false;

        if (balls.size() < capacity) {
            balls.push_back(ball);
            return true;
        }

        if (!divided) subdivide();

        for (auto& child : children) {
            if (child->insert(ball)) return true;
        }

        return false;
    }

    void query(sf::FloatRect range, std::vector<Ball*>& found) {
        if (!intersects(bounds, range)) return;

        for (auto ball : balls) {
            if (range.contains(ball->getCenter())) {
                found.push_back(ball);
            }
        }

        if (divided) {
            for (auto& child : children) {
                child->query(range, found);
            }
        }
    }
};

// Fungsi Brute Force dengan counter
int handleBallCollisionsBruteForce(std::vector<Ball>& balls) {
    int checks = 0;
    for (size_t i = 0; i < balls.size(); ++i) {
        for (size_t j = i + 1; j < balls.size(); ++j) {
            checks++; // Hitung setiap pemeriksaan
            sf::Vector2f pos1 = balls[i].getCenter();
            sf::Vector2f pos2 = balls[j].getCenter();
            sf::Vector2f diff = pos1 - pos2;
            float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            float minDistance = balls[i].radius + balls[j].radius;

            if (distance < minDistance) {
                sf::Vector2f temp = balls[i].velocity;
                balls[i].velocity = balls[j].velocity;
                balls[j].velocity = temp;

                sf::Vector2f overlap = (minDistance - distance) * 0.5f * (diff / distance);
                balls[i].shape.move(overlap);
                balls[j].shape.move(-overlap);
            }
        }
    }
    return checks;
}

// Fungsi Quadtree dengan counter
int handleBallCollisionsQuadtree(std::vector<Ball>& balls, sf::Vector2u windowSize) {
    int checks = 0;
    Quadtree qt(sf::FloatRect(sf::Vector2f(0, 0), sf::Vector2f(windowSize.x, windowSize.y)), 4);

    for (auto& ball : balls) {
        qt.insert(&ball);
    }

    // Set untuk melacak pasangan yang sudah dicek agar tidak double-check
    std::vector<std::pair<Ball*, Ball*>> checkedPairs;

    for (size_t i = 0; i < balls.size(); ++i) {
        std::vector<Ball*> neighbors;
        sf::FloatRect queryRange(sf::Vector2f(balls[i].getCenter().x - balls[i].radius * 2, 
                                              balls[i].getCenter().y - balls[i].radius * 2),
                                 sf::Vector2f(balls[i].radius * 4, balls[i].radius * 4));
        qt.query(queryRange, neighbors);

        for (auto other : neighbors) {
            if (other == &balls[i]) continue;
            
            // Cek apakah pasangan ini sudah pernah dicek
            bool alreadyChecked = false;
            for (const auto& pair : checkedPairs) {
                if ((pair.first == &balls[i] && pair.second == other) ||
                    (pair.first == other && pair.second == &balls[i])) {
                    alreadyChecked = true;
                    break;
                }
            }
            
            if (alreadyChecked) continue;
            checkedPairs.push_back({&balls[i], other});
            
            checks++; // Hitung setiap pemeriksaan
            sf::Vector2f pos1 = balls[i].getCenter();
            sf::Vector2f pos2 = other->getCenter();
            sf::Vector2f diff = pos1 - pos2;
            float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            float minDistance = balls[i].radius + other->radius;

            if (distance < minDistance && distance > 0) {
                sf::Vector2f temp = balls[i].velocity;
                balls[i].velocity = other->velocity;
                other->velocity = temp;

                sf::Vector2f overlap = (minDistance - distance) * 0.5f * (diff / distance);
                balls[i].shape.move(overlap);
                other->shape.move(-overlap);
            }
        }
    }
    return checks;
}

int main() {
    CollisionAlgorithm algorithm = QUADTREE;

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(1200, 800), 32), "Collision Simulation - Performance Comparison");
    window.setFramerateLimit(60);

    sf::Font font;
    bool fontLoaded = false;
    if (!fontLoaded) fontLoaded = font.openFromFile("arial.ttf");
    if (!fontLoaded) fontLoaded = font.openFromFile("C:/Windows/Fonts/arial.ttf");
    if (!fontLoaded) fontLoaded = font.openFromFile("C:/Windows/Fonts/Arial.ttf");

    std::unique_ptr<sf::Text> textPtr = nullptr;
    if (fontLoaded) {
        textPtr = std::make_unique<sf::Text>(font, "", 20);
        textPtr->setFillColor(sf::Color::White);
        textPtr->setPosition(sf::Vector2f(10, 10));
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDistX(50, 1150);
    std::uniform_real_distribution<float> posDistY(50, 750);
    std::uniform_real_distribution<float> velDist(-200, 200);
    std::uniform_int_distribution<int> colorDist(0, 255);

    std::vector<Ball> balls;
    // Mulai dengan 50 bola
    for (int i = 0; i < 50; ++i) {
        float x = posDistX(gen);
        float y = posDistY(gen);
        float radius = 15.0f;
        sf::Color color(colorDist(gen), colorDist(gen), colorDist(gen));
        sf::Vector2f velocity(velDist(gen), velDist(gen));
        balls.emplace_back(x, y, radius, color, velocity);
    }

    sf::Clock clock;
    sf::Clock fpsTimer;
    int frameCount = 0;
    float fps = 0.0f;
    int collisionChecks = 0;
    float collisionTime = 0.0f;

    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (event->is<sf::Event::KeyPressed>()) {
                auto keyEvent = event->getIf<sf::Event::KeyPressed>();
                // Toggle algoritma dengan C
                if (keyEvent->code == sf::Keyboard::Key::C) {
                    algorithm = (algorithm == BRUTE_FORCE) ? QUADTREE : BRUTE_FORCE;
                }
                // Tambah bola dengan tombol Up
                if (keyEvent->code == sf::Keyboard::Key::Up) {
                    for (int i = 0; i < 10; ++i) {
                        float x = posDistX(gen);
                        float y = posDistY(gen);
                        float radius = 15.0f;
                        sf::Color color(colorDist(gen), colorDist(gen), colorDist(gen));
                        sf::Vector2f velocity(velDist(gen), velDist(gen));
                        balls.emplace_back(x, y, radius, color, velocity);
                    }
                }
                // Kurangi bola dengan tombol Down
                if (keyEvent->code == sf::Keyboard::Key::Down) {
                    if (balls.size() >= 10) {
                        balls.erase(balls.end() - 10, balls.end());
                    }
                }
            }
        }

        float deltaTime = clock.restart().asSeconds();

        // Update posisi bola
        for (auto& ball : balls) {
            ball.update(deltaTime, window.getSize());
        }

        // Hitung waktu collision detection
        sf::Clock collisionClock;
        if (algorithm == BRUTE_FORCE) {
            collisionChecks = handleBallCollisionsBruteForce(balls);
        } else {
            collisionChecks = handleBallCollisionsQuadtree(balls, window.getSize());
        }
        collisionTime = collisionClock.getElapsedTime().asSeconds() * 1000.0f; // dalam ms

        // Hitung FPS
        frameCount++;
        if (fpsTimer.getElapsedTime().asSeconds() >= 1.0f) {
            fps = frameCount / fpsTimer.getElapsedTime().asSeconds();
            frameCount = 0;
            fpsTimer.restart();
        }

        // Update teks dengan informasi lengkap
        if (textPtr) {
            std::ostringstream ss;
            std::string mode = (algorithm == BRUTE_FORCE) ? "BRUTE FORCE" : "QUADTREE";
            ss << "=== COLLISION DETECTION ===\n";
            ss << "Mode: " << mode << "\n";
            ss << "Balls: " << balls.size() << "\n";
            ss << "FPS: " << std::fixed << std::setprecision(1) << fps << "\n";
            ss << "Collision Checks: " << collisionChecks << "\n";
            ss << "Collision Time: " << std::fixed << std::setprecision(3) << collisionTime << " ms\n";
            ss << "\n[C] Toggle Algorithm\n";
            ss << "[UP] Add 10 Balls\n";
            ss << "[DOWN] Remove 10 Balls";
            textPtr->setString(ss.str());
        }

        window.clear(sf::Color::Black);
        for (const auto& ball : balls) {
            window.draw(ball.shape);
        }
        if (textPtr) {
            window.draw(*textPtr);
        }
        window.display();
    }

    return 0;
}