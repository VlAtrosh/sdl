#include <SDL.h>
#include <SDL_image.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cmath>

namespace Constants {
    constexpr int SCREEN_WIDTH = 1200;
    constexpr int SCREEN_HEIGHT = 1000;

    constexpr float ROTATION_SPEED = 0.002f;
    constexpr float MOVE_SPEED = 0.1f;

    constexpr float CAMERA_ROTATION_SPEED = 0.002f;
    constexpr float ZOOM_SPEED = 0.01f;
    constexpr float INITIAL_CAMERA_DISTANCE = 7.0f;
    constexpr float MIN_CAMERA_DISTANCE = 1.0f;
    constexpr float MAX_CAMERA_DISTANCE = 20.0f;
    constexpr float ZOOM_SENSITIVITY = 0.8f;

    constexpr float CAMERA_HEIGHT_OFFSET = 0.5f;
    constexpr float MODEL_SCALE_FACTOR = 1.0f;
}

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

    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return Vector3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

Vector3 reflect(const Vector3& incident, const Vector3& normal) {
    return incident - normal * 2.0f * Vector3::dot(normal, incident);
}

struct Material {
    SDL_Color baseColor = { 255, 255, 255, 255 };
    float diffuseIntensity = 1.0f;
    float specularIntensity = 0.5f;
    float shininess = 32.0f;
};

class LightingSystem {
public:
    struct Light {
        Vector3 position;
        Vector3 color;
        float intensity;
        float attenuation;
        bool directional;
    };

    Vector3 ambientColor{ 0.3f, 0.3f, 0.3f };
    std::vector<Light> lights;

