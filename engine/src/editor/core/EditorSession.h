// do@Redlive

#pragma once

#include "bridge/EditorBackend.h"
#include "core/history/EditHistory.h"
#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"
#include "core/history/EditorHistory.h"
#include "core/EditorSelection.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace cakery {

enum class EditorSessionState {
    Created,
    OpeningProject,
    Ready,
    Degraded,
    Failed,
    Closing,
    Closed
};

class EditorSession {
public:
    explicit EditorSession(std::unique_ptr<IEditorBackend> backend);
    ~EditorSession();

    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    bool openProject(ProjectDescriptor project);
    bool openDocument(const std::string& documentPath);
    bool saveDocument(const std::string& documentPath);
    bool execute(EditorCommandMessage command);
    bool attachSceneSurface(SceneSurfaceDescriptor surface);
    void submitViewportMetrics(ViewportMetrics metrics);
    void tick();
    void shutdown();

    EditorSessionState state() const;
    const ProjectDescriptor& project() const;
    BackendCapabilities capabilities() const;
    BackendStatus status() const;
    std::string diagnostic() const;

    EditorDocumentModel& documentModel() { return m_documentModel; }
    const EditorDocumentModel& documentModel() const { return m_documentModel; }
    EditorSelection& selection() { return m_selection; }
    const EditorSelection& selection() const { return m_selection; }
    EditorHistory& history() { return m_history; }
    const EditorHistory& history() const { return m_history; }
    EditHistory& editHistory() { return m_editHistory; }
    const EditHistory& editHistory() const { return m_editHistory; }

    std::uint64_t createEntity(const std::string& name);
    bool deleteEntity(std::uint64_t uuid);
    bool renameEntity(std::uint64_t uuid, const std::string& name);
    bool addComponent(std::uint64_t uuid, const EditorComponent& component);
    bool removeComponent(std::uint64_t uuid, std::size_t nativeIndex);
    bool updateComponent(std::uint64_t uuid, std::size_t nativeIndex, const nlohmann::json& value);
    bool undo();
    bool redo();
    void notifyDocumentChanged();

private:
    bool canEditDocument() const;
    void handleBackendEvent(const BackendEventMessage& event);
    void applyTransformChange(std::uint64_t uuid, const nlohmann::json& value);

    std::unique_ptr<IEditorBackend> m_backend;
    ProjectDescriptor m_project;
    EditorSessionState m_state = EditorSessionState::Created;
    EditorDocumentModel m_documentModel;
    EditorSelection m_selection;
    EditorHistory m_history;
    EditHistory m_editHistory{m_history, m_documentModel};
    std::uint64_t m_lastViewportSequence = 0;
    bool m_surfaceAttached = false;
    bool m_hasPendingViewportMetrics = false;
    ViewportMetrics m_pendingViewportMetrics;
};

} // namespace cakery
