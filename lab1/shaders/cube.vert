#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(set = 0, binding = 0) uniform ObjectData {
    mat4 mvp;
    vec4 tint;
} object;
layout(location = 0) out vec3 vertexColor;
void main() {
    gl_Position = object.mvp * vec4(position, 1.0);
    vertexColor = mix(vec3(1.0), color, object.tint.a) * object.tint.rgb;
}