    LightingSystem() {
        lights.push_back({
            Vector3{0.5f, -1.0f, -1.0f}.normalize(),
            Vector3{1.0f, 1.0f, 1.0f},
            1.5f,
            0.0f,
            true
            });

        lights.push_back({
            Vector3{-0.5f, 0.5f, -0.5f}.normalize(),
            Vector3{0.8f, 0.8f, 1.0f},
            0.8f,
            0.0f,
            true
            });

        lights.push_back({
            Vector3{0.0f, 1.5f, 0.0f},
            Vector3{1.0f, 0.8f, 0.5f},
            1.2f,
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

            float diffuse = std::max(0.0f, Vector3::dot(normal, lightDir)) * material.diffuseIntensity;
            Vector3 diffuseColor = light.color * diffuse * light.intensity * attenuation;

            Vector3 viewDir = (Vector3{ 0, 0, -1 } - point).normalize();
            Vector3 reflectDir = reflect(-lightDir, normal);
            float specular = std::pow(std::max(0.0f, Vector3::dot(viewDir, reflectDir)), material.shininess) * material.specularIntensity;
            Vector3 specularColor = light.color * specular * light.intensity * attenuation;

            resultColor = resultColor + diffuseColor + specularColor;
        }

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

struct Matrix4 {
    float m[16] = { 0 };

    static Matrix4 identity() {
        Matrix4 mat;
        mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
        return mat;
    }

    static Matrix4 lookAt(const Vector3& eye, const Vector3& center, const Vector3& up) {
        Vector3 f = (center - eye).normalize();
        Vector3 s = Vector3::cross(f, up).normalize();
        Vector3 u = Vector3::cross(s, f);

        Matrix4 result = identity();
        result.m[0] = s.x;
        result.m[1] = u.x;
        result.m[2] = -f.x;
        result.m[4] = s.y;
        result.m[5] = u.y;
        result.m[6] = -f.y;
        result.m[8] = s.z;
        result.m[9] = u.z;
        result.m[10] = -f.z;
        result.m[12] = -Vector3::dot(s, eye);
        result.m[13] = -Vector3::dot(u, eye);
        result.m[14] = Vector3::dot(f, eye);
        return result;
    }
};

struct Camera {
    Vector3 position{ 0, 0, Constants::INITIAL_CAMERA_DISTANCE };
    Vector3 target{ 0, 0, 0 };
    Vector3 up{ 0, 1, 0 };
    float pitch = 0.0f;
    float yaw = 0.0f;

    void update() {
        Vector3 front = (target - position).normalize();
        Vector3 right = Vector3::cross(front, Vector3{ 0,1,0 }).normalize();
        up = Vector3::cross(right, front).normalize();
        pitch = asinf(front.y);
        yaw = atan2f(front.z, front.x);
    }

    float distance() const {
        return (position - target).length();
    }

    void move(const Vector3& offset) {
        position = position + offset;
        target = target + offset;
        update();
    }

    void rotate(float dp, float dy) {
        const float PI = 3.14159265358979323846f;
        const float minPitch = -PI / 2 + 0.1f;
        const float maxPitch = PI / 2 - 0.1f;

        pitch += dp;
        yaw += dy;

        if (pitch > maxPitch) pitch = maxPitch;
        if (pitch < minPitch) pitch = minPitch;

        Vector3 front;
        front.x = cosf(yaw) * cosf(pitch);
        front.y = sinf(pitch);
        front.z = sinf(yaw) * cosf(pitch);
        front = front.normalize();

        position = target - front * distance();
    }

    void zoom(float amount) {
        Vector3 dir = (position - target).normalize();
        float newDist = std::max(Constants::MIN_CAMERA_DISTANCE,
            std::min(Constants::MAX_CAMERA_DISTANCE,
                distance() - amount * Constants::ZOOM_SENSITIVITY));
        position = target + dir * newDist;
    }

    void centerOnObject(const Vector3& objectCenter) {
        target = objectCenter;
        position = target - Vector3{ 0, 0, Constants::INITIAL_CAMERA_DISTANCE };
        update();
    }
    Matrix4 getViewMatrix() const {
        return Matrix4::lookAt(position, target, up);
    }
};

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

    void translate(const Vector3& offset) {
        for (auto& p : points) {
            p = p + offset;
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
            }
        }

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

        if (!v.empty()) {
            int vi = std::stoi(v) - 1;
            if (vi >= 0 && vi < vertices.size()) {
                tri.points[index] = vertices[vi];
            }
        }

        if (!vt.empty()) {
            int vti = std::stoi(vt) - 1;
            if (vti >= 0 && vti < texCoords.size()) {
                tri.texCoords[index] = texCoords[vti];
            }
        }

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
    Vector3 position{ 0, 0, 0 };

    Vector3 computeCenter() const {
        if (triangles.empty()) return position;

        Vector3 min = triangles[0].points[0];
        Vector3 max = triangles[0].points[0];

        for (const auto& tri : triangles) {
            for (const auto& point : tri.points) {
                min.x = std::min(min.x, point.x);
                min.y = std::min(min.y, point.y);
                min.z = std::min(min.z, point.z);
                max.x = std::max(max.x, point.x);
                max.y = std::max(max.y, point.y);
                max.z = std::max(max.z, point.z);
            }
        }

        return position + (min + max) * 0.5f;
    }

    Vector3 computeTrueCenter() const {
        if (triangles.empty()) return position;

        Vector3 sum;
        int count = 0;
        for (const auto& tri : triangles) {
            for (const auto& p : tri.points) {
                sum = sum + p;
                count++;
            }
        }
        return position + (sum / float(count));
    }

    void translate(const Vector3& offset) {
        position = position + offset;
        for (auto& tri : triangles) {
            tri.translate(offset);
        }
    }

    void scale(float factor) {
        Vector3 center = computeTrueCenter();
        for (auto& tri : triangles) {
            for (int i = 0; i < 3; ++i) {
                tri.points[i] = center + (tri.points[i] - center) * factor;
            }
        }
    }

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
    bool showWireframe = false;
    bool backfaceCulling = false;
    Camera camera;
    bool mouseCaptured = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    bool wireframeMode = false;
    float objectScale = 1.0f;
    bool wireframeKeyPressed = false;
    bool plusKeyPressed = false;
    bool minusKeyPressed = false;
    SDL_Texture* texture = nullptr;
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

    state.window = SDL_CreateWindow("3D Renderer",
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
    const Camera& camera, const LightingSystem& lighting,
    SDL_Texture* texture, bool wireframe, bool backfaceCulling) {

    auto toInt = [](float f) { return static_cast<int>(std::round(f)); };

    Vector3 viewDir = camera.position - tri.getCenter();
    Vector3 triNormal = tri.getNormal();

    if (backfaceCulling && Vector3::dot(triNormal, viewDir) <= 0) {
        return;
    }

    SDL_Vertex vertices[3];
    Vector3 center = tri.getCenter();
    Vector3 normal = triNormal.normalize();

    const float fov = 60.0f;
    const float aspectRatio = (float)Constants::SCREEN_WIDTH / Constants::SCREEN_HEIGHT;
    const float scale = Constants::SCREEN_HEIGHT / (2.0f * tanf(fov * 3.14159265f / 360.0f));

    bool triangleVisible = false;

    for (int i = 0; i < 3; i++) {
        Vector3 point = tri.points[i] - camera.position;

        if (point.z <= 0.01f) point.z = 0.01f;

        float invZ = 1.0f / point.z;
        vertices[i].position.x = Constants::SCREEN_WIDTH / 2 + point.x * scale * invZ * aspectRatio;
        vertices[i].position.y = Constants::SCREEN_HEIGHT / 2 - point.y * scale * invZ;

        if (vertices[i].position.x >= 0 && vertices[i].position.x <= Constants::SCREEN_WIDTH &&
            vertices[i].position.y >= 0 && vertices[i].position.y <= Constants::SCREEN_HEIGHT) {
            triangleVisible = true;
        }

        vertices[i].color = lighting.calculateLight(center, normal, tri.material);

        if (texture) {
            vertices[i].tex_coord = tri.texCoords[i];
        }
    }

    if (wireframe) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = 0; i < 3; i++) {
            SDL_RenderDrawLine(renderer,
                toInt(vertices[i].position.x), toInt(vertices[i].position.y),
                toInt(vertices[(i + 1) % 3].position.x), toInt(vertices[(i + 1) % 3].position.y));
        }
    }
    else {
        SDL_RenderGeometry(renderer, texture, vertices, 3, nullptr, 0);
    }
}

void renderScene(AppState& state, const Mesh& mesh) {
    SDL_SetRenderDrawColor(state.renderer, 45, 45, 45, 255);
    SDL_RenderClear(state.renderer);

    std::vector<Triangle> sortedTris = mesh.triangles;
    std::sort(sortedTris.begin(), sortedTris.end(),
        [&](const Triangle& a, const Triangle& b) {
            return (a.getCenter() - state.camera.position).length() >
                (b.getCenter() - state.camera.position).length();
        });

    for (const auto& tri : sortedTris) {
        renderTriangle(
            state.renderer,
            tri,
            state.camera,
            state.lighting,
            mesh.texture,
            state.wireframeMode,
            state.backfaceCulling
        );
    }

    SDL_RenderPresent(state.renderer);
}

void handleInput(AppState& state, Mesh& mesh, float deltaTime) {
    static bool wireframeKeyPressed = false;
    static bool plusKeyPressed = false;
    static bool minusKeyPressed = false;

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float moveSpeed = Constants::MOVE_SPEED * deltaTime * 50.0f;
    float rotationSpeed = Constants::ROTATION_SPEED * deltaTime * 1000.0f;

    Vector3 move{ 0,0,0 };
    if (keys[SDL_SCANCODE_W]) move.z += 1;
    if (keys[SDL_SCANCODE_S]) move.z -= 1;
    if (keys[SDL_SCANCODE_A]) move.x -= 1;
    if (keys[SDL_SCANCODE_D]) move.x += 1;
    if (keys[SDL_SCANCODE_Q]) move.y -= 1;
    if (keys[SDL_SCANCODE_E]) move.y += 1;

    if (move.x != 0 || move.y != 0 || move.z != 0) {
        state.camera.move(move.normalize() * moveSpeed);
    }

    if (state.mouseCaptured) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        int deltaX = mouseX - state.lastMouseX;
        int deltaY = mouseY - state.lastMouseY;
        state.lastMouseX = mouseX;
        state.lastMouseY = mouseY;

        const float mouseSensitivity = 0.001f;
        state.camera.yaw -= deltaX * mouseSensitivity;
        state.camera.pitch -= deltaY * mouseSensitivity;

        state.camera.pitch = std::max(-1.5f, std::min(1.5f, state.camera.pitch));
    }

