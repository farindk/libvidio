include(CMakePackageConfigHelpers)

set(libvidio_headers
        vidio.h
        vidio_capturing_loop.h)

set(libvidio_sources
        vidio.cc
        vidio_error.cc
        vidio_error.h
        vidio_frame.cc
        vidio_frame.h
        vidio_input.cc
        vidio_input.h
        vidio_video_format.cc
        vidio_video_format.h
        vidio_format_converter.h
        vidio_format_converter.cc
        colorconversion/converter.h
        colorconversion/converter.cc
        colorconversion/yuv2rgb.h
        colorconversion/yuv2rgb.cc
        colorconversion/mjpeg.h
        colorconversion/mjpeg.cc
        colorconversion/ffmpeg.h
        colorconversion/ffmpeg.cc
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

if (WITH_VIDEO4LINUX2)
    target_compile_definitions(vidio PRIVATE WITH_VIDEO4LINUX2)

    add_subdirectory(v4l)
endif ()

if (WITH_JSON)
    add_subdirectory(third-party/nlohmann-json)
    target_include_directories(vidio PRIVATE third-party/nlohmann-json/include)
    target_compile_definitions(vidio PRIVATE WITH_JSON)
endif ()

if (WITH_RTSP)
    target_compile_definitions(vidio PRIVATE WITH_RTSP)
    add_subdirectory(rtsp)
endif ()

if (FFMPEG_avcodec_FOUND)
    target_compile_definitions(vidio PRIVATE WITH_FFMPEG)
    target_link_libraries(vidio PRIVATE ${FFMPEG_LIBRARIES})
endif ()

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

install(FILES ${libvidio_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(EXPORT ${PROJECT_NAME}-config DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
