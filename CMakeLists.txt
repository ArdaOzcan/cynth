cmake_minimum_required(VERSION 3.10)
project(Cynth C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

add_compile_options(-Werror -g)

set(BUILD_EXAMPLES OFF CACHE BOOL "Disable building examples")

FetchContent_Declare(
    ccore
    GIT_REPOSITORY https://github.com/ArdaOzcan/ccore.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ccore)

FetchContent_Declare(
    clog
    GIT_REPOSITORY https://github.com/ArdaOzcan/clog.git
    GIT_TAG master
)
FetchContent_MakeAvailable(clog)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE libpulse-simple REQUIRED)

set(LIB_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cynth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cynth_midi.c
)

add_library(cynth ${LIB_FILES})

target_include_directories(cynth
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PULSE_INCLUDE_DIRS}      
)

find_package(ALSA)

target_link_libraries(cynth PUBLIC ALSA::ALSA ccore clog m ${PULSE_LIBRARIES})

add_executable(cynth_example_synth ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_synth.c)
target_include_directories(cynth_example_synth PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${PULSE_INCLUDE_DIRS}       
)
target_link_libraries(cynth_example_synth PRIVATE cynth ccore ${PULSE_LIBRARIES})

add_executable(cynth_example_midi ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_midi.c)
target_include_directories(cynth_example_midi PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${PULSE_INCLUDE_DIRS}       
)
target_link_libraries(cynth_example_midi PRIVATE cynth ccore ${PULSE_LIBRARIES})

add_executable(cynth_example_async ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_async.c)
target_include_directories(cynth_example_async PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${PULSE_INCLUDE_DIRS}       
)
target_link_libraries(cynth_example_async PRIVATE cynth ccore ${PULSE_LIBRARIES})


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