    if (keys[SDL_SCANCODE_LEFT]) {
        for (auto& tri : mesh.triangles) {
            tri.rotateY(-rotationSpeed);
        }
    }
    if (keys[SDL_SCANCODE_RIGHT]) {
        for (auto& tri : mesh.triangles) {
            tri.rotateY(rotationSpeed);
        }
    }
    if (keys[SDL_SCANCODE_UP]) {
        for (auto& tri : mesh.triangles) {
            tri.rotateX(-rotationSpeed);
        }
    }
    if (keys[SDL_SCANCODE_DOWN]) {
        for (auto& tri : mesh.triangles) {
            tri.rotateX(rotationSpeed);
        }
    }

    if (keys[SDL_SCANCODE_F] && !wireframeKeyPressed) {
        state.wireframeMode = !state.wireframeMode;
        wireframeKeyPressed = true;
    }
    if (!keys[SDL_SCANCODE_F]) wireframeKeyPressed = false;

    const float scaleStep = 0.1f;
    if ((keys[SDL_SCANCODE_KP_PLUS] || keys[SDL_SCANCODE_EQUALS]) && !plusKeyPressed) {
        mesh.scale(1.0f + scaleStep);
        plusKeyPressed = true;
    }
    if (!keys[SDL_SCANCODE_KP_PLUS] && !keys[SDL_SCANCODE_EQUALS]) plusKeyPressed = false;

