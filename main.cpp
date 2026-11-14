#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <random>
#include <SDL3/SDL.h>
#include "rectangle.h"
#include "circle.h"

constexpr int WINDOW_WIDTH = 700;
constexpr int WINDOW_HEIGHT = 800;
constexpr int ENEMY_SLOTS = 3;
constexpr float VERTICAL_SCROLLING_SPEED = 100.0f;
constexpr int VERTICAL_SCROLLING_GRID_SQUARE_SIZE = 20;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Event currentEvent;
SDL_Color backgroundColor{255, 255, 255, 255};
int windowWidth = WINDOW_WIDTH;
int windowHeight = WINDOW_HEIGHT;
float DT = 0.0f;
bool quit = false;
bool firing = false;
float playerFireTimer = 0.0f;
float spawnTimer = 0.0f;
glm::vec2 mouseMovement{0.0f};
Rectangle player;
std::vector<Rectangle> scrollingGrid;
std::vector<Circle> playerProjectiles;
std::vector<Circle> enemyProjectiles;
std::mt19937 randomEngine{std::random_device{}()};

enum class GameState { Running, Won, Lost };
GameState gameState = GameState::Running;

struct Enemy {
    Rectangle ship;
    int slot = 0;
    float age = 0.0f;
    float nextShot = 1.0f;
};

std::vector<Enemy> enemies;

float randomFloat(float low, float high) {
    return std::uniform_real_distribution<float>(low, high)(randomEngine);
}

void initScollingRectangles() {
    scrollingGrid.clear();
    int count = windowHeight / (2 * VERTICAL_SCROLLING_GRID_SQUARE_SIZE) + 2;

    for (int i = 0; i < count; ++i) {
        for (int side = 0; side < 2; ++side) {
            Rectangle square;
            square.width = square.height = VERTICAL_SCROLLING_GRID_SQUARE_SIZE;
            square.pos = glm::vec2(
                side == 0 ? square.width / 2.0f : windowWidth - square.width / 2.0f,
                windowHeight - square.height / 2.0f - i * 2.0f * square.height
            );
            square.vel = glm::vec2(0.0f, VERTICAL_SCROLLING_SPEED);
            square.acc = glm::vec2(0.0f);
            square.color = SDL_Color{235, 235, 235, 255};
            scrollingGrid.push_back(square);
        }
    }
}

bool initWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL: %s", SDL_GetError());
        return false;
    }

    if (!SDL_CreateWindowAndRenderer("CodeRun - Vertical Shooter", WINDOW_WIDTH,
                                    WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE,
                                    &window, &renderer)) {
        SDL_Log("Window: %s", SDL_GetError());
        return false;
    }

    SDL_SetWindowMinimumSize(window, ENEMY_SLOTS * 50, 50);
    SDL_SetRenderVSync(renderer, 1);
    return true;
}

void clampPlayer() {
    float left = player.width / 2.0f;
    float right = windowWidth - left;
    float top = player.height / 2.0f;
    float bottom = windowHeight - top;

    if (player.pos.x < left) {
        player.pos.x = left;
        player.vel.x = std::max(0.0f, player.vel.x);
    }
    if (player.pos.x > right) {
        player.pos.x = right;
        player.vel.x = std::min(0.0f, player.vel.x);
    }
    if (player.pos.y < top) {
        player.pos.y = top;
        player.vel.y = std::max(0.0f, player.vel.y);
    }
    if (player.pos.y > bottom) {
        player.pos.y = bottom;
        player.vel.y = std::min(0.0f, player.vel.y);
    }
}

void spawnEnemy(int slot) {
    float sliceWidth = static_cast<float>(windowWidth) / ENEMY_SLOTS;
    Enemy enemy;
    enemy.slot = slot;
    enemy.ship.width = enemy.ship.height = 50.0f;
    enemy.ship.pos = glm::vec2(
        randomFloat(slot * sliceWidth + 25.0f, (slot + 1) * sliceWidth - 25.0f),
        -25.0f
    );
    enemy.ship.vel = glm::vec2(0.0f, randomFloat(50.0f, 150.0f));
    enemy.ship.acc = glm::vec2(0.0f);
    enemy.ship.color = SDL_Color{255, 165, 0, 255};
    enemies.push_back(enemy);
}

void resetGame() {
    gameState = GameState::Running;
    firing = false;
    playerFireTimer = 0.0f;
    spawnTimer = 0.0f;
    mouseMovement = glm::vec2(0.0f);
    playerProjectiles.clear();
    enemyProjectiles.clear();
    enemies.clear();

    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    player.width = player.height = 50.0f;
    player.pos = glm::vec2(windowWidth / 2.0f, windowHeight - 25.0f);
    player.vel = player.acc = glm::vec2(0.0f);
    player.color = SDL_Color{0, 255, 0, 255};
    initScollingRectangles();
    for (int slot = 0; slot < ENEMY_SLOTS; ++slot)
        spawnEnemy(slot);
    SDL_SetWindowRelativeMouseMode(window, true);
}

