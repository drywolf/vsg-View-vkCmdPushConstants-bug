cmake_minimum_required(VERSION 3.24)

set(USE_VCPKG TRUE)

if(USE_VCPKG)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(vsg-View-PushConst-bug)

if(USE_VCPKG)
    find_package(vsg CONFIG REQUIRED)
    link_libraries(vsg::vsg)
else()
    find_package(Vulkan)
    set(VSG_src_dir "your_path_to_VulkanSceneGraph_sources")
    link_directories(
        "${VSG_src_dir}/build/lib"
    )
    include_directories(
        "${VSG_src_dir}/include"
        "${VSG_src_dir}/build/include"
    )
    link_libraries(Vulkan::Vulkan vsgd.lib)
endif()

add_executable(vsg-View-PushConst-bug vsg-View-PushConst-bug.cpp)
add_executable(vsg-View-PushConst-bug-minimal vsg-View-PushConst-bug-minimal.cpp)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT vsg-View-PushConst-bug-minimal)
