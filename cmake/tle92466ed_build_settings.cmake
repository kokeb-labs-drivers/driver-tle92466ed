#===============================================================================
# TLE92466ED driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(TLE92466ED_TARGET_NAME "tle92466ed")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(TLE92466ED_VERSION_MAJOR 0)
set(TLE92466ED_VERSION_MINOR 1)
set(TLE92466ED_VERSION_PATCH 0)
set(TLE92466ED_VERSION "${TLE92466ED_VERSION_MAJOR}.${TLE92466ED_VERSION_MINOR}.${TLE92466ED_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(TLE92466ED_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/tle92466ed_version.h.in")
set(TLE92466ED_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/tle92466ed_generated")
set(TLE92466ED_VERSION_HEADER "${TLE92466ED_VERSION_HEADER_DIR}/tle92466ed_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${TLE92466ED_VERSION_HEADER_DIR}")
configure_file("${TLE92466ED_VERSION_TEMPLATE}" "${TLE92466ED_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(TLE92466ED_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${TLE92466ED_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/tle92466ed.ipp, included by inc/tle92466ed.hpp.
set(TLE92466ED_SOURCE_FILES)

set(TLE92466ED_IDF_REQUIRES)