void endGame(GameState result) {
    gameState = result;
    player.color = result == GameState::Won
        ? SDL_Color{0, 0, 255, 255} : SDL_Color{255, 0, 0, 255};
    player.vel = player.acc = glm::vec2(0.0f);
    enemies.clear();
    playerProjectiles.clear();
    enemyProjectiles.clear();
    firing = false;
    SDL_SetWindowRelativeMouseMode(window, false);
    SDL_SetWindowTitle(window, result == GameState::Won
        ? "VICTORIE - R pentru restart" : "INFRANGERE - R pentru restart");
}

void repelProjectiles() {
    for (Circle& projectile : enemyProjectiles) {
        glm::vec2 away = projectile.pos - player.pos;
        float distance = glm::length(away);
        if (distance > 0.0001f)
            projectile.vel += away * (10.0f / distance);
    }
}

void resizeScene() {
    int oldWidth = windowWidth;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    if (windowWidth < ENEMY_SLOTS * 50 || windowHeight < 50)
        return;
    float sliceWidth = static_cast<float>(windowWidth) / ENEMY_SLOTS;
    for (Enemy& enemy : enemies) {
        if (oldWidth > 0)
            enemy.ship.pos.x *= static_cast<float>(windowWidth) / oldWidth;
        enemy.ship.pos.x = std::clamp(enemy.ship.pos.x,
            enemy.slot * sliceWidth + 25.0f,
            (enemy.slot + 1) * sliceWidth - 25.0f);
    }
    clampPlayer();
    initScollingRectangles();
}

void processEvents() {
    mouseMovement = glm::vec2(0.0f);
    while (SDL_PollEvent(&currentEvent)) {
        if (currentEvent.type == SDL_EVENT_QUIT)
            quit = true;
        if (currentEvent.type == SDL_EVENT_KEY_DOWN) {
            if (currentEvent.key.key == SDLK_ESCAPE)
                quit = true;
            if (currentEvent.key.key == SDLK_R && !currentEvent.key.repeat &&
                gameState != GameState::Running) {
                resetGame();
                SDL_SetWindowTitle(window, "CodeRun - Vertical Shooter");
            }
        }
        if (currentEvent.type == SDL_EVENT_WINDOW_RESIZED)
            resizeScene();
        if (gameState != GameState::Running)
            continue;
        if (currentEvent.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            firing = false;
            player.vel = glm::vec2(0.0f);
        }
        if (currentEvent.type == SDL_EVENT_MOUSE_MOTION) {
            mouseMovement.x += currentEvent.motion.xrel;
            mouseMovement.y += currentEvent.motion.yrel;
        }
        if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (currentEvent.button.button == SDL_BUTTON_LEFT)
                firing = true;
            if (currentEvent.button.button == SDL_BUTTON_RIGHT)
                repelProjectiles();
        }
        if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            currentEvent.button.button == SDL_BUTTON_LEFT) {
            firing = false;
            playerFireTimer = 0.0f;
        }
    }
}

Circle makeProjectile(glm::vec2 position, glm::vec2 velocity, SDL_Color color) {
    Circle projectile(10.0f, position, color);
    projectile.vel = velocity;
    projectile.acc = glm::vec2(0.0f);
    return projectile;
}

void updatePlayer() {
    if (glm::length(mouseMovement) > 0.0f) {
        player.acc = mouseMovement * 400.0f;
        player.vel += player.acc * DT;
    } else {
        player.acc = glm::vec2(0.0f);
        float speed = glm::length(player.vel);
        if (speed > 0.0f)
            player.vel *= std::max(0.0f, speed - 800.0f * DT) / speed;
    }
    float speed = glm::length(player.vel);
    if (speed > 500.0f)
        player.vel *= 500.0f / speed;
    player.pos += player.vel * DT;
    clampPlayer();

    if (firing) {
        playerFireTimer -= DT;
        while (playerFireTimer <= 0.0f) {
            playerProjectiles.push_back(makeProjectile(player.pos,
                glm::vec2(0.0f, -500.0f), SDL_Color{255, 0, 0, 255}));
            playerFireTimer += 0.3f;
        }
    }
}

void enemyVolley(const Enemy& enemy) {
    glm::vec2 direction = player.pos - enemy.ship.pos;
    float distance = glm::length(direction);
    direction = distance > 0.0001f ? direction / distance : glm::vec2(0.0f, 1.0f);
    for (int index = -1; index <= 1; ++index) {
        float angle = index * 15.0f * 3.14159265358979323846f / 180.0f;
        glm::vec2 rotated(
            direction.x * std::cos(angle) - direction.y * std::sin(angle),
            direction.x * std::sin(angle) + direction.y * std::cos(angle)
        );
        enemyProjectiles.push_back(makeProjectile(enemy.ship.pos,
            rotated * 100.0f, SDL_Color{255, 0, 255, 255}));
    }
}

