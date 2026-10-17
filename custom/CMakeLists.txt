message(STATUS "Yousif Control Center: loading custom configuration")

# Include our resource list, which contains the status panel.
list(APPEND QGC_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc"
)

# Pass the updated resource list to the main build configuration.
set(QGC_RESOURCES "${QGC_RESOURCES}" PARENT_SCOPE)
