#pragma once

#include <functional>
#include <string>
#include <vector>
#include "runtime/core/utils/json.h"

namespace cakery {

class EditorContext;

struct CommandArgs {
    std::vector<std::string> positional;
    dodoe::Json named;
    dodoe::Json raw;
};

struct CommandResult {
    bool ok = true;
    std::string message;
    dodoe::Json data;

    static CommandResult Ok(std::string m = {}, dodoe::Json d = {}) {
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
    std::function<CommandResult(EditorContext&, const CommandArgs&)> handler;
};

class CommandRegistry {
public:
    static CommandRegistry& self();

    void add(CommandSpec spec);
    bool has(const std::string& name) const;

    CommandResult execute(EditorContext& ctx, const std::string& line);
    CommandResult executeStructured(EditorContext& ctx, const std::string& name, const dodoe::Json& args);

    std::vector<CommandSpec> list() const;
    dodoe::Json toolSchema() const;
    std::string help(const std::string& name) const;

private:
    CommandRegistry() = default;
    const CommandSpec* find(const std::string& name) const;
    static void parseLine(const std::string& line, std::string& outName, CommandArgs& outArgs);

    std::vector<CommandSpec> m_specs;
};

} // namespace cakery
