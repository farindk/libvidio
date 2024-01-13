cmake_minimum_required(VERSION 3.16.3) # Oldest Ubuntu LTS (20.04 currently)

project(libvidio LANGUAGES C CXX VERSION 0.0.1)

# https://cmake.org/cmake/help/v3.1/policy/CMP0054.html
cmake_policy(VERSION 3.0...3.22)
include(GNUInstallDirs)

# The version number.
set(PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

# Check for unistd.h

include(${CMAKE_ROOT}/Modules/CheckIncludeFile.cmake)

CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

if (HAVE_UNISTD_H)
    add_definitions(-DHAVE_UNISTD_H)
endif ()


if (NOT MSVC)
    add_definitions(-Wall)
    add_definitions(-Werror)
    add_definitions(-Wsign-compare)
    add_definitions(-Wconversion)
    add_definitions(-Wno-sign-conversion)
    add_definitions(-Wno-error=conversion)
    add_definitions(-Wno-error=unused-parameter)
    add_definitions(-Wno-error=deprecated-declarations)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_definitions(-Wno-error=tautological-compare)
        add_definitions(-Wno-error=tautological-constant-out-of-range-compare)
    endif ()
endif ()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Create the compile command database for clang by default
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wno-error=potentially-evaluated-expression has_potentially_evaluated_expression)
if (has_potentially_evaluated_expression)
    add_definitions(-Wno-error=potentially-evaluated-expression)
endif ()

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")


option(WITH_VIDEO4LINUX2 "Include Video4Linux2 backend" ON)

option(WITH_JSON "Enable JSON serialization" ON)

# ffmpeg

option(WITH_FFMPEG "FFMPEG decoders" ON)
if (WITH_FFMPEG)
    find_package(FFMPEG COMPONENTS avcodec avutil swscale)
endif ()

# --- Create libvidio pkgconfig file

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
if (IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(libdir "${CMAKE_INSTALL_LIBDIR}")
else ()
    set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif ()
if (IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else ()
    set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif ()
if (WITH_FFMPEG)
    list(APPEND REQUIRES_PRIVATE "libavcodec")
endif ()

list(JOIN REQUIRES_PRIVATE " " REQUIRES_PRIVATE)

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(_LIBCPP_VERSION cstdlib HAVE_LIBCPP)
if (HAVE_LIBCPP)
    set(LIBS_PRIVATE "-lc++")
else ()
    set(LIBS_PRIVATE "-lstdc++")
endif ()

configure_file(libvidio.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libvidio.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libvidio.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# ---

option(WITH_EXAMPLES "Build examples" ON)
option(WITH_REDUCED_VISIBILITY "Reduced symbol visibility in library" ON)

if (WITH_REDUCED_VISIBILITY)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
else ()
    set(CMAKE_CXX_VISIBILITY_PRESET default)
endif ()

if (WITH_EXAMPLES)
    add_subdirectory(examples)
endif ()

# --- API documentation

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/libvidio/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target(doc_doxygen ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM)
else (DOXYGEN_FOUND)
    message("Doxygen tool needs to be installed to generate the API documentation")
endif (DOXYGEN_FOUND)

# --- Testing

option(BUILD_TESTING "" OFF)
include(CTest)
if (BUILD_TESTING)
    # TODO: fix tests on windows.
    add_subdirectory(tests)
endif ()


if (CMAKE_CXX_COMPILER MATCHES "clang\\+\\+$")
    add_compile_options(-Wno-tautological-constant-out-of-range-compare)
endif ()

add_subdirectory(libvidio)


# --- packaging (source code)

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_PACKAGE_FILE_NAME libvidio-${PACKAGE_VERSION})
set(CPACK_SOURCE_IGNORE_FILES
        /.git/
        /.github/
        /.gitignore$
        /build/
        /cmake-build.*/
        /.deps/
        /.idea/
        /.clang-tidy
        ~$
        /third-party/.*/ # only exclude the sub-directories, but keep the *.cmd files
        /Testing/
        /logos/
        /Makefile$
        /libtool$
        /libvidio.pc$
        stamp-h1$
)
include(CPack)
