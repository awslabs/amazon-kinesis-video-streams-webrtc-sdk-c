find_path(JSMN_INCLUDE_DIRS jsmn.h)

find_library(JSMN_LIBRARY jsmn)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSMN DEFAULT_MSG
    JSMN_LIBRARY JSMN_INCLUDE_DIRS)

mark_as_advanced(JSMN_LIBRARY JSMN_INCLUDE_DIRS)
