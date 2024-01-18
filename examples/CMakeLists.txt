include_directories(${libvidio_BINARY_DIR} ${libvidio_SOURCE_DIR} ${libvidio_SOURCE_DIR}/include)

if (MSVC)
    set(getopt_sources
            extra/getopt.c
            extra/getopt.h
            extra/getopt_long.c
    )
    include_directories("extra")
endif ()

add_executable(vidio-grab ${getopt_sources}
        vidio_grab.cc
)

if (SDL2_FOUND)
    target_sources(vidio-grab PRIVATE sdl_window.h sdl_window.cc)
    target_compile_definitions(vidio-grab PRIVATE WITH_SDL)
    target_include_directories(vidio-grab PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(vidio-grab PRIVATE ${SDL2_LIBRARIES})
endif ()

target_link_libraries(vidio-grab PRIVATE vidio)
install(TARGETS vidio-grab RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# install(FILES vidio-grab.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
