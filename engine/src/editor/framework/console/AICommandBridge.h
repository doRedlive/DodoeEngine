#pragma once

#include <string>
#include <vector>
#include "runtime/core/utils/json.h"

namespace cakery {

class EditorContext;

class AICommandBridge {
public:
    explicit AICommandBridge(EditorContext& ctx) : m_ctx(ctx) {}

    std::string beginSession(const std::string& goal);
    void endSession(bool keep);

    dodoe::Json listCommands();
    dodoe::Json execute(const std::string& name, const dodoe::Json& args);
    dodoe::Json query(const std::string& name, const dodoe::Json& args);

    void setAllowlist(std::vector<std::string> names) { m_allowlist = std::move(names); }
    void setConfirmRequired(bool on) { m_confirmRequired = on; }

private:
    EditorContext& m_ctx;
    bool m_inSession = false;
    uint64_t m_sessionCheckpoint = 0;
    std::vector<std::string> m_allowlist;
    bool m_confirmRequired = false;

    bool isAllowed(const std::string& name) const;
};

} // namespace cakery
