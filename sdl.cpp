#include <SDL.h>
#include <SDL_image.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <string>

struct Vector2 {
    float x, y;
    Vector2(float x = 0, float y = 0) : x(x), y(y) {}
    operator SDL_FPoint() const { return { x, y }; }
};

struct Vector3 {
    float x, y, z;
    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    Vector3 operator-(const Vector3& other) const {
        return { x - other.x, y - other.y, z - other.z };
    }

    Vector3 normalize() const {
        float length = std::sqrt(x * x + y * y + z * z);
        return length > 0 ? Vector3(x / length, y / length, z / length) : *this;
    }
};

namespace Constants {
    constexpr int SCREEN_WIDTH = 800;
    constexpr int SCREEN_HEIGHT = 600;
    constexpr float CUBE_SIZE = 1.0f;
    constexpr float ROTATION_SPEED = 0.005f;
    constexpr float CAMERA_DISTANCE = 5.0f;
    constexpr float AMBIENT_LIGHT = 0.5f;
    constexpr float DIFFUSE_INTENSITY = 1.0f;
    const Vector3 LIGHT_DIR{ 0.5f, -1.0f, -1.0f };
};

struct AppState {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = true;
    float angle = 0.0f;
    SDL_Texture* texture = nullptr;
};

struct Triangle {
    Vector3 points[3];
    Vector2 texCoords[3];
    SDL_Color color;

    void rotateX(float angle) {
        float cos = std::cos(angle), sin = std::sin(angle);
        for (auto& p : points) {
            float y = p.y, z = p.z;
            p.y = y * cos - z * sin;
            p.z = y * sin + z * cos;
        }
    }

    void rotateY(float angle) {
        float cos = std::cos(angle), sin = std::sin(angle);
        for (auto& p : points) {
            float x = p.x, z = p.z;
            p.x = x * cos + z * sin;
            p.z = -x * sin + z * cos;
        }
    }

    Vector3 getNormal() const {
        Vector3 u = points[1] - points[0];
        Vector3 v = points[2] - points[0];
        return Vector3{
            u.y * v.z - u.z * v.y,
            u.z * v.x - u.x * v.z,
            u.x * v.y - u.y * v.x
        }.normalize();
    }
};

struct Mesh {
    std::vector<Triangle> triangles;

    static Mesh createCube(float size = Constants::CUBE_SIZE) {
        Mesh cube;
        Vector3 v[8] = {
            {-size,-size,-size}, {size,-size,-size}, {size,size,-size}, {-size,size,-size},
            {-size,-size,size}, {size,-size,size}, {size,size,size}, {-size,size,size}
        };

        int faces[12][3] = {
            {0,1,2}, {0,2,3}, {4,0,3}, {4,3,7}, {5,4,7}, {5,7,6},
            {1,5,6}, {1,6,2}, {3,2,6}, {3,6,7}, {4,5,1}, {4,1,0}
        };

        SDL_Color colors[6] = {
            {255,0,0}, {0,255,0}, {0,0,255},
            {255,255,0}, {0,255,255}, {255,0,255}
        };

        for (int i = 0; i < 12; ++i) {
            cube.triangles.push_back({
                {v[faces[i][0]], v[faces[i][1]], v[faces[i][2]]},
                {{0,0}, {1,0}, {1,1}},
                colors[i / 2]
                });
        }
        return cube;
    }

    static Mesh createTexturedCube(float size = Constants::CUBE_SIZE) {
        Mesh cube = createCube(size);
        Vector2 tex[6] = { {0,0}, {1,0}, {1,1}, {0,0}, {1,1}, {0,1} };
        for (auto& tri : cube.triangles) {
            for (int i = 0; i < 3; i++) {
                tri.texCoords[i] = tex[i];
                tri.color = { 255,255,255,255 };
            }
        }
        return cube;
    }
};

