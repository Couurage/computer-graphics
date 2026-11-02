#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_vulkan.h>
#include <map>
#include <filesystem>
#include <tuple>
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
    auto* window = glfwCreateWindow(1280,720,"Lab 3 - render test",nullptr,nullptr);
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
        auto* windowPanel=ImGui::FindWindowByName("Lab 3 - Texturing");
        auto id=windowPanel->GetID(label);
        require(controls.contains(label)||rectangles.contains(id),label);
        auto rect=controls.contains(label)?controls.at(label):rectangles.at(id);
        auto* panel=ImGui::FindWindowByName("Lab 3 - Texturing");
        if (rect.Min.y<panel->Pos.y+40 || rect.Max.y>panel->Pos.y+panel->Size.y-25) {
            ImGui::SetScrollY(panel,panel->Scroll.y+rect.Min.y-panel->Pos.y-55);
            frame(); frame();
            rect=controls.contains(label)?controls.at(label):rectangles.at(id);
        }
        io.AddMousePosEvent(rect.Min.x+(rect.Max.x-rect.Min.x)*fraction,(rect.Min.y+rect.Max.y)/2);
        frame();
        io.AddMouseButtonEvent(0,true); frame();
        io.AddMouseButtonEvent(0,false); frame();
    };
    auto capture=[&](const char* name) {
        graphics::internal::saveScreenshot(name); frame();
    };
    auto imageDifference=[&](const char* first,const char* second) {
        auto read=[](const char* path) {
            std::ifstream f(path,std::ios::binary); std::string magic; int w,h,max;
            f>>magic>>w>>h>>max; f.get();
            std::vector<unsigned char> bytes(size_t(w)*h*3); f.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
            require(bool(f),"Cannot read capture");
            return std::tuple{w,h,bytes};
        };
        auto [w,h,a]=read(first); auto [w2,h2,b]=read(second);
        require(w==w2&&h==h2,"Image dimensions changed");
        double difference=0; size_t count=0;
        for(int y=h/4;y<3*h/4;++y) for(int x=int(w*0.55);x<int(w*0.85);++x) for(int k=0;k<3;++k) {
            auto i=(size_t(y)*w+x)*3+k; difference+=std::abs(int(a[i])-int(b[i])); ++count;
        }
        return difference/count;
    };
    try {
        std::filesystem::create_directories("screenshots");
        frame(); frame(); frame();
        capture("screenshots/all-lights.ppm");
        auto memoryIsLocal=[&](VmaAllocation allocation) {
            VkMemoryPropertyFlags flags=0;
            vmaGetAllocationMemoryProperties(graphics::internal::context.allocator,allocation,&flags);
            require(flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,"Resource is not device-local");
        };
        memoryIsLocal(application::vertices.allocation);
        memoryIsLocal(application::indices.allocation);
        memoryIsLocal(application::lineBuffer.allocation);
        for (auto& texture : application::textures) memoryIsLocal(texture.allocation);
        require(application::textures[0].width==1&&application::textures[0].height==1,"Fallback must be 1x1");
        bool missingRejected=false;
        try { auto t=loadTexture(std::string(TEXTURE_DIR)+"/missing.png",true); destroyTexture(t); }
        catch(const std::exception& e) { missingRejected=std::string(e.what()).find("missing.png")!=std::string::npos; }
        require(missingRejected,"Missing image must give its path");
        capture("screenshots/time-zero.ppm");
        time+=1;
        capture("screenshots/time-one.ppm");
        double animated=imageDifference("screenshots/time-zero.ppm","screenshots/time-one.ppm");
        require(animated>0.5,"Animated texture does not change with time");
        click("Play");
        require(!application::animate,"Pause control");
        float paused=application::effectTime; time+=2; frame();
        require(application::effectTime==paused,"Paused time moved");
        capture("screenshots/paused-a.ppm"); time+=2; capture("screenshots/paused-b.ppm");
        require(imageDifference("screenshots/paused-a.ppm","screenshots/paused-b.ppm")==0,"Paused GPU image changed");
        click("Play"); require(application::animate,"Play control");
        application::speed=0; paused=application::effectTime; time+=2; frame();
        require(application::effectTime==paused,"Zero speed moved time"); application::speed=1;
        click("Animated sampling"); require(!application::effectEnabled,"Effect checkbox");
        capture("screenshots/static-a.ppm"); time+=2; capture("screenshots/static-b.ppm");
        require(imageDifference("screenshots/static-a.ppm","screenshots/static-b.ppm")==0,"Disabled effect still animates");
        click("Cube material"); frame(); click("Metal");
        require(application::cubeMaterial==1,"Cube material combo");
        capture("screenshots/metal.ppm");
        click("Cube material"); frame(); click("Tiles");
        require(application::cubeMaterial==2,"Second cube material combo");
        capture("screenshots/tiles.ppm");
        require(imageDifference("screenshots/metal.ppm","screenshots/tiles.ppm")>1,"Material choice did not reach GPU");
        click("Floor material"); frame(); click("Metal");
        require(application::floorMaterial==1,"Independent floor combo");
        require(application::cubeMaterial==2,"Floor selection changed cube");
        click("Camera"); frame();
        float yaw=application::camera.yaw; click("Yaw",0.5f);
        require(application::camera.yaw!=yaw,"Camera yaw UI");
        float fov=application::camera.fov; click("Field of view",0.7f);
        require(application::camera.fov!=fov,"Camera FOV UI");
        click("Reset scene"); require(application::cubeMaterial==0&&application::floorMaterial==2,"Reset materials");
        require(application::effectEnabled&&application::animate,"Reset effect");
        frame();
        click("Specular map"); require(!application::useSpecular,"Specular map UI");
        click("Emissive map"); require(!application::useEmissive,"Emissive map UI");
        click("Show light sources"); require(!application::showLights,"Light visibility UI");
        click("Show floor"); require(!application::showFloor,"Floor visibility UI");
        click("Point lights"); frame(); click("Add point");
        require(application::pointLights.size()==2,"Add point UI");
        click("Remove point"); click("Remove point"); require(application::pointLights.empty(),"Remove last point UI");
        click("Spot lights"); frame(); click("Add spot");
        require(application::spotLights.size()==2,"Add spot UI");
        click("Remove spot"); click("Remove spot"); require(application::spotLights.empty(),"Remove last spot UI");
        frame();
        application::reset();
        application::camera.position={0,0,5}; application::camera.yaw=-90; application::camera.pitch=0;
        application::cube.rotation=glm::vec3(0); application::cube.scale=glm::vec3(2.5f);
        application::showLights=application::showFloor=false;
        application::effectEnabled=false; application::animate=false;
        application::pointLights.clear(); application::spotLights.clear();
        application::ambientIntensity=application::directionalIntensity=0;
        application::useEmissive=true;
        capture("screenshots/emissive-on.ppm");
        application::useEmissive=false;
        capture("screenshots/emissive-off.ppm");
        double emission=imageDifference("screenshots/emissive-on.ppm","screenshots/emissive-off.ppm");
        require(emission>3,"Emissive texture must glow with all lights off");
        application::useEmissive=true; application::cubeMaterial=2;
        capture("screenshots/fallback-on.ppm");
        application::useEmissive=application::useSpecular=false;
        capture("screenshots/fallback-off.ppm");
        require(imageDifference("screenshots/fallback-on.ppm","screenshots/fallback-off.ppm")==0,"Black fallback emitted light");
        application::direction={0,0,-1}; application::directionalIntensity=2;
        application::cubeMaterial=0; application::albedo=glm::vec3(0);
        application::shininess=8; application::useSpecular=true;
        capture("screenshots/specular-on.ppm");
        application::useSpecular=false;
        capture("screenshots/specular-off.ppm");
        double highlight=imageDifference("screenshots/specular-on.ppm","screenshots/specular-off.ppm");
        require(highlight>3,"Specular map did not affect highlight");
        application::cubeMaterial=2; application::useSpecular=true;
        capture("screenshots/black-specular-on.ppm");
        application::useSpecular=false;
        capture("screenshots/black-specular-off.ppm");
        require(imageDifference("screenshots/black-specular-on.ppm","screenshots/black-specular-off.ppm")==0,"Black specular fallback produced highlight");
        application::reset(); application::pointLights.resize(130); application::spotLights.resize(70); frame();
        require(application::pointBuffer.capacity>=130*sizeof(application::PointData),"Point storage growth");
        require(application::spotBuffer.capacity>=70*sizeof(application::SpotData),"Spot storage growth");
        application::pointLights.clear(); application::spotLights.clear(); frame();
        application::direction=glm::vec3(0); application::cube.scale={0.1f,4,0.1f};
        application::camera.pitch=89; application::camera.fov=20; frame();
        application::camera.pitch=-89; application::camera.fov=100; frame();
        application::reset(); application::spotLights[0].direction=glm::vec3(0);
        application::spotLights[0].innerAngle=application::spotLights[0].outerAngle=20; frame();
        for (auto size : {glm::ivec2(640,480),glm::ivec2(1400,800),glm::ivec2(1280,720)}) {
            glfwSetWindowSize(window,size.x,size.y); glfwPollEvents();
            int w,h; glfwGetFramebufferSize(window,&w,&h); graphics::internal::resize(w,h);
            for(int i=0;i<3;++i) frame();
        }
        application::reset(); application::animate=false; application::effectTime=2;
        auto* panel=ImGui::FindWindowByName("Lab 3 - Texturing");
        for (const char* title : {"Camera","Model and material","Ambient and directional","Point lights","Spot lights"})
            panel->StateStorage.SetInt(panel->GetID(title),0);
        ImGui::SetScrollY(panel,0); frame(); frame();
        capture("screenshots/textured-scene.ppm");
        application::effectEnabled=false; application::cubeMaterial=1;
        capture("screenshots/metal-scene.ppm");
        application::cubeMaterial=0; application::ambientIntensity=application::directionalIntensity=0;
        application::pointLights.clear(); application::spotLights.clear(); application::showLights=false;
        panel->StateStorage.SetInt(panel->GetID("Ambient and directional"),1);
        frame(); frame();
        capture("screenshots/emission-scene.ppm");
        std::cout<<"Mean GPU image difference: animation="<<animated<<", emission="<<emission<<", specular="<<highlight<<'\n';
        std::cout<<"Device-local resources, texture combos, animation/pause, black fallbacks, masks, lighting controls, boundary modes and resize: passed\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    application::shutdown(); graphics::internal::shutdown(); ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate();
    return result;
}
