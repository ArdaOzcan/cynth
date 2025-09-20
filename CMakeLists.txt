cmake_minimum_required(VERSION 3.10)

project(Cynth C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE libpulse-simple REQUIRED)

set(LIB_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cynth.c
)

set(MAIN_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/main.c
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

target_link_libraries(cynth PUBLIC ccore m ${PULSE_LIBRARIES})

add_executable(cynth_example ${MAIN_FILE})
target_include_directories(cynth_example PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${PULSE_INCLUDE_DIRS}       
)
target_link_libraries(cynth_example PRIVATE cynth ccore ${PULSE_LIBRARIES})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
