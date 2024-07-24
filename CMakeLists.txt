cmake_minimum_required(VERSION 3.12)

project(vsgPoints
    VERSION 0.6.0
    DESCRIPTION "VulkanSceneGraph Point Cloud rendering."
    LANGUAGES CXX
)
set(VSGPOINTS_SOVERSION 0)
SET(VSGPOINTS_RELEASE_CANDIDATE 0)

set(VSGPOINTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Root source directory of vsgPoints")
set(VSGPOINTS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "Root binary directory of vsgPoints")

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)

# Find Vulkan and the VSG
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(vsg 1.1.4 REQUIRED)
find_package(vsgXchange 1.0.5 REQUIRED)

vsg_setup_build_vars()
vsg_setup_dir_vars()

vsg_add_target_clang_format(
    FILES
        include/vsgPoints/*.h
        src/vsgPoints/*.cpp
)
vsg_add_target_clobber()
vsg_add_target_cppcheck(
    FILES
        ${CMAKE_SOURCE_DIR}/src/vsgPoints/*.cpp
        ${CMAKE_SOURCE_DIR}/include/vsgPoints/*.h
    )
vsg_add_target_docs(
    FILES
        include
)
vsg_add_target_uninstall()

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

    vsg_add_option_maintainer(
        PREFIX v
        RCLEVEL ${VSGPOINTS_RELEASE_CANDIDATE}
    )

endif()

# source directories for vsgPoints library
add_subdirectory(src)
add_subdirectory(applications)

vsg_add_feature_summary()
