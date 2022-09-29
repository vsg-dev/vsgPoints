set(SOURCES
    vsggeneratepoints.cpp
)

add_executable(vsggeneratepoints ${SOURCES})

target_link_libraries(vsggeneratepoints vsg::vsg)

if (vsgXchange_FOUND)
    target_compile_definitions(vsggeneratepoints PRIVATE vsgXchange_FOUND)
    target_link_libraries(vsggeneratepoints vsgXchange::vsgXchange)
endif()

install(TARGETS vsggeneratepoints RUNTIME DESTINATION bin)
