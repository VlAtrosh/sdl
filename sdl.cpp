#include <SDL.h>
#include <SDL_image.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>

// Constants
namespace Constants {
    constexpr int SCREEN_WIDTH = 1000;
    constexpr int SCREEN_HEIGHT = 800;
    constexpr float ROTATION_SPEED = 0.01f;
    constexpr float CAMERA_DISTANCE = 2.5f;
}

// Vector Math
struct Vector2 {
    float x, y;
    Vector2(float x = 0, float y = 0) : x(x), y(y) {}
    operator SDL_FPoint() const { return { x, y }; }
};

struct Vector3 {
    float x, y, z;
    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    Vector3 operator-() const { return { -x, -y, -z }; }
    Vector3 operator+(const Vector3& other) const { return { x + other.x, y + other.y, z + other.z }; }
    Vector3 operator-(const Vector3& other) const { return { x - other.x, y - other.y, z - other.z }; }
    Vector3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
    Vector3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vector3 normalize() const { float len = length(); return len > 0 ? (*this) / len : *this; }

    static float dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
};

Vector3 reflect(const Vector3& incident, const Vector3& normal) {
    return incident - normal * 2.0f * Vector3::dot(normal, incident);
}

// Material System
struct Material {
    SDL_Color baseColor = { 255, 255, 255, 255 };
    float diffuseIntensity = 1.0f;
    float specularIntensity = 0.5f;
    float shininess = 32.0f;
};

// Enhanced Lighting System
class LightingSystem {
public:
    struct Light {
        Vector3 position;
        Vector3 color;
        float intensity;
        float attenuation;
        bool directional;
    };

    Vector3 ambientColor{ 0.3f, 0.3f, 0.3f }; // Brighter ambient
    std::vector<Light> lights;

    LightingSystem() {
        // Main directional light (sun)
        lights.push_back({
            Vector3{0.5f, -1.0f, -1.0f}.normalize(),
            Vector3{1.0f, 1.0f, 1.0f},
            1.5f, // Increased intensity
            0.0f,
            true
            });

        // Fill light
        lights.push_back({
            Vector3{-0.5f, 0.5f, -0.5f}.normalize(),
            Vector3{0.8f, 0.8f, 1.0f},
            0.8f,
            0.0f,
            true
            });

        // Point light
        lights.push_back({
            Vector3{0.0f, 1.5f, 0.0f},
            Vector3{1.0f, 0.8f, 0.5f},
            1.2f, // Increased intensity
            0.1f,
            false
            });
    }

    SDL_Color calculateLight(const Vector3& point, const Vector3& normal, const Material& material) const {
        Vector3 resultColor{
            ambientColor.x * material.baseColor.r / 255.0f,
            ambientColor.y * material.baseColor.g / 255.0f,
            ambientColor.z * material.baseColor.b / 255.0f
        };

        for (const auto& light : lights) {
            Vector3 lightDir;
            float attenuation = 1.0f;

            if (light.directional) {
                lightDir = -light.position;
            }
            else {
                lightDir = (light.position - point).normalize();
                float distance = (light.position - point).length();
                attenuation = 1.0f / (1.0f + light.attenuation * distance * distance);
            }

            // Enhanced diffuse with intensity
            float diffuse = std::max(0.0f, Vector3::dot(normal, lightDir)) * material.diffuseIntensity;
            Vector3 diffuseColor = light.color * diffuse * light.intensity * attenuation;

            // Enhanced specular
            Vector3 viewDir = (Vector3{ 0, 0, -1 } - point).normalize();
            Vector3 reflectDir = reflect(-lightDir, normal);
            float specular = std::pow(std::max(0.0f, Vector3::dot(viewDir, reflectDir)), material.shininess) * material.specularIntensity;
            Vector3 specularColor = light.color * specular * light.intensity * attenuation;

            resultColor = resultColor + diffuseColor + specularColor;
        }

        // Gamma correction
        resultColor.x = std::pow(resultColor.x, 1.0f / 2.2f);
        resultColor.y = std::pow(resultColor.y, 1.0f / 2.2f);
        resultColor.z = std::pow(resultColor.z, 1.0f / 2.2f);

        return {
            static_cast<Uint8>(std::min(255.0f, resultColor.x * 255.0f)),
            static_cast<Uint8>(std::min(255.0f, resultColor.y * 255.0f)),
            static_cast<Uint8>(std::min(255.0f, resultColor.z * 255.0f)),
            material.baseColor.a
        };
    }
};

