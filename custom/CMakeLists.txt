message(STATUS "Yousif Control Center: loading custom configuration")

# Bundle our QML panels and example JSON profiles.
list(APPEND QGC_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc"
)
set(QGC_RESOURCES "${QGC_RESOURCES}" PARENT_SCOPE)

# Compile our control-center components.
list(APPEND CUSTOM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ProfileStore.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ProfileStore.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ProfilePreferences.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ProfilePreferences.cc"
)
set(CUSTOM_SOURCES "${CUSTOM_SOURCES}" PARENT_SCOPE)
