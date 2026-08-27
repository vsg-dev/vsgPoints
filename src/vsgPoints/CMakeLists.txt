
# vars used to enable subdirectories to extend the build of the vsgPoints library in a loosely coupled way
set(EXTRA_DEFINES)
set(EXTRA_INCLUDES)

SET(HEADER_PATH ${VSGPOINTS_SOURCE_DIR}/include/vsgPoints)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/AsciiPoints.h
    ${HEADER_PATH}/BIN.h
    ${HEADER_PATH}/Brick.h
    ${HEADER_PATH}/Bricks.h
    ${HEADER_PATH}/BrickShaderSet.h
    ${HEADER_PATH}/Settings.h
    ${HEADER_PATH}/create.h
    ${HEADER_PATH}/utils/PointsPolytopeIntersector.h
    ${HEADER_PATH}/utils/PolygonIntersector.h
 )

set(SOURCES
    AsciiPoints.cpp
    BIN.cpp
    Brick.cpp
    Bricks.cpp
    BrickShaderSet.cpp
    create.cpp
    utils/PointsPolytopeIntersector.cpp
    utils/PolygonIntersector.cpp
)

# Group utils sub directories
foreach(header ${HEADERS})
    if (header MATCHES ".*/utils/.*")
        source_group("Header Files/utils" FILES ${header})
    endif()
endforeach()

foreach(source ${SOURCES})
    if (source MATCHES "utils/.*")
        # Explicitly group "utils" sources under "Source Files"
        source_group("Source Files/utils" FILES ${source})
    endif()
endforeach()

add_library(vsgPoints ${HEADERS} ${SOURCES})

# add definitions to enable building vsgPoints as part of submodule
add_library(vsgPoints::vsgPoints ALIAS vsgPoints)
set(vsgPoints_FOUND TRUE CACHE INTERNAL "vsgPoints found.")
set(CMAKE_DISABLE_FIND_PACKAGE_vsgPoints TRUE CACHE INTERNAL "Disable find_package(vsgPoints) as it's not necessary.")

set_property(TARGET vsgPoints PROPERTY VERSION ${VSGPOINTS_VERSION_MAJOR}.${VSGPOINTS_VERSION_MINOR}.${VSGPOINTS_VERSION_PATCH})
set_property(TARGET vsgPoints PROPERTY SOVERSION ${VSGPOINTS_SOVERSION})
set_property(TARGET vsgPoints PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgPoints PROPERTY CXX_STANDARD 17)

target_compile_definitions(vsgPoints PRIVATE ${EXTRA_DEFINES})

target_include_directories(vsgPoints PUBLIC
    $<BUILD_INTERFACE:${VSGPOINTS_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${VSGPOINTS_SOURCE_DIR}/include/vsgPoints>
    $<INSTALL_INTERFACE:include>
    ${EXTRA_INCLUDES}
)

target_link_libraries(vsgPoints
    PUBLIC
        vsg::vsg
    PRIVATE
        ${EXTRA_LIBRARIES}
)

install(TARGETS vsgPoints ${INSTALL_TARGETS_DEFAULT_FLAGS})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(vsgPoints INTERFACE VSGPOINTS_SHARED_LIBRARY)
endif()


install(DIRECTORY ${VSGPOINTS_SOURCE_DIR}/include/vsgPoints DESTINATION include)

vsg_add_cmake_support_files(
    CONFIG_TEMPLATE
        vsgPointsConfig.cmake.in
)
