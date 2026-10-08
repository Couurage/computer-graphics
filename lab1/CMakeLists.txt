cmake_minimum_required(VERSION 3.20)
include(FetchContent)

project(vulkan-starter-app LANGUAGES C CXX)

add_executable(${PROJECT_NAME} source/main.cpp source/graphics_internal.cpp source/application.cpp source/scene.cpp)

if(LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
elseif(APPLE)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
elseif(WIN32)
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4201)
		target_link_options(${PROJECT_NAME} PRIVATE /subsystem:windows /entry:mainCRTStartup)
	else()
		target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
		target_link_options(${PROJECT_NAME} PRIVATE -mwindows)
	endif()
	target_compile_definitions(${PROJECT_NAME} PRIVATE -D_USE_MATH_DEFINES)
else()
	message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED TRUE CXX_STANDARD 20)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.5.1
)

FetchContent_Declare(
	vk-bootstrap
	GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
	GIT_TAG v1.4.321
)

FetchContent_Declare(
	VulkanMemoryAllocator
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
	GIT_TAG v3.4.0
)

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG v1.92.9
)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
find_package(Vulkan REQUIRED)

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)

FetchContent_MakeAvailable(
	glfw
	vk-bootstrap
	VulkanMemoryAllocator
	imgui
    glm
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	glfw
    glm::glm
	Vulkan::Vulkan
	vk-bootstrap::vk-bootstrap
	GPUOpen::VulkanMemoryAllocator
)

target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_sources(${PROJECT_NAME} PRIVATE
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
)

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
foreach(SHADER cube.vert cube.frag)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER}.spv"
        COMMAND "${GLSLC}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER}.spv"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}"
        VERBATIM
    )
    list(APPEND SHADER_BINARIES "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER}.spv")
endforeach()
add_custom_target(shaders DEPENDS ${SHADER_BINARIES})
add_dependencies(${PROJECT_NAME} shaders)
target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders")
include(CTest)
if(BUILD_TESTING)
    add_executable(scene_tests tests/scene_tests.cpp source/scene.cpp)
    target_include_directories(scene_tests PRIVATE source)
    target_link_libraries(scene_tests PRIVATE glm::glm)
    target_compile_features(scene_tests PRIVATE cxx_std_20)
    add_test(NAME scene COMMAND scene_tests)
endif()

option(RENDER_TEST "Build interactive Vulkan checks" OFF)
if(RENDER_TEST)
    get_target_property(APP_SOURCES ${PROJECT_NAME} SOURCES)
    list(REMOVE_ITEM APP_SOURCES source/main.cpp source/application.cpp)
    add_executable(render_test tests/render_test.cpp ${APP_SOURCES})
    get_target_property(APP_INCLUDES ${PROJECT_NAME} INCLUDE_DIRECTORIES)
    get_target_property(APP_LIBRARIES ${PROJECT_NAME} LINK_LIBRARIES)
    get_target_property(APP_DEFINITIONS ${PROJECT_NAME} COMPILE_DEFINITIONS)
    target_include_directories(render_test PRIVATE ${APP_INCLUDES} source)
    target_link_libraries(render_test PRIVATE ${APP_LIBRARIES})
    target_compile_definitions(render_test PRIVATE ${APP_DEFINITIONS} IMGUI_ENABLE_TEST_ENGINE)
    target_compile_features(render_test PRIVATE cxx_std_20)
    add_dependencies(render_test shaders)
endif()
