set(TARGET_NAME Cakery)
set(PREVIEW_TARGET_NAME CakeryPreview)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

set(EDITOR_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge/EditorBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Signal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EditorSelection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EditorSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EditorSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/document/EditorDocument.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/document/EditorDocumentModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/document/EditorDocumentModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/document/EditorDocumentSerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/document/EditorDocumentSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/EditorCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/EditorCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CompositeCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CompositeCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/builtin_commands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/history/EditHistory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/history/EditHistory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/history/EditorHistory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/history/EditorHistory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/CommandRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/CommandRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/AICommandBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/AICommandBridge.cpp
)
add_library(EditorCore STATIC ${EDITOR_CORE_SOURCES})
target_link_libraries(EditorCore PUBLIC nlohmann_json::nlohmann_json)
target_include_directories(EditorCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(EditorCore PROPERTIES FOLDER "Engine/Editor")

set(EDITOR_SERVICES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorResourceLocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorResourceLocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorConfig.cpp
)
add_library(EditorServices STATIC ${EDITOR_SERVICES_SOURCES})
target_link_libraries(EditorServices PUBLIC nlohmann_json::nlohmann_json)
target_include_directories(EditorServices PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(EditorServices PROPERTIES FOLDER "Engine/Editor")

set(EDITOR_NULL_BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/null/NullEditorBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/null/NullEditorBackend.cpp
)
add_library(EditorNullBackend STATIC ${EDITOR_NULL_BACKEND_SOURCES})
target_link_libraries(EditorNullBackend PUBLIC EditorCore)
target_include_directories(EditorNullBackend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(EditorNullBackend PROPERTIES FOLDER "Engine/Editor")

set(EDITOR_RUNTIME_BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/RuntimeEditorBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/RuntimeEditorBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/EditorCamera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/EditorCamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/AssetDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/AssetDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/FieldAttributes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/FieldAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/TileCoord.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/TileCoord.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/TilePaintService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/TilePaintService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/UuidResolve.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/services/UuidResolve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/PaintTilesCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/PaintTilesCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/CreateTilemapCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/CreateTilemapCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/CreateTileLayerCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/CreateTileLayerCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/ReparentEntityCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/runtime/commands/ReparentEntityCommand.cpp
)
add_library(EditorRuntimeBackend STATIC ${EDITOR_RUNTIME_BACKEND_SOURCES})
target_link_libraries(EditorRuntimeBackend PRIVATE EditorCore EditorServices DodoeRuntime)
target_include_directories(EditorRuntimeBackend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINE_ROOT_DIR}/src
    ${ENGINE_ROOT_DIR}/src/runtime
    ${ENGINE_ROOT_DIR}/external/glm
    ${ENGINE_ROOT_DIR}/external/spdlog/include
)
set_target_properties(EditorRuntimeBackend PROPERTIES FOLDER "Engine/Editor")

set(EDITOR_UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/EditorApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/EditorApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/project_selection/ProjectConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/project_selection/ProjectConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/project_selection/ProjectManagerWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/app/project_selection/ProjectManagerWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/EditorWorkspaceContext.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/EditorIcons.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/shell/EditorWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/shell/EditorWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/inspector/EditorJsonWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/inspector/EditorJsonWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/HierarchyPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/HierarchyPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/ConsolePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/ConsolePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/HistoryPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/HistoryPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/InspectorPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/InspectorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/ProjectPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cakery/ui/panels/ProjectPanel.cpp
)
set_source_files_properties(
    ${ENGINE_ROOT_DIR}/res/editor/translations/Cakery_en.ts
    ${ENGINE_ROOT_DIR}/res/editor/translations/Cakery_zh_CN.ts
    PROPERTIES HEADER_FILE_ONLY TRUE
)
add_library(EditorUI STATIC ${EDITOR_UI_SOURCES})
target_link_libraries(EditorUI PUBLIC EditorCore EditorServices Qt6::Widgets qtadvanceddocking-qt6)
target_include_directories(EditorUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINE_ROOT_DIR}/external/qt-ads/src
)
set_target_properties(EditorUI PROPERTIES AUTOMOC ON FOLDER "Engine/Editor")

get_target_property(_qt_core_loc Qt6::Core IMPORTED_LOCATION)
if(_qt_core_loc AND NOT _qt_core_loc MATCHES "-NOTFOUND")
    cmake_path(GET _qt_core_loc PARENT_PATH QT6_BIN_DIR)
else()
    set(QT6_BIN_DIR "${CMAKE_PREFIX_PATH}/bin")
endif()
find_program(WINDEPLOYQT_EXE windeployqt HINTS "${QT6_BIN_DIR}")

function(setup_cakery_product product_target copy_runtime)
    set_target_properties(${product_target} PROPERTIES
        AUTOMOC ON
        WIN32_EXECUTABLE ON
        VS_DEBUGGER_WORKING_DIRECTORY "${BINARY_ROOT_DIR}"
        FOLDER "Apps"
    )
    set(POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${product_target}>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${product_target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:qtadvanceddocking-qt6>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:qtadvanceddocking-qt6>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/editor"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res/editor" "${BINARY_ROOT_DIR}/resources/editor"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/pictures"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res/pictures" "${BINARY_ROOT_DIR}/resources/pictures"
    )
    if(${copy_runtime})
        list(APPEND POST_BUILD_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:DodoeRuntime>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:DodoeRuntime>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res" "${BINARY_ROOT_DIR}/engine/res"
        )
    endif()
    if(WINDEPLOYQT_EXE)
        list(APPEND POST_BUILD_COMMANDS
            COMMAND ${WINDEPLOYQT_EXE}
                --$<IF:$<CONFIG:Debug>,debug,release>
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${product_target}>"
        )
    endif()
    add_custom_command(TARGET ${product_target} POST_BUILD ${POST_BUILD_COMMANDS})
endfunction()

qt_add_executable(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/products/cakery/main.cpp)
target_link_libraries(${TARGET_NAME}
    PRIVATE EditorUI
    PRIVATE EditorRuntimeBackend
    PRIVATE Qt6::Widgets
)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
setup_cakery_product(${TARGET_NAME} ON)

set(CAKERY_TRANSLATIONS
    ${ENGINE_ROOT_DIR}/res/editor/translations/Cakery_en.ts
    ${ENGINE_ROOT_DIR}/res/editor/translations/Cakery_zh_CN.ts
)
qt_add_translations(${TARGET_NAME}
    TS_FILES ${CAKERY_TRANSLATIONS}
    RESOURCE_PREFIX "/i18n"
)

qt_add_executable(${PREVIEW_TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/products/cakery_preview/main.cpp)
target_link_libraries(${PREVIEW_TARGET_NAME}
    PRIVATE EditorUI
    PRIVATE EditorNullBackend
    PRIVATE Qt6::Widgets
)
target_include_directories(${PREVIEW_TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
setup_cakery_product(${PREVIEW_TARGET_NAME} OFF)
qt_add_translations(${PREVIEW_TARGET_NAME}
    TS_FILES ${CAKERY_TRANSLATIONS}
    RESOURCE_PREFIX "/i18n"
)
