#include "scene.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
int main() {
    try {
        auto v=cubeVertices(); auto indices=cubeIndices();
        check(v.size()==24,"Cube needs independent face vertices");
        float volume=0;
        for (size_t i=0;i<indices.size();i+=3) {
            auto a=v[indices[i]],b=v[indices[i+1]],c=v[indices[i+2]];
            auto n=glm::normalize(glm::cross(b.position-a.position,c.position-a.position));
            check(near(glm::dot(a.normal,n),1),"Wrong procedural normal");
            check(glm::dot(n,(a.position+b.position+c.position)/3.0f)>0,"Inward face");
            volume+=glm::dot(a.position,glm::cross(b.position,c.position))/6;
        }
        check(near(volume,1),"Cube volume");
        Camera camera; camera.position={0,0,3}; camera.yaw=-90; camera.pitch=0;
        auto p=viewMatrix(camera)*glm::vec4(0,0,0,1);
        check(near(p.x,0)&&near(p.y,0)&&near(p.z,-3),"View must move origin behind camera");
        camera.position={3,4,5}; camera.yaw=27; camera.pitch=-42;
        p=viewMatrix(camera)*glm::vec4(camera.position,1);
        check(glm::length(glm::vec3(p))<0.0001f,"Camera position must map to origin");
        p=viewMatrix(camera)*glm::vec4(camera.position+cameraForward(camera)*2.0f,1);
        check(near(p.x,0)&&near(p.y,0)&&near(p.z,-2),"Camera yaw/pitch direction");
        auto projection=projectionMatrix(45,2);
        p=projection*glm::vec4(0,1,-0.1f,1);
        check(near(p.z/p.w,0)&&p.y<0,"Vulkan near plane/Y");
        p=projection*glm::vec4(0,0,-100,1);
        check(near(p.z/p.w,1),"Vulkan far plane");
        check(near(std::abs(projection[1][1]/projection[0][0]),2),"Aspect ratio");
        Cube cube; cube.rotation={0,0,90}; cube.position={3,4,5}; cube.scale={2,3,4};
        p=modelMatrix(cube)*glm::vec4(1,0,0,1);
        check(near(p.x,3)&&near(p.y,6)&&near(p.z,5),"Model transform");
        auto matrix=modelMatrix(cube);
        auto normal=glm::transpose(glm::inverse(glm::mat3(matrix)))*glm::normalize(glm::vec3(1,1,0));
        auto tangent=glm::mat3(matrix)*glm::vec3(1,-1,0);
        check(near(glm::dot(normal,tangent),0),"Normal transform with nonuniform scale");
        check(near(spotFactor(1,18,28),1),"Spot axis");
        check(near(spotFactor(std::cos(glm::radians(28.0f)),18,28),0),"Spot outer edge");
        float middle=(std::cos(glm::radians(18.0f))+std::cos(glm::radians(28.0f)))/2;
        check(near(spotFactor(middle,18,28),0.5f),"Spot smooth edge");
        check(near(spotFactor(1,20,20),1)&&near(spotFactor(0,20,20),0),"Hard spot edge");
        check(near(glm::length(lightDirection({0,0,0})),1),"Zero light direction fallback");
        SpotLight light;
        for (auto direction : {glm::vec3(0,-1,0),glm::vec3(0,1,0),glm::vec3(1,2,3),glm::vec3(0)}) {
            light.direction=direction;
            auto lines=coneLines(light,3);
            check(lines.size()==74,"Cone line count");
            auto axis=lightDirection(direction);
            for (size_t i=0;i<lines.size()-2;++i) {
                auto d=lines[i].position-light.position;
                if (glm::length(d)<0.0001f) continue;
                check(near(glm::dot(d,axis),3),"Cone cap plane");
                check(std::abs(glm::length(d-3.0f*axis)-3*std::tan(glm::radians(light.outerAngle)))<0.0001f,"Cone radius");
            }
        }
        std::cout<<"Cube normals, camera, projection, transforms and spot cone: passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
