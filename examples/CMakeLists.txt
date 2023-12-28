include_directories(${libvidio_BINARY_DIR} ${libvidio_SOURCE_DIR} ${libvidio_SOURCE_DIR}/include)

if (MSVC)
    set(getopt_sources
            ../extra/getopt.c
            ../extra/getopt.h
            ../extra/getopt_long.c
            )
    include_directories("../extra")
endif ()

add_executable(vidio-info ${getopt_sources}
        vidio_info.cc)
target_link_libraries(vidio-info vidio)
install(TARGETS vidio-info RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# install(FILES vidio-info.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
