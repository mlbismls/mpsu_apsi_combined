#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "visa::volePSI" for configuration "Release"
set_property(TARGET visa::volePSI APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(visa::volePSI PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libvolePSI.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS visa::volePSI )
list(APPEND _IMPORT_CHECK_FILES_FOR_visa::volePSI "${_IMPORT_PREFIX}/lib/libvolePSI.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
