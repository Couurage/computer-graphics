#include "application.hpp"
#include "scene.hpp"
#include <imgui.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace application {
namespace {

struct Buffer {
    VkBuffer handle{};
    VmaAllocation allocation{};
    void* data{};
};

struct alignas(16) ObjectData {
    glm::mat4 mvp;
    glm::vec4 tint;
};
static_assert(sizeof(ObjectData) == 80);
static_assert(offsetof(ObjectData, tint) == 64);

Buffer vertices, indices;
std::array<Buffer, 3> uniforms;
std::array<VkDescriptorSet, 3> sets{};
std::array<Cube, 3> cubes;
VkDescriptorSetLayout setLayout{};
VkDescriptorPool pool{};
VkPipelineLayout pipelineLayout{};
VkPipeline pipeline{};
int selected = 0;
int count = 1;
bool perspective = true;
bool colored = true;
bool playing = false;
float speed = 1;
float animationTime = 0;
float fov = 45;
float orthoSize = 3;
float distance = 7;
double lastTime = -1;

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

void createPipeline() {
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
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,color)}
        };
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = 1;
        input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = 2;
        input.pVertexAttributeDescriptions = attributes;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
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
        check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline), "Cannot create pipeline");
    } catch (...) {
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
        throw;
    }
    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
}

void reset() {
    cubes = {};
    cubes[1].position = {-1.8f,0,0};
    cubes[1].color = {0.4f,0.8f,1.0f};
    cubes[2].position = {1.8f,0,0};
    cubes[2].color = {1.0f,0.6f,0.35f};
    for (int i=0;i<3;++i) cubes[i].phase = i*glm::two_pi<float>()/3;
    selected = 0;
    count = 1;
    perspective = colored = true;
    playing = false;
    animationTime = 0;
    speed = 1;
    fov = 45;
    orthoSize = 3;
    distance = 7;
}
}

