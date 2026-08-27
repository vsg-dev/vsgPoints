set(SOURCES
    pointselection.cpp
)

add_executable(pointselection ${SOURCES})

target_link_libraries(pointselection vsg::vsg vsgPoints::vsgPoints)

if (vsgXchange_FOUND)
    target_compile_definitions(pointselection PRIVATE vsgXchange_FOUND)
    target_link_libraries(pointselection vsgXchange::vsgXchange)
endif()

install(TARGETS pointselection RUNTIME DESTINATION bin)
