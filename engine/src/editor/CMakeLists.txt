set(TARGET_NAME Cakery)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB_RECURSE FRAMEWORK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/*.h
)

add_library(EditorFramework STATIC ${FRAMEWORK_SOURCES})
target_link_libraries(EditorFramework PUBLIC DodoeRuntime)
target_compile_definitions(EditorFramework PUBLIC DODOE_EDITOR_ENABLED)
target_compile_definitions(EditorFramework PUBLIC $<$<CONFIG:Debug>:DODOE_DEBUG_ENABLED>)
target_include_directories(EditorFramework
    PUBLIC
        ${ENGINE_ROOT_DIR}/src
        ${ENGINE_ROOT_DIR}/src/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${FRAMEWORK_SOURCES})
set_target_properties(EditorFramework PROPERTIES FOLDER "Engine/Editor")

file(GLOB_RECURSE PROPERTY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/property/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/property/*.h
)

add_library(EditorProperty STATIC ${PROPERTY_SOURCES})
target_link_libraries(EditorProperty PUBLIC EditorFramework Qt6::Widgets)
target_include_directories(EditorProperty
    PUBLIC
        ${ENGINE_ROOT_DIR}/src
        ${ENGINE_ROOT_DIR}/src/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(EditorProperty PROPERTIES AUTOMOC ON)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROPERTY_SOURCES})
set_target_properties(EditorProperty PROPERTIES FOLDER "Engine/Editor")

set(CAKERY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/EditorApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/EditorWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/LayoutManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/PanelRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/project/ProjectConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/project/ProjectManagerWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/services/LogService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/widgets/DragSpinBox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/ScenePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/GamePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/HierarchyPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/InspectorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/ProjectPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/ConsolePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/TerminalPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/TilePalettePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/MemoryPanel.cpp
)

set(CAKERY_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/EditorApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/EditorWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/LayoutManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/PanelRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/project/ProjectConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/project/ProjectManagerWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/services/LogService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/widgets/DragSpinBox.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/Panel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/ScenePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/GamePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/HierarchyPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/InspectorPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/ProjectPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/ConsolePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/TerminalPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/TilePalettePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/panels/MemoryPanel.h
)

set(CAKERY_ALL_SOURCES ${CAKERY_SOURCES} ${CAKERY_HEADERS})

qt_add_executable(${TARGET_NAME} ${CAKERY_ALL_SOURCES})

target_link_libraries(${TARGET_NAME}
    PRIVATE EditorFramework
    PRIVATE EditorProperty
    PRIVATE Qt6::Widgets
    PRIVATE qtadvanceddocking-qt6
)

target_include_directories(${TARGET_NAME}
    PRIVATE
        ${ENGINE_ROOT_DIR}/src
        ${ENGINE_ROOT_DIR}/src/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/cakery
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINE_ROOT_DIR}/external/qt-ads/src
)

set_target_properties(${TARGET_NAME} PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE ON
    VS_DEBUGGER_WORKING_DIRECTORY "${BINARY_ROOT_DIR}"
    FOLDER "Apps"
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CAKERY_ALL_SOURCES})


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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/cakery/resources/style.qss" "${BINARY_ROOT_DIR}/resources/style.qss"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res/editor" "${BINARY_ROOT_DIR}/resources/editor"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res" "${BINARY_ROOT_DIR}/engine/res"
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
