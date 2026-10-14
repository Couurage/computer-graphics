#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_vulkan.h>
#include <map>
#include <filesystem>
#include "../source/application.cpp"

std::map<ImGuiID, ImRect> rectangles;
std::map<std::string, ImRect> controls;

void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID id, const ImRect& rect, const ImGuiLastItemData*) { rectangles[id] = rect; }
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags) { controls[label] = rectangles[id]; }
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return ""; }

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
    require(glfwInit(), "GLFW failed");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto* window = glfwCreateWindow(1280,720,"Lab 2 - render test",nullptr,nullptr);
    require(window, "Window failed");
    ImGui::CreateContext();
    GImGui->TestEngineHookItems = true;
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280,720};
    io.DeltaTime = 1.0f/60;
    require(graphics::internal::initialize(window),"Vulkan failed");
    require(application::initialize(),"Application failed");
    int result = 0;
    double time = 0;
    auto frame = [&] {
        glfwPollEvents();
        int width,height,fw,fh;
        glfwGetWindowSize(window,&width,&height);
        glfwGetFramebufferSize(window,&fw,&fh);
        io.DisplaySize = {float(width),float(height)};
        io.DisplayFramebufferScale = {float(fw)/width,float(fh)/height};
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
        time += 1.0/60;
        application::update(time);
        ImGui::Render();
        auto fd = graphics::internal::prepare();
        require(fd.command_buffer,"Acquire failed");
        application::render(fd);
        graphics::internal::submitAndPresent();
    };
    auto click = [&](const char* label, float fraction=0.15f) {
        require(controls.contains(label),label);
        auto rect = controls.at(label);
        auto* panel=ImGui::FindWindowByName("Lab 2 - Camera and lighting");
        if (rect.Min.y<panel->Pos.y+40 || rect.Max.y>panel->Pos.y+panel->Size.y-25) {
            ImGui::SetScrollY(panel,panel->Scroll.y+rect.Min.y-panel->Pos.y-55);
            frame(); frame();
            rect=controls.at(label);
        }
        io.AddMousePosEvent(rect.Min.x+(rect.Max.x-rect.Min.x)*fraction,(rect.Min.y+rect.Max.y)/2);
        frame();
        io.AddMouseButtonEvent(0,true); frame();
        io.AddMouseButtonEvent(0,false); frame();
    };
    auto capture=[&](const char* name) {
        graphics::internal::saveScreenshot(name); frame();
    };
    auto irradiance=[&](const char* path) {
        std::ifstream file(path,std::ios::binary);
        std::string magic; int w,h,max; file>>magic>>w>>h>>max; file.get();
        require(magic=="P6"&&max==255,"Invalid screenshot");
        std::vector<unsigned char> data(size_t(w)*h*3);
        file.read(reinterpret_cast<char*>(data.data()),data.size());
        require(bool(file),"Incomplete screenshot");
        float side=std::min(380.0f*io.DisplayFramebufferScale.x,w*0.48f);
        int x=int(side+(w-side)/2),y=h/2;
        float color=0;
        for (int dx=-2;dx<=2;++dx) for (int dy=-2;dy<=2;++dy) color+=data[((y+dy)*w+x+dx)*3]/255.0f;
        color/=25;
        float linear=color<=0.04045f?color/12.92f:std::pow((color+0.055f)/1.055f,2.4f);
        return linear/(1-linear);
    };
    try {
        std::filesystem::create_directories("screenshots");
        frame(); frame(); frame();
        capture("screenshots/all-lights.ppm");
        float oldYaw=application::camera.yaw;
        click("Yaw",0.5f);
        require(application::camera.yaw!=oldYaw,"Camera rotation UI");
        float oldFov=application::camera.fov;
        click("Field of view",0.7f);
        require(application::camera.fov!=oldFov,"Camera FOV UI");
        application::reset(); frame();
        click("Show light sources");
        require(!application::showLights,"Marker checkbox");
        click("Show light sources");
        click("Point lights"); frame();
        click("Add point");
        require(application::pointLights.size()==2,"Add point");
        require(application::selectedPoint==1,"Select new point");
        click("Remove point");
        require(application::pointLights.size()==1,"Remove point");
        click("Remove point");
        require(application::pointLights.empty(),"Remove last point");
        frame();
        click("Add point");
        click("Spot lights"); frame();
        click("Add spot");
        require(application::spotLights.size()==2,"Add spot");
        click("Remove spot");
        click("Remove spot");
        require(application::spotLights.empty(),"Remove last spot");
        frame();
        click("Add spot");
        application::pointLights.resize(130);
        application::spotLights.resize(70);
        frame(); frame();
        require(application::pointBuffer.capacity>=130*sizeof(application::PointData),"Point storage did not grow");
        require(application::spotBuffer.capacity>=70*sizeof(application::SpotData),"Spot storage did not grow");
        application::pointLights.clear(); application::spotLights.clear(); frame();
        application::reset();
        application::camera.position={0,0,5}; application::camera.yaw=-90; application::camera.pitch=0;
        application::cube.rotation=glm::vec3(0); application::cube.scale=glm::vec3(1);
        application::showLights=application::showFloor=false;
        application::ambientIntensity=application::directionalIntensity=0;
        application::albedo=glm::vec3(1); application::specular=glm::vec3(0);
        application::spotLights.clear();
        application::pointLights={{glm::vec3(0,0,2.5f),glm::vec3(1),4}};
        capture("screenshots/point-near.ppm");
        auto near=irradiance("screenshots/point-near.ppm");
        application::pointLights[0].position.z=4.5f;
        capture("screenshots/point-far.ppm");
        auto far=irradiance("screenshots/point-far.ppm");
        require(std::abs(near/far-4)<0.2f,"GPU inverse-square attenuation");
        std::cout<<"Point irradiance ratio (expected 4): "<<near/far<<'\n';
        application::pointLights.clear();
        SpotLight spot; spot.position={0,0,2.5f}; spot.direction={0,0,-1}; spot.color=glm::vec3(1); spot.intensity=4;
        application::spotLights={spot};
        capture("screenshots/spot-axis.ppm");
        auto axis=irradiance("screenshots/spot-axis.ppm");
        application::spotLights[0].position.z=4.5f;
        capture("screenshots/spot-far.ppm");
        float spotFar=irradiance("screenshots/spot-far.ppm");
        require(std::abs(axis/spotFar-4)<0.2f,"GPU spot inverse-square attenuation");
        application::spotLights[0].position.z=2.5f;
        application::spotLights[0].direction={std::sin(glm::radians(23.0f)),0,-std::cos(glm::radians(23.0f))};
        capture("screenshots/spot-edge.ppm");
        auto edge=irradiance("screenshots/spot-edge.ppm");
        require(edge/axis>0.3f&&edge/axis<0.7f,"GPU soft spot edge");
        application::spotLights[0].direction={1,0,0};
        capture("screenshots/spot-outside.ppm");
        require(irradiance("screenshots/spot-outside.ppm")<0.01f,"Spot outside cone must be dark");
        application::spotLights[0].direction={0,0,-1};
        application::spotLights[0].innerAngle=application::spotLights[0].outerAngle=20;
        frame();
        application::spotLights.clear();
        application::pointLights={PointLight{}};
        application::pointLights[0].position={0,0,2.5f};
        application::pointLights[0].color={0.2f,0.4f,0.6f};
        application::pointLights[0].intensity=0;
        application::showLights=true;
        capture("screenshots/marker-dark.ppm");
        float marker=irradiance("screenshots/marker-dark.ppm");
        application::pointLights[0].intensity=100;
        application::directionalIntensity=5;
        capture("screenshots/marker-lit.ppm");
        require(std::abs(marker-irradiance("screenshots/marker-lit.ppm"))<0.0001f,"Light markers must not receive lighting");
        application::reset();
        auto* panel=ImGui::FindWindowByName("Lab 2 - Camera and lighting");
        ImGui::SetScrollY(panel,0); frame(); frame();
        capture("screenshots/all-lights.ppm");
        application::pointLights.clear(); application::spotLights.clear();
        capture("screenshots/directional.ppm");
        application::reset(); application::directionalIntensity=0; application::pointLights.clear();
        capture("screenshots/spotlight.ppm");
        application::reset(); application::camera.yaw+=20; application::camera.pitch-=10; application::camera.fov=65;
        frame();
        for (auto size : {glm::ivec2(900,600),glm::ivec2(1400,800),glm::ivec2(1280,720)}) {
            glfwSetWindowSize(window,size.x,size.y); glfwPollEvents();
            int width,height; glfwGetFramebufferSize(window,&width,&height);
            graphics::internal::resize(width,height);
            for (int i=0;i<3;++i) frame();
        }
        std::cout<<"Real Vulkan render, UI add/remove, dynamic storage, inverse-square attenuation, spot edges and resize: passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    application::shutdown(); graphics::internal::shutdown(); ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate();
    return result;
}
