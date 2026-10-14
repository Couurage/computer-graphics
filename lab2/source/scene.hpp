#pragma once
#include <array>
#include <cstdint>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex { glm::vec3 position; glm::vec3 normal; };
struct Cube {
    glm::vec3 position{0};
    glm::vec3 rotation{15, 25, 0};
    glm::vec3 scale{1.7f};
};
struct Camera {
    glm::vec3 position{5, 3, 7};
    float yaw = -125.54f;
    float pitch = -19.23f;
    float fov = 45;
};
struct PointLight {
    glm::vec3 position{2, 2, 2};
    glm::vec3 color{1, 0.65f, 0.35f};
    float intensity = 18;
};
struct SpotLight {
    glm::vec3 position{-3, 3, 2};
    glm::vec3 direction{3, -3, -2};
    glm::vec3 color{0.3f, 0.65f, 1};
    float intensity = 50;
    float innerAngle = 18;
    float outerAngle = 28;
};
std::array<Vertex, 24> cubeVertices();
std::array<uint16_t, 36> cubeIndices();
glm::mat4 modelMatrix(const Cube& cube);
glm::vec3 cameraForward(const Camera& camera);
glm::mat4 viewMatrix(const Camera& camera);
glm::mat4 projectionMatrix(float fov, float aspect);
glm::vec3 lightDirection(glm::vec3 direction);
std::vector<Vertex> coneLines(const SpotLight& light, float length);
float spotFactor(float cosine, float innerAngle, float outerAngle);
