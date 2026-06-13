set(TARGET_NAME Cakery)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CAKERY_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/app/CakeryApplication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/app/MainWindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/project/ProjectConfig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/project/ProjectManagerWindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/services/CameraController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/services/EngineManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/services/LogService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/ComponentEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/DragSpinBox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/GenericComponentEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/HierarchyWidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/InspectorWidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/ProjectBrowserWidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/SceneWidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/ConsoleWidget.cpp
)

set(CAKERY_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/app/CakeryApplication.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/app/MainWindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/project/ProjectConfig.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/project/ProjectManagerWindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/services/CameraController.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/services/EngineManager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/services/LogService.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/ComponentEditor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/DragSpinBox.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/GenericComponentEditor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/HierarchyWidget.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/InspectorWidget.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/ProjectBrowserWidget.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/SceneWidget.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Cakery/widgets/ConsoleWidget.h
)

set(CAKERY_ALL_SOURCES ${CAKERY_SOURCES} ${CAKERY_HEADERS})

qt_add_executable(${TARGET_NAME} ${CAKERY_ALL_SOURCES})

target_link_libraries(${TARGET_NAME}
        PRIVATE DodoeRuntime
        PRIVATE Qt6::Widgets
        PRIVATE qtadvanceddocking-qt6
)

target_include_directories(${TARGET_NAME}
        PRIVATE
                ${ENGINE_ROOT_DIR}/src
                ${ENGINE_ROOT_DIR}/src/runtime
                ${CMAKE_CURRENT_SOURCE_DIR}/Cakery
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${ENGINE_ROOT_DIR}/external/qt-ads/src
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE ON
)

get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION)
if(_qt_core_loc AND NOT _qt_core_loc MATCHES "-NOTFOUND")
    cmake_path(GET _qt_core_loc PARENT_PATH QT6_BIN_DIR)
else()
    set(QT6_BIN_DIR "${CMAKE_PREFIX_PATH}/bin")
endif()
message(STATUS "Cakery: Qt6 binary dir = ${QT6_BIN_DIR}")

find_program(WINDEPLOYQT_EXE windeployqt HINTS "${QT6_BIN_DIR}")

set(POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${TARGET_NAME}>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:DodoeRuntime>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:DodoeRuntime>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:qtadvanceddocking-qt6>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:qtadvanceddocking-qt6>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Cakery/resources/style.qss" "${BINARY_ROOT_DIR}/resources/style.qss"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res/pictures" "${BINARY_ROOT_DIR}/resources/pictures"
)

if(WINDEPLOYQT_EXE)
    list(APPEND POST_BUILD_COMMANDS
        COMMAND ${WINDEPLOYQT_EXE}
            --$<IF:$<CONFIG:Debug>,debug,release>
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
    )
else()
    message(WARNING "windeployqt not found")
endif()

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        ${POST_BUILD_COMMANDS}
)
