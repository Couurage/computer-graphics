#include "application.hpp"
#include "scene.hpp"
#include "resources.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace application {
namespace {
struct Buffer { VkBuffer handle{}; VmaAllocation allocation{}; void* data{}; VkDeviceSize capacity{}; };
struct alignas(16) SceneData {
    glm::mat4 viewProjection;
    glm::vec4 camera, ambient, direction, directionalColor;
    glm::ivec4 counts;
};
struct alignas(16) ObjectData {
    glm::mat4 model, normalMatrix;
    glm::vec4 albedo, specular, parameters;
};
struct alignas(16) PointData { glm::vec4 position, color; };
struct alignas(16) SpotData { glm::vec4 position, color, direction, angles; };
static_assert(sizeof(SceneData)==144 && offsetof(SceneData,counts)==128);
static_assert(sizeof(ObjectData)==176 && offsetof(ObjectData,parameters)==160);
static_assert(sizeof(PointData)==32 && sizeof(SpotData)==64);
struct PushData { glm::vec4 effect; glm::vec4 maps; };
static_assert(sizeof(PushData)==32);
struct Draw { ObjectData data; bool lines=false; int material=0; bool effect=false; float tiling=1; };
struct Material { const char* name; int albedo=0,specular=0,emissive=0; VkDescriptorSet set{}; };
std::array<Material,3> materials{{{"Circuit"},{"Metal"},{"Tiles"}}};
std::vector<Texture> textures;
DeviceBuffer vertices,indices,lineBuffer;
int cubeMaterial=0,floorMaterial=2;
bool effectEnabled=true,animate=true,useSpecular=true,useEmissive=true;
float effectTime=0,speed=1,effectStrength=0.6f,emissiveStrength=1.5f;
double previousTime=-1;
Buffer sceneBuffer,pointBuffer,spotBuffer,objectBuffer;
VkDescriptorSetLayout sceneLayout{},objectLayout{};
VkDescriptorSet sceneSet{};
VkDescriptorPool pool{};
VkPipelineLayout pipelineLayout{};
VkPipeline pipeline{},linePipeline{};
VkDeviceSize objectStride=0;
Camera camera;
Cube cube;
glm::vec3 albedo{1},specular{1};
float shininess=64;
glm::vec3 ambientColor{1},direction{-1,-1,-1},directionalColor{1};
float ambientIntensity=0.06f,directionalIntensity=0.65f;
std::vector<PointLight> pointLights;
std::vector<SpotLight> spotLights;
bool showLights=true,showFloor=true;
int selectedPoint=0,selectedSpot=0;
void check(VkResult result, const char* message) {
    if (result != VK_SUCCESS) throw std::runtime_error(message);
}

void createBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage, const void* data = nullptr) {
    auto& context = graphics::internal::context;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO;
    allocation.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo details{};
    check(vmaCreateBuffer(context.allocator, &info, &allocation, &buffer.handle, &buffer.allocation, &details), "Cannot create buffer");
    buffer.data = details.pMappedData;
    buffer.capacity = size;
    if (data) {
        std::memcpy(buffer.data, data, size);
        check(vmaFlushAllocation(context.allocator, buffer.allocation, 0, size), "Cannot flush buffer");
    }
}

VkShaderModule loadShader(const char* name) {
    std::string path = std::string(SHADER_DIR) + "/" + name;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open shader: " + path);
    auto size = file.tellg();
    if (size <= 0 || size % 4 != 0) throw std::runtime_error("Invalid shader: " + path);
    std::vector<uint32_t> code(static_cast<size_t>(size)/4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(code.data()), size)) throw std::runtime_error("Cannot read shader");
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = static_cast<size_t>(size);
    info.pCode = code.data();
    VkShaderModule shader{};
    check(vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &shader), "Cannot create shader");
    return shader;
}

void createPipeline(VkPipeline& target, bool lines) {
    auto device = graphics::internal::context.device;
    VkShaderModule vert{}, frag{};
    try {
        vert = loadShader("cube.vert.spv");
        frag = loadShader("cube.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";
        VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[] = {
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,position)},
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal)},
            {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)}
        };
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = 1;
        input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = 3;
        input.pVertexAttributeDescriptions = attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = lines ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;
        VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = states;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &input;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &samples;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = pipelineLayout;
        info.renderPass = graphics::internal::context.render_pass;
        check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &target), "Cannot create pipeline");
    } catch (...) {
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
        throw;
    }
    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
}

