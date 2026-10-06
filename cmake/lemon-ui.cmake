# ==================================================================================
# Lemon UI
# ==================================================================================

set(LEMON_BASEDIR_UI ${CMAKE_SOURCE_DIR}/src)

set(LEMON_UI_SOURCES
    ${LEMON_BASEDIR_UI}/main.cpp
    ${LEMON_BASEDIR_UI}/qml/appcontroller.cpp
    ${LEMON_BASEDIR_UI}/qml/appcontroller.h
    ${LEMON_BASEDIR_UI}/qml/resultmodel.cpp
    ${LEMON_BASEDIR_UI}/qml/resultmodel.h
    ${LEMON_BASEDIR_UI}/qml/taskcontroller.cpp
    ${LEMON_BASEDIR_UI}/qml/taskcontroller.h
    ${LEMON_BASEDIR_UI}/qml/settingscontroller.cpp
    ${LEMON_BASEDIR_UI}/qml/settingscontroller.h
    ${LEMON_BASEDIR_UI}/qml/contesttools.cpp
    ${LEMON_BASEDIR_UI}/qml/contesttools.h
    ${LEMON_BASEDIR_UI}/qml/controlstyle.cpp
    ${LEMON_BASEDIR_UI}/qml/controlstyle.h
    ${LEMON_BASEDIR_UI}/qml/resultdetails.cpp
    ${LEMON_BASEDIR_UI}/qml/resultdetails.h
)

file(GLOB_RECURSE LEMON_QML_FILES CONFIGURE_DEPENDS ${LEMON_BASEDIR_UI}/qml/*.qml)
foreach(qml_file IN LISTS LEMON_QML_FILES)
    file(RELATIVE_PATH qml_name ${LEMON_BASEDIR_UI}/qml ${qml_file})
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