// Triangle and Mesh with improved rendering
struct Triangle {
    Vector3 points[3];
    Vector3 normals[3];
    Vector2 texCoords[3];
    Material material;

    void rotateX(float angle) {
        float cos = std::cos(angle), sin = std::sin(angle);
        for (auto& p : points) {
            float y = p.y, z = p.z;
            p.y = y * cos - z * sin;
            p.z = y * sin + z * cos;
        }
        for (auto& n : normals) {
            float y = n.y, z = n.z;
            n.y = y * cos - z * sin;
            n.z = y * sin + z * cos;
        }
    }

    void rotateY(float angle) {
        float cos = std::cos(angle), sin = std::sin(angle);
        for (auto& p : points) {
            float x = p.x, z = p.z;
            p.x = x * cos + z * sin;
            p.z = -x * sin + z * cos;
        }
        for (auto& n : normals) {
            float x = n.x, z = n.z;
            n.x = x * cos + z * sin;
            n.z = -x * sin + z * cos;
        }
    }

    Vector3 getCenter() const {
        return (points[0] + points[1] + points[2]) / 3.0f;
    }

    Vector3 getNormal() const {
        Vector3 edge1 = points[1] - points[0];
        Vector3 edge2 = points[2] - points[0];
        Vector3 normal = Vector3{
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };
        return normal.normalize();
    }
};

class ObjLoader {
public:
    static std::vector<Triangle> load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open OBJ file: " << filename << std::endl;
            return {};
        }

        std::vector<Vector3> vertices;
        std::vector<Vector2> texCoords;
        std::vector<Vector3> normals;
        std::vector<Triangle> triangles;
        Material currentMaterial;

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string type;
            iss >> type;

            if (type == "v") {
                float x, y, z;
                iss >> x >> y >> z;
                vertices.emplace_back(x, y, z);
            }
            else if (type == "vt") {
                float u, v;
                iss >> u >> v;
                texCoords.emplace_back(u, 1.0f - v);
            }
            else if (type == "vn") {
                float x, y, z;
                iss >> x >> y >> z;
                normals.emplace_back(x, y, z);
            }
            else if (type == "f") {
                std::vector<std::string> faceData;
                std::string token;
                while (iss >> token) faceData.push_back(token);

                if (faceData.size() >= 3) {
                    processFace(faceData, vertices, texCoords, normals, triangles, currentMaterial);
                }
            }
            else if (type == "usemtl") {
                // Here you would handle material changes if you have MTL files
            }
        }

        // Fix normals if they're missing
        for (auto& tri : triangles) {
            for (int i = 0; i < 3; i++) {
                if (tri.normals[i].length() < 0.001f) {
                    tri.normals[i] = tri.getNormal();
                }
            }
        }

        return triangles;
    }

