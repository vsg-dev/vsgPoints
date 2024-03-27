set(SOURCES
    ConvertMeshToPoints.cpp
    vsgpoints_example.cpp
)

add_executable(vsgpoints_example ${SOURCES})

target_link_libraries(vsgpoints_example vsg::vsg vsgPoints::vsgPoints)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgpoints_example PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgpoints_example vsgXchange::vsgXchange)
endif()

install(TARGETS vsgpoints_example RUNTIME DESTINATION bin)
