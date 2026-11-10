// do@Redlive
#pragma once
#include "dopch.h"

#include "core/bridge/EditorTypes.h"
#include "core/application/AssetController.h"
#include "core/application/DocumentController.h"
#include "core/application/EditorViewController.h"
#include "core/application/PlayModeController.h"
#include "core/application/RuntimeController.h"
#include "core/application/ScriptWindowController.h"
#include "core/application/SelectionController.h"
#include "core/application/ToolActionController.h"
#include "core/application/DomainEventBus.h"
#include "core/EditorSession.h"
#include "services/EditorConfig.h"
#include "services/EditorResourceLocator.h"
#include "ui/common/ThemeController.h"

#include <filesystem>
#include <functional>
#include <string>

namespace cakery {

class EditorWorkspaceContext {
public:
    EditorWorkspaceContext(EditorSession& session, EditorResourceLocator& resources,
                           ThemeController& theme, EditorConfig& config)
        : m_session(session), m_resources(resources), m_theme(theme), m_config(config),
          m_selection(session.selection()),
          m_documents(session, session.events()), m_play(session), m_assets(session, session.events()),
          m_view(session), m_tools(session), m_scriptWindows(session), m_runtime(session) {}

    std::function<bool()> confirmUnsavedChanges;

    EditorSession& session() { return m_session; }
    const EditorSession& session() const { return m_session; }
    EditorResourceLocator& resources() { return m_resources; }
    const EditorResourceLocator& resources() const { return m_resources; }
    SelectionController& selection() { return m_selection; }
    const SelectionController& selection() const { return m_selection; }
    ThemeController& theme() { return m_theme; }
    const ThemeController& theme() const { return m_theme; }
    EditorConfig& config() { return m_config; }
    const EditorConfig& config() const { return m_config; }
    DomainEventBus& events() { return m_session.events(); }
    const DomainEventBus& events() const { return m_session.events(); }
    EditorCommandBus& commandBus() { return m_session.commandBus(); }
    DocumentController& documents() { return m_documents; }
    const DocumentController& documents() const { return m_documents; }
    PlayModeController& playMode() { return m_play; }
    const PlayModeController& playMode() const { return m_play; }
    AssetController& assets() { return m_assets; }
    const AssetController& assets() const { return m_assets; }
    EditorViewController& view() { return m_view; }
    const EditorViewController& view() const { return m_view; }
    ToolActionController& tools() { return m_tools; }
    const ToolActionController& tools() const { return m_tools; }
    ScriptWindowController& scriptWindows() { return m_scriptWindows; }
    const ScriptWindowController& scriptWindows() const { return m_scriptWindows; }
    RuntimeController& runtime() { return m_runtime; }
    const RuntimeController& runtime() const { return m_runtime; }
    EditorDocumentModel& documentModel() { return m_session.documentModel(); }
    const EditorDocumentModel& documentModel() const { return m_session.documentModel(); }

    const ProjectDescriptor& project() const { return m_session.project(); }
    BackendCapabilities capabilities() const { return m_session.capabilities(); }
    BackendStatus status() const { return m_session.status(); }
    String diagnostic() const { return m_session.diagnostic(); }

    bool openDocument(const String& documentId) { return m_session.openDocument(documentId); }
    Result execute(EditorOperation command) { return m_session.commandBus().execute(std::move(command)); }

private:
    EditorSession& m_session;
    EditorResourceLocator& m_resources;
    ThemeController& m_theme;
    EditorConfig& m_config;
    SelectionController m_selection;
    DocumentController m_documents;
    PlayModeController m_play;
    AssetController m_assets;
    EditorViewController m_view;
    ToolActionController m_tools;
    ScriptWindowController m_scriptWindows;
    RuntimeController m_runtime;
};

} // namespace cakery