    if ((keys[SDL_SCANCODE_KP_MINUS] || keys[SDL_SCANCODE_MINUS]) && !minusKeyPressed) {
        mesh.scale(std::max(0.1f, 1.0f - scaleStep));
        minusKeyPressed = true;
    }
    if (!keys[SDL_SCANCODE_KP_MINUS] && !keys[SDL_SCANCODE_MINUS]) minusKeyPressed = false;

    static Mesh originalMesh = mesh;
    if (keys[SDL_SCANCODE_R]) {
        mesh = originalMesh;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) state.running = false;
        else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                state.mouseCaptured = !state.mouseCaptured;
                SDL_SetRelativeMouseMode(state.mouseCaptured ? SDL_TRUE : SDL_FALSE);
            }
        }
        else if (event.type == SDL_MOUSEWHEEL) {
            state.camera.zoom(event.wheel.y * 0.5f);
        }
    }
}

void cleanup(AppState& state) {
    if (state.texture) {
        SDL_DestroyTexture(state.texture);
    }
    if (state.renderer) {
        SDL_DestroyRenderer(state.renderer);
    }
    if (state.window) {
        SDL_DestroyWindow(state.window);
    }
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    AppState state;
    if (!initializeSDL(state)) {
        return 1;
    }

    Mesh mesh;
    try {
        mesh = Mesh::fromObj("model.obj");
        mesh.setTexture(state.renderer, "texture.png");
    }
    catch (...) {
        std::cout << "Using generated cube instead of OBJ file\n";
        mesh = Mesh::createCube();
    }

    Vector3 center = mesh.computeTrueCenter();
    state.camera.target = center;
    state.camera.position = center + Vector3(0, 0.5f, -8.0f);
    state.camera.update();

    Uint32 lastFrameTime = SDL_GetTicks();
    while (state.running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;

        handleInput(state, mesh, deltaTime);
        renderScene(state, mesh);
    }

    cleanup(state);
    return 0;
}