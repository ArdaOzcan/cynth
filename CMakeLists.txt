cmake_minimum_required(VERSION 3.16)

project(Cynth
    VERSION 0.1.0
    DESCRIPTION "Small C synthesis library (MIDI import, PCM playback)"
    LANGUAGES C
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CYNTH_IS_TOP_LEVEL ON)
else()
    set(CYNTH_IS_TOP_LEVEL OFF)
endif()

option(CYNTH_BUILD_EXAMPLES "Build Cynth example programs" ${CYNTH_IS_TOP_LEVEL})

if(CYNTH_IS_TOP_LEVEL AND NOT DEFINED CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_library(ccore STATIC vendor/ccore/ccore.c)
add_library(ccore::ccore ALIAS ccore)
target_include_directories(ccore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/ccore>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set_target_properties(ccore PROPERTIES
    C_STANDARD 90
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

add_library(clog STATIC vendor/clog/src/clog.c)
add_library(clog::clog ALIAS clog)
target_include_directories(clog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/clog/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(clog PUBLIC ccore::ccore)
set_target_properties(clog PROPERTIES
    C_STANDARD 90
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

add_library(cynth
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cynth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cynth_midi.c
)
add_library(Cynth::cynth ALIAS cynth)

target_include_directories(cynth PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cynth PRIVATE ccore::ccore clog::clog)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(ALSA REQUIRED)
    target_sources(cynth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/cynth_engine_alsa.c)
    target_link_libraries(cynth PRIVATE ALSA::ALSA)
elseif(WIN32)
    target_sources(cynth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/win32/cynth_engine_win32.c)
    # TODO: target_link_libraries(cynth PRIVATE winmm) once the WinMM backend lands.
else()
    message(WARNING "Cynth: no audio backend for ${CMAKE_SYSTEM_NAME}; "
        "building without cynth_engine_* implementation.")
endif()

if(UNIX)
    target_link_libraries(cynth PRIVATE m)
endif()

set_target_properties(cynth PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_compile_options(cynth PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
)
target_compile_options(ccore PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)
target_compile_options(clog PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

if(CYNTH_BUILD_EXAMPLES)
    add_executable(cynth_example_synth ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_synth.c)
    target_link_libraries(cynth_example_synth PRIVATE Cynth::cynth ccore::ccore)
    set_target_properties(cynth_example_synth PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF)

    add_executable(cynth_example_midi ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_midi.c)
    target_link_libraries(cynth_example_midi PRIVATE Cynth::cynth ccore::ccore clog::clog)
    set_target_properties(cynth_example_midi PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(cynth_example_async ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_async.c)
        target_link_libraries(cynth_example_async PRIVATE ALSA::ALSA)
        target_compile_options(cynth_example_async PRIVATE
            $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
        )
        if(UNIX)
            target_link_libraries(cynth_example_async PRIVATE m)
        endif()
    endif()
endif()

install(TARGETS ccore clog cynth
    EXPORT CynthTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ccore/ccore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ccore/vmem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/clog/include/clog.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT CynthTargets
    FILE CynthTargets.cmake
    NAMESPACE Cynth::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cynth
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CynthConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CynthConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cynth
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CynthConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CynthConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CynthConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Cynth
)

export(
    EXPORT CynthTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/CynthTargets.cmake
    NAMESPACE Cynth::
)
