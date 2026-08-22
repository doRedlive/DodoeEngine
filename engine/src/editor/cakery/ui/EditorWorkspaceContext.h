// do@Redlive

#pragma once

#include "bridge/EditorBackend.h"
#include "core/EditorSession.h"
#include "services/EditorResourceLocator.h"

#include <filesystem>
#include <string>

namespace cakery {

class EditorWorkspaceContext {
public:
    EditorWorkspaceContext(EditorSession& session, EditorResourceLocator& resources)
        : m_session(session), m_resources(resources) {}

    EditorSession& session() { return m_session; }
    const EditorSession& session() const { return m_session; }
    EditorResourceLocator& resources() { return m_resources; }
    const EditorResourceLocator& resources() const { return m_resources; }

    const ProjectDescriptor& project() const { return m_session.project(); }
    BackendCapabilities capabilities() const { return m_session.capabilities(); }
    BackendStatus status() const { return m_session.status(); }
    std::string diagnostic() const { return m_session.diagnostic(); }

    bool openDocument(const std::string& documentId) { return m_session.openDocument(documentId); }
    bool execute(EditorCommandMessage command) { return m_session.execute(std::move(command)); }

private:
    EditorSession& m_session;
    EditorResourceLocator& m_resources;
};

} // namespace cakery
