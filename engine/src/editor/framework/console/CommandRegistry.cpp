#include "CommandRegistry.h"
#include <sstream>
#include <algorithm>

namespace cakery {

CommandRegistry& CommandRegistry::self() {
    static CommandRegistry instance;
    return instance;
}

void CommandRegistry::add(CommandSpec spec) {
    m_specs.push_back(std::move(spec));
}

bool CommandRegistry::has(const std::string& name) const {
    return find(name) != nullptr;
}

const CommandSpec* CommandRegistry::find(const std::string& name) const {
    for (auto& s : m_specs) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

void CommandRegistry::parseLine(const std::string& line, std::string& outName, CommandArgs& outArgs) {
    std::istringstream iss(line);
    iss >> outName;

    std::string token;
    while (iss >> token) {
        auto eq = token.find('=');
        if (eq != std::string::npos) {
            std::string key = token.substr(0, eq);
            std::string val = token.substr(eq + 1);
            outArgs.named[key] = val;
        } else {
            outArgs.positional.push_back(token);
        }
    }
}

CommandResult CommandRegistry::execute(EditorContext& ctx, const std::string& line) {
    std::string name;
    CommandArgs args;
    parseLine(line, name, args);

    const auto* spec = find(name);
    if (!spec) {
        return CommandResult::Err("Unknown command: " + name);
    }
    if (!spec->handler) {
        return CommandResult::Err("Command has no handler: " + name);
    }
    return spec->handler(ctx, args);
}

CommandResult CommandRegistry::executeStructured(EditorContext& ctx, const std::string& name, const dodoe::Json& args) {
    const auto* spec = find(name);
    if (!spec) {
        return CommandResult::Err("Unknown command: " + name);
    }
    if (!spec->handler) {
        return CommandResult::Err("Command has no handler: " + name);
    }
    CommandArgs cargs;
    cargs.raw = args;
    return spec->handler(ctx, cargs);
}

std::vector<CommandSpec> CommandRegistry::list() const {
    return m_specs;
}

dodoe::Json CommandRegistry::toolSchema() const {
    dodoe::Json tools = dodoe::Json::array();
    for (auto& s : m_specs) {
        dodoe::Json t;
        t["name"] = s.name;
        t["description"] = s.summary;
        dodoe::Json props = dodoe::Json::object();
        for (auto& p : s.params) {
            dodoe::Json prop;
            prop["type"] = p.type;
            prop["description"] = p.help;
            props[p.name] = prop;
        }
        t["parameters"] = props;
        t["mutating"] = s.mutating;
        tools.push_back(t);
    }
    return tools;
}

std::string CommandRegistry::help(const std::string& name) const {
    const auto* spec = find(name);
    if (!spec) return "Unknown command: " + name;
    std::string result = spec->summary + "\nUsage: " + spec->usage;
    return result;
}

} // namespace cakery
