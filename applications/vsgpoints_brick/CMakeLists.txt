set(SOURCES
    vsgpoints_brick.cpp
)

add_executable(vsgpoints_brick ${SOURCES})

target_link_libraries(vsgpoints_brick vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgpoints_brick PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgpoints_brick vsgXchange::vsgXchange)
endif()

install(TARGETS vsgpoints_brick RUNTIME DESTINATION bin)