bool initialize() {
    try {
        reset();
        auto& context = graphics::internal::context;
        auto points = cubeVertices();
        auto faces = cubeIndices();
        createBuffer(vertices, sizeof(points), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, points.data());
        createBuffer(indices, sizeof(faces), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, faces.data());
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount = 1;
        layout.pBindings = &binding;
        check(vkCreateDescriptorSetLayout(context.device, &layout, nullptr, &setLayout), "Cannot create descriptor layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 3;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &size;
        check(vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &pool), "Cannot create descriptor pool");
        VkDescriptorSetLayout layouts[] = {setLayout,setLayout,setLayout};
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool = pool;
        allocate.descriptorSetCount = 3;
        allocate.pSetLayouts = layouts;
        check(vkAllocateDescriptorSets(context.device, &allocate, sets.data()), "Cannot allocate descriptor sets");
        for (int i=0;i<3;++i) {
            createBuffer(uniforms[i], sizeof(ObjectData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            VkDescriptorBufferInfo buffer{uniforms[i].handle,0,sizeof(ObjectData)};
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = sets[i];
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &buffer;
            vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
        }
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        info.setLayoutCount = 1;
        info.pSetLayouts = &setLayout;
        check(vkCreatePipelineLayout(context.device, &info, nullptr, &pipelineLayout), "Cannot create pipeline layout");
        createPipeline();
        ImGui::StyleColorsDark();
        return true;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        shutdown();
        return false;
    }
}

void shutdown() {
    auto& context = graphics::internal::context;
    vkDeviceWaitIdle(context.device);
    vkDestroyPipeline(context.device, pipeline, nullptr);
    vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(context.device, pool, nullptr);
    vkDestroyDescriptorSetLayout(context.device, setLayout, nullptr);
    for (auto& buffer : uniforms) vmaDestroyBuffer(context.allocator, buffer.handle, buffer.allocation);
    vmaDestroyBuffer(context.allocator, indices.handle, indices.allocation);
    vmaDestroyBuffer(context.allocator, vertices.handle, vertices.allocation);
}

void update(double time) {
    double elapsed = lastTime < 0 ? 0 : time-lastTime;
    lastTime = time;
    advanceTime(animationTime, elapsed, playing, speed);
    ImGui::SetNextWindowPos({16,16}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({340,650}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Lab 1 - Cube");
    ImGui::Text("Variant 1 | Vulkan");
    ImGui::Separator();
    ImGui::Checkbox("Perspective", &perspective);
    if (perspective) ImGui::SliderFloat("Field of view", &fov,20,90,"%.0f deg");
    else ImGui::SliderFloat("View size", &orthoSize,1,8);
    ImGui::SliderFloat("Camera distance", &distance,3,15);
    ImGui::Separator();
    ImGui::SliderInt("Cubes", &count,1,3);
    if (selected >= count) selected = count-1;
    for (int i=0;i<count;++i) {
        if (i) ImGui::SameLine();
        std::string label = "Cube " + std::to_string(i+1);
        ImGui::RadioButton(label.c_str(), &selected,i);
    }
    auto& cube = cubes[selected];
    ImGui::DragFloat3("Position", &cube.position.x,0.02f,-5,5,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat3("Rotation", &cube.rotation.x,-180,180,"%.0f");
    ImGui::DragFloat3("Scale", &cube.scale.x,0.01f,0.1f,3,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    ImGui::ColorEdit3("Color", &cube.color.x);
    ImGui::Checkbox("Vertex colors", &colored);
    ImGui::Separator();
    ImGui::Checkbox("Animate selected cube", &cube.animated);
    ImGui::SliderFloat("Path radius", &cube.radius,0,3);
    ImGui::SliderFloat("Path height", &cube.height,0,2);
    ImGui::SliderAngle("Phase", &cube.phase,0,360);
    if (ImGui::Button(playing ? "Pause" : "Play")) playing = !playing;
    ImGui::SameLine();
    if (ImGui::Button("Restart")) animationTime = 0;
    ImGui::SliderFloat("Speed", &speed,0.1f,3,"%.1fx");
    ImGui::Text("Time: %.2f s", animationTime);
    ImGui::Separator();
    if (ImGui::Button("Reset all")) reset();
    ImGui::SameLine();
    if (ImGui::Button("Save image")) graphics::internal::saveScreenshot("cube.ppm");
    ImGui::TextWrapped("Select a cube to edit it. Enable animation for that cube, then press Play.");
    ImGui::End();
}

void render(const graphics::internal::FrameData& fd) {
    auto& context = graphics::internal::context;
    auto extent = context.swapchain_extent;
    auto view = glm::lookAt(glm::vec3(0,1.8f,distance), glm::vec3(0), glm::vec3(0,1,0));
    float sidebar = std::min(360.0f * ImGui::GetIO().DisplayFramebufferScale.x, extent.width*0.45f);
    float width = extent.width-sidebar;
    auto projection = projectionMatrix(perspective,width/extent.height,perspective ? fov : orthoSize);
    for (int i=0;i<count;++i) {
        ObjectData data{projection*view*modelMatrix(cubes[i],animationTime),glm::vec4(cubes[i].color,colored ? 1 : 0)};
        std::memcpy(uniforms[i].data, &data,sizeof(data));
        check(vmaFlushAllocation(context.allocator,uniforms[i].allocation,0,sizeof(data)),"Cannot flush uniform");
    }
    auto cmd = fd.command_buffer;
    check(vkResetCommandBuffer(cmd,0),"Cannot reset command buffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(cmd,&begin),"Cannot begin command buffer");
    VkClearValue clear[2]{};
    clear[0].color = {{0.045f,0.055f,0.08f,1}};
    clear[1].depthStencil = {1,0};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = context.render_pass;
    pass.framebuffer = fd.framebuffer;
    pass.renderArea.extent = extent;
    pass.clearValueCount = 2;
    pass.pClearValues = clear;
    vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{sidebar,0,width,float(extent.height),0,1};
    VkRect2D scissor{{0,0},extent};
    vkCmdSetViewport(cmd,0,1,&viewport);
    vkCmdSetScissor(cmd,0,1,&scissor);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd,0,1,&vertices.handle,&offset);
    vkCmdBindIndexBuffer(cmd,indices.handle,0,VK_INDEX_TYPE_UINT16);
    for (int i=0;i<count;++i) {
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&sets[i],0,nullptr);
        vkCmdDrawIndexed(cmd,36,1,0,0,0);
    }
    vkCmdEndRenderPass(cmd);
    check(vkEndCommandBuffer(cmd),"Cannot end command buffer");
}
}
