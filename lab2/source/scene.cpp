#include "scene.hpp"
#include <algorithm>
#include <cmath>

std::array<Vertex, 24> cubeVertices() {
    const glm::vec3 p[] = {
        {-0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,-0.5f}, {0.5f,0.5f,-0.5f}, {-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f}, {0.5f,-0.5f,0.5f}, {0.5f,0.5f,0.5f}, {-0.5f,0.5f,0.5f}
    };
    const int faces[6][4] = {{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{0,1,5,4},{3,7,6,2}};
    std::array<Vertex,24> vertices{};
    for (int i=0; i<6; ++i) {
        auto normal = glm::normalize(glm::cross(p[faces[i][1]]-p[faces[i][0]],p[faces[i][2]]-p[faces[i][0]]));
        for (int j=0; j<4; ++j) vertices[i*4+j] = {p[faces[i][j]],normal};
    }
    return vertices;
}
std::array<uint16_t,36> cubeIndices() {
    std::array<uint16_t,36> indices{};
    for (int i=0; i<6; ++i) {
        int start=i*4;
        const int face[]={start,start+1,start+2,start,start+2,start+3};
        for (int j=0;j<6;++j) indices[i*6+j]=static_cast<uint16_t>(face[j]);
    }
    return indices;
}
glm::mat4 modelMatrix(const Cube& cube) {
    auto model=glm::translate(glm::mat4(1),cube.position);
    model=glm::rotate(model,glm::radians(cube.rotation.z),{0,0,1});
    model=glm::rotate(model,glm::radians(cube.rotation.y),{0,1,0});
    model=glm::rotate(model,glm::radians(cube.rotation.x),{1,0,0});
    return glm::scale(model,cube.scale);
}
glm::vec3 cameraForward(const Camera& camera) {
    float yaw=glm::radians(camera.yaw), pitch=glm::radians(camera.pitch);
    return {std::cos(pitch)*std::cos(yaw),std::sin(pitch),std::cos(pitch)*std::sin(yaw)};
}
glm::mat4 viewMatrix(const Camera& camera) {
    return glm::lookAt(camera.position,camera.position+cameraForward(camera),glm::vec3(0,1,0));
}
glm::mat4 projectionMatrix(float fov,float aspect) {
    auto p=glm::perspective(glm::radians(fov),aspect,0.1f,100.0f);
    p[1][1]*=-1;
    return p;
}
glm::vec3 lightDirection(glm::vec3 direction) {
    return glm::length(direction)>0.0001f ? glm::normalize(direction) : glm::vec3(0,-1,0);
}
std::vector<Vertex> coneLines(const SpotLight& light,float length) {
    auto direction=lightDirection(light.direction);
    auto axis=std::abs(direction.y)<0.95f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    auto right=glm::normalize(glm::cross(direction,axis));
    auto up=glm::cross(right,direction);
    auto center=light.position+length*direction;
    float radius=length*std::tan(glm::radians(light.outerAngle));
    std::vector<Vertex> vertices;
    for (int i=0;i<32;++i) {
        auto point=[&](int n) {
            float a=glm::two_pi<float>()*n/32;
            return center+radius*(std::cos(a)*right+std::sin(a)*up);
        };
        vertices.push_back({point(i),{0,1,0}});
        vertices.push_back({point(i+1),{0,1,0}});
        if (i%8==0) {
            vertices.push_back({light.position,{0,1,0}});
            vertices.push_back({point(i),{0,1,0}});
        }
    }
    vertices.push_back({light.position,{0,1,0}});
    vertices.push_back({center,{0,1,0}});
    return vertices;
}
float spotFactor(float cosine,float innerAngle,float outerAngle) {
    float inner=std::cos(glm::radians(innerAngle)),outer=std::cos(glm::radians(outerAngle));
    if (innerAngle==outerAngle) return cosine>=inner ? 1.0f : 0.0f;
    float t=std::clamp((cosine-outer)/(inner-outer),0.0f,1.0f);
    return t*t*(3-2*t);
}
