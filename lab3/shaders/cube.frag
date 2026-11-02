#version 450
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=0) out vec4 outColor;
layout(location=2) in vec2 uv;
layout(set=1,binding=1) uniform sampler2D albedoMap;
layout(set=1,binding=2) uniform sampler2D specularMap;
layout(set=1,binding=3) uniform sampler2D emissiveMap;
layout(push_constant) uniform Sampling { vec4 effect; vec4 maps; } sampling;
vec3 surfaceAlbedo, surfaceSpecular;
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
    return radiance*(surfaceAlbedo*diffuse+surfaceSpecular*specular);
}
void main() {
    if (object.parameters.y>0.5) { outColor=vec4(object.albedo.rgb,1); return; }
    vec2 baseUV=uv*sampling.effect.w;
    vec3 texel=texture(albedoMap,baseUV).rgb;
    if (sampling.effect.x>0.5) {
        float t=sampling.effect.y;
        float strength=sampling.effect.z;
        vec2 wave=0.07*strength*vec2(sin(baseUV.y*12.0+t*2.0),cos(baseUV.x*10.0-t*1.5));
        vec3 first=texture(albedoMap,baseUV+wave+vec2(t*0.08,0)).rgb;
        vec3 second=texture(albedoMap,baseUV*1.7-wave+vec2(0,-t*0.06)).rgb;
        vec3 modulation=vec3(0.7)+0.3*cos(t+vec3(0,2,4));
        texel=mix(texel,mix(first,second,0.4)*modulation,strength);
    }
    surfaceAlbedo=texel*object.albedo.rgb;
    surfaceSpecular=texture(specularMap,baseUV).rgb*object.specular.rgb*sampling.maps.x;
    vec3 N=safeNormalize(worldNormal);
    vec3 V=safeNormalize(scene.camera.xyz-worldPosition);
    vec3 color=scene.ambient.rgb*scene.ambient.a*surfaceAlbedo;
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
    color+=texture(emissiveMap,baseUV).rgb*sampling.maps.y*sampling.maps.z;
    color=color/(vec3(1)+color);
    outColor=vec4(max(color,vec3(0)),1);
}
