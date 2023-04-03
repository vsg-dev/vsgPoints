set(SOURCES
    vsgpoints_raw.cpp
)

add_executable(vsgpoints_raw ${SOURCES})

target_link_libraries(vsgpoints_raw vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsgpoints_raw PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsgpoints_raw vsgXchange::vsgXchange)
endif()

install(TARGETS vsgpoints_raw RUNTIME DESTINATION bin)
