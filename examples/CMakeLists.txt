include_directories(${libvidio_BINARY_DIR} ${libvidio_SOURCE_DIR} ${libvidio_SOURCE_DIR}/include)

if (MSVC)
    set(getopt_sources
            extra/getopt.c
            extra/getopt.h
            extra/getopt_long.c
    )
    include_directories("extra")
endif ()

add_executable(vidio-info ${getopt_sources}
        vidio_info.cc
)

option(ENABLE_SDL2 "Enable SDL2" ON)
if (ENABLE_SDL2)
    find_package(SDL2)
endif()


if (SDL2_FOUND)
    target_sources(vidio-info PRIVATE sdl_window.h sdl_window.cc)
    target_compile_definitions(vidio-info PRIVATE WITH_SDL2)
    target_include_directories(vidio-info PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(vidio-info PRIVATE ${SDL2_LIBRARIES})
endif ()

target_link_libraries(vidio-info PRIVATE vidio)
install(TARGETS vidio-info RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# install(FILES vidio-info.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
