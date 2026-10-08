#include "scene.hpp"
#include <algorithm>
#include <cmath>

std::array<Vertex, 8> cubeVertices() {
    std::array<Vertex, 8> vertices{};
    const glm::vec3 positions[] = {
        {-0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,-0.5f},
        {0.5f,0.5f,-0.5f}, {-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f}, {0.5f,-0.5f,0.5f},
        {0.5f,0.5f,0.5f}, {-0.5f,0.5f,0.5f}
    };
    for (size_t i=0; i<vertices.size(); ++i)
        vertices[i] = {positions[i], positions[i] + glm::vec3(0.5f)};
    return vertices;
}

std::array<uint16_t, 36> cubeIndices() {
    return {0,2,1, 0,3,2, 4,5,6, 4,6,7,
            0,4,7, 0,7,3, 1,2,6, 1,6,5,
            0,1,5, 0,5,4, 3,7,6, 3,6,2};
}

glm::mat4 modelMatrix(const Cube& cube, float time) {
    auto position = cube.position;
    auto rotation = glm::radians(cube.rotation);
    if (cube.animated) {
        float t = time + cube.phase;
        position += glm::vec3(cube.radius*std::cos(t), cube.height*std::sin(2*t), cube.radius*std::sin(t));
        rotation += glm::vec3(t*0.7f, t, t*0.3f);
    }
    auto model = glm::translate(glm::mat4(1), position);
    model = glm::rotate(model, rotation.z, {0,0,1});
    model = glm::rotate(model, rotation.y, {0,1,0});
    model = glm::rotate(model, rotation.x, {1,0,0});
    return glm::scale(model, cube.scale);
}

glm::mat4 projectionMatrix(bool perspective, float aspect, float size) {
    auto projection = perspective
        ? glm::perspective(glm::radians(size), aspect, 0.1f, 100.0f)
        : glm::ortho(-size*aspect, size*aspect, -size, size, 0.1f, 100.0f);
    projection[1][1] *= -1;
    return projection;
}

void advanceTime(float& time, double elapsed, bool playing, float speed) {
    if (playing) time += static_cast<float>(std::max(0.0, elapsed)) * speed;
}