private:
    static void processFace(const std::vector<std::string>& faceData,
        const std::vector<Vector3>& vertices,
        const std::vector<Vector2>& texCoords,
        const std::vector<Vector3>& normals,
        std::vector<Triangle>& outTriangles,
        const Material& material) {
        Triangle tri;
        tri.material = material;

        for (int i = 0; i < 3; ++i) {
            parseVertex(faceData[i], vertices, texCoords, normals, tri, i);
        }
        outTriangles.push_back(tri);

        // Triangulate polygons with more than 3 vertices
        for (size_t i = 3; i < faceData.size(); ++i) {
            Triangle newTri;
            newTri.material = material;
            parseVertex(faceData[0], vertices, texCoords, normals, newTri, 0);
            parseVertex(faceData[i - 1], vertices, texCoords, normals, newTri, 1);
            parseVertex(faceData[i], vertices, texCoords, normals, newTri, 2);
            outTriangles.push_back(newTri);
        }
    }

    static void parseVertex(const std::string& vertexData,
        const std::vector<Vector3>& vertices,
        const std::vector<Vector2>& texCoords,
        const std::vector<Vector3>& normals,
        Triangle& tri, int index) {
        std::istringstream viss(vertexData);
        std::string v, vt, vn;

        std::getline(viss, v, '/');
        std::getline(viss, vt, '/');
        std::getline(viss, vn, '/');

        // Parse vertex position
        if (!v.empty()) {
            int vi = std::stoi(v) - 1;
            if (vi >= 0 && vi < vertices.size()) {
                tri.points[index] = vertices[vi];
            }
        }

        // Parse texture coordinate
        if (!vt.empty()) {
            int vti = std::stoi(vt) - 1;
            if (vti >= 0 && vti < texCoords.size()) {
                tri.texCoords[index] = texCoords[vti];
            }
        }

        // Parse normal
        if (!vn.empty()) {
            int vni = std::stoi(vn) - 1;
            if (vni >= 0 && vni < normals.size()) {
                tri.normals[index] = normals[vni];
            }
        }
    }
};

struct Mesh {
    std::vector<Triangle> triangles;
    SDL_Texture* texture = nullptr;

    static Mesh createCube(float size = 1.0f) {
        Mesh cube;
        Vector3 v[8] = {
            {-size,-size,-size}, {size,-size,-size}, {size,size,-size}, {-size,size,-size},
            {-size,-size,size}, {size,-size,size}, {size,size,size}, {-size,size,size}
        };

        int faces[6][4] = {
            {0,1,2,3}, {4,5,6,7}, {3,2,6,7},
            {0,1,5,4}, {0,3,7,4}, {1,2,6,5}
        };

        Vector3 normals[6] = {
            {0,0,-1}, {0,0,1}, {0,1,0},
            {0,-1,0}, {-1,0,0}, {1,0,0}
        };

        SDL_Color colors[6] = {
            {255,0,0}, {0,255,0}, {0,0,255},
            {255,255,0}, {0,255,255}, {255,0,255}
        };

        for (int i = 0; i < 6; ++i) {
            Material mat;
            mat.baseColor = colors[i];
            mat.diffuseIntensity = 1.0f;
            mat.specularIntensity = 0.5f;
            mat.shininess = 32.0f;

            Triangle tri1;
            tri1.points[0] = v[faces[i][0]];
            tri1.points[1] = v[faces[i][1]];
            tri1.points[2] = v[faces[i][2]];
            tri1.normals[0] = tri1.normals[1] = tri1.normals[2] = normals[i];
            tri1.texCoords[0] = { 0,0 };
            tri1.texCoords[1] = { 1,0 };
            tri1.texCoords[2] = { 1,1 };
            tri1.material = mat;
            cube.triangles.push_back(tri1);

            Triangle tri2;
            tri2.points[0] = v[faces[i][0]];
            tri2.points[1] = v[faces[i][2]];
            tri2.points[2] = v[faces[i][3]];
            tri2.normals[0] = tri2.normals[1] = tri2.normals[2] = normals[i];
            tri2.texCoords[0] = { 0,0 };
            tri2.texCoords[1] = { 1,1 };
            tri2.texCoords[2] = { 0,1 };
            tri2.material = mat;
            cube.triangles.push_back(tri2);
        }
        return cube;
    }

    static Mesh fromObj(const std::string& filename) {
        Mesh mesh;
        mesh.triangles = ObjLoader::load(filename);
        return mesh;
    }

