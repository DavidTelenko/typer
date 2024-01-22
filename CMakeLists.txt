# -- general project settings --
cmake_minimum_required(VERSION 3.21)

project(Typer)

file(GLOB_RECURSE sources src/*.cpp)

# -- external packages (essentials included) --

find_package(fmt CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)

# -- target adding target --

add_executable(${CMAKE_PROJECT_NAME} ${sources})

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE
    cxx_std_23
)

# -- os and compiler specific options --

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -fdiagnostics-color=always
        -Wall 
        -Wextra
        -Wfatal-errors
    )
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options (${CMAKE_PROJECT_NAME} PRIVATE
        -fcolor-diagnostics
        -fansi-escape-codes
        --target=x86_64-w64-mingw32
        -Wall
        -Wextra
        -Wfatal-errors
    )
endif ()


# -- linkage and inclusion --

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/libs
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    fmt::fmt-header-only
    argparse::argparse
    range-v3::meta
    range-v3::concepts
    range-v3::range-v3
)

# -- post build stage --

file(GLOB dlls ${PROJECT_SOURCE_DIR}/libs/*.dll)

if(dlls)
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${dlls}
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}> 
        COMMAND_EXPAND_LISTS
    )
endif()

# copying all data to build folder to make life easier

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/res
    $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/res
    COMMAND_EXPAND_LISTS
)


include(CPack)