void reset() {
    camera=Camera{}; cube=Cube{};
    albedo=specular=glm::vec3(1); shininess=64;
    cubeMaterial=0; floorMaterial=2;
    effectEnabled=animate=useSpecular=useEmissive=true;
    effectTime=0; previousTime=-1; speed=1; effectStrength=0.6f; emissiveStrength=1.5f;
    ambientColor=glm::vec3(1); ambientIntensity=0.06f;
    direction={-1,-1,-1}; directionalColor=glm::vec3(1); directionalIntensity=0.65f;
    pointLights={PointLight{}}; spotLights={SpotLight{}};
    selectedPoint=selectedSpot=0;
    showLights=showFloor=true;
}
void destroyBuffer(Buffer& buffer) {
    if (buffer.handle) vmaDestroyBuffer(graphics::internal::context.allocator,buffer.handle,buffer.allocation);
    buffer={};
}
void ensureBuffer(Buffer& buffer,VkDeviceSize size,VkBufferUsageFlags usage) {
    size=std::max<VkDeviceSize>(size,16);
    if (buffer.capacity>=size) return;
    destroyBuffer(buffer);
    createBuffer(buffer,size,usage);
}
void upload(Buffer& buffer,const void* data,VkDeviceSize size) {
    if (!size) return;
    std::memcpy(buffer.data,data,size);
    check(vmaFlushAllocation(graphics::internal::context.allocator,buffer.allocation,0,size),"Cannot flush buffer");
}
ObjectData objectData(glm::mat4 model,glm::vec3 color,glm::vec3 highlight,float exponent,bool unlit=false) {
    return {model,glm::transpose(glm::inverse(model)),glm::vec4(color,1),glm::vec4(highlight,1),{exponent,unlit?1.0f:0.0f,0,0}};
}
void updateDescriptors() {
    VkDescriptorBufferInfo buffers[]={
        {sceneBuffer.handle,0,sizeof(SceneData)},
        {pointBuffer.handle,0,pointBuffer.capacity},
        {spotBuffer.handle,0,spotBuffer.capacity}
    };
    VkWriteDescriptorSet writes[3]{};
    for (int i=0;i<3;++i) {
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet=sceneSet; writes[i].dstBinding=i;
        writes[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].descriptorCount=1; writes[i].pBufferInfo=&buffers[i];
    }
    auto device=graphics::internal::context.device;
    vkUpdateDescriptorSets(device,3,writes,0,nullptr);
    for (auto& material : materials) {
        VkDescriptorBufferInfo model{objectBuffer.handle,0,sizeof(ObjectData)};
        VkDescriptorImageInfo images[3]{};
        int ids[]={material.albedo,material.specular,material.emissive};
        VkWriteDescriptorSet materialWrites[4]{};
        for (int i=0;i<4;++i) {
            auto& w=materialWrites[i]; w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=material.set; w.dstBinding=i; w.descriptorCount=1;
            w.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            if (i==0) w.pBufferInfo=&model;
            else {
                auto& texture=textures.at(ids[i-1]);
                images[i-1]={texture.sampler,texture.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                w.pImageInfo=&images[i-1];
            }
        }
        vkUpdateDescriptorSets(device,4,materialWrites,0,nullptr);
    }
}
void loadMaterials() {
    auto load=[&](const char* file,bool srgb) {
        textures.push_back(loadTexture(std::string(TEXTURE_DIR)+"/"+file,srgb));
        return int(textures.size())-1;
    };
    load("black.png",false);
    materials[0].albedo=load("circuit-albedo.png",true);
    materials[0].specular=load("circuit-specular.png",false);
    materials[0].emissive=load("circuit-emissive.png",true);
    materials[1].albedo=load("metal-albedo.png",true);
    materials[1].specular=load("metal-specular.png",false);
    materials[1].emissive=0;
    materials[2].albedo=load("tiles-albedo.png",true);
    materials[2].specular=materials[2].emissive=0;
}
}

