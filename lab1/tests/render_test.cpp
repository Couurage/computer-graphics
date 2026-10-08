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
    auto* window = glfwCreateWindow(1280,720,"Lab 1 - render test",nullptr,nullptr);
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
        io.AddMousePosEvent(rect.Min.x+(rect.Max.x-rect.Min.x)*fraction,(rect.Min.y+rect.Max.y)/2);
        frame();
        io.AddMouseButtonEvent(0,true); frame();
        io.AddMouseButtonEvent(0,false); frame();
    };
    try {
        std::filesystem::create_directories("screenshots");
        frame(); frame(); frame();
        graphics::internal::saveScreenshot("screenshots/perspective.ppm"); frame();
        click("Perspective");
        require(!application::perspective,"Projection checkbox failed");
        graphics::internal::saveScreenshot("screenshots/orthographic.ppm"); frame();
        click("Vertex colors");
        require(!application::colored,"Vertex colors checkbox failed");
        click("Vertex colors");
        click("Cubes",0.55f);
        require(application::count >= 2,"Cube count slider failed");
        click("Cube 2");
        require(application::selected==1,"Object selection failed");
        click("Animate selected cube");
        require(application::cubes[1].animated,"Animation checkbox failed");
        click("Play");
        require(application::playing,"Play button failed");
        for (int i=0;i<45;++i) frame();
        require(application::animationTime>0.7f,"Animation does not advance");
        click("Pause");
        auto frozen = application::animationTime;
        for (int i=0;i<5;++i) frame();
        require(application::animationTime==frozen,"Pause does not freeze animation");
        graphics::internal::saveScreenshot("screenshots/animation.ppm"); frame();
        click("Restart");
        require(application::animationTime==0,"Restart failed");
        click("Reset all");
        require(application::count==1 && application::perspective && !application::playing,"Reset failed");
        click("Cubes",0.7f);
        require(application::count==3,"Cannot select three cubes");
        graphics::internal::saveScreenshot("screenshots/multiple.ppm"); frame();
        auto edit = [&](const char* label, const char* value) {
            auto* window = ImGui::FindWindowByName("Lab 1 - Cube");
            auto parent = ImHashStr(label,0,window->IDStack[0]);
            int component = 0;
            auto child = ImHashData(&component,sizeof(component),parent);
            auto id = ImHashStr("",0,child);
            require(rectangles.contains(id),"Missing component field");
            auto rect = rectangles.at(id);
            io.AddMousePosEvent(rect.Min.x+20,(rect.Min.y+rect.Max.y)/2); frame();
            io.AddKeyEvent(io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl,true);
            io.AddMouseButtonEvent(0,true); frame();
            io.AddMouseButtonEvent(0,false);
            io.AddKeyEvent(io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl,false); frame();
            io.AddInputCharactersUTF8(value); frame();
            io.AddKeyEvent(ImGuiKey_Enter,true); frame();
            io.AddKeyEvent(ImGuiKey_Enter,false); frame();
        };
        edit("Position","1.2");
        require(std::abs(application::cubes[0].position.x-1.2f)<0.01f,"Position input failed");
        edit("Rotation","60");
        require(std::abs(application::cubes[0].rotation.x-60)<0.01f,"Rotation input failed");
        edit("Scale","1.4");
        require(std::abs(application::cubes[0].scale.x-1.4f)<0.01f,"Scale input failed");
        click("Reset all");
        for (auto size : {glm::ivec2(900,600),glm::ivec2(1400,800),glm::ivec2(1280,720)}) {
            glfwSetWindowSize(window,size.x,size.y);
            glfwPollEvents();
            int width,height;
            glfwGetFramebufferSize(window,&width,&height);
            graphics::internal::resize(width,height);
            for (int i=0;i<5;++i) frame();
        }
        std::cout << "Render, UI controls, animation and resize: passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result=1; }
    application::shutdown();
    graphics::internal::shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
