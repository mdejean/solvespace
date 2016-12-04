# Find the libspnav library and header.
#
# Sets the usual variables expected for find_package scripts:
#
# SPACEWARE_INCLUDE_DIR - header location
# SPACEWARE_LIBRARIES - library to link against
# SPACEWARE_FOUND - true if pugixml was found.

if(UNIX)

    find_path(SPACEWARE_INCLUDE_DIR
        spnav.h)

    find_library(SPACEWARE_LIBRARY
        NAMES spnav libspnav)

elseif(WIN32)

    find_path(SPACEWARE_INCLUDE_DIR
        si.h
        PATHS ${CMAKE_SOURCE_DIR}/extlib/si)

    find_library(SPACEWARE_LIBRARY
        NAMES siapp
        PATHS ${CMAKE_SOURCE_DIR}/extlib/si)

endif()

# Support the REQUIRED and QUIET arguments, and set SPACEWARE_FOUND if found.
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SPACEWARE DEFAULT_MSG
    SPACEWARE_LIBRARY SPACEWARE_INCLUDE_DIR)

if(SPACEWARE_FOUND)
    set(SPACEWARE_LIBRARIES ${SPACEWARE_LIBRARY})
endif()

mark_as_advanced(SPACEWARE_LIBRARY SPACEWARE_INCLUDE_DIR)

if(WIN32)
    # Test that the library links correctly
    try_compile(SPACEWARE_FOUND ${CMAKE_BINARY_DIR}/sitest
        SOURCES ${CMAKE_SOURCE_DIR}/cmake/sitest.c
        CMAKE_FLAGS "-DEXE_LINKER_FLAGS=/SAFESEH:NO"
            "-DLINK_LIBRARIES=${SPACEWARE_LIBRARY}"
            "-DINCLUDE_DIRECTORIES=${SPACEWARE_INCLUDE_DIR}")

    if (SPACEWARE_FOUND)
        message("Using prebuilt SpaceWare")
    else()
        message("Not using prebuilt SpaceWare (incompatible library)")
    endif()
endif()

