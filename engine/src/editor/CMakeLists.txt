set(TARGET_NAME Cakery)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

set(EDITOR_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/bridge/EditorTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/bridge/EditorCommands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/bridge/EditorCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/Signal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EditorSelection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EditorSession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EditorSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/benchmark/BenchmarkRunner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/benchmark/BenchmarkRunner.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/ExecutionRecorder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/ExecutionRecorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/WorkflowTemplate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/WorkflowTemplate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/AICommandBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/AICommandBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/McpServer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/McpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/McpAutostart.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/McpAutostart.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/SelectionController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/DomainEventBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/Result.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/EditorCommandBus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/EditorCommandBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/AssetController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/AssetController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/DocumentController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/DocumentController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/PlayModeController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/PlayModeController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/EditorViewController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/EditorViewController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/ToolActionController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/ToolActionController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/ScriptWindowController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/ScriptWindowController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/RuntimeController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/application/RuntimeController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/mcp/McpHttp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/console/mcp/McpRpc.h
)
add_library(EditorCore STATIC ${EDITOR_CORE_SOURCES})
target_link_libraries(EditorCore PUBLIC nlohmann_json::nlohmann_json DodoeRuntime)
if(TARGET DodoeProfiling)
    target_link_libraries(EditorCore PUBLIC DodoeProfiling)
endif()
if(WIN32)
    target_link_libraries(EditorCore PRIVATE ws2_32)
endif()
target_include_directories(EditorCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINE_ROOT_DIR}/src
    ${ENGINE_ROOT_DIR}/src/shared
    ${ENGINE_ROOT_DIR}/src/runtime
    ${ENGINE_ROOT_DIR}/external/glm
    ${ENGINE_ROOT_DIR}/external/spdlog/include
)
set_target_properties(EditorCore PROPERTIES FOLDER "Engine/Editor")

set(EDITOR_SERVICES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/services/AssetFileOperations.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/AssetFileOperations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorResourceLocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorResourceLocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/EditorConfig.cpp
)
set(EDITOR_BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RuntimeBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RuntimeBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RuntimeBackendScene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RuntimeBackendGizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RuntimeBackendTilemap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/RuntimeBackendThumbnail.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/script/EditorScriptBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/script/EditorScriptBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/EditorCamera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/EditorCamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/AssetDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/AssetDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/FieldAttributes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/FieldAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/TileCoord.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/TileCoord.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/TilePaintService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/TilePaintService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/UuidResolve.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/UuidResolve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/PaintTilesCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/PaintTilesCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTilemapCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTilemapCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTileLayerCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTileLayerCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTilesetCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTilesetCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/RemoveTilesetCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/RemoveTilesetCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ReorderTileLayerCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ReorderTileLayerCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ResizeTilemapCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ResizeTilemapCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/UpdateTilesetCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/UpdateTilesetCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/TilemapDocumentRefs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/TilemapDocumentRefs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ReparentEntityCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ReparentEntityCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ImportSpriteCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ImportSpriteCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ImportMeshCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ImportMeshCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTilemapWithTilesetCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/CreateTilemapWithTilesetCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ImportTiledMapCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/ImportTiledMapCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/InstantiatePrefabCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/commands/InstantiatePrefabCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/services/PrefabService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/services/PrefabService.cpp
)
target_sources(EditorCore PRIVATE ${EDITOR_BACKEND_SOURCES} ${EDITOR_SERVICES_SOURCES})

set(EDITOR_UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/EditorApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/EditorApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/project_selection/ProjectConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/project_selection/ProjectConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/project_selection/ProjectManagerWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/project_selection/ProjectManagerWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/project_selection/ProjectSelectionController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/project_selection/ProjectSelectionController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/common/EditorWorkspaceContext.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/common/EditorIcons.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/common/QtStringInterop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/common/ThemeController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/common/ThemeController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/ViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/shell/ShellViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/shell/ShellViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/hierarchy/HierarchyViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/hierarchy/HierarchyViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/inspector/InspectorViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/inspector/InspectorViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/project/ProjectViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/project/ProjectViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/console/ConsoleViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/console/ConsoleViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/history/HistoryViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/history/HistoryViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/tilelayers/TileLayersViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/tilelayers/TileLayersViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/tilepalette/TilePaletteViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/tilepalette/TilePaletteViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/workflow/WorkflowViewModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/presentation/workflow/WorkflowViewModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/EditorLayoutController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/EditorLayoutController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/EditorWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/EditorWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/SceneSurface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/SceneSurface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/FramelessChrome.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell/FramelessChrome.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/inspector/EditorJsonWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/inspector/EditorJsonWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/inspector/EditorRemoteWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/inspector/EditorRemoteWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/inspector/InspectorWidgets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/HierarchyPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/HierarchyPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/ConsolePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/ConsolePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/HistoryPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/HistoryPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/WorkflowPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/WorkflowPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/widgets/WorkflowGraphView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/widgets/WorkflowGraphView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/InspectorPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/InspectorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/ProjectPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/ProjectPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/SettingsPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/SettingsPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/TileLayersPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/TileLayersPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/TilePalettePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/TilePalettePanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/TilesetPreviewDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/dialogs/TilesetPreviewDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/script/CSharpCodeEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/script/CSharpCodeEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/script/CSharpHighlighter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/script/CSharpHighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/script/CSharpSymbolIndex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/script/CSharpSymbolIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/ScriptPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/panels/ScriptPanel.cpp
)
set_source_files_properties(
    ${ENGINE_ROOT_DIR}/res/editor/translations/Cakery_en.ts
    ${ENGINE_ROOT_DIR}/res/editor/translations/Cakery_zh_CN.ts
    PROPERTIES HEADER_FILE_ONLY TRUE
)
add_library(EditorUI STATIC ${EDITOR_UI_SOURCES})
target_link_libraries(EditorUI PUBLIC EditorCore Qt6::Widgets qtadvanceddocking-qt6)
if(TARGET DodoeProfiling)
    target_link_libraries(EditorUI PUBLIC DodoeProfiling)
endif()
target_include_directories(EditorUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINE_ROOT_DIR}/src
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
        VS_DEBUGGER_COMMAND "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${product_target}>"
        FOLDER "Apps"
    )
    set(POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${product_target}>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:${product_target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${product_target}>" "${BINARY_ROOT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:qtadvanceddocking-qt6>" "${BINARY_ROOT_DIR}/$<TARGET_FILE_NAME:qtadvanceddocking-qt6>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/editor"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res/editor" "${BINARY_ROOT_DIR}/resources/editor"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/pictures"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/res/pictures" "${BINARY_ROOT_DIR}/resources/pictures"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}/resources/template"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ENGINE_ROOT_DIR}/template" "${BINARY_ROOT_DIR}/resources/template"
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

qt_add_executable(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(${TARGET_NAME}
    PRIVATE EditorUI
    PRIVATE DodoeNewDelete
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
