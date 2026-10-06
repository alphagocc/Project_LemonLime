# SPDX-FileCopyrightText: 2026 Project LemonLime
# SPDX-License-Identifier: GPL-3.0-or-later

find_package(${LEMON_QT_LIBNAME} ${LEMON_QT_MIN_VERSION} COMPONENTS Test REQUIRED)

set(QML_UI_TEST_SOURCES ${LEMON_UI_SOURCES})
list(REMOVE_ITEM QML_UI_TEST_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_executable(tst_qmlui
    tst_qmlui.cpp
    ${QML_UI_TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/resource.qrc
    ${LEMON_EMBED_TRANSLATION_QRC}
    ${QM_FILES}
)
if(NOT WIN32)
    target_sources(tst_qmlui PRIVATE ${CMAKE_BINARY_DIR}/watcher.qrc)
    add_dependencies(tst_qmlui watcher_unix)
endif()

foreach(qml_file IN LISTS LEMON_QML_FILES)
    file(RELATIVE_PATH qml_name ${CMAKE_SOURCE_DIR}/src/qml ${qml_file})
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
qt_add_qml_module(tst_qmlui
    URI LemonLime
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/LemonLime
    QML_FILES ${LEMON_QML_FILES}
)

target_include_directories(tst_qmlui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/qml
    ${CMAKE_BINARY_DIR}
    ${spdlog_DIR}/include
)
target_precompile_headers(tst_qmlui PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)
target_link_libraries(tst_qmlui PRIVATE
    lemon-core lemon-base lemon-svg-icon
    SingleApplication::SingleApplication spdlog::spdlog
    ${LEMON_QT_LIBS}
    ${LEMON_QT_LIBNAME}::Test
    ${LEMON_QT_LIBNAME}::Qml
    ${LEMON_QT_LIBNAME}::Quick
    ${LEMON_QT_LIBNAME}::QuickControls2
)
target_compile_definitions(tst_qmlui PRIVATE
    QML_UI_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/screenshots"
)
add_test(NAME qml-ui COMMAND tst_qmlui -o qml-ui-results.txt,txt -o -,txt)
if(WIN32)
    set(qml_test_platform windows)
    set(qml_test_rendering "QSG_RHI_BACKEND=d3d11;QT_QUICK_BACKEND=")
else()
    set(qml_test_platform offscreen)
    set(qml_test_rendering "QT_QUICK_BACKEND=software")
endif()
set_tests_properties(qml-ui PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    ENVIRONMENT "QT_QPA_PLATFORM=${qml_test_platform};${qml_test_rendering};QT_FORCE_STDERR_LOGGING=1;QT_ASSUME_STDERR_HAS_CONSOLE=1"
    TIMEOUT 180
)
