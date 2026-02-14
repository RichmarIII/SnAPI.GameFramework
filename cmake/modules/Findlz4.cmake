include(FindPackageHandleStandardArgs)

if (TARGET LZ4::lz4)
    set(lz4_FOUND TRUE)
    return()
endif()

if (TARGET lz4)
    add_library(LZ4::lz4 INTERFACE IMPORTED)
    set_target_properties(LZ4::lz4 PROPERTIES INTERFACE_LINK_LIBRARIES lz4)
    get_target_property(_lz4_include_dirs lz4 INTERFACE_INCLUDE_DIRECTORIES)
    if (_lz4_include_dirs)
        set_target_properties(LZ4::lz4 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_lz4_include_dirs}")
    endif()
    set(lz4_FOUND TRUE)
    return()
endif()

find_path(LZ4_INCLUDE_DIR NAMES lz4.h)
find_library(LZ4_LIBRARY NAMES lz4)

find_package_handle_standard_args(lz4
    REQUIRED_VARS
        LZ4_INCLUDE_DIR
        LZ4_LIBRARY
)

if (lz4_FOUND AND NOT TARGET LZ4::lz4)
    add_library(LZ4::lz4 UNKNOWN IMPORTED)
    set_target_properties(LZ4::lz4 PROPERTIES
        IMPORTED_LOCATION "${LZ4_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LZ4_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(LZ4_INCLUDE_DIR LZ4_LIBRARY)
