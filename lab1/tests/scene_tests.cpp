#include "scene.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }

int main() {
    try {
        auto vertices = cubeVertices();
        auto indices = cubeIndices();
        float volume = 0;
        for (size_t i = 0; i < indices.size(); i += 3) {
            check(indices[i] < 8 && indices[i+1] < 8 && indices[i+2] < 8, "Index outside vertex buffer");
            auto a = vertices[indices[i]].position;
            auto b = vertices[indices[i+1]].position;
            auto c = vertices[indices[i+2]].position;
            auto normal = glm::cross(b-a, c-a);
            check(glm::length(normal) > 0.9f, "Degenerate triangle");
            check(glm::dot(normal, (a+b+c)/3.0f) > 0, "Face points inward");
            volume += glm::dot(a, glm::cross(b,c)) / 6.0f;
        }
        check(near(volume, 1), "Cube volume must be one");
        for (auto& v : vertices) {
            for (int j=0; j<3; ++j) {
                check(near(std::abs(v.position[j]), 0.5f), "Vertex off cube surface");
                check(near(v.color[j], v.position[j]+0.5f), "Wrong procedural color");
            }
        }
        Cube cube;
        cube.position = {3,4,5};
        cube.rotation = {0,0,90};
        cube.scale = {2,3,4};
        auto p = modelMatrix(cube, 0) * glm::vec4(1,0,0,1);
        check(near(p.x,3) && near(p.y,6) && near(p.z,5), "Wrong transform order");
        cube = Cube{};
        cube.rotation = {0,0,0};
        cube.animated = true;
        p = modelMatrix(cube,0)*glm::vec4(0,0,0,1);
        check(near(p.x,1.5f) && near(p.y,0) && near(p.z,0), "Wrong path origin");
        p = modelMatrix(cube,glm::half_pi<float>())*glm::vec4(0,0,0,1);
        check(near(p.x,0) && near(p.y,0) && near(p.z,1.5f), "Wrong path quarter turn");
        for (bool perspective : {false,true}) {
            auto matrix = projectionMatrix(perspective,2,4);
            auto front = matrix*glm::vec4(0,0,-0.1f,1);
            auto back = matrix*glm::vec4(0,0,-100,1);
            check(near(front.z/front.w,0) && near(back.z/back.w,1), "Depth must be Vulkan zero-to-one");
            auto up = matrix*glm::vec4(0,1,-4,1);
            check(up.y/up.w < 0, "Vulkan Y must be flipped");
            check(near(std::abs(matrix[1][1]/matrix[0][0]),2), "Wrong aspect ratio");
        }
        float time=1;
        advanceTime(time,0.25,false,2);
        check(near(time,1), "Pause does not freeze animation");
        advanceTime(time,0.25,true,2);
        check(near(time,1.5f), "Speed or delta time ignored");
        advanceTime(time,-1,true,2);
        check(near(time,1.5f), "Clock must not run backwards");
        std::cout << "Geometry, transforms, projections and animation: passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
