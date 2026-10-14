#version 450
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=0) out vec4 outColor;
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
struct PointLight { vec4 position; vec4 color; };
struct SpotLight { vec4 position; vec4 color; vec4 direction; vec4 angles; };
layout(std430,set=0,binding=1) readonly buffer PointLights { PointLight lights[]; } points;
layout(std430,set=0,binding=2) readonly buffer SpotLights { SpotLight lights[]; } spots;
vec3 safeNormalize(vec3 v) { return v*inversesqrt(max(dot(v,v),0.000001)); }
vec3 blinnPhong(vec3 L,vec3 radiance,vec3 N,vec3 V) {
    float diffuse=max(dot(N,L),0);
    vec3 H=safeNormalize(L+V);
    float specular=diffuse>0 ? pow(max(dot(N,H),0),object.parameters.x) : 0;
    return radiance*(object.albedo.rgb*diffuse+object.specular.rgb*specular);
}
void main() {
    if (object.parameters.y>0.5) { outColor=vec4(object.albedo.rgb,1); return; }
    vec3 N=safeNormalize(worldNormal);
    vec3 V=safeNormalize(scene.camera.xyz-worldPosition);
    vec3 color=scene.ambient.rgb*scene.ambient.a*object.albedo.rgb;
    color+=blinnPhong(-scene.direction.xyz,scene.directionalColor.rgb*scene.directionalColor.a,N,V);
    for (int i=0;i<scene.counts.x;++i) {
        vec3 delta=points.lights[i].position.xyz-worldPosition;
        float distanceSquared=max(dot(delta,delta),0.0001);
        vec3 radiance=points.lights[i].color.rgb*points.lights[i].color.a/distanceSquared;
        color+=blinnPhong(safeNormalize(delta),radiance,N,V);
    }
    for (int i=0;i<scene.counts.y;++i) {
        vec3 delta=spots.lights[i].position.xyz-worldPosition;
        float distanceSquared=max(dot(delta,delta),0.0001);
        vec3 L=safeNormalize(delta);
        float cosine=dot(-L,spots.lights[i].direction.xyz);
        float inner=spots.lights[i].angles.x,outer=spots.lights[i].angles.y;
        float edge=inner>outer ? smoothstep(outer,inner,cosine) : step(inner,cosine);
        vec3 radiance=spots.lights[i].color.rgb*spots.lights[i].color.a*edge/distanceSquared;
        color+=blinnPhong(L,radiance,N,V);
    }
    color=color/(vec3(1)+color);
    outColor=vec4(max(color,vec3(0)),1);
}
