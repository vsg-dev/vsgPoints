
# vars used to enable subdirectories to extend the build of the vsgPoints library in a loose coupled way
set(EXTRA_DEFINES)
set(EXTRA_INCLUDES)

SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/vsgPoints)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/BrickBuilder.h
 )

set(SOURCES
    BrickBuilder.cpp
)

add_library(vsgPoints ${HEADERS} ${SOURCES})
add_library(vsgPoints::vsgPoints ALIAS vsgPoints)

set_property(TARGET vsgPoints PROPERTY VERSION ${VSGPOINTS_VERSION_MAJOR}.${VSGPOINTS_VERSION_MINOR}.${VSGPOINTS_VERSION_PATCH})
set_property(TARGET vsgPoints PROPERTY SOVERSION ${VSGPOINTS_SOVERSION})
set_property(TARGET vsgPoints PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgPoints PROPERTY CXX_STANDARD 17)

target_compile_definitions(vsgPoints PRIVATE ${EXTRA_DEFINES})

target_include_directories(vsgPoints PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/vsgPoints>
    $<INSTALL_INTERFACE:include>
    ${GDAL_INCLUDE_DIR}
    ${EXTRA_INCLUDES}
)

target_link_libraries(vsgPoints
    PUBLIC
        vsg::vsg
    PRIVATE
        ${GDAL_LIBRARY}
        ${EXTRA_LIBRARIES}
)

install(TARGETS vsgPoints ${INSTALL_TARGETS_DEFAULT_FLAGS})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(vsgPoints INTERFACE VSGPOINTS_SHARED_LIBRARY)
endif()


install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/vsgPoints DESTINATION include)

vsg_add_cmake_support_files(
    CONFIG_TEMPLATE
        vsgPointsConfig.cmake.in
)