bool initializeSDL(AppState& state) {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Улучшенная инициализация SDL_image с проверкой
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
        int initialized = IMG_Init(imgFlags);
        std::cout << "SDL_image initialized with flags: " << initialized << std::endl;

        if ((initialized & imgFlags) != imgFlags) {
            std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl;
            std::cerr << "Supported formats: PNG=" << (initialized & IMG_INIT_PNG)
                << " JPG=" << (initialized & IMG_INIT_JPG) << std::endl;
        }

    state.window = SDL_CreateWindow("3D Cube",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Constants::SCREEN_WIDTH, Constants::SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (!state.window) {
        std::cerr << "Window creation error: " << SDL_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    state.renderer = SDL_CreateRenderer(state.window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!state.renderer) {
        std::cerr << "Renderer creation error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(state.window);
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    // Улучшенная загрузка текстуры с отладочной информацией
    std::string texturePath = "texture.png";
    std::cout << "Loading texture from: " << texturePath << std::endl;

    SDL_Surface* surface = IMG_Load(texturePath.c_str());
    if (!surface) {
        std::cerr << "Failed to load texture: " << IMG_GetError() << std::endl;

        // Создаем тестовую текстуру если загрузка не удалась
        surface = SDL_CreateRGBSurface(0, 64, 64, 32, 0, 0, 0, 0);
        SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 255, 0, 0));
        std::cout << "Using fallback test texture" << std::endl;
    }
    else {
        std::cout << "Texture loaded successfully. Size: "
            << surface->w << "x" << surface->h << std::endl;
    }

    state.texture = SDL_CreateTextureFromSurface(state.renderer, surface);
    SDL_FreeSurface(surface);

    if (!state.texture) {
        std::cerr << "Texture creation error: " << SDL_GetError() << std::endl;
    }
    else {
        std::cout << "Texture created successfully" << std::endl;
    }

    return true;
}

void renderTriangle(SDL_Renderer* renderer, const Triangle& tri, SDL_Texture* texture) {
    SDL_Vertex vertices[3];

    for (int i = 0; i < 3; i++) {
        float z = tri.points[i].z + Constants::CAMERA_DISTANCE;
        vertices[i].position.x = Constants::SCREEN_WIDTH / 2 + tri.points[i].x / z * Constants::SCREEN_WIDTH / 2;
        vertices[i].position.y = Constants::SCREEN_HEIGHT / 2 - tri.points[i].y / z * Constants::SCREEN_HEIGHT / 2;
        vertices[i].color = tri.color;
        vertices[i].tex_coord = tri.texCoords[i];
    }

    SDL_RenderGeometry(renderer, texture, vertices, 3, nullptr, 0);
}

void runMainLoop(AppState& state, Mesh& cube) {
    while (state.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) state.running = false;
        }

        SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(state.renderer);

        state.angle += Constants::ROTATION_SPEED;

        // Сортировка по глубине
        std::sort(cube.triangles.begin(), cube.triangles.end(),
            [](const Triangle& a, const Triangle& b) {
                float za = (a.points[0].z + a.points[1].z + a.points[2].z) / 3;
                float zb = (b.points[0].z + b.points[1].z + b.points[2].z) / 3;
                return za > zb;
            });

        // Рендеринг
        for (auto& tri : cube.triangles) {
            Triangle rotated = tri;
            rotated.rotateX(state.angle);
            rotated.rotateY(state.angle * 0.5f);

            if (rotated.getNormal().z < 0) {
                if (state.texture) {
                    renderTriangle(state.renderer, rotated, state.texture);
                }
                else {
                    float light = std::max(0.0f,
                        -(rotated.getNormal().x * Constants::LIGHT_DIR.x +
                            rotated.getNormal().y * Constants::LIGHT_DIR.y +
                            rotated.getNormal().z * Constants::LIGHT_DIR.z));
                    light = Constants::AMBIENT_LIGHT + light * Constants::DIFFUSE_INTENSITY;

                    Triangle shaded = rotated;
                    shaded.color = {
                        Uint8(tri.color.r * light),
                        Uint8(tri.color.g * light),
                        Uint8(tri.color.b * light),
                        255
                    };
                    renderTriangle(state.renderer, shaded, nullptr);
                }
            }
        }

        SDL_RenderPresent(state.renderer);
    }
}

void cleanup(AppState& state) {
    if (state.texture) SDL_DestroyTexture(state.texture);
    if (state.renderer) SDL_DestroyRenderer(state.renderer);
    if (state.window) SDL_DestroyWindow(state.window);
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    AppState state;
    if (!initializeSDL(state)) return 1;

    Mesh cube = state.texture ? Mesh::createTexturedCube() : Mesh::createCube();
    runMainLoop(state, cube);
    cleanup(state);

    return 0;
}