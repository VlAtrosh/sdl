#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include <iostream>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

struct Vector3 {
    float x, y, z;

    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct Triangle {
    Vector3 p[3];
    SDL_Color color;
};

struct Mesh {
    std::vector<Triangle> tris;
};

bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow("3D Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void CreateCube(Mesh& mesh, float size = 1.0f) {
    std::vector<Vector3> vertices = {
        {-size, -size, -size}, { size, -size, -size}, { size,  size, -size}, {-size,  size, -size},
        {-size, -size,  size}, { size, -size,  size}, { size,  size,  size}, {-size,  size,  size}
    };

    // Индексы для треугольников (по 2 на грань)
    std::vector<std::vector<int>> faces = {
        {0,1,2}, {0,2,3}, // передняя
        {4,0,3}, {4,3,7}, // левая
        {5,4,7}, {5,7,6}, // задняя
        {1,5,6}, {1,6,2}, // правая
        {3,2,6}, {3,6,7}, // верхняя
        {4,5,1}, {4,1,0}  // нижняя
    };

    // Цвета для граней
    std::vector<SDL_Color> colors = {
        {255, 0, 0, 255},   // красный
        {0, 255, 0, 255},   // зеленый
        {0, 0, 255, 255},   // синий
        {255, 255, 0, 255}, // желтый
        {0, 255, 255, 255}, // голубой
        {255, 0, 255, 255}  // пурпурный
    };

    // Создаем треугольники
    for (size_t i = 0; i < faces.size(); i++) {
        Triangle tri;
        tri.p[0] = vertices[faces[i][0]];
        tri.p[1] = vertices[faces[i][1]];
        tri.p[2] = vertices[faces[i][2]];
        tri.color = colors[i / 2]; // Один цвет на две грани
        mesh.tris.push_back(tri);
    }
}

// Вращение вокруг оси X
void RotateX(Triangle& tri, float angle) {
    float cosTheta = cos(angle);
    float sinTheta = sin(angle);

    for (int i = 0; i < 3; i++) {
        float y = tri.p[i].y;
        float z = tri.p[i].z;

        tri.p[i].y = y * cosTheta - z * sinTheta;
        tri.p[i].z = y * sinTheta + z * cosTheta;
    }
}

// Вращение вокруг оси Y
void RotateY(Triangle& tri, float angle) {
    float cosTheta = cos(angle);
    float sinTheta = sin(angle);

    for (int i = 0; i < 3; i++) {
        float x = tri.p[i].x;
        float z = tri.p[i].z;

        tri.p[i].x = x * cosTheta + z * sinTheta;
        tri.p[i].z = -x * sinTheta + z * cosTheta;
    }
}

// Вычисление нормали треугольника
Vector3 GetTriangleNormal(const Triangle& tri) {
    Vector3 line1 = { tri.p[1].x - tri.p[0].x, tri.p[1].y - tri.p[0].y, tri.p[1].z - tri.p[0].z };
    Vector3 line2 = { tri.p[2].x - tri.p[0].x, tri.p[2].y - tri.p[0].y, tri.p[2].z - tri.p[0].z };

    // Векторное произведение
    Vector3 normal = {
        line1.y * line2.z - line1.z * line2.y,
        line1.z * line2.x - line1.x * line2.z,
        line1.x * line2.y - line1.y * line2.x
    };

    // Нормализация (необязательно для нашего случая)
    float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > 0) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    }

    return normal;
}

// Рендеринг треугольника
void DrawTriangle(const Triangle& tri) {
    SDL_Vertex vertices[3];

    for (int i = 0; i < 3; i++) {
        // Перспективная проекция
        float z = tri.p[i].z + 5.0f; // Сдвиг по z
        vertices[i].position.x = SCREEN_WIDTH / 2 + tri.p[i].x / z * SCREEN_WIDTH / 2;
        vertices[i].position.y = SCREEN_HEIGHT / 2 - tri.p[i].y / z * SCREEN_HEIGHT / 2;
        vertices[i].color = tri.color;
    }

    SDL_RenderGeometry(renderer, nullptr, vertices, 3, nullptr, 0);
}

int main(int argc, char* argv[]) {
    if (!initSDL()) {
        return -1;
    }

    Mesh cube;
    CreateCube(cube, 1.0f);

    bool running = true;
    SDL_Event event;
    float angle = 0.0f;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Очистка экрана
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Обновление угла вращения
        angle += 0.01f;

        // Рендеринг каждого треугольника
        for (auto& tri : cube.tris) {
            Triangle rotated = tri;
            RotateX(rotated, angle);
            RotateY(rotated, angle * 0.5f);

            // Проверка видимости (отсечение задних граней)
            Vector3 normal = GetTriangleNormal(rotated);
            if (normal.z < 0) {
                // Простое затенение на основе нормали
                float light = -normal.z * 0.5f + 0.5f;
                SDL_Color shadedColor = {
                    static_cast<Uint8>(tri.color.r * light),
                    static_cast<Uint8>(tri.color.g * light),
                    static_cast<Uint8>(tri.color.b * light),
                    255
                };

                Triangle shaded = rotated;
                shaded.color = shadedColor;
                DrawTriangle(shaded);
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}