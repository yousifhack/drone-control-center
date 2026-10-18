message(STATUS "Yousif Control Center: loading custom configuration")

# Domain, persistence, simulation and supervised local inference. No panel changes.
add_subdirectory(foundation)
list(APPEND CUSTOM_LIBRARIES YccFoundationQml YccFoundationQmlplugin)
set(CUSTOM_LIBRARIES "${CUSTOM_LIBRARIES}" PARENT_SCOPE)

# Bundle our QML panels and example JSON profiles.
list(APPEND QGC_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc"
)
set(QGC_RESOURCES "${QGC_RESOURCES}" PARENT_SCOPE)

# Compile our C++ profile store into the application.
list(APPEND CUSTOM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ProfileStore.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ProfileStore.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/QgcReadBridge.cc"
)
set(CUSTOM_SOURCES "${CUSTOM_SOURCES}" PARENT_SCOPE)
