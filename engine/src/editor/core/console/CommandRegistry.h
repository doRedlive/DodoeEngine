// do@Redlive
#pragma once
#include "dopch.h"

#include <functional>
#include <string>
#include <vector>

namespace cakery {

class EditorSession;

struct CommandArgs {
    DynamicArray<String> positional;
    Json named;
    Json raw;
};

struct CommandResult {
    bool ok = true;
    String message;
    Json data;

    static CommandResult Ok(String m = {}, Json d = {}) {
        return {true, std::move(m), std::move(d)};
    }
    static CommandResult Err(String m) {
        return {false, std::move(m), {}};
    }
};

struct ParamSpec {
    String name;
    String type;
    String help;
    bool required = false;
    DynamicArray<String> enumValues;
};

struct CommandContext {
    String author = "user";
    String session;
};

struct CommandSpec {
    String name;
    String summary;
    String usage;
    DynamicArray<ParamSpec> params;
    bool mutating = true;
    std::function<CommandResult(EditorSession&, const CommandArgs&)> handler;
};

class CommandRegistry {
public:
    CommandRegistry();

    void add(CommandSpec spec);
    bool has(const String& name) const;

    CommandResult execute(EditorSession& session, const String& line);
    CommandResult executeStructured(EditorSession& session, const String& name, const Json& args);
    CommandResult executeStructured(EditorSession& session, const String& name,
                                    const Json& args, const CommandContext& context);

    DynamicArray<CommandSpec> list() const;
    Json toolSchema() const;
    String help(const String& name) const;

private:
    const CommandSpec* find(const String& name) const;
    static void parseLine(const String& line, String& outName, CommandArgs& outArgs);
    CommandResult executeStructuredInternal(EditorSession& session, const String& name,
                                            const Json& args);
    CommandResult invoke(EditorSession& session, const String& name, const CommandArgs& args);

    DynamicArray<CommandSpec> m_specs;
};

void RegisterBuiltinCommands(CommandRegistry& registry);

} // namespace cakery
