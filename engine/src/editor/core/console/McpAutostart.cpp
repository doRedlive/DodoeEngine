// do@Redlive

#include "dopch.h"

#include "McpAutostart.h"

#include "McpServer.h"
#include "core/EditorSession.h"
#include "services/EditorConfig.h"

#include <cstdlib>

namespace cakery {

McpAutostart::McpAutostart(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr || String(argv[i]) != "--mcp-port") {
            continue;
        }
        if (i + 1 >= argc || argv[i + 1] == nullptr) {
            break;
        }
        m_port = static_cast<UInt16>(std::strtoul(argv[++i], nullptr, 10));
        m_requested = true;
    }
}

void McpAutostart::startIfConfigured(EditorSession& session) const
{
    bool autostart = m_requested;
    UInt16 port = m_port;

    const Json& editor = EditorConfig::self().editorJson();
    const auto mcpIt = editor.find("mcp");
    if (mcpIt != editor.end() && mcpIt->is_object()) {
        const auto autostartIt = mcpIt->find("autostart");
        if (autostartIt != mcpIt->end() && autostartIt->is_boolean() &&
            autostartIt->get<bool>()) {
            autostart = true;
        }
        const auto portIt = mcpIt->find("port");
        if (portIt != mcpIt->end() && portIt->is_number_integer() && port == 0) {
            const long long configured = portIt->get<long long>();
            if (configured >= 0 && configured <= 65535) {
                port = static_cast<UInt16>(configured);
            }
        }
    }

    if (autostart) {
        session.mcpServer().start(session, port);
    }
}

} // namespace cakery
