#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(set=0,binding=0) uniform SceneData {
    mat4 viewProjection;
    vec4 camera;
    vec4 ambient;
    vec4 direction;
    vec4 directionalColor;
    ivec4 counts;
} scene;
layout(set=1,binding=0) uniform ObjectData {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specular;
    vec4 parameters;
} object;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
void main() {
    vec4 world=object.model*vec4(position,1);
    worldPosition=world.xyz;
    worldNormal=mat3(object.normalMatrix)*normal;
    gl_Position=scene.viewProjection*world;
}
