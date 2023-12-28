include(CMakePackageConfigHelpers)

set(libvidio_headers
        vidio.h)

set(libvidio_sources
        vidio.cc
        ${libvidio_headers})

add_library(vidio ${libvidio_sources})

# Needed to find libvidio/vidio_version.h while compiling the library
target_include_directories(vidio PRIVATE ${libvidio_SOURCE_DIR})

# Propagate include/libheif to consume the headers from other projects
#target_include_directories(heif
#        PUBLIC
#        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}>
#        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
#        $<INSTALL_INTERFACE:include>)

set_target_properties(vidio
        PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})

if (APPLE)
    set_target_properties(vidio
            PROPERTIES
	    LINK_FLAGS "-Wl,-compatibility_version,${MACOS_COMPATIBLE_VERSION}")
endif ()

target_compile_definitions(vidio
        PUBLIC
        LIBVIDIO_EXPORTS
        HAVE_VISIBILITY)

target_compile_definitions(vidio PRIVATE VIDIO_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
target_compile_definitions(vidio PRIVATE VIDIO_VERSION_MINOR=${PROJECT_VERSION_MINOR})
target_compile_definitions(vidio PRIVATE VIDIO_VERSION_PATCH=${PROJECT_VERSION_PATCH})

if (ENABLE_MULTITHREADING_SUPPORT)
    find_package(Threads)
    target_link_libraries(vidio PRIVATE ${CMAKE_THREAD_LIBS_INIT})

    target_compile_definitions(vidio PRIVATE ENABLE_MULTITHREADING_SUPPORT=1)
endif ()

write_basic_package_version_file(${PROJECT_NAME}-config-version.cmake COMPATIBILITY ExactVersion)

install(TARGETS vidio EXPORT ${PROJECT_NAME}-config
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )

install(FILES ${vidio_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(EXPORT ${PROJECT_NAME}-config DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
