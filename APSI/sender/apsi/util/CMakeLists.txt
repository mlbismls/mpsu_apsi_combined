# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
set(APSI_SOURCE_FILES ${APSI_SOURCE_FILES}
    ${CMAKE_CURRENT_LIST_DIR}/cuckoo_filter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/cuckoo_filter_table.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sm4.cpp
)

# Add header files for installation
install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/cuckoo_filter.h
        ${CMAKE_CURRENT_LIST_DIR}/cuckoo_filter_table.h
        ${CMAKE_CURRENT_LIST_DIR}/hash.h
        ${CMAKE_CURRENT_LIST_DIR}/sm4.h
    DESTINATION
        ${APSI_INCLUDES_INSTALL_DIR}/apsi/util
)

set(APSI_SOURCE_FILES ${APSI_SOURCE_FILES} PARENT_SCOPE)
