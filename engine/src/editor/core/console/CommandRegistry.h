// do@Redlive

#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cakery {

class EditorSession;

struct CommandArgs {
    std::vector<std::string> positional;
    nlohmann::json named;
    nlohmann::json raw;
};

struct CommandResult {
    bool ok = true;
    std::string message;
    nlohmann::json data;

    static CommandResult Ok(std::string m = {}, nlohmann::json d = {}) {
        return {true, std::move(m), std::move(d)};
    }
    static CommandResult Err(std::string m) {
        return {false, std::move(m), {}};
    }
};

struct ParamSpec {
    std::string name;
    std::string type;
    std::string help;
    bool required = false;
};

struct CommandSpec {
    std::string name;
    std::string summary;
    std::string usage;
    std::vector<ParamSpec> params;
    bool mutating = true;
    std::function<CommandResult(EditorSession&, const CommandArgs&)> handler;
};

class CommandRegistry {
public:
    static CommandRegistry& self();

    void add(CommandSpec spec);
    bool has(const std::string& name) const;

    CommandResult execute(EditorSession& session, const std::string& line);
    CommandResult executeStructured(EditorSession& session, const std::string& name, const nlohmann::json& args);

    std::vector<CommandSpec> list() const;
    nlohmann::json toolSchema() const;
    std::string help(const std::string& name) const;

private:
    CommandRegistry() = default;
    const CommandSpec* find(const std::string& name) const;
    static void parseLine(const std::string& line, std::string& outName, CommandArgs& outArgs);

    std::vector<CommandSpec> m_specs;
};

void RegisterBuiltinCommands();

} // namespace cakery