bool initialize() {
    try {
        reset();
        auto& context=graphics::internal::context;
        auto points=cubeVertices(); auto faces=cubeIndices();
        uploadDeviceBuffer(vertices,points.data(),sizeof(points),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        uploadDeviceBuffer(indices,faces.data(),sizeof(faces),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        loadMaterials();
        createBuffer(sceneBuffer,sizeof(SceneData),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(context.physical_device,&properties);
        auto alignment=properties.limits.minUniformBufferOffsetAlignment;
        objectStride=(sizeof(ObjectData)+alignment-1)/alignment*alignment;
        VkDescriptorSetLayoutBinding sceneBindings[]={
            {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}
        };
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount=3; layout.pBindings=sceneBindings;
        check(vkCreateDescriptorSetLayout(context.device,&layout,nullptr,&sceneLayout),"Cannot create scene layout");
        VkDescriptorSetLayoutBinding modelBindings[]={
            {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}
        };
        layout.bindingCount=4; layout.pBindings=modelBindings;
        check(vkCreateDescriptorSetLayout(context.device,&layout,nullptr,&objectLayout),"Cannot create material layout");
        VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,3},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,9}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets=4; poolInfo.poolSizeCount=4; poolInfo.pPoolSizes=sizes;
        check(vkCreateDescriptorPool(context.device,&poolInfo,nullptr,&pool),"Cannot create descriptor pool");
        VkDescriptorSetLayout allocationLayouts[]={sceneLayout,objectLayout,objectLayout,objectLayout};
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool=pool; allocate.descriptorSetCount=4; allocate.pSetLayouts=allocationLayouts;
        VkDescriptorSet sets[4]{};
        check(vkAllocateDescriptorSets(context.device,&allocate,sets),"Cannot allocate descriptor sets");
        sceneSet=sets[0];
        for (int i=0;i<3;++i) materials[i].set=sets[i+1];
        VkDescriptorSetLayout layouts[]={sceneLayout,objectLayout};
        VkPushConstantRange pushRange{VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(PushData)};
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        info.setLayoutCount=2; info.pSetLayouts=layouts;
        info.pushConstantRangeCount=1; info.pPushConstantRanges=&pushRange;
        check(vkCreatePipelineLayout(context.device,&info,nullptr,&pipelineLayout),"Cannot create pipeline layout");
        createPipeline(pipeline,false); createPipeline(linePipeline,true);
        ImGui::StyleColorsDark();
        return true;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; shutdown(); return false; }
}
void shutdown() {
    auto& context=graphics::internal::context;
    vkDeviceWaitIdle(context.device);
    vkDestroyPipeline(context.device,linePipeline,nullptr);
    vkDestroyPipeline(context.device,pipeline,nullptr);
    vkDestroyPipelineLayout(context.device,pipelineLayout,nullptr);
    vkDestroyDescriptorPool(context.device,pool,nullptr);
    vkDestroyDescriptorSetLayout(context.device,objectLayout,nullptr);
    vkDestroyDescriptorSetLayout(context.device,sceneLayout,nullptr);
    for (auto* buffer : {&sceneBuffer,&pointBuffer,&spotBuffer,&objectBuffer}) destroyBuffer(*buffer);
    for (auto* buffer : {&vertices,&indices,&lineBuffer}) destroyDeviceBuffer(*buffer);
    for (auto& texture : textures) destroyTexture(texture);
    textures.clear();
}
void update(double time) {
    if (previousTime>=0 && animate) effectTime+=float(std::max(0.0,time-previousTime))*speed;
    previousTime=time;
    ImGui::SetNextWindowPos({16,16},ImGuiCond_Always);
    ImGui::SetNextWindowSize({350,std::max(200.0f,ImGui::GetIO().DisplaySize.y-32)},ImGuiCond_Always);
    ImGui::Begin("Lab 3 - Texturing",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
    ImGui::PushItemWidth(170);
    ImGui::Text("Variant 1 - Cube | Vulkan");
    if (ImGui::Button("Reset scene")) reset();
    ImGui::SameLine();
    if (ImGui::Button("Save image")) graphics::internal::saveScreenshot("texturing.ppm");
    ImGui::Checkbox("Show light sources",&showLights);
    ImGui::Checkbox("Show floor",&showFloor);
    ImGui::SeparatorText("Textures");
    const char* names[]={materials[0].name,materials[1].name,materials[2].name};
    ImGui::Combo("Cube material",&cubeMaterial,names,3);
    ImGui::Combo("Floor material",&floorMaterial,names,3);
    ImGui::Checkbox("Animated sampling",&effectEnabled);
    ImGui::Checkbox("Play",&animate);
    ImGui::SliderFloat("Speed",&speed,0,3);
    ImGui::SliderFloat("Effect strength",&effectStrength,0,1);
    ImGui::Checkbox("Specular map",&useSpecular);
    ImGui::Checkbox("Emissive map",&useEmissive);
    ImGui::SliderFloat("Emission",&emissiveStrength,0,5);
    ImGui::SetNextItemOpen(false,ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::DragFloat3("Camera position",&camera.position.x,0.05f,-30,30,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Yaw",&camera.yaw,-180,180,"%.1f deg");
        ImGui::SliderFloat("Pitch",&camera.pitch,-89,89,"%.1f deg");
        ImGui::SliderFloat("Field of view",&camera.fov,20,100,"%.0f deg");
    }
    if (ImGui::CollapsingHeader("Model and material")) {
        ImGui::DragFloat3("Position",&cube.position.x,0.02f,-5,5,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat3("Rotation",&cube.rotation.x,-180,180,"%.0f");
        ImGui::DragFloat3("Scale",&cube.scale.x,0.01f,0.1f,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("Albedo",&albedo.x);
        ImGui::ColorEdit3("Specular color",&specular.x);
        ImGui::SliderFloat("Shininess",&shininess,1,256,"%.0f");
    }
    if (ImGui::CollapsingHeader("Ambient and directional")) {
        ImGui::ColorEdit3("Ambient color",&ambientColor.x);
        ImGui::SliderFloat("Ambient intensity",&ambientIntensity,0,1);
        ImGui::DragFloat3("Light direction",&direction.x,0.02f,-1,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("Directional color",&directionalColor.x);
        ImGui::SliderFloat("Directional intensity",&directionalIntensity,0,5);
    }
    if (ImGui::CollapsingHeader("Point lights")) {
        if (ImGui::Button("Add point")) { pointLights.push_back(PointLight{}); selectedPoint=int(pointLights.size())-1; }
        ImGui::SameLine();
        if (ImGui::Button("Remove point")&&!pointLights.empty()) { pointLights.erase(pointLights.begin()+selectedPoint); selectedPoint=std::max(0,selectedPoint-1); }
        if (!pointLights.empty()) {
            ImGui::SliderInt("Selected point",&selectedPoint,0,int(pointLights.size())-1);
            auto& light=pointLights[selectedPoint];
            ImGui::DragFloat3("Point position",&light.position.x,0.05f,-10,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
            ImGui::ColorEdit3("Point color",&light.color.x);
            ImGui::SliderFloat("Point intensity",&light.intensity,0,100);
        }
        ImGui::Text("Point count: %d",int(pointLights.size()));
    }
    if (ImGui::CollapsingHeader("Spot lights")) {
        if (ImGui::Button("Add spot")) { spotLights.push_back(SpotLight{}); selectedSpot=int(spotLights.size())-1; }
        ImGui::SameLine();
        if (ImGui::Button("Remove spot")&&!spotLights.empty()) { spotLights.erase(spotLights.begin()+selectedSpot); selectedSpot=std::max(0,selectedSpot-1); }
        if (!spotLights.empty()) {
            ImGui::SliderInt("Selected spot",&selectedSpot,0,int(spotLights.size())-1);
            auto& light=spotLights[selectedSpot];
            ImGui::DragFloat3("Spot position",&light.position.x,0.05f,-10,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat3("Spot direction",&light.direction.x,0.05f,-10,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
            ImGui::ColorEdit3("Spot color",&light.color.x);
            ImGui::SliderFloat("Spot intensity",&light.intensity,0,200);
            ImGui::SliderFloat("Inner angle",&light.innerAngle,0,light.outerAngle,"%.1f deg");
            ImGui::SliderFloat("Outer angle",&light.outerAngle,light.innerAngle,80,"%.1f deg");
        }
        ImGui::Text("Spot count: %d",int(spotLights.size()));
    }
    ImGui::PopItemWidth();
    ImGui::End();
}
void render(const graphics::internal::FrameData& fd) {
    auto& context=graphics::internal::context;
    auto extent=context.swapchain_extent;
    float sidebar=std::min(380.0f*ImGui::GetIO().DisplayFramebufferScale.x,extent.width*0.48f);
    float width=extent.width-sidebar;
    SceneData scene{projectionMatrix(camera.fov,width/extent.height)*viewMatrix(camera),glm::vec4(camera.position,1),glm::vec4(ambientColor,ambientIntensity),glm::vec4(lightDirection(direction),0),glm::vec4(directionalColor,directionalIntensity),{int(pointLights.size()),int(spotLights.size()),0,0}};
    upload(sceneBuffer,&scene,sizeof(scene));
    std::vector<PointData> points;
    for (auto& light : pointLights) points.push_back({glm::vec4(light.position,1),glm::vec4(light.color,light.intensity)});
    std::vector<SpotData> spots;
    for (auto& light : spotLights) spots.push_back({glm::vec4(light.position,1),glm::vec4(light.color,light.intensity),glm::vec4(lightDirection(light.direction),0),{std::cos(glm::radians(light.innerAngle)),std::cos(glm::radians(light.outerAngle)),0,0}});
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(context.physical_device,&properties);
    auto pointSize=std::max(sizeof(PointData),points.size()*sizeof(PointData));
    auto spotSize=std::max(sizeof(SpotData),spots.size()*sizeof(SpotData));
    if (pointSize>properties.limits.maxStorageBufferRange||spotSize>properties.limits.maxStorageBufferRange) throw std::runtime_error("Light arrays exceed device storage buffer limit");
    ensureBuffer(pointBuffer,pointSize,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    ensureBuffer(spotBuffer,spotSize,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    upload(pointBuffer,points.data(),points.size()*sizeof(PointData));
    upload(spotBuffer,spots.data(),spots.size()*sizeof(SpotData));
    std::vector<Draw> draws{{objectData(modelMatrix(cube),albedo,specular,shininess),false,cubeMaterial,effectEnabled,1}};
    if (showFloor) {
        Cube floor; floor.position={0,-1.35f,0}; floor.rotation=glm::vec3(0); floor.scale={10,0.15f,10};
        draws.push_back({objectData(modelMatrix(floor),glm::vec3(1),glm::vec3(0.4f),24),false,floorMaterial,false,6});
    }
    std::vector<Vertex> lines;
    if (showLights) {
        auto marker=[&](glm::vec3 position,glm::vec3 color) {
            auto model=glm::scale(glm::translate(glm::mat4(1),position),glm::vec3(0.13f));
            draws.push_back({objectData(model,color,glm::vec3(0),1,true)});
        };
        for (auto& light : pointLights) marker(light.position,light.color);
        for (auto& light : spotLights) {
            marker(light.position,light.color);
            auto cone=coneLines(light,2);
            lines.insert(lines.end(),cone.begin(),cone.end());
        }
        if (!lines.empty()) draws.push_back({objectData(glm::mat4(1),glm::vec3(1),glm::vec3(0),1,true),true});
    }
    if (draws.size()>std::numeric_limits<uint32_t>::max()/objectStride) throw std::runtime_error("Too many visualization objects");
    ensureBuffer(objectBuffer,draws.size()*objectStride,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    for (size_t i=0;i<draws.size();++i) std::memcpy(static_cast<char*>(objectBuffer.data)+i*objectStride,&draws[i].data,sizeof(ObjectData));
    check(vmaFlushAllocation(context.allocator,objectBuffer.allocation,0,VK_WHOLE_SIZE),"Cannot flush model data");
    if (!lines.empty()) {
        uploadDeviceBuffer(lineBuffer,lines.data(),lines.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
    updateDescriptors();
    auto cmd=fd.command_buffer;
    check(vkResetCommandBuffer(cmd,0),"Cannot reset commands");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(cmd,&begin),"Cannot begin commands");
    VkClearValue clear[2]{}; clear[0].color={{0.035f,0.045f,0.065f,1}}; clear[1].depthStencil={1,0};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass=context.render_pass; pass.framebuffer=fd.framebuffer; pass.renderArea.extent=extent; pass.clearValueCount=2; pass.pClearValues=clear;
    vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{sidebar,0,width,float(extent.height),0,1}; VkRect2D scissor{{0,0},extent};
    vkCmdSetViewport(cmd,0,1,&viewport); vkCmdSetScissor(cmd,0,1,&scissor);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&sceneSet,0,nullptr);
    for (size_t i=0;i<draws.size();++i) {
        auto offset=static_cast<uint32_t>(i*objectStride);
        auto materialSet=materials.at(draws[i].material).set;
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,1,1,&materialSet,1,&offset);
        PushData push{{draws[i].effect?1.0f:0.0f,effectTime,effectStrength,draws[i].tiling},
            {useSpecular?1.0f:0.0f,useEmissive?1.0f:0.0f,emissiveStrength,0}};
        vkCmdPushConstants(cmd,pipelineLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
        VkDeviceSize vertexOffset=0;
        auto buffer=draws[i].lines?lineBuffer.handle:vertices.handle;
        vkCmdBindVertexBuffers(cmd,0,1,&buffer,&vertexOffset);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,draws[i].lines?linePipeline:pipeline);
        if (draws[i].lines) vkCmdDraw(cmd,static_cast<uint32_t>(lines.size()),1,0,0);
        else { vkCmdBindIndexBuffer(cmd,indices.handle,0,VK_INDEX_TYPE_UINT16); vkCmdDrawIndexed(cmd,36,1,0,0,0); }
    }
    vkCmdEndRenderPass(cmd);
    check(vkEndCommandBuffer(cmd),"Cannot end commands");
}
}
