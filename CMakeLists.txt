cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
project(sapienvulkan2 LANGUAGES C CXX CUDA)
set(CMAKE_CXX_STANDARD 20)

option(SVULKAN2_SHARED "Force svulkan2 to be shared" ON)
option(SVULKAN2_PROFILE "Build with profiler" OFF)
option(SVULKAN2_BUILD_TEST "Build with unit test" OFF)
option(SVULKAN2_CUDA_INTEROP "Allow CUDA to use Vulkan buffer" ON)

if(MSVC)
    add_compile_options("/W4" "$<$<CONFIG:Release>:/O2>")
else()
    add_compile_options("-Wall"
        "$<$<CONFIG:Debug>:-g3>" "$<$<CONFIG:Debug>:-O0>"
        "$<$<CONFIG:Release>:-O3>")
endif()

add_compile_definitions("$<$<CONFIG:DEBUG>:_DEBUG>")
add_compile_definitions("$<$<CONFIG:DEBUG>:VK_VALIDATION>")
add_compile_definitions("$<$<NOT:$<CONFIG:Debug>>:NDEBUG>")

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(imgui)

find_package(glm REQUIRED)
find_package(assimp REQUIRED)
find_package(spdlog REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Ktx REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(glslang REQUIRED)
find_package(SPIRV-Tools REQUIRED)
find_package(spirv_cross_core REQUIRED)
find_package(spirv_cross_glsl REQUIRED)
find_package(spirv_cross_cpp REQUIRED)
find_package(OpenEXR REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_check_modules(OPENVR REQUIRED openvr)
include_directories(${OPENVR_INCLUDE_DIRS})
link_directories(${OPENVR_LIBRARY_DIRS})

pkg_check_modules(CUDART REQUIRED cudart)
include_directories(${CUDART_INCLUDE_DIRS})
link_directories(${CUDART_LIBRARY_DIRS})

file(GLOB_RECURSE SVULKAN2_SRC "src/*.cpp")

if(SVULKAN2_SHARED)
    add_library(svulkan2 SHARED ${SVULKAN2_SRC})
else()
    add_library(svulkan2 ${SVULKAN2_SRC})
endif()

set_target_properties(svulkan2 PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions(imgui PRIVATE VK_NO_PROTOTYPES)
target_compile_definitions(svulkan2 PUBLIC VK_NO_PROTOTYPES)

target_include_directories(svulkan2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(svulkan2 PRIVATE
    "3rd_party/stb" "3rd_party/OptiX7SDK/include"
    "3rd_party/tinygltf"
)
target_link_libraries(svulkan2 PUBLIC
    Vulkan::Headers glm::glm
)

find_package(Threads REQUIRED)
target_link_libraries(svulkan2 PRIVATE
    assimp::assimp glslang::glslang glslang::SPIRV
    spirv-cross-cpp glfw spdlog::spdlog KTX::ktx
    imgui ${CMAKE_DL_LIBS} Threads::Threads
    OpenEXR ${CUDART_LIBRARIES}
)
if (MSVC)
    target_link_libraries(svulkan2 PRIVATE openvr_api64)
else()
    target_link_libraries(svulkan2 PRIVATE ${OPENVR_LIBRARIES})
endif()

if (${SVULKAN2_PROFILE})
    target_compile_definitions(svulkan2 PUBLIC SVULKAN2_PROFILE)
else()
    target_compile_definitions(svulkan2 PRIVATE NVTX_DISABLE=1)
endif()


if (${SVULKAN2_CUDA_INTEROP})
    target_compile_definitions(svulkan2 PUBLIC SVULKAN2_CUDA_INTEROP)
    target_include_directories(svulkan2 PRIVATE "$ENV{CUDA_PATH}/include")
    if (MSVC)
        target_link_libraries(svulkan2 PRIVATE "$ENV{CUDA_PATH}/lib/x64/cudart_static.lib")
    endif()

    target_link_libraries(svulkan2 PRIVATE OpenImageDenoise)
endif()

# TODO: add option to control the following variables
# this library exports the storage api
target_compile_definitions(svulkan2 PRIVATE VULKAN_HPP_STORAGE_SHARED_EXPORT)
# other libraries use the exported storage api
target_compile_definitions(svulkan2 PUBLIC VULKAN_HPP_STORAGE_SHARED)

if (${SVULKAN2_CUDA_INTEROP})
    install(TARGETS svulkan2
        EXPORT svulkan2Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
else ()
    install(TARGETS svulkan2
        EXPORT svulkan2Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif ()

install(EXPORT svulkan2Targets
    FILE svulkan2Config.cmake
    NAMESPACE svulkan2::
    DESTINATION lib/cmake/svulkan2
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/svulkan2 DESTINATION "include")

add_executable(svulkan2_main app/main.cpp)
target_link_libraries(svulkan2_main svulkan2 glfw imgui)
if (UNIX)
    target_link_libraries(svulkan2_main stdc++fs)
endif()

add_executable(svulkan2_vr app/vr.cpp)
target_link_libraries(svulkan2_vr svulkan2 glfw imgui)
if (UNIX)
    target_link_libraries(svulkan2_vr stdc++fs)
endif()

add_executable(gencube app/gencube.cpp)
target_link_libraries(gencube svulkan2)

add_executable(glslc_embed app/glslc_embed.cpp)
target_link_libraries(glslc_embed svulkan2)
if (UNIX)
    target_link_libraries(glslc_embed stdc++fs)
endif()
