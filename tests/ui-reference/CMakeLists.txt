# SPDX-FileCopyrightText: 2026 Project LemonLime
# SPDX-License-Identifier: GPL-3.0-or-later

# Opt-in reference tool. The original checkout remains read-only.
set(LEMON_UI_REFERENCE_SOURCE_DIR "" CACHE PATH "Original QWidget checkout for UI reference captures")
if(NOT EXISTS "${LEMON_UI_REFERENCE_SOURCE_DIR}/src/lemon.cpp")
    message(FATAL_ERROR "Set LEMON_UI_REFERENCE_SOURCE_DIR to the original QWidget checkout")
endif()

file(READ "${CMAKE_SOURCE_DIR}/src/base/settings.cpp" reference_settings_source)
if(NOT reference_settings_source MATCHES "QSettings settings\\(QSettings::defaultFormat\\(\\), QSettings::UserScope")
    message(FATAL_ERROR "UI reference requires Settings to honor QSettings::defaultFormat()")
endif()

# Copy only the original UI into a build-local tree. This ensures every include of
# base/core resolves to the same current library headers, including unqualified
# includes inherited through those libraries' public include directories.
set(reference_original_src "${LEMON_UI_REFERENCE_SOURCE_DIR}/src")
set(reference_src "${CMAKE_CURRENT_BINARY_DIR}/reference-src")
file(MAKE_DIRECTORY "${reference_src}/forms" "${reference_src}/component/exportutil")
file(GLOB reference_original_sources CONFIGURE_DEPENDS
    "${reference_original_src}/*.cpp" "${reference_original_src}/*.h"
    "${reference_original_src}/component/exportutil/*.cpp"
    "${reference_original_src}/component/exportutil/*.h"
)
list(REMOVE_ITEM reference_original_sources "${reference_original_src}/main.cpp")
set(reference_sources)
foreach(original_source IN LISTS reference_original_sources)
    file(RELATIVE_PATH relative_source "${reference_original_src}" "${original_source}")
    set(copied_source "${reference_src}/${relative_source}")
    configure_file("${original_source}" "${copied_source}" COPYONLY)
    list(APPEND reference_sources "${copied_source}")
endforeach()
# MSVC decorates member symbols with their access level, so test access must be
# consistent in every translation unit rather than changed by a local macro.
file(READ "${reference_src}/lemon.h" reference_main_header)
string(REPLACE "private:" "public:" reference_main_header "${reference_main_header}")
string(REPLACE "private slots:" "public slots:" reference_main_header "${reference_main_header}")
file(WRITE "${reference_src}/lemon.h" "${reference_main_header}")
file(READ "${reference_src}/lemon.cpp" reference_main_source)
string(REPLACE
    "QSettings settings(\"LemonLime\", \"lemon\");"
    "QSettings settings(QSettings::IniFormat, QSettings::UserScope, \"LemonLime\", \"lemon\");"
    reference_main_source "${reference_main_source}")
if(reference_main_source MATCHES "QSettings settings\\(\"LemonLime\"")
    message(FATAL_ERROR "Reference window settings isolation replacement failed")
endif()
file(WRITE "${reference_src}/lemon.cpp" "${reference_main_source}")
file(GLOB reference_original_forms CONFIGURE_DEPENDS "${reference_original_src}/forms/*.ui")
set(reference_forms)
foreach(original_form IN LISTS reference_original_forms)
    get_filename_component(form_name "${original_form}" NAME)
    file(READ "${original_form}" form_source)
    string(REPLACE "../../resource.qrc" "${LEMON_UI_REFERENCE_SOURCE_DIR}/resource.qrc"
        form_source "${form_source}")
    file(WRITE "${reference_src}/forms/${form_name}" "${form_source}")
    list(APPEND reference_forms "${reference_src}/forms/${form_name}")
endforeach()
qt_add_translation(reference_qm
    "${LEMON_UI_REFERENCE_SOURCE_DIR}/translations/zh_CN.ts"
    "${LEMON_UI_REFERENCE_SOURCE_DIR}/translations/zh_TW.ts"
    "${LEMON_UI_REFERENCE_SOURCE_DIR}/translations/en_US.ts"
)
set(reference_translation_xml "<RCC><qresource prefix=\"/lang\">")
foreach(qm_file IN LISTS reference_qm)
    get_filename_component(qm_name "${qm_file}" NAME)
    string(APPEND reference_translation_xml "<file alias=\"${qm_name}\">${qm_file}</file>")
endforeach()
string(APPEND reference_translation_xml "</qresource></RCC>")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference_translations.qrc" "${reference_translation_xml}")
add_executable(lemon-ui-reference EXCLUDE_FROM_ALL
    capture.cpp
    ${reference_sources}
    ${reference_forms}


    "${LEMON_UI_REFERENCE_SOURCE_DIR}/resource.qrc"
    ${reference_qm}
    "${CMAKE_CURRENT_BINARY_DIR}/reference_translations.qrc"
)
set_target_properties(lemon-ui-reference PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    AUTOUIC_SEARCH_PATHS "${reference_src}/forms"
)
target_include_directories(lemon-ui-reference BEFORE PRIVATE
    "${reference_src}"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}"
    "${spdlog_DIR}/include"
)
target_link_libraries(lemon-ui-reference PRIVATE
    lemon-core lemon-base lemon-svg-icon
    SingleApplication::SingleApplication spdlog::spdlog
    ${LEMON_QT_LIBS}
)
target_precompile_headers(lemon-ui-reference PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")
target_compile_definitions(lemon-ui-reference PRIVATE
    UI_REFERENCE_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/screenshots"
    UI_REFERENCE_TRANSLATION="${CMAKE_CURRENT_BINARY_DIR}/zh_CN.qm"
    UI_REFERENCE_SOURCE_DIR="${LEMON_UI_REFERENCE_SOURCE_DIR}"
)
if(NOT WIN32)
    target_sources(lemon-ui-reference PRIVATE "${CMAKE_BINARY_DIR}/watcher.qrc")
    add_dependencies(lemon-ui-reference watcher_unix)
endif()