bool circleHitsRectangle(const Circle& circle, const Rectangle& rectangle) {
    float x = std::clamp(circle.pos.x,
        rectangle.pos.x - rectangle.width / 2.0f,
        rectangle.pos.x + rectangle.width / 2.0f);
    float y = std::clamp(circle.pos.y,
        rectangle.pos.y - rectangle.height / 2.0f,
        rectangle.pos.y + rectangle.height / 2.0f);
    float dx = circle.pos.x - x;
    float dy = circle.pos.y - y;
    return dx * dx + dy * dy <= circle.radius * circle.radius;
}

bool rectanglesCollide(const Rectangle& a, const Rectangle& b) {
    return std::abs(a.pos.x - b.pos.x) <= (a.width + b.width) / 2.0f &&
           std::abs(a.pos.y - b.pos.y) <= (a.height + b.height) / 2.0f;
}

void checkCollisions() {
    for (const Enemy& enemy : enemies) {
        if (rectanglesCollide(player, enemy.ship)) {
            endGame(GameState::Lost);
            return;
        }
    }
    for (const Circle& projectile : enemyProjectiles) {
        if (circleHitsRectangle(projectile, player)) {
            endGame(GameState::Lost);
            return;
        }
    }
    bool destroyedEnemy = false;
    for (std::size_t i = 0; i < playerProjectiles.size();) {
        bool hit = false;
        for (std::size_t j = 0; j < enemies.size(); ++j) {
            if (circleHitsRectangle(playerProjectiles[i], enemies[j].ship)) {
                enemies.erase(enemies.begin() + j);
                hit = destroyedEnemy = true;
                break;
            }
        }
        if (hit)
            playerProjectiles.erase(playerProjectiles.begin() + i);
        else
            ++i;
    }
    if (destroyedEnemy && enemies.empty())
        endGame(GameState::Won);
}

void animate() {
    if (gameState != GameState::Running)
        return;
    updatePlayer();
    for (Enemy& enemy : enemies) {
        enemy.ship.pos += enemy.ship.vel * DT;
        enemy.age += DT;
        while (enemy.age >= enemy.nextShot) {
            enemyVolley(enemy);
            enemy.nextShot += 3.0f;
        }
    }
    for (Circle& projectile : playerProjectiles)
        projectile.pos += projectile.vel * DT;
    for (Circle& projectile : enemyProjectiles)
        projectile.pos += projectile.vel * DT;

    checkCollisions();
    if (gameState != GameState::Running)
        return;

    auto outside = [](const Circle& projectile) {
        return projectile.pos.x + projectile.radius < 0.0f ||
               projectile.pos.x - projectile.radius > windowWidth ||
               projectile.pos.y + projectile.radius < 0.0f ||
               projectile.pos.y - projectile.radius > windowHeight;
    };
    playerProjectiles.erase(std::remove_if(playerProjectiles.begin(),
        playerProjectiles.end(), outside), playerProjectiles.end());
    enemyProjectiles.erase(std::remove_if(enemyProjectiles.begin(),
        enemyProjectiles.end(), outside), enemyProjectiles.end());
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](const Enemy& enemy) {
            return enemy.ship.pos.y - enemy.ship.height / 2.0f > windowHeight;
        }), enemies.end());

    spawnTimer += DT;
    while (spawnTimer >= 3.0f) {
        spawnTimer -= 3.0f;
        for (int slot = 0; slot < ENEMY_SLOTS; ++slot) {
            bool occupied = std::any_of(enemies.begin(), enemies.end(),
                [slot](const Enemy& enemy) { return enemy.slot == slot; });
            if (!occupied)
                spawnEnemy(slot);
        }
    }
    for (Rectangle& square : scrollingGrid) {
        square.pos += square.vel * DT;
        if (square.pos.y > windowHeight + square.height / 2.0f)
            square.pos.y -= square.height * scrollingGrid.size();
    }
}

void drawFrame() {
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g,
                           backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(renderer);
    for (Rectangle& square : scrollingGrid)
        square.draw(renderer);
    for (Enemy& enemy : enemies)
        enemy.ship.draw(renderer);
    for (Circle& projectile : playerProjectiles)
        projectile.draw(renderer);
    for (Circle& projectile : enemyProjectiles)
        projectile.draw(renderer);
    player.draw(renderer);
    SDL_RenderPresent(renderer);
}

void cleanup() {
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}

int main() {
    if (!initWindow()) {
        cleanup();
        return 1;
    }
    SDL_zero(currentEvent);
    resetGame();
    Uint64 previousTime = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (!quit) {
        Uint64 frameStart = SDL_GetPerformanceCounter();
        DT = static_cast<float>((frameStart - previousTime) / frequency);
        previousTime = frameStart;
        DT = std::min(DT, 0.05f);
        processEvents();
        if (quit)
            break;
        animate();
        drawFrame();
        double elapsedMs = 1000.0 * (SDL_GetPerformanceCounter() - frameStart) / frequency;
        if (elapsedMs < 1000.0 / 60.0)
            SDL_Delay(static_cast<Uint32>(std::ceil(1000.0 / 60.0 - elapsedMs)));
    }
    cleanup();
    return 0;
}
