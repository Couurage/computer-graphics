#pragma once
#include <array>
#include <cstdint>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

struct Cube {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{20.0f, 30.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 color{1.0f};
    bool animated = false;
    float radius = 1.5f;
    float height = 0.7f;
    float phase = 0.0f;
};

std::array<Vertex, 8> cubeVertices();
std::array<uint16_t, 36> cubeIndices();
glm::mat4 modelMatrix(const Cube& cube, float time);
glm::mat4 projectionMatrix(bool perspective, float aspect, float size);
void advanceTime(float& time, double elapsed, bool playing, float speed);
