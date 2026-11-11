#===============================================================================
# TLE92466ED driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   tle92466ed, consumed as tle92466ed::tle92466ed both through add_subdirectory() and
#   through find_package(tle92466ed) after install.
#
#   cmake -S . -B build -DTLE92466ED_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/tle92466ed_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${TLE92466ED_SOURCE_FILES}
        INCLUDE_DIRS ${TLE92466ED_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${TLE92466ED_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(TLE92466ED_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(tle92466ed_driver
    VERSION ${TLE92466ED_VERSION}
    DESCRIPTION "C++20 driver for the Infineon TLE92466ED six-channel low-side solenoid driver with integrated current control (SPI)."
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because tle92466ed.hpp includes
# "../src/tle92466ed.ipp" relative to itself:
#   <prefix>/include/tle92466ed/inc/*.hpp, tle92466ed_version.h
#   <prefix>/include/tle92466ed/src/tle92466ed.ipp
set(TLE92466ED_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/tle92466ed/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${TLE92466ED_TARGET_NAME} INTERFACE)
target_include_directories(${TLE92466ED_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${TLE92466ED_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${TLE92466ED_INSTALL_INCLUDEDIR}>
)
target_compile_features(${TLE92466ED_TARGET_NAME} INTERFACE cxx_std_20)

# tle92466ed::tle92466ed everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(tle92466ed::tle92466ed ALIAS ${TLE92466ED_TARGET_NAME})
set_target_properties(${TLE92466ED_TARGET_NAME} PROPERTIES EXPORT_NAME tle92466ed)

option(TLE92466ED_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links tle92466ed::tle92466ed" OFF)
if(TLE92466ED_ENABLE_WARNINGS)
    target_compile_options(${TLE92466ED_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(TLE92466ED_BUILD_TESTS "Build the host tests in tests/" OFF)
option(TLE92466ED_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(TLE92466ED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${TLE92466ED_TARGET_NAME} EXPORT tle92466edTargets
    INCLUDES DESTINATION ${TLE92466ED_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${TLE92466ED_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${TLE92466ED_VERSION_HEADER}" DESTINATION ${TLE92466ED_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tle92466ed/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT tle92466edTargets
    FILE tle92466edTargets.cmake
    NAMESPACE tle92466ed::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tle92466ed)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/tle92466edConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tle92466edConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tle92466ed)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tle92466edConfigVersion.cmake"
    VERSION ${TLE92466ED_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tle92466edConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tle92466edConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tle92466ed)
