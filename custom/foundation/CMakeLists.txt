cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(YccFoundation VERSION 0.1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    include(CTest)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Sql)
add_library(YccControlCore STATIC
    ../src/domain/Contracts.cc
    ../src/configuration/ProfileRepository.cc
    ../src/recording/EventJournal.cc
    ../src/adapters/simulation/SimulatedGripper.cc
    ../src/application/ControlCore.cc
    ../src/application/ControlCore.h
    ../src/ai/AiSupervisor.cc
    ../src/ai/AiSupervisor.h
)
target_include_directories(YccControlCore PUBLIC ../src)
target_link_libraries(YccControlCore PUBLIC Qt6::Core Qt6::Sql)
set_target_properties(YccControlCore PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(YccControlCore PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(YccControlCore PUBLIC /utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Qml)
qt_add_qml_module(YccFoundationQml
    STATIC
    URI Yousif.Foundation
    VERSION 1.0
    CLASS_NAME YccFoundationQmlPlugin
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Yousif/Foundation"
    SOURCES ../src/presentation/Foundation.h ../src/presentation/Foundation.cc
)
target_link_libraries(YccFoundationQml PUBLIC YccControlCore Qt6::Qml)
target_include_directories(YccFoundationQml PUBLIC ../src/presentation)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(ycc-foundation ../tools/foundation_cli.cc)
    target_link_libraries(ycc-foundation PRIVATE YccControlCore)
endif()

if(BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_executable(ycc-foundation-tests ../tests/FoundationTest.cc)
    target_link_libraries(ycc-foundation-tests PRIVATE YccControlCore YccFoundationQml YccFoundationQmlplugin Qt6::Test)
    target_compile_definitions(ycc-foundation-tests PRIVATE
        YCC_TEST_PYTHON="${Python3_EXECUTABLE}"
        YCC_TEST_WORKER="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/ai_worker_fixture.py"
    )
    add_test(NAME YccFoundation COMMAND ycc-foundation-tests)
    set_tests_properties(YccFoundation PROPERTIES LABELS "Unit")
endif()
