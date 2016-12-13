# Find the libspnav library and header.
#
# Sets the usual variables expected for find_package scripts:
#
# SPACEWARE_INCLUDE_DIR - header location
# SPACEWARE_LIBRARIES - library to link against
# SPACEWARE_FOUND - true if spaceware was found.

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

    if (MSVC)
        set(SPACEWARE_LINK_FLAGS /SAFESEH:NO)
    endif()

endif()

# Support the REQUIRED and QUIET arguments, and set SPACEWARE_FOUND if found.
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SPACEWARE DEFAULT_MSG
    SPACEWARE_LIBRARY SPACEWARE_INCLUDE_DIR)

if(WIN32)
    # Test that the library links correctly
    set(SPACEWARE_TEST_FILE ${CMAKE_BINARY_DIR}/CMakeTmp/sitest.c)
    file(WRITE ${SPACEWARE_TEST_FILE}
        "#include <si.h>\n"
        "#include <siapp.h>\n"
        "int main(void) {SiInitialize();SiTerminate();return 0;}")
    try_compile(SPACEWARE_FOUND ${CMAKE_BINARY_DIR}/CMakeTmp/sitest
        SOURCES ${SPACEWARE_TEST_FILE}
        CMAKE_FLAGS "-DEXE_LINKER_FLAGS=${SPACEWARE_LINK_FLAGS}"
            "-DLINK_LIBRARIES=${SPACEWARE_LIBRARY}"
            "-DINCLUDE_DIRECTORIES=${SPACEWARE_INCLUDE_DIR}")
    if (SPACEWARE_FOUND)
        message("Using prebuilt SpaceWare")
    else()
        message("Not using prebuilt SpaceWare (incompatible library)")
    endif()
endif()

if(SPACEWARE_FOUND)
    set(SPACEWARE_LIBRARIES ${SPACEWARE_LIBRARY})
    add_library(SpaceWare STATIC IMPORTED)
    set_target_properties(SpaceWare PROPERTIES
        IMPORTED_LOCATION ${SPACEWARE_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${SPACEWARE_INCLUDE_DIR})
    #INTERFACE_LINK_FLAGS doesn't exist
    set(CMAKE_EXE_LINKER_FLAGS   "${CMAKE_EXE_LINKER_FLAGS} ${SPACEWARE_LINK_FLAGS}")
endif()

mark_as_advanced(SPACEWARE_LIBRARY SPACEWARE_INCLUDE_DIR)
