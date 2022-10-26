set(SOURCES
    vsgpoints_3dc.cpp
)

add_executable(vsgpoints_3dc ${SOURCES})

target_link_libraries(vsgpoints_3dc vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgpoints_3dc PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgpoints_3dc vsgXchange::vsgXchange)
endif()

install(TARGETS vsgpoints_3dc RUNTIME DESTINATION bin)
