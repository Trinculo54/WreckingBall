cmake_minimum_required(VERSION 3.20)
# Bump VERSION before tagging a release (the tag build checks they match).
project(wreckingball VERSION 1.4.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Targets Windows. On Linux use -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    message(FATAL_ERROR "Build with llvm-mingw (clang): the mod relies on its MSVC-style SEH and GNU extensions together.")
else()
    add_compile_options(-Wall -Wno-missing-field-initializers -fms-extensions)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# An FH2ML v4 mod (https://github.com/Refilewski/FH2ML): FH2ML loads it from
# mods\wreckingball\ and provides every service it uses. FH2ML_v4.h is vendored
# in third_party/fh2ml (MIT, see its LICENSE).
set(WB_MOD_ID wreckingball)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/fh2_version.h)
# mod.json, from the same PROJECT_VERSION as the descriptor's WB_VERSION, so the two always match.
configure_file(cmake/mod.json.in ${CMAKE_BINARY_DIR}/mod.json @ONLY)

# The mod: mods\wreckingball\WreckingBall.dll
add_library(wreckingball SHARED src/wreckingball.cpp src/version.rc)
target_include_directories(wreckingball PRIVATE src third_party/fh2ml ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(wreckingball PRIVATE WB_VERSION="${PROJECT_VERSION}")
set_target_properties(wreckingball PROPERTIES OUTPUT_NAME WreckingBall PREFIX "")