    void setTexture(SDL_Renderer* renderer, const std::string& imagePath) {
        SDL_Surface* surface = IMG_Load(imagePath.c_str());
        if (surface) {
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }
        else {
            std::cerr << "Failed to load texture: " << imagePath << " - " << IMG_GetError() << std::endl;
        }
    }
};

struct AppState {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    LightingSystem lighting;
    bool running = true;
    float angle = 0.0f;
    bool showWireframe = false;
    bool backfaceCulling = false;
};

bool initializeSDL(AppState& state) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    state.window = SDL_CreateWindow("3D Renderer with Enhanced Lighting",
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

    return true;
}

void renderTriangle(SDL_Renderer* renderer, const Triangle& tri,
    const LightingSystem& lighting, SDL_Texture* texture,
    bool showWireframe) {
    SDL_Vertex vertices[3];
    Vector3 center = tri.getCenter();
    Vector3 normal = tri.getNormal().normalize();

    for (int i = 0; i < 3; i++) {
        float z = tri.points[i].z + Constants::CAMERA_DISTANCE;
        float invZ = 1.0f / z;
        vertices[i].position.x = Constants::SCREEN_WIDTH / 2 + tri.points[i].x * invZ * Constants::SCREEN_WIDTH / 2;
        vertices[i].position.y = Constants::SCREEN_HEIGHT / 2 - tri.points[i].y * invZ * Constants::SCREEN_HEIGHT / 2;

        if (texture) {
            vertices[i].tex_coord = tri.texCoords[i];
            vertices[i].color = lighting.calculateLight(center, normal, tri.material);
        }
        else {
            vertices[i].color = lighting.calculateLight(center, normal, tri.material);
        }
    }

    if (showWireframe) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            SDL_RenderDrawLine(renderer,
                vertices[i].position.x, vertices[i].position.y,
                vertices[j].position.x, vertices[j].position.y);
        }
    }
    else {
        SDL_RenderGeometry(renderer, texture, vertices, 3, nullptr, 0);
    }
}

void runMainLoop(AppState& state, Mesh& mesh) {
    while (state.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                state.running = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_w) {
                    state.showWireframe = !state.showWireframe;
                }
                else if (event.key.keysym.sym == SDLK_b) {
                    state.backfaceCulling = !state.backfaceCulling;
                }
            }
        }

        SDL_SetRenderDrawColor(state.renderer, 91, 91, 91, 255);
        SDL_RenderClear(state.renderer);

        state.angle += Constants::ROTATION_SPEED;

        // Sort triangles back-to-front
        std::sort(mesh.triangles.begin(), mesh.triangles.end(),
            [](const Triangle& a, const Triangle& b) {
                return (a.points[0].z + a.points[1].z + a.points[2].z) / 3 >
                    (b.points[0].z + b.points[1].z + b.points[2].z) / 3;
            });

        // Render each triangle
        for (auto& tri : mesh.triangles) {
            Triangle rotated = tri;
            rotated.rotateX(state.angle);
            rotated.rotateY(state.angle * 0.5f);

            // Backface culling with toggle
            if (!state.backfaceCulling || rotated.getNormal().z < 0) {
                renderTriangle(state.renderer, rotated, state.lighting, mesh.texture, state.showWireframe);
            }
        }

        SDL_RenderPresent(state.renderer);
    }
}

void cleanup(AppState& state) {
    if (state.renderer) SDL_DestroyRenderer(state.renderer);
    if (state.window) SDL_DestroyWindow(state.window);
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    AppState state;
    if (!initializeSDL(state)) return 1;

    Mesh mesh;
    try {
        mesh = Mesh::fromObj("cube.obj");
        mesh.setTexture(state.renderer, "texture.png");
    }
    catch (...) {
        std::cout << "Using generated cube instead of OBJ file\n";
        mesh = Mesh::createCube();
    }

    runMainLoop(state, mesh);
    cleanup(state);

    return 0;
}